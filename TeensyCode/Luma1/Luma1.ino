
#include "LM_PinMap.h"              // Arduino/Teensy pin mapping
#include "LM_DrumTriggers.h"        // i2c interface to capture drum triggers
#include "LM_Z80Bus.h"              // Z-80 bus request / release and bus operations
/* -----------------------------------------------------------------------------------------------------------
     _                             __       _                                             _     _            
    | |                           /_ |     | |                                           | |   (_)           
    | |_   _ _ __ ___   __ _ ______| |   __| |_ __ _   _ _ __ ___    _ __ ___   __ _  ___| |__  _ _ __   ___ 
    | | | | | '_ ` _ \ / _` |______| |  / _` | '__| | | | '_ ` _ \  | '_ ` _ \ / _` |/ __| '_ \| | '_ \ / _ \
    | | |_| | | | | | | (_| |      | | | (_| | |  | |_| | | | | | | | | | | | | (_| | (__| | | | | | | |  __/
    |_|\__,_|_| |_| |_|\__,_|      |_|  \__,_|_|   \__,_|_| |_| |_| |_| |_| |_|\__,_|\___|_| |_|_|_| |_|\___|
                                                                                                          
    Traditional, with feeling.

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

#include "LM_Z80Keys.h"             // Let Teensy use the Z-80 side keyboard
#include "LM_Z80Patches.h"          // Surgical changes to the Z-80 code
#include "LM_LUI.h"                 // Teensy UI that uses Z-80 keyboard, displays, and drum I/O, and uses i2c OLED display for detailed UI
#include "LM_Voices.h"              // Sample loading routines
#include "LM_MIDI.h"                // USB & DIN-5 MIDI support, note on/off, start/stop, MIDI clock, Sysex sample download
#include "LM_OLED.h"                // OLED display support
#include "LM_SDCard.h"              // SD card load / save / format
#include "LM_Fan.h"                 // read temperature, control fan
#include "LM_Utilities.h"           // misc - reboot, BCD/Decimal, etc.

#include "LM1_RAM.h"                // 8KB default snapshot of Z-80 RAM with patterns loaded


/* ===========================================================================================================
    Version and Serial Number
*/

#define   LUMA1_FW_VERSION_MAJOR    0
#define   LUMA1_FW_VERSION_MINOR    921

char serial_number[9];                              // we terminate with 0 so we can use it as a string


/* ===========================================================================================================
*/

// Use startup_middle_hook() to run some init code very quickly after power on, but before the whole Arduino runtime is set up

extern "C" void startup_middle_hook(void);

void startup_middle_hook(void) {

  // ===============================
  // Set up Z-80 control, LM-1 ROM, Drum trigger interface
  
  pinMode( nVOICE_WR, OUTPUT );
  digitalWrite( nVOICE_WR, 1 );                     // voice board write strobe
  
  z80_reset( true );                                // hardware will power up with Z-80 /RESET = 0, sync our state
  
  init_z80_if();                                    // set up /BUSRQ, /BUSAK

  z80_reset( false );

  teensy_drives_z80_bus( true );                    // *** Teensy owns Z-80 bus

  trig_voice( STB_BASS,     0x00 );                 // make sure everyone is quiet
  trig_voice( STB_SNARE,    0x00 );
//  trig_voice( STB_HIHAT,    0x00 );
  trig_voice( STB_CLAPS,    0x00 );
  trig_voice( STB_CABASA,   0x00 );
  trig_voice( STB_TAMB,     0x00 );
  trig_voice( STB_TOMS,     0x00 );
  trig_voice( STB_COWBELL,  0x00 );
  trig_voice( STB_CLAVE,    0x00 );
  trig_voice( STB_CLICK,    0x00 );

  z80_bus_write( LINK_DISPLAY, 0xff );              // LED displays off
  z80_bus_write( PATT_DISPLAY, 0xff );

  z80_bus_write( LED_SET_1, 0 );
  z80_bus_write( LED_SET_2, 0 );

  pinMode( LM1_LED_A, INPUT );                      // STORE LED, used to catch when STORE button is pressed (invokes Local UI)

  pinMode( TAPE_FSK_TTL, INPUT );                   // TTL version of what LM-1 drives to the SYNC OUT jack when playing/recording

  set_tape_sync_mode( TAPE_SYNC_MODE_CLK );         // drives what was TAPE SYNC clock, now MIDI clock

  // ===============================
  // Force millis() to be 300 to skip startup delays
  
  systick_millis_count = 300;
}


void setup() {
  Serial.begin( 115200 );
  delay( 50 );
  Serial.println("Hello, LM-1derful people");

  eeprom_get_serial_number( serial_number );
  Serial.printf( "Serial Number: %s\n", serial_number );
  
  if( setup_oled_display() )
    display_oled_bootscreen();
  
  // ===============================
  // Set up Drum trigger interface

  init_drum_trig_expander();

  // ===============================
  // Check if STORE is down -- if so, DON'T start Z-80. go into Local UI mode.

  init_keyboard();                                    // for local UI, Teensy scans LM-1 keyboard  
  scan_keyboard();                                    // check for any keys down

  if(   (get_keycode() != KEY_MENU) &&                // hold down MENU key to boot straight into Teensy mode
        init_sd_card() &&                             // SD card OK?
        load_z80_rom_file( "Z80_CODE" ) ) {           // able to load the Z-80 code?
                                                      //    load z-80 RAM and sounds, start z-80
    // ===============================
    // use this to load patterns from RAM image
    
    //load_z80_ram( factory_ram );                            

    // ===============================
    // patch Z-80 ROM STORE

    apply_z80_patches();

    // ===============================
    // restore settings from eeprom

    z80_bus_write( D802_SHADOW, 0 );                  // starting up, zero the D802 reg shadow (fan_enable uses it)
    
    set_fan_mode( eeprom_load_fan_mode() );           // actually only gets evaluated (taking fan action) in loop
    
    // ===============================
    // --- Load initial sounds
  
    load_voice_bank( voice_load_bm, 0 );              // XXX TODO - store & restore boot bank
      
    teensy_drives_z80_bus( false );                   // *** Teensy releases Z-80 bus

    z80_reset( true );                                // quickly reboot Z-80 to run code we just loaded

    delay( 10 );

    z80_reset( false );                               // Here we go!
      
    delay( 250 );                                     // 250ms to let the Z-80 get up and running

    // ===============================
    // Set up MIDI
  
    init_midi();
  
    // ===============================
    // Interrupts
  
    enable_drum_trig_interrupt();
    enable_midi_start_stop_clock( true );
  
    // ===============================
    // Temperature, Fan Control
    
    init_fan();

    Serial.println("\n\nSetup complete. It's time to party, it's time to jam.\n");
  }
  else {
    forced_local_ui_mode = true;
    set_LED_SET_2( LED_STORE );                       // STORE LED on, will take us into local ui mode in main loop

    Serial.printf("\n=== FORCED ENTRY INTO LOCAL UI MODE, type ? in terminal for diag commands\n\n");
  }
}



/*
    loop()

    In general, nothing in loop() should block. We want to keep handling of MIDI data low-latency.

    One exception:  When we are in "Local UI" mode, the Z-80 is halted and the Teensy is driving the keyboard and LEDs
                    for things like MIDI config, voice loading, etc. In this mode, delay() is sometimes used.
*/

// These are tasks that have tight timing requirements. 
// This may be called from other places that take a long time (like the OLED display update routine).

void loop_time_critical() {
  
  // -- MIDI
  
  handle_midi_in();
  handle_midi_out();
}


void loop() {

  loop_time_critical();
  
  // -- Local UI -- loading voices, setting MIDI channel, saving/loading RAM, etc.

  handle_local_ui();                                // Teensy-driven UI when STORE button pressed  

  handle_oled_display();                            // if i2c OLED display fitted, update it

  handle_debug_commands();                          // commands over the USB serial port

  // -- Things to do only if NOT in local ui mode
  
  if( !in_local_ui() ) {
    
    // -- Fan
    
    handle_fan();
  }  
}
