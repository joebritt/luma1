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

#include "LM_EPROM_Reader.h"
#include "LM_Z80Bus.h"

uint8_t eprom_dump_buf[32*1024];

uint16_t eprom_dump_voice;          // voice card we should copy the EPROM data into
uint16_t eprom_size;                // EPROM size in bytes
uint16_t eprom_checksum;

/*
    Pass in a type, get back a pointer to a buffer with that EPROM's data

    ASSUMES TEENSY HAS THE Z-80 BUS
    
    ﻿EPROM Reading is a total hack.
    
    1. MENU -> EPROM DUMP, pause Z-80, Teensy takes bus
    2. Prompt user for EPROM size
    3. Tell user how to set power jumper
    4. Data bus Z-80 -> Teensy
    5. Addr bus Z-80 <- Teensy
    6. Drive A13..A0 Z-80 Addr bus
    7. Drive A14 = 0 on LED_B = 0
    
    8. Drive ﻿TTL_TAPE_SYNC_CLK  = 1 (This is /CE to the EPROM)
    9. Drive /OE = 1, LED_C = 1
    
    10. Ask user to insert EPROM, hit a key

    11. Read EPROM: 
                    Drive /OE = 0, LED_C = 0
                    Present Addr[0..13], Addr[14] on LED_B
                    Drive ﻿TTL_TAPE_SYNC_CLK  = 0
                    Wait 1uS
                    Sample Data bus, save value read
                    Drive ﻿TTL_TAPE_SYNC_CLK  = 1
                    Repeat for all bytes
        
    12. Drive /OE = 1, LED_C = 1
    
    14. Save buffer someplace on SD, and/or in voice RAM
*/


void set_eprom_a14( uint16_t addr ) {
  if( addr & 0x4000 ) {
    Serial.printf("Setting A14 to 1\n");
    set_LED_SET_2( LED_B );                           // LED_B = A14
  }
  else {
    Serial.printf("Setting A14 to 0\n");
    clr_LED_SET_2( LED_B );
  }
}


void set_eprom_oe( int oe ) {
  if( oe )
    set_LED_SET_2( LED_C );                           // LED_C = /OE
  else
    clr_LED_SET_2( LED_C );
}


void set_eprom_ce( int ce ) {
  set_tape_sync_clk_gpo( ce );
}


// Call to set up the interface BEFORE the user has inserted the EPROM

void dump_eprom_prologue() {  
  Serial.println("dump eprom prologue");

  save_LED_SET_2();                                                 // remember current state of D802

  set_eprom_oe( 1 );                                                // make sure /OE = 1

  set_eprom_ce( 1 );                                                // make sure /CE = 1                    
}



uint8_t read_eprom_byte( uint16_t addr ) {
  uint8_t b;

  // ----- FROM HERE NO NORMAL Z-80 STYLE ACCESSES

  // --- Make sure part is deselected

  z80_drive_data( false );                                          // Data bus == INPUTS

  set_eprom_ce( 1 );                                                // /CE = 1 (Teensy GPIO)

  set_z80_addr( addr );                                             // setup & drive A0-A13

  set_eprom_ce( 0 );                                                // /CE = 0

  delayMicroseconds( 1 );                                           // settle

  b = get_z80_data();                                               // capture byte from EPROM

  set_eprom_ce( 1 );                                                // /CE = 1

  // ----- FROM HERE BACK TO NORMAL Z-80 STYLE ACCESSES

  return b;
}



// Call to dump the data AFTER the user has inserted the EPROM

uint8_t *dump_eprom_data() {
  uint16_t orig_eprom_size;                // EPROM size in bytes

  Serial.println("dump eprom worker");

  Serial.printf("Dumping %d bytes\n", eprom_size);

  eprom_checksum = 0;

  orig_eprom_size = eprom_size;                             // for hack for 32K EPROMs

  memset( eprom_dump_buf, 0, 32768 );                       // zero buffer


  // --- Need to drive Vpp high
  //     Also drive the address we are reading
  
  if( eprom_size == 2048 )
    addr |= 0x0800;                                                 // drive Vpp = 1, that pin on 2716 is connected to A11

  if( (eprom_size == 8192) || (eprom_size == 16384) )
    addr |= 0x4000;                                                 // drive Vpp = 1, that pin on 2764 or 27128 is connected to A14

  set_eprom_a14( addr );                                            // A14, only used on 27256, but Vpp on 2764 and 27128
                                                                    // ---> NOTE! This will run a z-80 bus write cycle (bit in LED reg)

  if( orig_eprom_size == 32768 )                                    // HACK for reading 27256 in 2 halves
    eprom_size = 16384;

  // --- Read up to 16KB

  set_eprom_oe( 0 );                                                // /OE = 0 ---> NOTE! This will run a z-80 bus write cycle

  for( int addr = 0; addr != eprom_size; addr++ ) {
    eprom_dump_buf[addr] = read_eprom_byte( addr );
    eprom_checksum += eprom_dump_buf[addr];
  }

  set_eprom_oe( 1 );                                                // /OE = 1 ---> NOTE! This will run a z-80 bus write cycle


  // --- Total hack for 32K EPROM, if 27256 now read UPPER half

  if( orig_eprom_size == 32768 ) {
    set_eprom_a14( 0x4000 );                                        // A14, only used on 27256, but Vpp on 2764 and 27128
                                                                    // ---> NOTE! This will run a z-80 bus write cycle (bit in LED reg)

    set_eprom_oe( 0 );                                              // /OE = 0 ---> NOTE! This will run a z-80 bus write cycle

    for( int addr = 0; addr != eprom_size; addr++ ) {
      eprom_dump_buf[addr+0x4000] = read_eprom_byte( addr );
      eprom_checksum += eprom_dump_buf[addr+0x4000];
    }

    set_eprom_oe( 1 );                                              // /OE = 1 ---> NOTE! This will run a z-80 bus write cycle
  }

  // UNDO the hack

  if( orig_eprom_size == 32768 )                                    // HACK for reading 27256 in 2 halves
    eprom_size = 32768;


  for( int xxx = 0; xxx != 16; xxx++ )
    Serial.printf("%02x ", eprom_dump_buf[xxx] );

  Serial.println();

  for( int xxx = eprom_size-16; xxx != eprom_size; xxx++ )
    Serial.printf("%02x ", eprom_dump_buf[xxx] );
    
  Serial.println();
  
  return eprom_dump_buf;
}


// Call to clean up AFTER the user has REMOVED the EPROM

void dump_eprom_epilogue() {
  char vname[16];
  
  Serial.println("dump eprom epilogue");

  restore_LED_SET_2();                                      // D802 back like it was before

  fan_enable( fan_running() );                              // XXX FIXME, should not need to do this

  // blast it in

/*
  disable_drum_trig_interrupt();                            // don't detect drum writes as triggers  
  load_voice_prologue();
  */
  
  set_rst_hihat( 1 );                                       // XXX FIXME should not need to do this, getting stomped

  // send to the voice board  

  snprintf( vname, 15, "E_%04X", eprom_checksum );

  set_voice( eprom_dump_voice, eprom_dump_buf, eprom_size, vname);

  /*
  set_voice( eprom_dump_voice, 
              eprom_dump_buf, 
              ((eprom_size == 16384) && (eprom_dump_voice != STB_CONGAS) && (eprom_dump_voice != STB_TOMS)) ? 32768 : eprom_size, vname);       // only Conga & Tom can be 16K
  */

/*
  load_voice_epilogue();
  restore_drum_trig_interrupt();                            // back to normal
*/
}
