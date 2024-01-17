/*
    The Luma-1 Drum Machine Project
    Copyright 2021-2024, Joe Britt
    
    Redistribution and use in source and binary forms, with or without modification, are permitted provided 
    that the following conditions are met:
    
    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
       following disclaimer.
    
    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and 
       the following disclaimer in the documentation and/or other materials provided with the distribution.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED 
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
    TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
    POSSIBILITY OF SUCH DAMAGE.
*/

#include <InternalTemperature.h>

#include "LM_Fan.h"

elapsedMillis temp_check_timer;


/*  
    12V fan power supply control and temperature sensing

    Fan control requires a little dance using some signals on the Z-80 side.
    It's done this funky way because I was out of Teensy I/Os.
*/

#define TEMP_CHECK_TIME         (120*1000)        // every 2 minutes

bool fan_is_on = false;

int fan_mode = FAN_OFF;
int last_fan_mode = 99;                           // initialize to invalid value to force first pass setting

float fan_on_temp = 70;                           // 70C Teensy die temp, ambient will be cooler. Max die temp is 95C
float last_temp_reading = 0;                      // can't believe I'm using floating point for something


float get_temperature() {
  return last_temp_reading;
}

int get_fan_mode() {
  return fan_mode;
}

bool fan_running(){
  return fan_is_on;
}


void init_fan() {
  last_temp_reading = InternalTemperature.readTemperatureC();
  temp_check_timer = 0;

  fan_enable( false );                            // make sure it's off, flip-flop can come up in either state
}


void set_fan_mode( int mode ) {
  switch( mode ) {
    case 00:  Serial.println("Fan Mode AUTO");
              fan_mode = FAN_AUTO;
              break;

    case 01:  Serial.println("Fan Mode ON");
              fan_mode = FAN_ON;
              break;

    case 02:  Serial.println("Fan Mode OFF");
              fan_mode = FAN_OFF;
              break;

    default:  Serial.println("Invalid fan mode, ignoring");
              break;
  }
}



void fan_enable( bool en ) {
  byte b;
  bool old_en = enable_midi_start_stop_clock( false );        // disable midi clock check

  Serial.printf("Fan: turning %s\n", en?"ON":"off");
  
  teensy_drives_z80_bus( true );                              // *** Teensy takes Z-80 bus

  save_LED_SET_2();                                           // the way we were

    if( en )
      set_LED_SET_2( LED_STORE );                             // STORE = 1 -> fan ON, STORE = 0 -> fan OFF
    else
      clr_LED_SET_2( LED_STORE );
  
    b = (LED_VERIFY | TAPE_FSK_OUT);                          // both hi to drive flop clk low
  
    set_LED_SET_2( b );                                       // drive flop clk low
    clr_LED_SET_2( b );                                       // drive flop clk hi while keeping state bit in D0, will latch
  
  restore_LED_SET_2();                                        // back like we were before
  
  teensy_drives_z80_bus( false );                             // *** Teensy releases Z-80 bus

  fan_is_on = en;

  enable_midi_start_stop_clock( old_en );                     // restore midi clock check
}



// call periodically from main loop

void handle_fan() {
  if( last_fan_mode != get_fan_mode() ) {                     // handles boot case and where user changes mode
    switch( get_fan_mode() ) {
      case FAN_AUTO:  temp_check_timer = (TEMP_CHECK_TIME+1000);  // force temp check
                      break;
                      
      case FAN_ON:    fan_enable( true );
                      break;
                      
      case FAN_OFF:   fan_enable( false );
                      break;
    }
    
    last_fan_mode = get_fan_mode();
  }

  if( fan_running() && luma_is_playing() ) {                  // fan off when we start playing
    fan_enable( false );
  }

  if( (get_fan_mode()     == FAN_ON) &&
      (fan_running()      == false) &&
      (luma_is_playing()  == false) ) {                       // user has fan_mode set to ON, we turned it off to play, now we can turn it back on again
    fan_enable( true );
  }

  if( temp_check_timer > TEMP_CHECK_TIME ) {                  // poll temperature every TEMP_CHECK_TIME
    temp_check_timer = 0;

    if( last_temp_reading != InternalTemperature.readTemperatureC() ) {
      last_temp_reading = InternalTemperature.readTemperatureC();
      Serial.print("internal temperature: "); Serial.print(last_temp_reading, 1); Serial.println(" C");
    }

    if( get_fan_mode() == FAN_AUTO ) {
      if( (last_temp_reading >= fan_on_temp) ) {
        if( !fan_running() ) {
          if( luma_is_playing() == false ) {                  // only turn on fan when pattern is not playing
            Serial.print("Fan mode is AUTO, turning fan ON: temp = "); Serial.print( last_temp_reading, 1 ); Serial.print(", threshold = "); Serial.println( fan_on_temp, 1 );
            fan_enable( true );
          }
          else {
            Serial.println("Fan Mode is AUTO, not turning fan on because Luma is playing");
          }
        }
      }
      else {
        if( fan_running() ) {
          Serial.print("Fan mode is AUTO, turning fan OFF: temp = "); Serial.print( last_temp_reading, 1 ); Serial.print(", threshold = "); Serial.println( fan_on_temp, 1 );
          fan_enable( false );          // 1 minute temp checks provides implicit hysteresis
        }
      }
    }
  }  
}
