// **********************************************************************************
// 
// Name:    AndesiteOrbit.h
// Class:   <AndesiteOrbit>
// Author:  Gabriel Gonzalez
// 
// Description: The skeleton for the Wireless Sensore Nodes (WSN) that are part of 
//              the ANDESITE Satellite.
//              
// Notes: None.
// 
// **********************************************************************************


// ========
// Includes
// ========

// Includes
#include "AndesiteWSN.h"
#include "AndesiteData.h"
#include "AndesiteCollect.h"
#include "AndesiteRadio.h"
#include "AndesiteOrbit.h"
#include "AndesiteFile.h"
#include "libandesite.h"



// //////////////////////////////
// ///// ANDESITE WSN CLASS /////
// //////////////////////////////

void(* resetFunc) (void) = 0;

// Initialize the wireless sensor node class
AndesiteWSN::AndesiteWSN() {
    _science   = (unsigned long) 20 * 60 * 1000; // [min]
    _transfer  = (unsigned long) 70 * 60 * 1000; // [min]
    _magfreq   = (unsigned long) 33 * 1000;      // [us] microsec
    _ggfreq    = (unsigned long)100 * 1000;      // [us] microsec
    _orb_start = false;
}



// Setup wireless sensor nodes
int AndesiteWSN::init() {
	
    // Turn on radios
    pinMode(ACDH_SS_PIN, OUTPUT);
    pinMode(4, OUTPUT);    
    digitalWrite(4, LOW);
    delay(100);
    
    // Setup ADC
    // acdh_adc_setup();
    
    // Initialize SD card
    if ( !SD.begin(ACDH_SD_PIN) ) {
        Serial.println("ERROR: SD card initialization failed.");
        SD.initErrorHalt();
        delay(10000);
        resetFunc();
	}
	
    // Initialize radio 
    if ( _Radio.init() != 0 ) {
        Serial.println("ERROR: Radio setup failed.");
        delay(10000);
        resetFunc();
    }
	
    // Setup science instruments
    DOF.begin();
    Wire.begin();
    
    // Setup LEDs
    acdh_init_led();
	
    // Set number of the previous orbit completed to zero
    _File.init();
    
    // Set led for waiting
    acdh_led_set(1);
    
	return 0;
}



// // /////////////////////////
// // ///// MISSION MODES /////
// // /////////////////////////

// Check if science mode should be entered
boolean AndesiteWSN::isScienceMode() {
    //set state to science mode
    state = 's';

	// Print stuff
    Serial.print("\nOrbit #");
    Serial.println(_Orbit.getOrbit());
	
	
    // Check for simulated GPS string-in from the computer
    if ( !_Orbit.setLatitude() && !_orb_start ) {
        Serial.print(":: Waiting for GPS string from computer...");
        while ( !_Orbit.setLatitude() ) {}
        Serial.println("Done.");
        _orb_start = true;
    }
    
    //make sure there is enough battery for science mode
    if(!batteryCheck()){
        return false
    }
        
    // Make sure latitude is above the 65th, and that the file position is 0
    bool orb_check  = (_Orbit.getLatitude() >= _Orbit.getModeSwitchLatitude());
    
    // Check that file does not already have data in it (true if empty)
    bool file_check = true;
        
    if ( SD.exists(_File.name().c_str()) ) {
        File handle = SD.open(_File.name().c_str(), O_READ);
         
        if ( !handle ) 
            return true;
        
        file_check = !(handle.size() > 20);
        
        Serial.print("Le File Size: ");
        Serial.println(handle.size());
        Serial.print("Le Bool: ");
        Serial.println(file_check);
        
        handle.close();
    }
    
    Serial.print("Le Both: ");
    Serial.println((orb_check && file_check));
    
    return ( orb_check && file_check );
}



// Start Science mode
int AndesiteWSN::scienceMode() {
    Serial.println(":: Entering Science mode...");
    Serial.print("Writing to file: ");
    Serial.println(_File.name());
    acdh_led_set(2);
	
    double timer_run          = micros();    
    unsigned long timer_mag   = 0;
    unsigned long timer_gg    = 0;
    unsigned long timer_now   = 0;
    unsigned long timer_lat   = millis();
    unsigned long timer_start = millis();
	uint32_t count_mag        = 0;
    uint32_t count_gg         = 0;
    boolean recently_wrote    = false;
    
    // ADC counter
    // uint8_t count_adc = 0;
    
    // Enter Science Mode when latitude is ABOVE a certain threshold	
    while ( _Orbit.getLatitude() >= _Orbit.getModeSwitchLatitude() ) {
        
		// // chip select and slave select
        // if ( !count_adc ) { 
        //     // acdh_adc_setup();
        //     count_adc = 1;
        // }
        
        // Enter low power mode when in Science Mode for 30 min
        if ( (millis() - timer_start) >= _science ) {
            Serial.println("Science Timeout.");
            delay(2000);
            _File.write();
            _File.write("\n");
            return 1;
        }
        
        // Collect magnetometer data
        if ( ((timer_now=micros()) - timer_mag) >= _magfreq ) {
            AndesiteCollect::mag();
            timer_mag = timer_now;
			++count_mag;
        }
        
        // Collect gyroscope and gps data
        if ( ((timer_now=micros()) - timer_gg) >= _ggfreq ) {
            AndesiteCollect::gyro();
            AndesiteCollect::gps();
            timer_gg = timer_now;
			++count_gg;
        }
		
        if ( _File.status() ) {
            _File.write();
            recently_wrote = true;
        } else
            recently_wrote = false;
        
        // Read in GPS coordinates
        if ( (millis() - timer_lat) >= 1000 ) {
            timer_lat = millis();
            
            if ( !_Orbit.setLatitude() ) { Serial.println("No GPS found."); }
        }
    }
	
    // Close SD card file after finished gathering data
    Serial.println(":: Done with Science Mode.");
    if ( !recently_wrote )
        _File.write();
    _File.write("\n");
    
    Serial.print("Science Mode: ");
	timer_run = (micros() - timer_run) / 1e6;
    Serial.print(timer_run);
    Serial.println(" sec");
	
	// Display sampling rate
	double Hm  = (double) count_mag / timer_run;
	double Hgg = (double) count_gg  / timer_run;
	
	Serial.print("Mag Sample Rate = ");
	Serial.print(Hm);
	Serial.println(" Hz");
	Serial.print("GG Sample Rate = ");
	Serial.print(Hgg);
	Serial.println(" Hz");
	
    return 0;
}



// Send confirmation
boolean AndesiteWSN::isTransferMode() {
    
    //set state to data transfer mode
    state = 't';

    //check if there is sufficient battery for date transfer mode
    if(!batteryCheck()){
        return false;
    }

    // Check current position (latitude)
    if ( _Orbit.getLatitude() >= _Orbit.getModeSwitchLatitude() ) {
        Serial.print("Latitude is still in science mode");
        return false;
    }
    
    // Check data file sent status
    if ( _Orbit.readHeader(_File.name(), 2) == 1 ) {
        Serial.print("File has already been completed: ");
        Serial.println(_File.name());
        return false;
    }
    
    // Check file size
    if ( _File.size() < 20 ) {
        Serial.print("File size is too small to send: ");
        Serial.println(_File.size());
        return false;
    }
    
    // Send finished with Science mode message (had listen timeouts)
    Serial.print(":: Sending done collecting data message to Mule...");
    
    return _Orbit.waitTransferStartCmd();
}



// Start Transfer mode
int AndesiteWSN::transferMode() {
    Serial.println(":: Entering Transfer mode...");
    acdh_led_set(4);
    double timer_start = millis();
    int num = 0;
    
    if ( _File.send() ) {
        num = 1;
        _Orbit.setStatus(num);
    } else {
        num = 0;
        _Orbit.setStatus(num);
    }
    
	Serial.print("File Transfer: ");
	timer_start = (millis() - timer_start) / (double) 1000;
	Serial.print(timer_start);
	Serial.print(" sec");
	Serial.print(" | ");
	Serial.println(num);
    
    File handle = SD.open(_File.name().c_str(), O_WRITE);
    _Orbit.writeHeader(handle);
    handle.close();
    
    return 0;
}



// Wait for orbit to finish
void AndesiteWSN::wait() {
    
    // This function always returns true...(maybe doesn't need to be boolean?)
    if ( !_Orbit.waitOrbitFinish() ) return;
    
    Serial.print("Using info file: ");
    Serial.println(_File.name());
    Serial.print("Orbit is: ");
    Serial.println(_Orbit.getOrbit());
    
    // Update orbit values
    if ( _Orbit.update(_File.name()) ) {
        
        // Update file name
        Serial.print("Before file name: ");
        Serial.println(_File.name());
        
        _File.set();
        
        Serial.print("After file name: ");
        Serial.println(_File.name());
        
        // Write new header to new file
        File handle = SD.open(_File.name().c_str(), O_CREAT | O_WRITE);
        _Orbit.writeHeader(handle);
        handle.close();
    }
    
    // _File.setLines(0);
}
bool AndesiteWSN::BatteryCheck(){
    Serial.println("in battery check");
    batteryLevel = 100;  //edit to get this value from actual WSN battery - EPS?
    switch (state){
        case 's':
            if(batteryLevel > 60){ //edit with percent battery required for science mode
                Serial.println("SUFFICIENT BATTERY FOR SCIENCE MODE");
                return true;
            } 
            else {
                return false;
            }
            break;
        case 't':
            if(batteryLevel > 75){  //edit with percent battery required for data transfer mode
                Serial.println("SUFFICIENT BATTERY FOR DATA TRANSFER MODE")
                return true;
            }
            else {
                return false;
            }
            break;
        default:
            //enter error checking case here
            return false;
            break;
    }
}
