
/*
    Patch some of the Z-80 code in the Z-80 ROM image once it's loaded into SRAM

    MENU / STORE Patch
      Writes a z-80 "ret" instruction into code that would do cassette tape data store.
      We use the STORE LED turning on to tell Teensy to take over for local UI mode.
      We don't need the Z-80 cassette tape data storage routines. Without this patch, we have to wait
       for the Z-80 code to time out when we exit MENU mode.


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

#include "LM_Z80Patches.h"


// --- called at boot

void apply_z80_patches() {

    Serial.print("Applying fixed Z-80 code patches...");
    
    // MENU / STORE Patch

    z80_bus_write( 0x9374, 0xc9 );

    Serial.println("done.");
}



// --- bits in status byte at z80 address 0xa001

#define Z80_SEQ_PLAYING         0x04
#define Z80_SEQ_RECORDING       0x02
#define Z80_SEQ_CHAIN_ON        0x01

bool z80_sequencer_running() {                      // *** CALL ONLY WHEN TEENSY HAS THE BUS
  bool r = false;


  //Serial.printf("cur_state: %02x\n", z80_bus_read(0xa001));

  r = (z80_bus_read(0xa001) & Z80_SEQ_PLAYING);

  return r;
}


// --- simulate press/release the PLAY/STOP footpedal

#define FOOT_DOWN_TIME_MS                           250

elapsedMillis footswitch_time;
int footswitch_up_time = 0;                         // time in the future to lift footswitch



// the footswitch is checked in 2 separate places in the ROM code

void z80_patch_footswitch( bool down ) {            // *** CALL ONLY WHEN TEENSY HAS THE BUS

  Serial.printf("foot %s\n", down?"DOWN":"UP");

  if( down ) {                                      // override check
    z80_bus_write( 0x8725, 0x18 );                  // 18 = jr            in scan_keys()

    footswitch_time = 0;                            // reset clock...
    footswitch_up_time = FOOT_DOWN_TIME_MS;         // ...will raise it at this time in the future
  }
  else 
  {
    z80_bus_write( 0x8725, 0x20 );                  // 20 = jr nz         in scan_keys()

    footswitch_up_time = 0;                         // 0 -> it's already up
  }
}



void z80_seq_ctl( bool state ) {

  if( footswitch_up_time != 0 ) {                   // should not happen
    Serial.printf("*** footswitch_up_time !- 0 !!!\n");
    return;
  }

  teensy_drives_z80_bus( true );                    // *** Teensy owns Z-80 bus

  if( state == Z80_SEQ_START ) {
    if( !z80_sequencer_running() )                  // if z80 seq not running...
      z80_patch_footswitch( true );                 // ...start it
    else
      Serial.printf("--> Z80 sequencer already running\n");
  } 
  else 
  {
    if( z80_sequencer_running() )                   // if z80 seq IS running...
      z80_patch_footswitch( true );                 // ...STOP it
    else
      Serial.printf("--> Z80 sequencer already stopped\n");
  }

  teensy_drives_z80_bus( false );                   // *** Teensy releases Z-80 bus
}


// --- call frequently, handles timers that change patch states

void handle_z80_patches() {

  // --- if FOOT_DOWN_TIME_MS has elapsed since "pressing" the foot switch, let it up

  if( (footswitch_up_time > 0) && (footswitch_time >= footswitch_up_time) ) {
      //Serial.printf("foot up\n");
    teensy_drives_z80_bus( true );                    // *** Teensy owns Z-80 bus
    z80_patch_footswitch( false );
    teensy_drives_z80_bus( false );                   // *** Teensy releases Z-80 bus

    footswitch_up_time = 0;                           // 0 -> it's already up
  }

}               


