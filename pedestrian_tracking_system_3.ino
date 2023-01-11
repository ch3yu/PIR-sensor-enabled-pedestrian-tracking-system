/*****************************************************************************************************************************
 * pedestrian_tracking_system_3 is for the second appointment for the pedestrain tracking system implemented in Mellon Park.
 * It detects pedestrians' travel directions by leveraging the property of an analog PIR sensor that the output waveforms are 
 * different with respect to different travel directions.
 * 
 * Arduino expects analog signal from a PIR sensor and logs the time information and travel direction onto a SD card once it
 * senses passings of pedestrians.
 * 
 * Author: Cheyu Lin
 *****************************************************************************************************************************/

// Include libraries for GPS, time management, and SD card modules
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <SD.h>
#include <SPI.h>

# define baud 9600   // Baud rate for serial input and output
# define warmup_pir 120000  // Warm up time for PIR sensor - 120000 milliseconds
# define mapping_bit 1024  // mapping_bit and mapping_volt are for ADC conversions
# define mapping_volt 5
# define daylight_saving_2am 2  // The hour when daylight saving happens
# define time_diff_4hrs -240
# define time_diff_5hrs -300
# define delay_2sec 2000  // 2000 milliseconds
# define delay_10sec 10000  // 10000 milliseconds

// Instance declaration
SoftwareSerial ss(7, 8);
File myFile;
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, daylight_saving_2am, time_diff_4hrs};  //UTC - 4 hours (240 mins)
TimeChangeRule usEST = {"EST", First, Sun, Nov, daylight_saving_2am, time_diff_5hrs};   //UTC - 5 hours (300 mins)
Timezone usEastern(usEDT, usEST);


// GPIO pin assignment
int pir = A1;  // Reading analog signal from a PIR sensor
int gps_wake = 6;
int pinCS = 10;
int led1 = A2;
int led2 = A3;
int gps_signal = A4;

// Variable declaration
char pirPosition[2] = "L";
float threshold_high = 3.0;
float threshold_low = 2.0;
float input;
float val;
unsigned long time_window = 1000; // milliseconds
unsigned long dead_indicator = 0;  // the byte position in the SD card file to log time information to track the status of the battery
unsigned long dead_interval = 900;
unsigned long dead_timer = 0;
time_t eastern, utc;

int recalibrate_hr = 8;
int recalibrate_min = 0;
int recalibrate_sec1 = 0;
int recalibrate_sec2 = 2;


/*****************************************************************************************************************************
 * pir_struct is used to track the progression of a PIR wave.
 * 
 * Below are the explanation for different fields:
 *  time_first: Documents the time when the signal crosses either the high threshold (3 volts) or the low threshold (2 volts)
 *  time_second: Keeps being updated until a wave finishes.
 *  flag_trigger: Tracks the progression of a PIR wave.
 *  
 * With time_first and time_second, we can check the status of the PIR sensor. Under normal circumstances, a PIR wave finishes
 * within 1 second. If most signals take more than 1 second to end, it indicates a need to replace the current sensor.
 *****************************************************************************************************************************/
struct pir_struct{
  unsigned long time_first;
  unsigned long time_second;
  int flag_trigger;
};

// Initialize pir struct
pir_struct pir_trigger;


void setup() {
  Serial.begin(baud);
  ss.begin(baud);

  // Declare modes of all pins.
  pinMode(pir, INPUT);
  pinMode(gps_wake, OUTPUT);
  pinMode(pinCS, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(gps_signal, OUTPUT);

  // Capture time information
  getTime();

  // Check if SD card module is operating correctly
  if(SD.begin(pinCS)){
    Serial.println(F("SD card is ready to use."));
  }
  else{
    Serial.println(F("SD card initialization failed."));
  }

  // Initialize a pir_struct struct
  pir_trigger.flag_trigger = 0x0;
  pir_trigger.time_first = 0;
  pir_trigger.time_second = 0;

  // Open the the file in the SD card.
  myFile = SD.open("test.txt", O_RDWR | O_CREAT);
  // Check if file is opened sucessfully.
  if(myFile)
  {
    // Check if it is an empty file, namely new SD card.
    myFile.seek(myFile.size());
    if(!myFile.available())
    {
      Serial.println(F("No file in the SD card!"));
    }
    else
    {
      Serial.println(F("A file is found in the SD card!"));
    }
  }
  else
  {
    Serial.println(F("Error opening test.txt!"));
  }
  // Close the opened file to ensure data is physically stored in the SD card.
  myFile.close(); 
  
  logSD(1);
  logSD(2);

  delay(warmup_pir);
}

void loop() {
  // Recalirbrate at designated time every day
  eastern = changeTimeZone();
  if((hour(eastern) == recalibrate_hr) && (minute(eastern) == recalibrate_min) && (second(eastern) >= recalibrate_sec1) && (second(eastern) <= recalibrate_sec2)){
    getTime();
  }
  
  // Update the time stamp below "Start" every 15 minutes
  if((now() - dead_timer) >= dead_interval){
    dead_timer = now();
    logSD(2);
  }


/*****************************************************************************************************************************
 *            __                                           __
 * 3.0V___0x1/  \0x2__________________________________0X40/  \0X80________
 *          /    \                                       /    \
 * 2.5V----/      \      /-----------------------\      /      \----------
 *     ____________\    /_________________________\    /__________________
 * 2.0V          0x4\__/0x8                    0X10\__/0X20
 *                  
 *         <Left to right>                        <Right to left>   
 *         
 * When motions are detected, an analog PIR sensor produces different signals
 * depending on the motions, which are a wave offset by 2.5V with the wave crest
 * comes first if an heating body moves from left to right and a wave with a
 * preceding wave trough otherwise respectively. To differentiate the two directions,
 * we track the progression of a wave by tagging points that cross the 2V or 3V
 * threshold as in the graph above - a left to right travel will be recorded
 * only when a wave gets tagged with 0x1, 0x2, 0x4, and 0x8, and a right to left travel
 * will be documented only when a wave goes through 0x10, 0x20, 0x40, and 0x80
 * completely. A condition statement that quits the tracking if a wave stays at either
 * 0x2 or 0x20 over two seconds is set to ensure the code doesn't grow stagnant
 * halfway through a wave.
 *****************************************************************************************************************************/
  // Read analog PIR signal
  input = analogRead(pir);
  // ADC conversion
  val = input / mapping_bit * mapping_volt;

  // Check if two triggers span over 1 seconds
  if(pir_trigger.flag_trigger == 0x2 || pir_trigger.flag_trigger == 0x20)
  {
   pir_trigger.time_second = millis();
   if(pir_trigger.time_second > time_window + pir_trigger.time_first)
   {
    pir_trigger.time_first = 0;
    pir_trigger.time_second = 0;
    pir_trigger.flag_trigger = 0x0;
   }
  }
  
  // Trigger if pir gives voltage greater than 3 volts
  if(val >= threshold_high)
  {
    if(pir_trigger.flag_trigger == 0x0)
    {
      pir_trigger.flag_trigger = 0x1;
      pir_trigger.time_first = millis();
      pir_trigger.time_second = millis();
    }
    else if(pir_trigger.flag_trigger == 0x1) pir_trigger.flag_trigger = 0x2;
    else if(pir_trigger.flag_trigger == 0x20) pir_trigger.flag_trigger = 0x40;
    else if(pir_trigger.flag_trigger == 0x40) pir_trigger.flag_trigger = 0x80;
    else if(pir_trigger.flag_trigger == 0x80)
    {
      while(val >= threshold_high)
      {
        input = analogRead(pir);
        val = input / mapping_bit * mapping_volt;
      }
      pir_trigger.flag_trigger = 0x0;
      pir_trigger.time_first = 0;
      pir_trigger.time_second = 0;
      logSD(4);
      digitalWrite(led1, HIGH);
      delay(delay_2sec);
      digitalWrite(led1, LOW);
    }
  }
  
  // Trigger if pir gives voltage less than 2 volts
  else if(val < threshold_low)
  {
    if(pir_trigger.flag_trigger == 0x0)
    {
      pir_trigger.flag_trigger = 0x10;
      pir_trigger.time_first = millis();
      pir_trigger.time_second = millis();
    }
    else if(pir_trigger.flag_trigger == 0x10) pir_trigger.flag_trigger = 0x20;
    else if(pir_trigger.flag_trigger == 0x2) pir_trigger.flag_trigger = 0x4;
    else if(pir_trigger.flag_trigger == 0x4) pir_trigger.flag_trigger = 0x8;
    else if(pir_trigger.flag_trigger == 0x8)
    {
      while(val <= threshold_low)
      {
        input = analogRead(pir);
        val = input / mapping_bit * mapping_volt;
      }
      pir_trigger.flag_trigger = 0x0;
      pir_trigger.time_first = 0;
      pir_trigger.time_second = 0;
      logSD(3);
      digitalWrite(led2, HIGH);
      delay(delay_2sec);
      digitalWrite(led2, LOW);
    }    
  }
}


/*
 * Functions
 */
/*****************************************************************************************************************************
 * getTime is used to capture time information from the gps - GP635 and update the time in Arduino. Specifically, we extract 
 * GPRMC, which is one of the NMEA messages. 
 * 
 * First, we wait until the GPS information is valid by waiting till the status byte in GPRMC message becomes "A". We then extract
 * time and date by counting commas. Time information comes after the first comma, whereas date information follows the ninth comma.
 * 
 * @param void
 * @return void
 * *****************************************************************************************************************************/
void getTime(){
  static const unsigned int MAX_MESSAGE_LENGTH = 67;
  bool TIME_CHECK = false;
  // Ping gps to turn it on
  digitalWrite(gps_wake, HIGH);
  // Delay 10 seconds to get rid of gibberish
  // delay(delay_10sec);
  while(!TIME_CHECK){
    // Check to see if anything is available to in the serial receive buffer
    while(ss.available() > 0){
      // Create a place to hold the incoming message
      static char message[MAX_MESSAGE_LENGTH];
      static unsigned int message_pos = 0;

      // Read the next available byte in the serial reciver buffer
      char inByte = ss.read();

      // Message coming in (check not terminating character)
      if(inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1)){
        // Add the coming byte to our message
        message[message_pos] = inByte;
        message_pos++;
      }
      // Full message received
      else{
        // Check if the full message is '$GPRMC,' or not
        if((message[0] == '$') && (message[1] == 'G') && (message[2] == 'P') && (message[3] == 'R') && (message[4] == 'M') && (message[5] == 'C') && (message[6] == ',')){
          Serial.println(message);
          // Check the validity of the data by looking at the status byte
          if(message[17] == 'A'){
            int message_cnt = 0;
            int comma_cnt = 0;
            int hr, mins, secs, yr, mth, dayy;

            // Convert GPS characters into numerical values
            while(message_cnt < MAX_MESSAGE_LENGTH){
              if(message[message_cnt] == ','){
                comma_cnt++;
              }
              if(comma_cnt == 1){
                message_cnt++;
                hr = (int(message[message_cnt++]) - 48) * 10;
                hr = hr + (int(message[message_cnt++]) - 48);
                mins = (int(message[message_cnt++]) - 48) * 10;
                mins = mins + (int(message[message_cnt++]) - 48);
                secs = (int(message[message_cnt++]) - 48) * 10;
                secs = secs + (int(message[message_cnt]) - 48);
                comma_cnt++;
              }
              if(comma_cnt == 10){
                message_cnt++;
                dayy = (int(message[message_cnt++]) - 48) * 10;
                dayy = dayy + (int(message[message_cnt++]) - 48);
                mth = (int(message[message_cnt++]) - 48) * 10;
                mth = mth + (int(message[message_cnt++]) - 48);
                yr = (int(message[message_cnt++]) - 48) * 10;
                yr = yr + (int(message[message_cnt]) - 48);
                comma_cnt++;
              }
              message_cnt++;
            }
            // Set system time
            setTime(hr, mins, secs, dayy, mth, yr);
            TIME_CHECK = true;
            Serial.print(F("Year: ")); Serial.println(yr);
            Serial.print(F("Month: ")); Serial.println(mth);
            Serial.print(F("Day: ")); Serial.println(dayy);
            Serial.print(F("Hour: ")); Serial.println(hr);
            Serial.print(F("Minute: ")); Serial.println(mins);
            Serial.print(F("Second: ")); Serial.println(secs);
            
            // Turn gps off
            digitalWrite(gps_signal, HIGH);
            digitalWrite(gps_wake, LOW);
            // Flush all the data in the serial buffer
            while(ss.available() > 0){
              ss.read();
            }
            // Set all the data in the message to '0'
            memset(message, '0', MAX_MESSAGE_LENGTH);
          }
        }
        message_pos = 0;
      }
    }
  }
}

/*****************************************************************************************************************************
 * changeTimeZone changes the time zone from UTC+0 to UTC-5 (or UTC-4 during daylight saving). 
 * 
 * @param void
 * @return void
 * *****************************************************************************************************************************/
time_t changeTimeZone(){
 utc = now();
 return usEastern.toLocal(utc);
}

/*****************************************************************************************************************************
 * logSD logs data onto the file in the SD card depending on the input option:
 *  Option 1: Write "Start" at the end of each file every time Arduino is rebooted.
 *  Option 2: Write time below the latest "Start" and update it every 15 mins to document battery status. With this functionailty, 
 *            we can determine the time when the battery was out - it would be within 15 mins of the latest updated time. Additionally, 
 *            we can deduce the down time of the battery by comparing two times.
 *  Option 3: Log the time and direction of a pedestrian traveling from right to left.
 *  Option 4: Log the time and direction of a pedestrian traveling from left to right.
 * 
 * @param void
 * @return void
 * *****************************************************************************************************************************/
void logSD(int option){
  eastern = changeTimeZone();
  myFile = SD.open("test.txt", O_RDWR | O_CREAT);
  if(myFile){
    myFile.seek(myFile.size()); 
    if(option == 1){
      myFile.write("START\n");
      dead_indicator = myFile.size();
      Serial.println(F("Start."));
    }
    else if(option == 2){
      myFile.seek(dead_indicator);
      char str_time[25];
      sprintf(str_time, "%02d,%02d,%04d,%02d,%02d,%02d\n", month(eastern), day(eastern) , year(eastern), hour(eastern) ,minute(eastern), second(eastern));
      myFile.write(str_time);
      Serial.println(str_time);
    }
    else if(option == 3){
      myFile.seek(myFile.size());
      strcpy(pirPosition, "L");
      char str[25];
      sprintf(str, "%s,%02d,%02d,%04d,%02d,%02d,%02d\n", pirPosition, month(eastern), day(eastern), year(eastern), hour(eastern), minute(eastern), second(eastern));
      myFile.write(str);
      Serial.println(str);
    }
    else if(option == 4){
      myFile.seek(myFile.size());
      strcpy(pirPosition, "R");
      char str[25];
      sprintf(str, "%s,%02d,%02d,%04d,%02d,%02d,%02d\n", pirPosition, month(eastern), day(eastern), year(eastern), hour(eastern), minute(eastern), second(eastern));
      myFile.write(str);
      Serial.println(str);
    }
  }
  else{
    Serial.println(F("Error opening test.txt."));
  }
  // Close the opened file to ensure data is physically stored in the SD card
  myFile.close();
}
