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

#ifndef LM_EEPROM_H_
#define LM_EEPROM_H_

/*
    Luma-1 Teensy EEPROM Usage

    Teensy 4.1 has 4284 bytes of EEPROM.

    Bytes    0 -  511:      General purpose single-byte parameter storage, see defines below.
           512 - 1023:      512 bytes of Serial Number, User Name, and Machine Name
*/

#define LM_EEPROM_FAN_MODE          0
#define LM_EEPROM_BOOT_SCREEN       1
#define LM_EEPROM_MIDI_CHANNEL      2

#define LM_EEPROM_MIDI_NO_OUT       3
#define LM_EEPROM_MIDI_NO_IN        4
#define LM_EEPROM_MIDI_CL_OUT       5
#define LM_EEPROM_MIDI_CL_IN        6
#define LM_EEPROM_MIDI_SYSEX        7
#define LM_EEPROM_MIDI_SOFT_THRU    8
#define LM_EEPROM_MIDI_START_HONOR  9

// ... available ...

#define LM_EEPROM_SERIAL_NUM        512
#define SERIAL_NUM_LEN              8


void eeprom_save_fan_mode( uint8_t m );
uint8_t eeprom_load_fan_mode();

void eeprom_save_boot_screen_select( uint8_t m );
uint8_t eeprom_load_boot_screen_select();


// --- MIDI settings

void eeprom_save_midi_channel( uint8_t m );
uint8_t eeprom_load_midi_channel();


void eeprom_save_midi_note_out_route( uint8_t m );
uint8_t eeprom_load_midi_note_out_route();

void eeprom_save_midi_note_in_route( uint8_t m );
uint8_t eeprom_load_midi_note_in_route();

void eeprom_save_midi_clock_out_route( uint8_t m );
uint8_t eeprom_load_midi_clock_out_route();

void eeprom_save_midi_clock_in_route( uint8_t m );
uint8_t eeprom_load_midi_clock_in_route();

void eeprom_save_midi_sysex_route( uint8_t m );
uint8_t eeprom_load_midi_sysex_route();

void eeprom_save_midi_soft_thru( bool on );
bool eeprom_load_midi_soft_thru();

void eeprom_save_midi_start_honor( bool on);
bool eeprom_load_midi_start_honor();


// --- Serial #

void eeprom_set_serial_number( char *sn );
char *eeprom_get_serial_number();


/*
// Extensible Machine Info struct, read into RAM at boot

typedef struct {
  uint32_t  machine_info_id;                  // will be "LUMA" for version 1
  char      serial_number[8];                 // 8 byte ASCII serial number
  char      machine_name[32];
  char      user_name[64];  
} __attribute__((packed)) machine_info_t;

void eeprom_save_mach_info( machine_info_t *mi );
void eeprom_load_mach_info( machine_info_t *mi );
*/

#endif
