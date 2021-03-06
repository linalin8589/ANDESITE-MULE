// ========
// Includes
// ========

#include "AndesiteMule.h"
#include "AndesiteRadio.h"
#include "libandesite.h"

#include <SPI.h>
#include <Wire.h>
#include <RF22Mesh.h>
#include <SdFat.h>
#include <SFE_LSM9DS0.h> 
#include <TinyGPS.h>


// ========
// Declares
// ========

// Create instance of Radio and Sd card
RF22Mesh RF22(ACDH_MULE_ADDR, 9);
SdFat SD;

AndesiteRadio _Radio;
AndesiteMule _Mule;

// ////////////////////
// ///// RUN MULE ///// 
// ////////////////////

// Setup the Mule 
void setup() {
    // Setup serial connection rate
    delay(500);
    Serial.begin(ACDH_SERIAL_BAUD);
    delay(500);
    Serial.flush();
    delay(500);
    _Mule.message = "";
    //_Mule.nextChar = Serial.read();
    //Serial.println("before connection");
    while(_Mule.message != "initSuccess\n"){
      if(Serial.available() > 0){
        _Mule.message = Serial.readString();
        //Serial.print(_Mule.message);
      }
      delay(100);
    }
    Serial.println("initSuccess");
    _Mule.message = "";
    while(_Mule.message != "radioInit\n"){
      if(Serial.available() > 0){
        _Mule.message = Serial.readString();
      }
      delay(1000);
    }
    if ( _Mule.radioInit() != 0 ) {
        //Serial.println(":: Mule initialization failed, fix errors and try again");
        //while (1) {}
    }
    //Serial.println("Done with main setup.");
}

// Receive messages from the wireless sensor nodes (WSN)
void loop() {
  if(Serial.available() > 0){
    _Mule.message = "";
    _Mule.message = Serial.readString();
    //Serial.println(_Mule.message);
    _Mule.processMessage(_Mule.message);
  }
  _Mule.listen();
  _Mule.request(); // Has a return status
}
