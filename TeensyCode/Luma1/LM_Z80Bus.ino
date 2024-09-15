
/* ---------------------------------------------------------------------------------------
    Z-80 Bus Control

    Teensy GPIOs are connected through level shifters with the Z-80 Address bus, Data bus,
    and control signals.

    Teensy can be a bus master, take over the bus from the Z-80, and run Z-80-style bus cycles
    to access all Z-80 memory and peripherals.


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

#include "LM_Z80Bus.h"

uint8_t led_set_2_shadow = 0;

void z80_drive_data( bool d ) {
  if( d ) {

#ifdef ARDUINO_TEENSY41
    digitalWriteFast( nDRIVE_DATA, 0 );       // turn level shifters around, Teensy -> Z-80
#endif

    pinMode( DATA_0, OUTPUT );
    pinMode( DATA_1, OUTPUT );
    pinMode( DATA_2, OUTPUT );
    pinMode( DATA_3, OUTPUT );
  
    pinMode( DATA_4, OUTPUT );
    pinMode( DATA_5, OUTPUT );
    pinMode( DATA_6, OUTPUT );
    pinMode( DATA_7, OUTPUT );
  
  } else {

    pinMode( DATA_0, INPUT );
    pinMode( DATA_1, INPUT );
    pinMode( DATA_2, INPUT );
    pinMode( DATA_3, INPUT );
  
    pinMode( DATA_4, INPUT );
    pinMode( DATA_5, INPUT );
    pinMode( DATA_6, INPUT );
    pinMode( DATA_7, INPUT );
    
#ifdef ARDUINO_TEENSY41
    digitalWriteFast( nDRIVE_DATA, 1 );           // turn level shifters around, Teensy <- Z-80
#endif
  }
}


int bus_drive_counting_semaphore = 0;

void teensy_drives_z80_bus_hw( bool drive );      // actual dangerous routine


void teensy_drives_z80_bus( bool drive ) {
  if( drive ) {                                       // if we are being asked to drive it
    if( bus_drive_counting_semaphore == 0 ) {         // and we are currently NOT
      bus_drive_counting_semaphore++;                 //   mark that we are now driving it
      teensy_drives_z80_bus_hw( true );               //   and do so
    }
    else
      bus_drive_counting_semaphore++;                 // we ARE already driving it, just mark that there is another layer of request
  }
  else {                                              // if we are being asked to release it
    if( bus_drive_counting_semaphore > 0 )            // and we currently ARE driving it
      bus_drive_counting_semaphore--;

    if( bus_drive_counting_semaphore == 0 ) {         // if that got us to 0, it's time to release it
      teensy_drives_z80_bus_hw( false );              //   let it go... let it go...
    }
  }
}

void teensy_drives_z80_bus_hw( bool drive ) {
  int mode = (drive ? OUTPUT : INPUT);
  
  if( drive ) {                               // if we are going to drive, first get the Z-80 off the bus
                              
    if( !z80_in_reset ) {                     // we will only see an nBUSAK if the Z-80 is not in reset           
                       
      digitalWriteFast( nBUSRQ, 0 );

      while( digitalRead( nBUSAK ) != 0 )
        delayNanoseconds( 500 );
    }
  }
  
  // control signals
  pinMode( nWR, mode );         digitalWriteFast( nWR,      1 );
  pinMode( nRD, mode );         digitalWriteFast( nRD,      1 );
  
  pinMode( nMREQ, mode );       
#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,    0 );    // deassert
#else
  digitalWriteFast( nMREQ,    1 );
#endif

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nDRIVE_ADDR, (drive ? 0 : 1 ) );    // point addr bus buffers the right way
#endif

  // address bus
  pinMode( ADDR_0,  mode );
  pinMode( ADDR_1,  mode );
  pinMode( ADDR_2,  mode );
  pinMode( ADDR_3,  mode );

  pinMode( ADDR_4,  mode );
  pinMode( ADDR_5,  mode );
  pinMode( ADDR_6,  mode );
  pinMode( ADDR_7,  mode );

  pinMode( ADDR_8,  mode );
  pinMode( ADDR_9,  mode );
  pinMode( ADDR_10, mode );
  pinMode( ADDR_11, mode );

  pinMode( ADDR_12, mode );
  pinMode( ADDR_13, mode );

  set_z80_addr( 0x3fff );

  // data bus
  z80_drive_data( false );
  set_z80_data( 0xff );

  if( !drive ) {
    //if( !z80_in_reset )
      digitalWrite( nBUSRQ, 1 );          // let the Z-80 have the bus back (doesn't matter if we are in reset or not)
  }

  teensy_driving_bus = drive;
}


#define _bit(_addr, _sel) ((_addr & (1<<_sel))!=0)

void set_z80_addr( uint16_t a ) {

  digitalWriteFast( ADDR_13, _bit(a, 13) );
  digitalWriteFast( ADDR_12, _bit(a, 12) );
  
  digitalWriteFast( ADDR_11, _bit(a, 11) );
  digitalWriteFast( ADDR_10, _bit(a, 10) );
  digitalWriteFast( ADDR_9,  _bit(a, 9) );
  digitalWriteFast( ADDR_8,  _bit(a, 8) );

  digitalWriteFast( ADDR_7,  _bit(a, 7) );
  digitalWriteFast( ADDR_6,  _bit(a, 6) );
  digitalWriteFast( ADDR_5,  _bit(a, 5) );
  digitalWriteFast( ADDR_4,  _bit(a, 4) );

  digitalWriteFast( ADDR_3,  _bit(a, 3) );
  digitalWriteFast( ADDR_2,  _bit(a, 2) );
  digitalWriteFast( ADDR_1,  _bit(a, 1) );
  digitalWriteFast( ADDR_0,  _bit(a, 0) );
}


void set_z80_data( uint8_t d ) {
  digitalWriteFast( DATA_7,  (d & (1<<7)) );
  digitalWriteFast( DATA_6,  (d & (1<<6)) );
  digitalWriteFast( DATA_5,  (d & (1<<5)) );
  digitalWriteFast( DATA_4,  (d & (1<<4)) );

  digitalWriteFast( DATA_3,  (d & (1<<3)) );
  digitalWriteFast( DATA_2,  (d & (1<<2)) );
  digitalWriteFast( DATA_1,  (d & (1<<1)) );
  digitalWriteFast( DATA_0,  (d & (1<<0)) );
}


uint8_t get_z80_data( ) {
  uint8_t c = 0;

  if( digitalReadFast( DATA_7 ) ) c |= 0x80;
  if( digitalReadFast( DATA_6 ) ) c |= 0x40;
  if( digitalReadFast( DATA_5 ) ) c |= 0x20;
  if( digitalReadFast( DATA_4 ) ) c |= 0x10;

  if( digitalReadFast( DATA_3 ) ) c |= 0x08;
  if( digitalReadFast( DATA_2 ) ) c |= 0x04;
  if( digitalReadFast( DATA_1 ) ) c |= 0x02;
  if( digitalReadFast( DATA_0 ) ) c |= 0x01;

  return c;
}


void init_z80_if( void ) {

  z80_reset( true );
  
#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nBUSRQ, 1 );          // XXX SHOULD NOT NEED TO START WITH BUSRQ LOW XXX -- look with logic analyzer
#else
  digitalWriteFast( nBUSRQ, 1 );
#endif

  pinMode( nBUSRQ, OUTPUT );
  
  pinMode( nBUSAK, INPUT );

#ifdef ARDUINO_TEENSY41
  pinMode( nDRIVE_ADDR, OUTPUT );
  digitalWriteFast( nDRIVE_ADDR, 1 );     // address bus Teensy <- Z-80

  pinMode( nDRIVE_DATA, OUTPUT );
  digitalWriteFast( nDRIVE_DATA, 1 );     // data bus Teensy <- Z-80
#endif
}



void z80_bus_write_speed( uint16_t a, uint8_t d, int ns ) {

  set_z80_addr( a );                    // set up the address

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  1 );        // drop /MREQ
#else
  digitalWriteFast( nMREQ,  0 );        // drop /MREQ
#endif

  z80_drive_data( true );               // drive Data bus
  
  set_z80_data( d );                    // drive the data bus

  digitalWriteFast( nWR,    0 );        // drop /WR

  delayNanoseconds( ns );

  digitalWriteFast( nWR,    1 );        // raise /WR

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  0 );        // raise /MREQ
#else
  digitalWriteFast( nMREQ,  1 );        // raise /MREQ
#endif

  z80_drive_data( false );              // Data bus == INPUTS

  delayNanoseconds( ns );
}


void z80_bus_write( uint16_t a, uint8_t d ) {
  z80_bus_write_speed( a, d, 1000 );
}


/*
void z80_bus_write( uint16_t a, uint8_t d ) {
  set_z80_addr( a );                    // set up the address

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  1 );        // drop /MREQ
#else
  digitalWriteFast( nMREQ,  0 );        // drop /MREQ
#endif

  z80_drive_data( true );               // drive Data bus
  
  set_z80_data( d );                    // drive the data bus

  digitalWriteFast( nWR,    0 );        // drop /WR

  delayNanoseconds( 1000 );

  digitalWriteFast( nWR,    1 );        // raise /WR

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  0 );        // raise /MREQ
#else
  digitalWriteFast( nMREQ,  1 );        // raise /MREQ
#endif

  z80_drive_data( false );              // Data bus == INPUTS

  delayNanoseconds( 1000 );
}
*/

unsigned char z80_bus_read( uint16_t a ) {
  uint8_t d;

  set_z80_addr( a );                    // set up the address

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  1 );        // drop /MREQ
#else
  digitalWriteFast( nMREQ,  0 );        // drop /MREQ
#endif

  z80_drive_data( false );              // Data bus == INPUTS

  digitalWriteFast( nRD,    0 );        // drop /RD

  delayMicroseconds( 1 );

  d = get_z80_data();

  digitalWriteFast( nRD,    1 );        // raise /RD

#ifdef ARDUINO_TEENSY41
  digitalWriteFast( nMREQ,  0 );        // raise /MREQ
#else
  digitalWriteFast( nMREQ,  1 );        // raise /MREQ
#endif

  delayMicroseconds( 1 );

  return d;
}



void copy_z80_ram( uint8_t *img ) {
  uint16_t addr = 0xa000;
  
  Serial.print("Copying Z-80 RAM image to Teensy buffer...");

  teensy_drives_z80_bus( true );                    // this gets called from a couple of places, there is a counting semaphore around taking the bus

  for( int xxx = 0; xxx != (8*1024); xxx++ )
    img[xxx] = z80_bus_read( addr++ );

  teensy_drives_z80_bus( false );

  Serial.println("done!");
}


void load_z80_ram( uint8_t *img ) {
  uint16_t addr = 0xa000;
  
  Serial.print("Loading Z-80 RAM image...");

  for( int xxx = 0; xxx != (8*1024); xxx++ )
    z80_bus_write( addr++, img[xxx] );

  Serial.println("done!");
}



void load_z80_rom( uint8_t *rom_image ) {
    
  Serial.print("Loading Z-80 ROM code...");
  
  for( int xxx = 0; xxx != (3*2048); xxx++ ) {      // copy the ROM code to the SRAM that the Z-80 sees as ROM
    z80_bus_write( xxx, rom_image[xxx] );
  }
  
  for( int xxx = 0; xxx != (3*2048); xxx++ ) {      // did everything stick?
    if( z80_bus_read( xxx ) != rom_image[xxx] ) {
      Serial.print("\nROM load memory mismatch, location: "); Serial.print( xxx ); Serial.print(", expected: "); Serial.println( rom_image[xxx], HEX );
      dump_z80_mem( xxx, 256 );
      while( 1 )
        delay( 100 );
    }
  }

  Serial.println("done!");
}


void dump_z80_mem( uint16_t startAddr, uint16_t len );

void dump_z80_mem( uint16_t startAddr, uint16_t len ) {
  for( int xxx = startAddr; xxx != startAddr + len; xxx += 16 ) {
    printHex4( xxx );
    Serial.print(":");
    
    for( int yyy = 0; yyy != 16; yyy++ ) {
      printHex2( z80_bus_read( xxx+yyy ) );
      Serial.print(" ");
    }

    Serial.println();
  }
}



void z80_reset( bool inreset ) {
  Serial.print("z-80: "); 

  if( inreset ) {

#ifdef ARDUINO_TEENSY41
    digitalWrite( nRST, 1);
    pinMode( nRST, OUTPUT );                     // make RST_3V3 an input (all others outputs), let hardware pullup cause inverter output to go low
#else
    digitalWrite( nRST, 1 );
    pinMode( nRST, INPUT );                     // make RST_3V3 an input (all others outputs), let hardware pullup cause inverter output to go low
#endif

    z80_in_reset = true;
    Serial.println("in reset"); 
    
  } else {

#ifdef ARDUINO_TEENSY41
    digitalWrite( nRST, 0 );
    pinMode( nRST, OUTPUT );                     
#else
    pinMode( nRST, OUTPUT );
    digitalWrite( nRST, 0 );                    // drive it low, which will make the inverter output (to Z-80 /RESET) high
#endif
    
    z80_in_reset = false;
    Serial.println("out of reset"); 
  }
}



void set_LED_SET_2( uint8_t val ) {
  //led_set_2_shadow = z80_bus_read( D802_SHADOW );     // Z-80's copy of LED_SET_2 state
  led_set_2_shadow |= val;
  z80_bus_write( LED_SET_2, led_set_2_shadow );
}

void clr_LED_SET_2( uint8_t val ) {
  //led_set_2_shadow = z80_bus_read( D802_SHADOW );     // Z-80's copy of LED_SET_2 state
  led_set_2_shadow &= ~val;
  z80_bus_write( LED_SET_2, led_set_2_shadow );
}

uint8_t restore_led_set_2;

uint8_t save_LED_SET_2() {
  restore_led_set_2 = led_set_2_shadow = z80_bus_read( D802_SHADOW );     // Z-80's copy of LED_SET_2 state
  return restore_led_set_2;                                               // in case we care
}

void restore_LED_SET_2() {  
  z80_bus_write( LED_SET_2, restore_led_set_2 );        // put it back
  led_set_2_shadow = restore_led_set_2;
}
