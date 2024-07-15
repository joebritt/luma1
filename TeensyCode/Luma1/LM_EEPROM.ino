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

#include <EEPROM.h>
#include "LM_EEPROM.h"

#include "LM_Fan.h"

/* ---------------------------------------------------------------------------------------
    Settings persistent storage, in EEPROM of Teensy
*/

// -----------------------------
// FAN MODE

void eeprom_save_fan_mode( uint8_t m ) {
  Serial.print("Saving Fan Mode: "); Serial.println( m );
  
  EEPROM.write( LM_EEPROM_FAN_MODE, m );
}


uint8_t eeprom_load_fan_mode() {
  uint8_t m;
  
  m = EEPROM.read( LM_EEPROM_FAN_MODE );

  Serial.print("Loaded Fan Mode: "); Serial.println( m );

  if( (m != FAN_AUTO) && (m != FAN_ON) && (m != FAN_OFF) ) {
    m = FAN_AUTO;
    eeprom_save_fan_mode( m );
  }

  return m;
}

// -----------------------------
// BOOT SCREEN SELECT

void eeprom_save_boot_screen_select( uint8_t m ) {
  Serial.print("Saving Boot Screen Select: "); Serial.println( m );
  
  EEPROM.write( LM_EEPROM_BOOT_SCREEN, m );
}


uint8_t eeprom_load_boot_screen_select() {
  uint8_t m;
  
  m = EEPROM.read( LM_EEPROM_BOOT_SCREEN );

  Serial.print("Loaded Boot Screen Select: "); Serial.println( m );

  if( m > LAST_BOOT_SCREEN_INDEX )
    m = 0;

  return m;
}


// -----------------------------
// MIDI CHANNEL

void eeprom_save_midi_channel( uint8_t m ) {
  Serial.print("Saving MIDI Channel setting: "); Serial.println( m );
  
  EEPROM.write( LM_EEPROM_MIDI_CHANNEL, m );
}


uint8_t eeprom_load_midi_channel() {
  uint8_t m;
  
  m = EEPROM.read( LM_EEPROM_MIDI_CHANNEL );

  Serial.printf("Loaded MIDI Channel setting: %d %s\n", m, (m==0)?"OMNI/ALL":" ");

  if( m > 16 )
    m = 0;

  return m;
}


// -----------------------------
// MIDI ROUTING

#define MIDI_ROUTE_VALID_FLAG         0xa0        // put in upper nybble to validate
#define MIDI_ROUTE_VALID_MASK         0xf0

void save_midi_route_common( char *s, int a, uint8_t d, uint8_t vflag ) {
  Serial.printf("Saving %s: %02x\n", s, d);
  EEPROM.write( a, (d | vflag) );
}

uint8_t load_midi_route_common( char *s, int a ) {
  uint8_t m;
  m = EEPROM.read( a );
  Serial.printf("Loaded %s: %02x - %s %s\n", s, m&~MIDI_ROUTE_VALID_MASK, (m&ROUTE_DIN5)?"DIN5 ":" ", (m&ROUTE_USB)?"USB":" " );
  return m;
}


// --- NOTE OUT

void eeprom_save_midi_note_out_route( uint8_t m )   {   save_midi_route_common( (char*)"MIDI Note OUT routing", LM_EEPROM_MIDI_NO_OUT,   m, MIDI_ROUTE_VALID_FLAG );         }
  
uint8_t eeprom_load_midi_note_out_route() {
  uint8_t r;

  r = load_midi_route_common( (char*)"MIDI Note OUT routing", LM_EEPROM_MIDI_NO_OUT );

  if( (r & MIDI_ROUTE_VALID_MASK) != MIDI_ROUTE_VALID_FLAG ) {
    Serial.printf( "Invalid route, using DIN5 & USB\n" );
    r = ROUTE_DIN5_USB;
    eeprom_save_midi_note_out_route( r );
  }

  r &= ~MIDI_ROUTE_VALID_MASK;
  return r;
}

 // --- NOTE IN 
 
void eeprom_save_midi_note_in_route( uint8_t m )    {   save_midi_route_common( (char*)"MIDI Note IN routing", LM_EEPROM_MIDI_NO_IN,     m, MIDI_ROUTE_VALID_FLAG );         }

uint8_t eeprom_load_midi_note_in_route() {
  uint8_t r;

  r = load_midi_route_common( (char*)"MIDI Note IN routing", LM_EEPROM_MIDI_NO_IN );

  if( (r & MIDI_ROUTE_VALID_MASK) != MIDI_ROUTE_VALID_FLAG ) {
    Serial.printf( "Invalid route, using DIN5 & USB\n" );
    r = ROUTE_DIN5_USB;
    eeprom_save_midi_note_in_route( r );
  }

  r &= ~MIDI_ROUTE_VALID_MASK;
  return r;
}


// --- CLOCK OUT

void eeprom_save_midi_clock_out_route( uint8_t m )  {   save_midi_route_common( (char*)"MIDI Clock OUT routing", LM_EEPROM_MIDI_CL_OUT,  m, MIDI_ROUTE_VALID_FLAG );         }

uint8_t eeprom_load_midi_clock_out_route() {
  uint8_t r;

  r = load_midi_route_common( (char*)"MIDI Clock OUT routing", LM_EEPROM_MIDI_CL_OUT );

  if( (r & MIDI_ROUTE_VALID_MASK) != MIDI_ROUTE_VALID_FLAG ) {
    Serial.printf( "Invalid route, using DIN5 & USB\n" );
    r = ROUTE_DIN5_USB;
    eeprom_save_midi_clock_out_route( r );
  }

  r &= ~MIDI_ROUTE_VALID_MASK;
  return r;
}

// --- CLOCK IN

void eeprom_save_midi_clock_in_route( uint8_t m )   {   save_midi_route_common( (char*)"MIDI Clock IN routing", LM_EEPROM_MIDI_CL_IN,    m, MIDI_ROUTE_VALID_FLAG );         }

uint8_t eeprom_load_midi_clock_in_route() {
  uint8_t r;

  r = load_midi_route_common( (char*)"MIDI Clock IN routing", LM_EEPROM_MIDI_CL_IN );

  if( (r & MIDI_ROUTE_VALID_MASK) != MIDI_ROUTE_VALID_FLAG ) {
    Serial.printf( "Invalid route, using USB\n" );
    r = ROUTE_USB;
    eeprom_save_midi_clock_in_route( r );
  }

  r &= ~MIDI_ROUTE_VALID_MASK;
  return r;
}


// --- SYSEX

void eeprom_save_midi_sysex_route( uint8_t m )      {   save_midi_route_common( (char*)"MIDI Sysex routing", LM_EEPROM_MIDI_SYSEX,       m, MIDI_ROUTE_VALID_FLAG );         }

uint8_t eeprom_load_midi_sysex_route() {
  uint8_t r;

  r = load_midi_route_common( (char*)"MIDI Sysex routing", LM_EEPROM_MIDI_SYSEX );

  if( (r & MIDI_ROUTE_VALID_MASK) != MIDI_ROUTE_VALID_FLAG ) {
    Serial.printf( "Invalid route, using USB\n" );
    r = ROUTE_USB;
    eeprom_save_midi_sysex_route( r );
  }

  r &= ~MIDI_ROUTE_VALID_MASK;
  return r;
}


// --- SOFT THRU

void eeprom_save_midi_soft_thru( bool on ) {
  Serial.printf("Saving Soft Thru: %s\n", on?"ON":"off");
  EEPROM.write( LM_EEPROM_MIDI_SOFT_THRU, on?1:0 );
}

bool eeprom_load_midi_soft_thru() {
  uint8_t m;
  m = EEPROM.read( LM_EEPROM_MIDI_SOFT_THRU );
  Serial.printf("Loaded Soft Thru: %s\n", m?"ON":"off" );
  return m?true:false;
}


// --- MIDI START HONOR

void eeprom_save_midi_start_honor( bool on) {
  Serial.printf("Saving MIDI Start Honor: %s\n", on?"ENABLED":"disabled");
  EEPROM.write( LM_EEPROM_MIDI_START_HONOR, on?1:0 );
}

bool eeprom_load_midi_start_honor() {
  uint8_t m;
  m = EEPROM.read( LM_EEPROM_MIDI_START_HONOR );
  Serial.printf("Loaded MIDI START Honor: %s\n", m?"ENABLED":"disabled" );
  return m?true:false;
}


// --- MIDI SEND VELOCITY

void eeprom_save_midi_send_velocity( bool on) {
  Serial.printf("Saving MIDI Send Velocity: %s\n", on?"ENABLED":"disabled");
  EEPROM.write( LM_EEPROM_MIDI_SEND_VEL, on?1:0 );
}

bool eeprom_load_midi_send_velocity() {
  uint8_t m;
  m = EEPROM.read( LM_EEPROM_MIDI_SEND_VEL );
  Serial.printf("Loaded MIDI Send Velocity: %s\n", m?"ENABLED":"disabled" );
  return m?true:false;
}


// --- MONOTONICALLY INCREMENTING NUMBER FOR RAM BANK NAMES

uint16_t eeprom_next_rambank_num() {
  uint16_t s, n;
  uint8_t m;

  m = EEPROM.read( LM_EEPROM_RAMBANK_HI );
  s = m;
  s <<= 8;

  m = EEPROM.read( LM_EEPROM_RAMBANK_LO );
  s &= 0xff00;
  s += (m & 0xff);

  if( s > 9999 )      // they go from 0000 -> 9999, then wrap
    s = 0;

  n = s + 1;
  if( n > 9999 )      // they go from 0000 -> 9999, then wrap
    n = 0;

  EEPROM.write( LM_EEPROM_RAMBANK_LO, (n & 0xff) );
  EEPROM.write( LM_EEPROM_RAMBANK_HI, (n >> 8) );

  Serial.printf( "RAMBANK number: %04d, next will be %04d\n", s, n);

  return s;
}


// -----------------------------
// SERIAL NUMBER

/*
void eeprom_save_mach_info( machine_info_t *mi );
void eeprom_load_mach_info( machine_info_t *mi );

machine_info_t machine_info;

void eeprom_save_mach_info( machine_info_t *mi ) {
  Serial.println("Saving Machine Info to EEPROM");
  
  EEPROM.put( LM_EEPROM_MACHINE_INFO, mi );
}

void eeprom_load_mach_info( machine_info_t *mi ) {
  Serial.println("Loading Machine Info from EEPROM");

  EEPROM.get( LM_EEPROM_MACHINE_INFO, mi );
}
*/

void eeprom_set_serial_number( char *sn ) {
  Serial.printf("Saving Serial Number %s to EEPROM...", sn);

  for( int xxx = LM_EEPROM_SERIAL_NUM; xxx != (LM_EEPROM_SERIAL_NUM + SERIAL_NUM_LEN); xxx++ ) {
    EEPROM.write( xxx, *sn++ );
  }

  Serial.println("done.");
}


void eeprom_get_serial_number( char *sn ) {
  Serial.print("Loading Serial Number from EEPROM...");

  for( int xxx = LM_EEPROM_SERIAL_NUM; xxx != (LM_EEPROM_SERIAL_NUM + SERIAL_NUM_LEN); xxx++ ) {
    *sn++ = EEPROM.read( xxx );
  }

  *sn = 0;      // null terminate

  Serial.println("done.");
}
