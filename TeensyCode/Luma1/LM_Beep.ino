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

#include "LM_Beep.h"

/* ---------------------------------------------------------------------------------------
    Beep using LM-1 beep hardware
*/
          
elapsedMillis beeptime;

// assumes Teensy has z-80 bus

void drum_beep( int freq, int dur ) {           // dur is ms
  int per;

  per = 1000000 / freq;

  beeptime = 0;
  
  teensy_drives_z80_bus( true );                // grab the bus
  delay( 10 );

  while( beeptime < dur ) {
    clr_LED_SET_2( BEEP_OUT );
    delayMicroseconds( per/2 );
    set_LED_SET_2( BEEP_OUT );
    delayMicroseconds( per/2 );
  }

  teensy_drives_z80_bus( false );               // release the bus
  delay( 10 );
}


void beep_success() {
  drum_beep( 880, 150 );
  delay( 10 );
  drum_beep( 880, 150 );
  delay( 50 );
  drum_beep( 1760, 200 );
}

void beep_failure() {
  drum_beep( 440, 150 );
  delay( 50 );
  drum_beep( 440, 200 );
}
