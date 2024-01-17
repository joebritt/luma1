/* ---------------------------------------------------------------------------------------
    Luma-1 MIDI

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

#include "LM_MIDI.h"
#include "LM_Utilities.h"

#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, HW_MIDI, midiDIN);

// HW_MIDI is on Serial1, which is i.MX RT1062 LPUART UART6. This is for a hack to invert the TX line (which Luma's hardware needs)

#define IMXRT_LPUART6_ADDRESS     0x40198000
#define IMXRT_LPUART_CTRL         0x18
#define TXINV_FORCE               0x10000000        // TXINV bit to set


// --- MIDI interface routing

uint8_t midi_note_out_route;      // can be DIN5, USB, or Both
uint8_t midi_note_in_route;       // can be DIN4, USB, or Both

uint8_t midi_clock_out_route;     // can be DIN5, USB, or Both
uint8_t midi_clock_in_route;      // can be DIN5 or USB only

uint8_t midi_sysex_route;         // can be DIN5 or USB only


// --- Updated by key, sequence, or midi drum trigger, selects which sound to load

uint16_t last_drum = STB_BASS;


// --- Drum Table data structure to manage sending Note Offs after triggers

bool map_midi_2_strobe( byte note, byte vel, uint16_t *strobe, uint8_t *flags );

// we send a NOF this many milliseconds after the corresponding NON

#define NOF_TIME_MS         40

typedef struct {
  byte midi_note;
  bool drum_playing;
  byte flags;
  uint16_t strobe;
  elapsedMillis drum_time_elapsed; 
} __attribute__((packed)) drum_midi_map_t;

drum_midi_map_t drums[13];

#define drum_BASS           0
#define drum_SNARE          1
#define drum_HIHAT          2
#define drum_HIHAT_OPEN     3
#define drum_CLAPS          4
#define drum_CABASA         5
#define drum_TAMB           6
#define drum_TOM_UP         7
#define drum_TOM_DN         8
#define drum_CONGA_UP       9
#define drum_CONGA_DN       10
#define drum_COWBELL        11
#define drum_CLAVE          12


void set_drum_table_entry( int entry, byte note, uint16_t stb, byte flgs ) {
  
  drums[entry].midi_note = note;
  drums[entry].drum_playing = false;
  drums[entry].strobe = stb;
  drums[entry].flags = flgs;

  //Serial.printf("drum: %02d : %02d : %04x : %02x\n", entry, note, stb, flgs);
}


bool map_midi_2_strobe( byte note, byte vel, uint16_t *strobe, uint8_t *flags ) {

  //Serial.printf("note: %02d\n", note);
  
  for( int xxx = 0; xxx != 13; xxx++ ) {                    // check all 13 drums
    
    if( drums[xxx].midi_note == note ) {                    // one of our notes?

      *strobe = drums[xxx].strobe;                          // get the hardware strobe address

      if( drums[xxx].flags == 0 )                           // if flags == 0, it's a loud/soft voice
        *flags = ( (vel>MIDI_VEL_SOFT) ? 0x03 : 0x01 );
      else
        *flags = drums[xxx].flags;                          // otherwise it's special

      return true;
    }
    
  }

  return false;
}


/* ---------------------------------------------------------------------------------------
    All MIDI functions for both the DIN-5 and USB interfaces
*/

void init_midi() {

  // -- restore settings from eeprom
  
  midi_chan = eeprom_load_midi_channel();
  
  set_midi_note_out_route( eeprom_load_midi_note_out_route() );
  set_midi_note_in_route( eeprom_load_midi_note_in_route() );

  set_midi_clock_out_route( eeprom_load_midi_clock_out_route() );
  set_midi_clock_in_route( eeprom_load_midi_clock_in_route() );

  set_midi_sysex_route( eeprom_load_midi_sysex_route() );


  // -- set up the drum voice management (used for NOFs) table

  set_drum_table_entry( drum_BASS,        MIDI_NOTE_BASS,         STB_BASS,         0       );        // flags of 0 -> this voice can play quietly (0x01) or loudly (0x03), use velocity to decide
  set_drum_table_entry( drum_SNARE,       MIDI_NOTE_SNARE,        STB_SNARE,        0       );

  set_drum_table_entry( drum_HIHAT,       MIDI_NOTE_HIHAT,        STB_HIHAT,        0       );
  set_drum_table_entry( drum_HIHAT_OPEN,  MIDI_NOTE_HIHAT_OPEN,   STB_HIHAT,        0x07    );

  set_drum_table_entry( drum_CLAPS,       MIDI_NOTE_CLAPS,        STB_CLAPS,        0x01    );
  set_drum_table_entry( drum_CABASA,      MIDI_NOTE_CABASA,       STB_CABASA,       0       );
  set_drum_table_entry( drum_TAMB,        MIDI_NOTE_TAMB,         STB_TAMB,         0       );

  set_drum_table_entry( drum_TOM_UP,      MIDI_NOTE_TOM_UP,       STB_TOMS,         0x03    );
  set_drum_table_entry( drum_TOM_DN,      MIDI_NOTE_TOM_DN,       STB_TOMS,         0x01    );

  set_drum_table_entry( drum_CONGA_UP,    MIDI_NOTE_CONGA_UP,     STB_CONGAS,       0x03    );
  set_drum_table_entry( drum_CONGA_DN,    MIDI_NOTE_CONGA_DN,     STB_CONGAS,       0x01    );

  set_drum_table_entry( drum_COWBELL,     MIDI_NOTE_COWBELL,      STB_COWBELL,      0x01    );
  set_drum_table_entry( drum_CLAVE,       MIDI_NOTE_CLAVE,        STB_CLAVE,        0x01    );
  
  
  // -- DIN-5 old-skool MIDI

  midiDIN.setHandleNoteOn(          din_myNoteOn          );
  midiDIN.setHandleNoteOff(         din_myNoteOff         );

  midiDIN.setHandleProgramChange(   din_myProgramChange   );
  
  midiDIN.setHandleClock(           din_myClock           );
  midiDIN.setHandleStart(           din_myStart           );
  midiDIN.setHandleStop(            din_myStop            );
  
  midiDIN.setHandleSystemExclusive( din_mySystemExclusiveChunk  );

  midiDIN.begin();

  // -- Total hack: manually whack the TXINV bit in the LPUART6 block. 
  //    We need inverted TX for Luma-1's hardware, but MIDI library doesn't expose serial port format settings
  
  *(volatile uint32_t *)(IMXRT_LPUART6_ADDRESS + IMXRT_LPUART_CTRL) |= TXINV_FORCE;
  

  // -- modern USB hotness MIDI

  usbMIDI.setHandleNoteOn(          usb_myNoteOn          );
  usbMIDI.setHandleNoteOff(         usb_myNoteOff         );

  usbMIDI.setHandleProgramChange(   usb_myProgramChange   );
  
  usbMIDI.setHandleClock(           usb_myClock           );
  usbMIDI.setHandleStart(           usb_myStart           );
  usbMIDI.setHandleStop(            usb_myStop            );
  
  usbMIDI.setHandleSystemExclusive( usb_mySystemExclusiveChunk  );
}



/* ---------------------------------------------------------------------------------------
    Interface routing
*/

// --- Notes

void set_midi_note_out_route( uint8_t r )   {     midi_note_out_route = r;      }
void set_midi_note_in_route( uint8_t r )    {     midi_note_in_route = r;       }

uint8_t get_midi_note_out_route()           {     return midi_note_out_route;   }
uint8_t get_midi_note_in_route()            {     return midi_note_in_route;    }

// --- Start / Stop / Clock

void set_midi_clock_out_route( uint8_t r )  {     midi_clock_out_route = r;     }
void set_midi_clock_in_route( uint8_t r )   {     midi_clock_in_route = r;      }

uint8_t get_midi_clock_out_route()          {     return midi_clock_out_route;  }
uint8_t get_midi_clock_in_route()           {     return midi_clock_in_route;   }

// --- Sysex

void set_midi_sysex_route( uint8_t r )      {     midi_sysex_route = r;         }

uint8_t get_midi_sysex_route()              {     return midi_sysex_route;      }



/* ---------------------------------------------------------------------------------------
    Interface-specific trampolines, which choose whether or not to call the actual handler based on interface enable settings
*/

// ======================================================================
// DIN-5 MIDI Trampolines

void din_myNoteOn(byte channel, byte note, byte velocity) {
  if( get_midi_note_in_route() & ROUTE_DIN5 ) {
    myNoteOn( channel, note, velocity );
    midi_din_in_event();
  }
}

void din_myNoteOff(byte channel, byte note, byte velocity) {
  if( get_midi_note_in_route() & ROUTE_DIN5 ) {
    myNoteOff( channel, note, velocity );
    midi_din_in_event();
  }
}


void din_myProgramChange(byte channel, byte pgm) {
  if( get_midi_note_in_route() & ROUTE_DIN5 ) {
    myProgramChange( channel, pgm );
    midi_din_in_event();
  }
}


void din_myClock() {
  if( get_midi_clock_in_route() & ROUTE_DIN5 ) {
    myClock();
    midi_din_in_event();
  }
}

void din_myStart() {
  if( get_midi_clock_in_route() & ROUTE_DIN5 ) {
    myStart();
    midi_din_in_event();
  }
}

void din_myStop() {
  if( get_midi_clock_in_route() & ROUTE_DIN5 ) {
    myStop();
    midi_din_in_event();
  }
}


/*
    VERY hacky fixes to some broken behavior in the DIN-5 (UART) MIDI Sysex receive handling.
    
      - The blocks we get passed are smaller (not a problem)
      - The blocks have unexpected 0xf0 and 0xf7 characters (a problem)
      - The "last" flag doesn't go true on the last block (another problem)

    Here are some blocks as received, and how I massage them to get rid of the bad stuff:

    --> original
    Dumping 128 bytes: 
    0000: f0 69 00 00 41 66 72 69 63 61 00 42 6c 00 00 00  |   i     A f r i c a   B l       
    0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |                                 
    0020: 00 00 01 00 00 00 00 00 00 31 67 50 30 57 57 2f  |                   1 g P 0 W W / 
    0030: 54 44 06 47 55 4f 27 50 60 2f 1c 77 76 73 7f 7c  | T D   G U O ' P ` /   w v s   | 
    0040: 6f 79 30 5d 54 5b 38 73 7d 77 31 4f 7a 79 6e 7f  | o y 0 ] T [ 8 s } w 1 O z y n   
    0050: 6d 63 7c 74 6e 64 74 6b 74 7a 78 60 7e 7a 6c 57  | m c | t n d t k t z x ` ~ z l W 
    0060: 71 4d 70 6f 74 6f 3c 7c 7f 77 61 5e 6f 55 7c 7c  | q M p o t o < |   w a ^ o U | | 
    0070: 53 71 63 74 5d 6c 7c 69 7a 7f 47 7f 4b 79 62 f0  | S q c t ] l | i z   G   K y b   
    --> fixed
    Dumping 127 bytes: 
    0000: f0 69 00 00 41 66 72 69 63 61 00 42 6c 00 00 00  |   i     A f r i c a   B l       
    0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |                                 
    0020: 00 00 01 00 00 00 00 00 00 31 67 50 30 57 57 2f  |                   1 g P 0 W W / 
    0030: 54 44 06 47 55 4f 27 50 60 2f 1c 77 76 73 7f 7c  | T D   G U O ' P ` /   w v s   | 
    0040: 6f 79 30 5d 54 5b 38 73 7d 77 31 4f 7a 79 6e 7f  | o y 0 ] T [ 8 s } w 1 O z y n   
    0050: 6d 63 7c 74 6e 64 74 6b 74 7a 78 60 7e 7a 6c 57  | m c | t n d t k t z x ` ~ z l W 
    0060: 71 4d 70 6f 74 6f 3c 7c 7f 77 61 5e 6f 55 7c 7c  | q M p o t o < |   w a ^ o U | | 
    0070: 53 71 63 74 5d 6c 7c 69 7a 7f 47 7f 4b 79 62 f0  | S q c t ] l | i z   G   K y b   
    sysex: 127, last: 0, err: 0
    
    --> original
    Dumping 128 bytes: 
    0000: f7 7d 7f 7a 06 56 7c 7b 37 72 59 72 04 7f 7f 7f  |   }   z   V | { 7 r Y r         
    0010: 64 4d 48 64 33 3f 62 5f 54 50 7b 7f 5f 78 4f 58  | d M H d 3 ? b _ T P {   _ x O X 
    0020: 7f 7f 79 52 0e 6f 7f 7c 12 77 5e 7f 18 7f 7c 54  |     y R   o   |   w ^       | T 
    0030: 67 6f 7f 7f 39 4f 75 7f 78 3e 71 43 7f 7f 7f 77  | g o     9 O u   x > q C       w 
    0040: 5f 39 6f 7d 40 71 4a 6b 67 5f 70 7e 00 7b 67 59  | _ 9 o } @ q J k g _ p ~   { g Y 
    0050: 71 7e 7f 72 39 54 37 48 3e 5f 61 73 7f 7f 7f 75  | q ~   r 9 T 7 H > _ a s       u 
    0060: 5e 52 6b 7f 61 7f 6f 62 7b 7b 4c 67 02 57 7f 7f  | ^ R k   a   o b { { L g   W     
    0070: 7f 74 48 3d 0f 77 7b 57 6d 75 74 75 7f 77 78 f0  |   t H =   w { W m u t u   w x   
    --> fixed
    Dumping 126 bytes: 
    0000: 7d 7f 7a 06 56 7c 7b 37 72 59 72 04 7f 7f 7f 64  | }   z   V | { 7 r Y r         d 
    0010: 4d 48 64 33 3f 62 5f 54 50 7b 7f 5f 78 4f 58 7f  | M H d 3 ? b _ T P {   _ x O X   
    0020: 7f 79 52 0e 6f 7f 7c 12 77 5e 7f 18 7f 7c 54 67  |   y R   o   |   w ^       | T g 
    0030: 6f 7f 7f 39 4f 75 7f 78 3e 71 43 7f 7f 7f 77 5f  | o     9 O u   x > q C       w _ 
    0040: 39 6f 7d 40 71 4a 6b 67 5f 70 7e 00 7b 67 59 71  | 9 o } @ q J k g _ p ~   { g Y q 
    0050: 7e 7f 72 39 54 37 48 3e 5f 61 73 7f 7f 7f 75 5e  | ~   r 9 T 7 H > _ a s       u ^ 
    0060: 52 6b 7f 61 7f 6f 62 7b 7b 4c 67 02 57 7f 7f 7f  | R k   a   o b { { L g   W       
    0070: 74 48 3d 0f 77 7b 57 6d 75 74 75 7f 77 78 f0 f0  | t H =   w { W m u t u   w x     
    sysex: 126, last: 0, err: 0

    This seems to work, at least with my setup (Mac, using SysEx Librarian and Greg Simon's
      LumaTool to send files).
*/

bool getting_1st_din5_block = true;

void din_mySystemExclusiveChunk(const byte *d, uint16_t len, bool last) {
  if( get_midi_sysex_route() & ROUTE_DIN5 ) {
      
    //Serial.printf("--> original\n");
    //dumpit( d, len );
    
    // --> In the blocks AFTER the first block, there is a bogus 0xf7 in the first byte.

    if( getting_1st_din5_block == false ) {
      if( d[0] == 0xf7 ) {
        len--;
        memcpy( d, &d[1], len );
      }
    }
    else
      getting_1st_din5_block = false;

    // --> In each block, there is a bogus 0xf0 in the last byte.

    if( d[len-1] == 0xf0 )
      len--;

    //Serial.printf("--> fixed\n");
    //dumpit( d, len );
      
    if( (int)len < 126 ) {                  // XXX "last" seems to be broken on non-USB MIDI, hacky workaround --> 126 is len when 2 bytes pulled from 128 byte block
      last = true;
      getting_1st_din5_block = true;        // reset this other hack
    }
    else
      last = false;
      
    mySystemExclusiveChunk( d, len, last );                                                                      // XXX total hack, it sends 128 when false, and not 128 when true
    midi_din_in_event();
  }
}


// ======================================================================
// USB MIDI Trampolines

void usb_myNoteOn(byte channel, byte note, byte velocity) {
  if( get_midi_note_in_route() & ROUTE_USB ) {
    myNoteOn( channel, note, velocity );
    midi_usb_in_event();
  }
}

void usb_myNoteOff(byte channel, byte note, byte velocity) {
  if( get_midi_note_in_route() & ROUTE_USB ) {
    myNoteOff( channel, note, velocity );
    midi_usb_in_event();
  }
}


void usb_myProgramChange(byte channel, byte pgm) {
  if( get_midi_note_in_route() & ROUTE_USB ) {
    myProgramChange( channel, pgm );
    midi_usb_in_event();
  }
}


void usb_myClock() {
  if( get_midi_clock_in_route() & ROUTE_USB ) {
    myClock();
    midi_usb_in_event();
  }
}

void usb_myStart() {
  if( get_midi_clock_in_route() & ROUTE_USB ) {
    myStart();
    midi_usb_in_event();
  }
}

void usb_myStop() {
  if( get_midi_clock_in_route() & ROUTE_USB ) {
    myStop();
    midi_usb_in_event();
  }
}


void usb_mySystemExclusiveChunk(const byte *d, uint16_t len, bool last) {
  if( get_midi_sysex_route() & ROUTE_USB ) {
    mySystemExclusiveChunk( d, len, last );
    midi_usb_in_event();
  }
}


/* ---------------------------------------------------------------------------------------
    MIDI traffic indicators
*/

elapsedMillis since_din_out;
elapsedMillis since_din_in;

elapsedMillis since_usb_out;
elapsedMillis since_usb_in;

#define SINCE_DWELL_MAX     100             // show for up to this many milliseconds

bool did_din_out;
bool did_din_in;

bool did_usb_out;
bool did_usb_in;

void midi_din_out_event() {                 // these are called by MIDI transmit and reeceive routines
  did_din_out = true;
  since_din_out = 0;
}

void midi_din_in_event() {
  did_din_in = true;
  since_din_in = 0;
}

void midi_usb_out_event() {
  did_usb_out = true;
  since_usb_out = 0;
}

void midi_usb_in_event() {
  did_usb_in = true;
  since_usb_in = 0;
}


                                            // these are called by the code that draws the activity arrows on the OLED
bool midi_din_out_active() {
  if( did_din_out && (since_din_out < SINCE_DWELL_MAX) )
    return true;
  else
    did_din_out = false;

  return false;
}

bool midi_din_in_active() {
  if( did_din_in && (since_din_in < SINCE_DWELL_MAX) )
    return true;
  else
    did_din_in = false;

  return false;
}

bool midi_usb_out_active() {
  if( did_usb_out && (since_usb_out < SINCE_DWELL_MAX) )
    return true;
  else
    did_usb_out = false;

  return false;}

bool midi_usb_in_active() {
  if( did_usb_in && (since_usb_in < SINCE_DWELL_MAX) )
    return true;
  else
    did_usb_in = false;

  return false;
}


/* ---------------------------------------------------------------------------------------
    __  __ _____ _____ _____   _____ _   _ 
   |  \/  |_   _|  __ \_   _| |_   _| \ | |
   | \  / | | | | |  | || |     | | |  \| |
   | |\/| | | | | |  | || |     | | | . ` |
   | |  | |_| |_| |__| || |_   _| |_| |\  |
   |_|  |_|_____|_____/_____| |_____|_| \_|
                                         
    MIDI In Handling

    NOTE:
    handle_midi_in() must be called frequently -- it is what triggers processing of received messages
*/

void handle_midi_in() {
  char c;

  // -- DIN-5 old-skool MIDI

  if( midi_chan == 0 )                // 0 = OMNI
    midiDIN.read();
  else
    midiDIN.read( midi_chan );


  // -- modern USB hotness MIDI
 
  if( midi_chan == 0 )                // 0 = OMNI
    usbMIDI.read();
  else
    usbMIDI.read( midi_chan );

}


void myNoteOn(byte channel, byte note, byte velocity) {
  
  play_midi_drm( note, velocity );
  
}

void myNoteOff(byte channel, byte note, byte velocity) {
}



void play_midi_drm( byte note, byte vel ) {
  uint16_t strobe;
  uint8_t flags;

  if( map_midi_2_strobe( note, vel, &strobe, &flags ) ) {   // is it valid? map to strobe, set loudness flag
    
    if( vel != 0 ) {                                        // velocity 0 = NOF
      teensy_drives_z80_bus( true );                        // grab the bus
    
      disable_drum_trig_interrupt();                        // don't detect drum writes as triggers
    
      trig_voice( strobe, 0x00 );                           // stop
      trig_voice( strobe, flags );                          // start
      
      restore_drum_trig_interrupt();                        // the way we were
    
      teensy_drives_z80_bus( false );                       // back to our regularly scheduled program
    }

    last_drum = strobe;                                     // NON & NOF used to select target voice for sysex sample download
  }
}


// Program Change messages load the voice bank with that program number

void myProgramChange(byte channel, byte pgm) {

  if( (uint8_t)pgm <= 99 ) {
    Serial.printf("Program Change val = %d\n", pgm);
    teensy_drives_z80_bus( true );
    load_voice_bank( voice_load_bm, pgm );
    teensy_drives_z80_bus( false );
  }
  else
    Serial.printf("### ERROR: Program Change val = %d\n", pgm);
}




/* ---------------------------------------------------------------------------------------
    __  __ _____ _____ _____    ____  _    _ _______ 
   |  \/  |_   _|  __ \_   _|  / __ \| |  | |__   __|
   | \  / | | | | |  | || |   | |  | | |  | |  | |   
   | |\/| | | | | |  | || |   | |  | | |  | |  | |   
   | |  | |_| |_| |__| || |_  | |__| | |__| |  | |   
   |_|  |_|_____|_____/_____|  \____/ \____/   |_|
 
    MIDI Out Handling
*/

elapsedMillis no_tempo_clk_detect = 0;                        // detect "no clk" -- often switch on rear set incorrectly

bool luma_is_playing() {
  return !( no_tempo_clk_detect > MS_TO_NO_CLK );             // used to display "Awaiting Tempo Clock" and control when fan can run
}

  


// These are used to send Note Off (NOF) messages at some time after a NON. We can't tell when a sample is finished playing, so we use a fixed time.

void check_NOF_times() {
  for( int xxx = 0; xxx != 13; xxx++ ) {                                  // check all 13 drums
    
    if( drums[xxx].drum_playing == true ) {                               // is this one playing?

      if ( drums[xxx].drum_time_elapsed > NOF_TIME_MS ) {                 // is it time to send the NOF?

        if( get_midi_note_out_route() & ROUTE_DIN5 ) {
          midiDIN.sendNoteOff(  drums[xxx].midi_note, 127, (midi_chan == 0)?1:midi_chan );
          midi_din_out_event();
        }

        if( get_midi_note_out_route() & ROUTE_USB ) {
          usbMIDI.sendNoteOff(  drums[xxx].midi_note, 127, (midi_chan == 0)?1:midi_chan );
          midi_usb_out_event();
        }

        drums[xxx].drum_playing = false;
      }

    }
  }
}


char *drum_name( int idx, byte vel ) {
  switch( idx ) {
                  case drum_BASS:       if(vel>MIDI_VEL_SOFT) return "BASS";    else return "bass";     break;
                  case drum_SNARE:      if(vel>MIDI_VEL_SOFT) return "SNARE";   else return "snare";    break;
                  case drum_HIHAT:      if(vel>MIDI_VEL_SOFT) return "HIHAT";   else return "hihat";    break;
                  case drum_HIHAT_OPEN: return "HIHAT OPEN";                                            break;
                  case drum_CLAPS:      return "CLAPS";                                                 break;
                  case drum_CABASA:     if(vel>MIDI_VEL_SOFT) return "CABASA";  else return "cabasa";   break;
                  case drum_TAMB:       if(vel>MIDI_VEL_SOFT) return "TAMB";    else return "tamb";     break;
                  case drum_TOM_UP:     return "TOM ^";                                                 break;    
                  case drum_TOM_DN:     return "TOM v";                                                 break;
                  case drum_CONGA_UP:   return "CONGA ^";                                               break;
                  case drum_CONGA_DN:   return "CONGA v";                                               break;
                  case drum_COWBELL:    return "COWBELL";                                               break;
                  case drum_CLAVE:      return "RIMSHOT";                                               break;          
  }
}


void send_midi_drm( int drum_idx, byte vel ) {                            // if we are in OMNI mode, send on channel 1

  Serial.printf("%s: %d %d\n", drum_name(drum_idx,vel), drums[drum_idx].midi_note, vel );

  if( get_midi_note_out_route() & ROUTE_DIN5 ) {
    midiDIN.sendNoteOn(   drums[drum_idx].midi_note, vel, (midi_chan == 0)?1:midi_chan );
    midi_din_out_event();
  }

  if( get_midi_note_out_route() & ROUTE_USB ) {
    usbMIDI.sendNoteOn(   drums[drum_idx].midi_note, vel, (midi_chan == 0)?1:midi_chan );
    midi_usb_out_event();
  }
  
  drums[drum_idx].drum_time_elapsed = 0;                                  // prepare to send NOF in the near future
  drums[drum_idx].drum_playing = true;

  // this is used to target a drum voice when loading a sample via sysex
  last_drum = drums[drum_idx].strobe;
  
  // if any drum keys hit, make sure the bootscreen goes away
  bootscreen_showing = false;
}



void handle_midi_out() {

  // ===============================
  // Go see if it's time to send any Note Offs for drums that were triggered
  
  check_NOF_times();


  // ===============================
  // Cry, the Clock Said

  if( song_is_started && (sinceLastFSKClk > 100) ) {          // think song is running but 100ms passed with no FSK clock interrupt?

    if( get_midi_clock_out_route() & ROUTE_DIN5 ) {           // STOP -> DIN-5
      midiDIN.sendRealTime(MIDI_NAMESPACE::Stop);
      midi_din_out_event();
    }

    if( get_midi_clock_out_route() & ROUTE_USB ) {            // STOP -> USB
      usbMIDI.sendRealTime(usbMIDI.Stop);
      midi_usb_out_event();
    }
    
    song_is_started = false;
    send_midi_clk = false;
    Serial.println("sent MIDI Stop");
  }
  
  if( send_midi_clk ) {
    if( !song_is_started ) {

      if( get_midi_clock_out_route() & ROUTE_DIN5 ) {         // START -> DIN-5
        midiDIN.sendRealTime(MIDI_NAMESPACE::Start);
        midi_din_out_event();
      }

      if( get_midi_clock_out_route() & ROUTE_USB ) {          // START -> USB
        usbMIDI.sendRealTime(usbMIDI.Start);
        midi_usb_out_event();
      }

      song_is_started = true;
      Serial.println("sent MIDI Start");
    }

    if( get_midi_clock_out_route() & ROUTE_DIN5 ) {           // CLOCK -> DIN-5
      midiDIN.sendRealTime(MIDI_NAMESPACE::Clock);
      midi_din_out_event();
    }

    if( get_midi_clock_out_route() & ROUTE_USB ) {            // CLOCK -> USB    
      usbMIDI.sendRealTime(usbMIDI.Clock);
      midi_usb_out_event();
    }
    
    send_midi_clk = false;

    no_tempo_clk_detect = 0;                                        // reset "no clk" detector
  }

  
  // ===============================
  // see if any drums were hit, look at trigger bits captured from the i2c port expander
  
  if( check_triggers ) {
    /*
    Serial.print("Trig: "); printHex2( drum_triggers_a&0x7f ); Serial.print(" / "); printHex2( drum_triggers_b&0xf0 ); 
                                                          Serial.print(" / "); printHex2( drum_modifiers ); Serial.println();
    */
    
    if( drum_triggers_a ) {
      if( drum_triggers_a & 0x01 )  send_midi_drm( drum_CABASA,     (drum_modifiers & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
        
      if( drum_triggers_a & 0x02 )  send_midi_drm( drum_TAMB,       (drum_modifiers & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );

      if( drum_triggers_a & 0x04 ) {
        if( drum_modifiers & 0x04 ) 
          send_midi_drm( ( drum_modifiers & 0x02 ) ? drum_CONGA_UP : drum_CONGA_DN,           MIDI_VEL_LOUD );
        else
          send_midi_drm( ( drum_modifiers & 0x02 ) ? drum_TOM_UP : drum_TOM_DN,               MIDI_VEL_LOUD );
        }
      
      if( drum_triggers_a & 0x10 )  send_midi_drm( drum_COWBELL,                              MIDI_VEL_LOUD );
      
      if( drum_triggers_a & 0x20 )  send_midi_drm( drum_CLAVE,                                MIDI_VEL_LOUD );
    }
  
    
    if( drum_triggers_b ) {
      if( drum_triggers_b & 0x80 )  send_midi_drm( drum_CLAPS,                                MIDI_VEL_LOUD );

      // HIHAT / hihat / HIHAT SPLASH (OPEN)
      if( drum_triggers_b & 0x10 ) {      
        if( drum_modifiers & 0x04 ) send_midi_drm( drum_HIHAT_OPEN,                           MIDI_VEL_LOUD );
        else                        send_midi_drm( drum_HIHAT,      (drum_modifiers & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
      }
        
      if( drum_triggers_b & 0x40 )  send_midi_drm( drum_BASS,                                 (drum_modifiers & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
        
      if( drum_triggers_b & 0x20 )  send_midi_drm( drum_SNARE,      (drum_modifiers & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
    }
  
    check_triggers = false;
  }  
}


/* ---------------------------------------------------------------------------------------
    __  __ _____ _____ _____    _____ _      ____   _____ _  __
   |  \/  |_   _|  __ \_   _|  / ____| |    / __ \ / ____| |/ /
   | \  / | | | | |  | || |   | |    | |   | |  | | |    | ' / 
   | |\/| | | | | |  | || |   | |    | |   | |  | | |    |  <  
   | |  | |_| |_| |__| || |_  | |____| |___| |__| | |____| . \ 
   |_|  |_|_____|_____/_____|  \_____|______\____/ \_____|_|\_\
     
    MIDI CLOCK IN & OUT

    MIDI Clock In drives the DECODED_TAPE_SYNC_CLK at the correct frequency to make the LM-1 firmware think
     that it is receiving a Tape Sync signal (like from an FSK Tape Sync stripe laid down on tape from an original LM-1).

    MIDI Clock out is generated when the LM-1 firmware sequencer is running. This is detected by the presence of a clock
     on the TAPE_FSK_TTL signal (which was originally the FSK Tape Sync output).

    The LM-1 internal clock runs at 48 PPQN.
    MIDI clock runs at 24 PPQN.
    Teensy software adjusts both as needed.
     
*/


/* ---------------------------------------------------------------------------------------
    Manage the TAPE_SYNC_MODE_CLK signal

    This gets used by the MIDI clock code AND the EPROM dumping code.
    These routines manage the mode of the TAPE_SYNC_MODE_CLK pin.
*/

int decoded_tape_sync_mode = TAPE_SYNC_MODE_CLK;
uint32_t decoded_tape_sync_freq = 0;


/*  we do this becuase the DECODED_TAPE_SYNC_CLK signal is used for 2 things:

      - General Purpose Output to drive /CE on EPROM socket for EPROM dumping
      - Clock to LM-1 firmware to simulate Tape Sync Clock
*/


int set_tape_sync_mode( int mode ) {
  int curmode = decoded_tape_sync_mode;

  pinMode( DECODED_TAPE_SYNC_CLK, OUTPUT );                                                                   // always an output, either mode
  
  switch( mode ) {
    case TAPE_SYNC_MODE_CLK:      analogWriteFrequency( DECODED_TAPE_SYNC_CLK, decoded_tape_sync_freq );      // last requested clk freq
                                  analogWrite( DECODED_TAPE_SYNC_CLK, 0 );                                    // off
                                  break;

    case TAPE_SYNC_MODE_GPO:      analogWrite( DECODED_TAPE_SYNC_CLK, 0 );
                                  digitalWrite( DECODED_TAPE_SYNC_CLK, 1 );                                   // default to hi
                                  break;
  }

  decoded_tape_sync_mode = mode;
  
  return curmode;
}


void set_tape_sync_clk_freq( uint32_t freq ) {
  decoded_tape_sync_freq = freq;                                                    // remember last requested freq, even if not in CLK mode
  
  if( decoded_tape_sync_mode == TAPE_SYNC_MODE_CLK )                                // if we are in CLK mode...
    analogWriteFrequency( DECODED_TAPE_SYNC_CLK, decoded_tape_sync_freq );
  else
    Serial.println("*** Requested set_tape_sync_clk_run when not in CLK mode");
}

void set_tape_sync_clk_run( bool en ) {
  if( decoded_tape_sync_mode == TAPE_SYNC_MODE_CLK )                                // if we are in CLK mode...
    analogWrite( DECODED_TAPE_SYNC_CLK, en ? 128 : 0 );                             // ...if we are running set to 128 -> 50% duty cycle
  else
    Serial.println("*** Requested set_tape_sync_clk_run when not in CLK mode");
}


void set_tape_sync_clk_gpo( bool state ) {
  if( decoded_tape_sync_mode == TAPE_SYNC_MODE_GPO )
    digitalWrite( DECODED_TAPE_SYNC_CLK, state );
  else
    Serial.println("*** Requested set_tape_sync_clk_gpo when not in GPO mode");
}



/* ---------------------------------------------------------------------------------------
    MIDI Clock time calculation
*/

int clocks;
uint32_t clocktime24;

elapsedMicros sinceLastMIDIClk;                               // use this to keep track of time since previous MIDI clk


// look for new times that are +/- 10% of the previous

bool clocktime_sanity_check( uint32_t prev, uint32_t cur ) {
  uint32_t safe_band = prev / 10;

  if( (cur > (prev + safe_band)) || (cur < (prev - safe_band)) )
    return false;
  else
    return true;
}


// -- Called when we get a MIDI Clock

void myClock() {
  uint32_t lastclocktime = clocktime24;                       // use this to catch single clocktimes that are > sanity check threshold
  uint32_t f;
  
  clocktime24 = sinceLastMIDIClk;
  sinceLastMIDIClk = 0;

  if( clocktime_sanity_check( lastclocktime, clocktime24 ) ) {
    //Serial.print( clocktime24 ); Serial.print(" - ");

    f = clocktime24 / 10;                                     // time is in microseconds, chop it down 1 digit
    f = 100000 / f;                                           // this is really like a fixed-point 1/x

    //Serial.println( f );

    set_tape_sync_clk_freq( f*2 );                            // MIDI clk is 24 PPQN, LM-1 uses 48 PPQN
    set_tape_sync_clk_run( true );                            // run the clock
  }
}


// -- Called when we get a MIDI Start

void myStart() {
  Serial.println("received MIDI start");
  sinceLastMIDIClk = 0;
  clocks = 0;
}


// -- Called when we get a MIDI Stop

void myStop() {
  Serial.println("received MIDI stop");
  set_tape_sync_clk_run( false );                             // stop the clock
}




/* ---------------------------------------------------------------------------------------
    MIDI Clock Interrupt Handling
*/

bool midi_start_stop_clk_enable = false;

bool enable_midi_start_stop_clock( bool en ) {        // enable/disable interrupt used to detect FSK TTL clock
  bool prev_en = midi_start_stop_clk_enable;          // remember what it was

  midi_start_stop_clk_enable = en;
  
  if( en ) {
    Serial.println("enable clk detect");
    attachInterrupt( digitalPinToInterrupt(TAPE_FSK_TTL),  internal_tempo_clock, FALLING );
  }
  else {
    Serial.println("DISABLE clk detect");
    detachInterrupt( digitalPinToInterrupt(TAPE_FSK_TTL) );
  }

  return prev_en;                                     // so we can put it back like it was
}


int int_tempo_clk_counts = 0;

void internal_tempo_clock( void ) {
  if( !digitalRead( LM1_LED_A ) ) {
    int_tempo_clk_counts++;
    if( int_tempo_clk_counts >= 2 ) {                 // MIDI clock is 24 PPQN, FSK clock is 48 PPQN, so send a MIDI clock every other FSK clock
      int_tempo_clk_counts = 0;
      send_midi_clk = true;
    }
  
    sinceLastFSKClk = 0;                              // keep pulling this back to 0, it's how we detect that the FSK clock has stopped
  }
}


// This gets used for cases where we have not been able to keep an eye on the FSK clock

void reset_FSK_clock_check() {
  sinceLastFSKClk = 0;
  int_tempo_clk_counts = 0;
  send_midi_clk = false;
}





/* ---------------------------------------------------------------------------------------
     _______     _______ ________   __
    / ____\ \   / / ____|  ____\ \ / /
   | (___  \ \_/ / (___ | |__   \ V / 
    \___ \  \   / \___ \|  __|   > <  
    ____) |  | |  ____) | |____ / . \ 
   |_____/   |_| |_____/|______/_/ \_\
                                                          
    Sysex for:
      - Sample Download / Upload
      - Pattern Data Download / Upload
      - Utility Commands (setting serial number, bank name, etc.)

    FORMAT:
            0xF0 0x69 ...bytes of packed data... 0xF7
*/

#define OUR_MIDI_MFR_ID         0x69                                    // 0x60 - 0x7f [Reserved for Other Uses]

#define SYSEX_HEADER_SIZE       32

// ENCODING

#define ENCODE_BUF_SIZE         (65536 + SYSEX_HEADER_SIZE)             // large buffers to handle up to 32K samples, room to optimize but we have plenty of RAM

uint8_t sysex_encode_buf[ENCODE_BUF_SIZE];                              // working buf for sysex out operations
int sysex_encode_idx = 0;

int pack_sysex_data( int len, unsigned char *in, unsigned char *out );  // encode input buffer for sysex transmission (no high bits set)


// DECODING

#define DECODE_BUF_SIZE         (65536 + SYSEX_HEADER_SIZE)             // large buffers to handle up to 32K samples, room to optimize but we have plenty of RAM

uint8_t sysex_decode_buf[DECODE_BUF_SIZE];                              // working buf for sysex in operations, scratchpad for encode operations
int sysex_decode_idx = 0;

void init_sysex_decoder();
bool process_sysex_byte( uint8_t b );                                   // returns true when end of stream (0xf7) found

#define SYSEX_INITIALIZE        0                                       // stream decoder state machine
#define SYSEX_FIND_F0           1
#define SYSEX_FIND_MFR_ID       2
#define SYSEX_GET_B7S           3
#define SYSEX_GET_BYTE_1        4
#define SYSEX_GET_BYTE_2        5
#define SYSEX_GET_BYTE_3        6
#define SYSEX_GET_BYTE_4        7
#define SYSEX_GET_BYTE_5        8
#define SYSEX_GET_BYTE_6        9
#define SYSEX_GET_BYTE_7        10

int process_sysex_state = SYSEX_INITIALIZE;                             // this happens after an error or we successfully load a sound

bool check_store_byte( uint8_t b );
uint8_t b7s;

bool sysex_err_abort = false;

void sysex_load_prologue();
void sysex_load_epilogue();


// utilities

void send_sysex( int len, uint8_t *b );


// sysex data structures

/*
    1 byte header command:

    vvvv Rccc
    \__/ |\_/_____ 3-bit command
      |  +-------- 1-bit "Request" flag (1 -> Luma-1 should send some data back)
      +----------- 4-bit version, 0000 for v1.0 


    8 possible commands

    0     sample data receive / send (LEGACY)
    1     bank sample data receive / send
    2     bank ram data receive / send
    3     - unused -
    4     parameter set / get
    5     utility
    6     - unused -
    7     escape code, look at next byte for extended command

    32 Byte Header Format

    typedef struct {
      uint8_t cmd;                          // see table of cmds
      uint8_t params[31];                   // cmd-specific parameter data
    } __attribute__((packed)) sx_hdr_t;
                                            // struct is followed by cmd-specific data (e.g., sample data for cmd 0)

    All Requests will be responded to with the corresponding non-Request message (cmd bit 3 cleared).
    May return error in name field:
                                    SD ERROR
                                    NOT FOUND
    cmd  req
    ---  ---
    00 / 08   SAMPLE Down / Upload (to/from active bank), 24-char sample name, 7-byte pad (to 32 bytes), followed by uLaw sample data
    01 / 09   BANK SAMPLE Down / Upload (to/from SD bank/drum), 1 byte bank#, 1 byte drum sel, 24 byte name, pad 5, uLaw sample data
    02 / 0a   BANK RAM Down / Upload (to/from SD bank), 1 byte bank#, 24 byte name, 6 byte pad, followed by 8KB RAM data
    03 / 0b   
    04 / 0c   8-bit PARAMETER (see param table below), 8-bit param, 8-bit val
    05 / 0d   UTILITY, 8-bit util id, up to 30 bytes of id specifit data, see table below
    06        ERROR REPLY
    07 / 0f   ESCAPE, next byte is an extended cmd, currently unused
 
    In an error situation, Luma-1 will reply with a cmd = 06 message with one of the following strings in the name field:

    SD ERROR        SD Card error
    NOT FOUND       no file in that bank                                       
*/

#define CMD_SAMPLE          0x00
#define CMD_SAMPLE_BANK     0x01
#define CMD_RAM_BANK        0x02

#define CMD_PARAM           0x04
#define CMD_UTIL            0x05

#define CMD_REQUEST         0x08        // OR into CMD to turn it into a REQUEST


// === SAMPLES

//  cmd = 0x00 / 0x08 is sample load / request, current active bank (STAGING)
//  
//  LEGACY COMMAND, leaving in for backward compatibility with older LumaTool. Send/Get last selected drum.

typedef struct {
  uint8_t cmd;                          // 0x00 or 0x08
  char name[24];                        // sample name, C string
  uint8_t pad[7];                       // unused, pad
} __attribute__((packed)) sx_sample_hdr_t;
                                            // struct is followed by 8-bit uLaw sample data
                                            
//  cmd = 0x01 / 0x09 is sample store in specific bank and drum, or request for specific bank and drum

typedef struct {
  uint8_t cmd;                          // 0x01 or 0x09
  char name[24];                        // sample name, C string
  uint8_t bank;                         // 00 - 99, 0xff for current bank (STAGING) ---> XXX NOTE at least for now, all go to current bank.
  uint8_t drum_sel;                     // drum select, 0xff for last triggered drum
  uint8_t pad[5];                       // unused, pad
} __attribute__((packed)) sx_sample_bank_hdr_t;


// === RAM

//  cmd = 0x02 / 0x0a is ram load, to/from selected RAM bank OR active RAM for bank == 0xff, name is C string name, or request current RAM

typedef struct {
  uint8_t cmd;                          // 0x02 or 0x0a
  char name[24];                        // ram bank name, C string
  uint8_t bank;                         // 00 - 99, 0xff = currently active RAM
  uint8_t pad[6];                       // unused, pad
} __attribute__((packed)) sx_ram_bank_hdr_t;
                                        // struct is followed by RAM data, 8KB

// === PARAMETERS

//  cmd = 0x04 / 0x0c is set / get parameter

typedef struct {
  uint8_t cmd;                          // 0x04 or 0x0c
  uint8_t param;                        
  uint8_t val;                          // data or empty for fetch
  uint8_t pad[29];                      // unused, pad
} __attribute__((packed)) sx_parm_hdr_t;
                                        // nothing after, all data in struct
// Params:
#define SX_PARAM_FAN          0x00      // fan mode
#define SX_PARAM_BOOTSCREEN   0x01      // boot screen
#define SX_PARAM_KEYPRESS     0x02      // jam in a key
#define SX_SAVE_VOICE_BANK    0x03      // move STAGING voice bank to bank specified by val (use Program Change to load voice banks)


// === UTILITIES

//  cmd = 0x05 / 0x0d is utility write / read

//  cmd = 0x05 / 0x0d, uid = 0x00, Serial Number write / read
//  cmd = 0x05 / 0x0d, uid = 0x01, Machine Name write / read
//  cmd = 0x05 / 0x0d, uid = 0x02, User Name write / read

typedef struct {
  uint8_t cmd;                          // 0x05 or 0x0d
  uint8_t uid;                          // util id
  char s[24];                           // C string value
  uint8_t pad[6];                       // unused, pad
} __attribute__((packed)) sx_util_str_hdr_t;



/* ---------------------------------------------------------------------------------------
    SysEx Pattern RAM transfers

    Use bank # 0xff for current RAM
*/

void send_pattern_RAM_sysex( uint8_t banknum ) {
  int encoded_size = 0;
  uint8_t *ram;
  sx_ram_bank_hdr_t *hdr = (sx_ram_bank_hdr_t*)sysex_decode_buf;

  Serial.printf("SysEx send Pattern\n");
  
  // -- go get it, either from SD bank # or STAGING

  ram = get_ram_bank( banknum );

  if( ram ) {                                                       // valid?
  
    // -- zero working buf
  
    memset( sysex_decode_buf, 0, sizeof(sysex_decode_buf) );
    
    // --- build the unencoded message
  
    hdr->cmd = CMD_RAM_BANK;
    hdr->bank = banknum;                                            // 0xff for current working pattern RAM
  
    sprintf( hdr->name, get_ram_bank_name( banknum ) );
  
    memcpy( &sysex_decode_buf[sizeof(sx_ram_bank_hdr_t)], ram, 8192 );
  
    //Serial.printf("===> WILL ENCODE: \n");
    //dumpit( sysex_decode_buf, 256 );
  
    // --- encode
  
    /*
        The MIDI library wraps our data with the needed F0 start and F7 end markers.
        We jam our mfr ID at the start, unencoded and always with the hi bit clear.
  
        F0
        mfr ID
        ...encoded data, all hi bits clear...
        F7
    */
      
    sysex_encode_buf[0] = OUR_MIDI_MFR_ID;
    
    encoded_size = pack_sysex_data( 8192 + sizeof(sx_ram_bank_hdr_t), sysex_decode_buf, &sysex_encode_buf[1] );
  
    encoded_size += 1;                                // for the unencoded mfr ID in location 0
  
    // --- transmit
  
    send_sysex( encoded_size, sysex_encode_buf );
  }
  else {
    Serial.printf("### Error loading bank %02x\n", banknum);
    beep_failure();
  }
}


void sysex_ram_load( uint8_t *se, int len ) {
  char vname[24];

  Serial.printf("Pattern RAM len: %d\n", len - SYSEX_HEADER_SIZE );
  Serial.printf("Pattern RAM name: %s\n", (char*)&se[1] );
  
  // --- sanity check the name
  
  if( strlen( (char*)&sysex_decode_buf[1] ) == 0 )                                                                  // no name?
    snprintf( vname, 24, "RAM_BANK_%04X.BIN", checksum((uint8_t*)&se[SYSEX_HEADER_SIZE],8192) );                    // default name w/checksum
  else
    snprintf( vname, 24, "%s", (char*)&sysex_decode_buf[1] );

  // --- Z-80 was halted in sysex_load_prologue(), now load new RAM

  load_z80_ram( (uint8_t*)&se[SYSEX_HEADER_SIZE] );
  
  delay( 250 );
}


void sysex_ram_request( uint8_t *se, int len ) {
  sx_ram_bank_hdr_t *hdr = (sx_ram_bank_hdr_t*)se;

  Serial.printf("Pattern RAM request: Bank %02d\n", hdr->bank);

  send_pattern_RAM_sysex( hdr->bank );
}


/* ---------------------------------------------------------------------------------------
    SysEx Sample transfers
*/

uint16_t drum_sel_2_voice( uint8_t drum_sel ) {
  switch( drum_sel ) {
    case DRUM_SEL_BASS:     return STB_BASS;
    case DRUM_SEL_SNARE:    return STB_SNARE;
    case DRUM_SEL_HIHAT:    return STB_HIHAT;
    case DRUM_SEL_CLAPS:    return STB_CLAPS;
    case DRUM_SEL_CABASA:   return STB_CABASA;
    case DRUM_SEL_TAMB:     return STB_TAMB;
    case DRUM_SEL_TOM:      return STB_TOMS;
    case DRUM_SEL_CONGA:    return STB_CONGAS;
    case DRUM_SEL_COWBELL:  return STB_COWBELL;
    case DRUM_SEL_CLAVE:    return STB_CLAVE;
    case 0xff:              return last_drum;           // last active drum
    default:                return 0;                   // error, should not happen
  }
}


char *drum_sel_2_name( uint8_t drum_sel ) {
  switch( drum_sel ) {
    case DRUM_SEL_BASS:     return "BASS";
    case DRUM_SEL_SNARE:    return "SNARE";
    case DRUM_SEL_HIHAT:    return "HIHAT";
    case DRUM_SEL_CLAPS:    return "CLAPS";
    case DRUM_SEL_CABASA:   return "CABASA";
    case DRUM_SEL_TAMB:     return "TAMB";
    case DRUM_SEL_TOM:      return "TOMS";
    case DRUM_SEL_CONGA:    return "CONGAS";
    case DRUM_SEL_COWBELL:  return "COWBELL";
    case DRUM_SEL_CLAVE:    return "CLAVE/RIMSHOT";
    case 0xff:              return "- LAST DRUM -";     // last active drum
    default:                return "* ERROR *";         // error, should not happen
  }
}


void send_sample_sysex( uint8_t banknum, uint8_t drum_sel ) {
  char vname[24];
  int vlen;
  int encoded_size = 0;
  uint8_t *sample;
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)sysex_decode_buf;

  Serial.printf("SysEx send Sample\n");
  
  // -- go get it from SD card

  sample = get_voice( banknum, drum_sel_2_voice( drum_sel ), vname, &vlen );    // will fill in vname and vlen

  if( sample ) {                                                                // valid?
  
    // -- zero working buf
  
    memset( sysex_decode_buf, 0, sizeof(sysex_decode_buf) );
    
    // --- build the unencoded message
  
    hdr->cmd = CMD_SAMPLE_BANK;
    hdr->bank = banknum;                                                        // 0xff for current working pattern RAM
    hdr->drum_sel = drum_sel;
  
    sprintf( hdr->name, vname );
  
    memcpy( &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)], sample, vlen );
  
    // --- encode
  
    /*
        The MIDI library wraps our data with the needed F0 start and F7 end markers.
        We jam our mfr ID at the start, unencoded and always with the hi bit clear.
  
        F0
        mfr ID
        ...encoded data, all hi bits clear...
        F7
    */
      
    sysex_encode_buf[0] = OUR_MIDI_MFR_ID;
    
    encoded_size = pack_sysex_data( vlen + sizeof(sx_sample_bank_hdr_t), sysex_decode_buf, &sysex_encode_buf[1] );
  
    encoded_size += 1;                                // for the unencoded mfr ID in location 0
  
    // --- transmit
  
    send_sysex( encoded_size, sysex_encode_buf );
  }
  else {
    Serial.printf("### Error loading bank %02x\n", banknum);
    beep_failure();
  }
}


void sysex_sample_load( uint8_t *se, int len ) {
  char vname[24];
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)se;

  Serial.printf("Sample len: %d\n", len - SYSEX_HEADER_SIZE );
  Serial.printf("Sample name: %s\n", (char*)&se[1] );                 // name is just past cmd byte
  
  // sanity check the name
  
  if( strlen( (char*)&sysex_decode_buf[1] ) == 0 )                    // any string there?
    snprintf( vname, 24, "NONAME.BIN" );
  else
    snprintf( vname, 24, "%s", (char*)&sysex_decode_buf[1] );

  if( se[0] == CMD_SAMPLE )                                                                               // legacy cmd 0 sample?                            
    set_voice( last_drum, &sysex_decode_buf[SYSEX_HEADER_SIZE], (len - SYSEX_HEADER_SIZE), vname );       // load into last active drum
  else {                                                                                                  // must be cmd 1, with bank and drum selects
    set_voice( drum_sel_2_voice(hdr->drum_sel), 
                  &sysex_decode_buf[SYSEX_HEADER_SIZE], (len - SYSEX_HEADER_SIZE), vname );               // load into selected drum
  }
}


void sysex_sample_request( uint8_t *se, int len ) {
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)se;

  Serial.printf("Sample request: Bank %02d, Drum %02d (%s)\n", hdr->bank, hdr->drum_sel, drum_sel_2_name(hdr->drum_sel));

  send_sample_sysex( hdr->bank, hdr->drum_sel );
}


/* ---------------------------------------------------------------------------------------
    SysEx Parameter Set / Get
*/

void sysex_param( uint8_t *se, int len ) {
  Serial.printf("Param set: \n");
}


void sysex_param_request( uint8_t *se, int len ) {
  Serial.printf("Param request: \n");
}


/* ---------------------------------------------------------------------------------------
    SysEx Utility Commands / Requests
*/

void sysex_util( uint8_t *se, int len ) {
  Serial.printf("Utility: \n");
}


void sysex_util_request( uint8_t *se, int len ) {
  Serial.printf("Utility request: \n");
}



/* ---------------------------------------------------------------------------------------
    SysEx utilities
*/

void send_sysex( int len, uint8_t *b ) {

  Serial.printf("Sending %d bytes of SysEx...", len );
  
  if( get_midi_sysex_route() & ROUTE_USB ) {
    Serial.printf("via USB...");
    usbMIDI.sendSysEx( len, b );
  }
  
  if( get_midi_sysex_route() & ROUTE_DIN5 ) {
    Serial.printf("via DIN-5...");
    midiDIN.sendSysEx( len, b );
  }
  
  Serial.printf("done!\n");
}


/* ---------------------------------------------------------------------------------------
    SysEx DECODER
*/

void init_sysex_decoder() {
  process_sysex_state = SYSEX_FIND_F0;                // set up to hunt for F0 (start of sysex)
}

/*
    We are receiving a SysEx message containing sample data.
    Get ready to write it to a voice board.
*/

void sysex_load_prologue() {
  
  teensy_drives_z80_bus( true );                      // grab the bus

  set_rst_hihat( 1 );                                 // XXX FIXME should not need to do this, getting stomped

  z80_bus_write( PATT_DISPLAY, voice_num_map[(last_drum & 0xf) - 4]);     // PATT didsplay shows voice we are loading into
}


void sysex_load_epilogue() {

  set_rst_hihat( 0 );                                 // XXX FIXME should not need to do this, getting stomped
  
  delay(100);
  teensy_drives_z80_bus( false );                     // release the bus
  delay(100);

  process_sysex_state = SYSEX_INITIALIZE;             // done, either success or failure, but get ready for the next one
  sysex_err_abort = false;
}



bool check_store_byte( uint8_t b ) {
  if( b == 0xf7 ) {
    return true;
  }
  else {
    b7s <<= 1;
    sysex_decode_buf[sysex_decode_idx++] = (b | (b7s & 0x80));

    if( sysex_decode_idx >= DECODE_BUF_SIZE ) {         // don't blow past end of buf
      Serial.print("### sysex_buf_size limit, forcing stop "); Serial.println( sysex_decode_idx );
      
      sysex_err_abort = true;                         // checked below, will bail on sysex operation
      
      return true;
    }
  }

  return false;
}


uint8_t hex2bcd[64] = { 0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
                        0xf8,0xf9,0x10,0x11,0x12,0x13,0x14,0x15,
                        0x16,0x17,0x18,0x19,0x20,0x21,0x22,0x23,
                        0x24,0x25,0x26,0x27,0x28,0x29,0x30,0x31,
                        0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
                        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
                        0x48,0x49,0x50,0x51,0x52,0x53,0x54,0x55,
                        0x56,0x57,0x58,0x59,0x60,0x61,0x62,0x63 };

bool process_sysex_byte( uint8_t b ) {
  bool r = false;
    
  switch( process_sysex_state ) {
    case SYSEX_INITIALIZE:  sysex_decode_idx = 0;                                  // ensure buffer index is reset
                            sysex_err_abort = false;                            // not in error state
                                                                                // fall thru to the F0 hunter
    case SYSEX_FIND_F0:     if( b == 0xf0 )
                              process_sysex_state = SYSEX_FIND_MFR_ID;    break;
    
    case SYSEX_FIND_MFR_ID: if( b == OUR_MIDI_MFR_ID ) {
                              process_sysex_state = SYSEX_GET_B7S;
                              sysex_load_prologue();                            // it's for us, get ready
                            }    
                            else
                              process_sysex_state = SYSEX_FIND_F0;        break;
    
    case SYSEX_GET_B7S:     b7s = b;                    
                            process_sysex_state = SYSEX_GET_BYTE_1;       break;

    case SYSEX_GET_BYTE_1:
    case SYSEX_GET_BYTE_2:
    case SYSEX_GET_BYTE_3:
    case SYSEX_GET_BYTE_4:
    case SYSEX_GET_BYTE_5:
    case SYSEX_GET_BYTE_6:  // OLED STATUS MSG HERE
                              
                            r = check_store_byte(b); 
                            if( !r )
                              process_sysex_state++;                      // time for next byte in this block                  
                            else                                          // we're done, get ready for next time
                              init_sysex_decoder();                       break;
    
    case SYSEX_GET_BYTE_7:  r = check_store_byte(b); 
                            if( !r )  
                              process_sysex_state = SYSEX_GET_B7S;        // time for next block                  
                            else                                          // we're done, get ready for next time
                              init_sysex_decoder();                       break;

    default:                init_sysex_decoder();                         break;
  }

  z80_bus_write( LINK_DISPLAY, hex2bcd[(sysex_decode_idx/1024)] );        // LINK display shows number of Kbytes received
    
  return r;
}             



#define CMD_SAMPLE          0x00
#define CMD_SAMPLE_BANK     0x01
#define CMD_RAM_BANK        0x02

#define CMD_PARAM           0x04
#define CMD_UTIL            0x05

#define CMD_REQUEST         0x08        // OR into CMD to turn it into a REQUEST


// mySystemExclusiveChunk() called for variable-sized sysex chunks as received. We use a state machine to decode a stream of these
//  variable-sized sysex chunks. "last" will be set to true for the final chunk.
//
//  ---> EXCEPT, "last" does not seem to be valid for DIN-5 sysex transfers, only USB.

void mySystemExclusiveChunk(const byte *d, uint16_t len, bool last) {

  Serial.printf("sysex: %d, last: %d, err: %d\n", len, last, sysex_err_abort );

  for( int xxx = 0; xxx != len; xxx++ ) {                                                         // walk thru all bytes received in this block
    if( !sysex_err_abort                                                                          // if no errors detected
        && process_sysex_byte( *d++ )                                                             // and we processed thru the last byte
        && last ) {                                                                               // and teensy agrees that this is the last block
          
      sysex_decode_idx++;                                                                         // add 1 to make index = len
    
      Serial.printf("### found end after %d bytes\n", sysex_decode_idx );

      switch( sysex_decode_buf[0] ) {

        // --- sysex DOWNLOAD types
        
        case CMD_SAMPLE:
        case CMD_SAMPLE_BANK:                   sysex_sample_load( sysex_decode_buf, sysex_decode_idx );          break;

        case CMD_RAM_BANK:                      sysex_ram_load( sysex_decode_buf, sysex_decode_idx );             break;

        case CMD_PARAM:                         sysex_param( sysex_decode_buf, sysex_decode_idx );                break;

        case CMD_UTIL:                          sysex_util( sysex_decode_buf, sysex_decode_idx );                 break;

        // --- sysex UPLOAD REQUEST types

        case (CMD_SAMPLE + CMD_REQUEST):        
        case (CMD_SAMPLE_BANK + CMD_REQUEST):   sysex_sample_request( sysex_decode_buf, sysex_decode_idx );       break;

        case (CMD_RAM_BANK + CMD_REQUEST):      sysex_ram_request( sysex_decode_buf, sysex_decode_idx );          break;

        case (CMD_PARAM + CMD_REQUEST):         sysex_param_request( sysex_decode_buf, sysex_decode_idx );        break;

        case (CMD_UTIL + CMD_REQUEST):          sysex_util_request( sysex_decode_buf, sysex_decode_idx );         break;
      }

      sysex_load_epilogue();
    }
  }

  if( sysex_err_abort && last ) {                                                                   // sysex_err_abort will have stopped the processing
    Serial.printf("### ERROR receiving sysex\n");                                                   // notify the user, clean up, and give up :-)
    beep_failure();
    sysex_load_epilogue();
  }
}


/* ---------------------------------------------------------------------------------------
    SysEx ENCODER (for uploads)
*/

int pack_sysex_data( int len, uint8_t *in, uint8_t *out ) {
  int in_idx = 0;
  int out_idx = 0;
  uint8_t b7s;
  uint8_t w;
  int yyy;
  
  do {
      b7s = 0;
      
      for( yyy = 1; yyy != 8; yyy++ ) {             // process 7 bytes of input at a time,
                                                    //  generate 8 bytes of output
        b7s <<= 1;                                  // 0 in low bit
        
        w = in[in_idx++];
        
        // printf("i %d %02x\n", yyy, w );
        
        if( w & 0x80 ) b7s |= 1;                    // if 1 in b7, set corresponding bit in b7s
        
        w &= 0x7f;
        
        // printf("o %d    %02x\n", yyy, w );
        
        out[out_idx+yyy] = w;
        
        if( in_idx >= len )
                break;
      }
      
      // printf("-         %02x\n", b7s );
      
      out[out_idx] = b7s;
      
      out_idx += yyy;

  } while( in_idx < len );
  
  Serial.printf("\n== %d %d / %d\n\n", len, in_idx, out_idx);
  
  return out_idx;
}