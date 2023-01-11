# PIR-sensor-enabled-pedestrian-tracking-system

**:closed_book:Introduction**

Passive infrared (PIR) sensors are motion sensors that detect IR stimuli emitted by heating bodies and convert them into electrical outputs. They are used in a variety of applications, such as home automation, surveillance systems, and human trajectory analysis since they are low-cost, reliable and anonymous compared to other sensors like camera and Bluetooth tags, which are relatively expensive and privacy-invasive. 

PIR sensors are categorized into digital and analog sensors, and they both have their advantages and disadvantages.  Digital PIR sensors are the more common option that outputs 1 when motions are detected and 0 otherwise. A considerable number of applications that leverage their binary outputs since they require less effort to process and off-the-shelf digital PIR sensors are easy to access due to its prevalence. However, the digital characteristics also narrow the information availability to users – digital PIR sensors can reflect nothing more than the presence of motions. Whereas its analog counterpart can not only sense motions, but also provide direction, velocity, and distance of pedestrians relative to the sensor by giving different outputs, which requires multiple digital PIR sensors to work synergistically to collect the same information. As informative as analog PIR sensors can be, they require additional amplification since the raw analog outputs are not prominent enough to be observed and processed directly by microcontrollers. Despite the implication of an additional amplifier, we still prefer analog PIR sensors since they are not only more informative, but they are also more energy-efficient in contrast to the digital counterpart when requiring the same capabilities.

To enable more people to benefit from analog PIR sensors, we designed a sensing node that pivots around an analog PIR sensor and included a GPS, SD card module, Arduino UNO, a divided cone, and a power supply system. In the sensing node, we have a plastic circuit board (PCB) that harbors the amplifier for the analog output, SD card module for data storage, and GPS capability to document information captured by the PIR sensor chronologically. The PCB also serves as a shield that sits atop an Arduino UNO so that users can expand the functionalities of the PCB of their own accord. Additionally, we also devised a cone with a divider that increases the sensitivity of the sensor. All components reside in a water-proof enclosure and are powered by a battery, which is sustainably charged by a solar panel.

In this GitHub page, we have a sensor node building manual, bill of materials, design files, and code in different branches. Please refer to the index section for further information for the purposes of each file.

**:scroll:Index**

- Pedestrian tracking system sensing node assembling manual: A step-by-step instruction that goes through the process of building a sensing node pivoting around an analog PIR sensor.
- Bill of materials: A list that includes all the materials needed to build a sensing node.
- Design files:
  - Pedestrian tracking system: A compressed folder that contains the Gerber files for the Arduino shield.
  - Cone and tube stl files: The stl files that can be sent to a 3D printing manufacturer to print out the cone and the tube.
- Pedestrian tracking system code: The code that operates to document direction of travel of passengers.
