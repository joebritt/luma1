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
#include "LM_DrumTriggers.h"

#include <MIDI.h>

#include <USBHost_t36.h>          // access to USB MIDI devices (plugged into 2nd USB port)

// Create the ports for USB devices plugged into Teensy's 2nd USB port (via hubs)

USBHost usbHOST;
USBHub hub1(usbHOST);
USBHub hub2(usbHOST);
USBHub hub3(usbHOST);
USBHub hub4(usbHOST);

// We assume max 3 MIDI client devices connected to the host. This is needed for controllers that present several MIDI ports

MIDIDevice midiHOST01(usbHOST);
MIDIDevice midiHOST02(usbHOST);
MIDIDevice midiHOST03(usbHOST);


MIDI_CREATE_INSTANCE(HardwareSerial, HW_MIDI, midiDIN);

// HW_MIDI is on Serial1, which is i.MX RT1062 LPUART UART6. This is for a hack to invert the TX line (which Luma's hardware needs)

#define IMXRT_LPUART6_ADDRESS     0x40198000
#define IMXRT_LPUART_CTRL         0x18
#define TXINV_FORCE               0x10000000        // TXINV bit to set


int midi_chan = 1;


// --- MIDI interface routing

uint8_t midi_note_out_route;      // can be DIN5, USB, or Both
uint8_t midi_note_in_route;       // can be DIN4, USB, or Both

uint8_t midi_clock_out_route;     // can be DIN5, USB, or Both
uint8_t midi_clock_in_route;      // can be DIN5 or USB only

uint8_t midi_sysex_route;         // can be DIN5 or USB only


// --- SOFT THRU

bool midi_soft_thru;              // initialized from EEPROM


// --- SEND VELOCITY

bool midi_send_velocity;          // initialized from EEPROM


// --- HONOR MIDI START/STOP

bool honor_start_stop;            // initialized from EEPROM


// --- SYSEX CHUNK DELAY

uint8_t sysex_chunk_delay;        // initialized from EEPROM, delay between chunks, in ms



// --- Updated by key, sequence, or midi drum trigger, selects which sound to load

uint16_t last_drum = STB_BASS;


// --- Drum Table data structure to manage sending Note Offs after triggers

bool map_midi_2_strobe( byte note, byte vel, uint16_t *strobe, uint8_t *flags );

// we send a NOF this many milliseconds after the corresponding NON

#define NOF_TIME_MS         40

typedef struct {
  byte midi_note;
  bool drum_playing;
  bool drum_soft;         // this was a soft one, need this if midi_send_velocity == false
  byte flags;
  uint16_t strobe;
  elapsedMillis drum_time_elapsed; 
} drum_midi_map_t;

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
  drums[entry].drum_soft = false;
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

void din_midi_begin( int chan );

void din_midi_begin( int chan ) {
  if( midi_chan != 0 ) {                            // only seem to need to do this for the DIN interface
    Serial.printf("MIDI Channel = %d, starting DIN-5 interface for non-omni mode\n", midi_chan);
    midiDIN.begin();
  }
  else {
    Serial.printf("MIDI Channel = %d, starting DIN-5 interface for OMNI mode\n", midi_chan);
    midiDIN.begin( MIDI_CHANNEL_OMNI );
  }
}


void init_midi() {

  // -- restore settings from eeprom
  
  midi_chan = eeprom_load_midi_channel();
  
  set_midi_note_out_route( eeprom_load_midi_note_out_route() );
  set_midi_note_in_route( eeprom_load_midi_note_in_route() );

  set_midi_clock_out_route( eeprom_load_midi_clock_out_route() );
  set_midi_clock_in_route( eeprom_load_midi_clock_in_route() );

  set_midi_sysex_route( eeprom_load_midi_sysex_route() );
  set_midi_sysex_delay( eeprom_load_midi_sysex_delay() );

  midi_soft_thru = eeprom_load_midi_soft_thru();

  set_midi_soft_thru( midi_soft_thru );

  midi_send_velocity = eeprom_load_midi_send_velocity();        // are we sending velocity or separate keys for soft sounds? (can always receive velocity)

  honor_start_stop = eeprom_load_midi_start_honor();            // are we trying to "push" the start button upon MIDI Start?


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
  midiDIN.setHandleContinue(        din_myContinue        );
  midiDIN.setHandleStop(            din_myStop            );

  midiDIN.setHandleSystemExclusive( din_mySystemExclusiveChunk  );

  din_midi_begin( midi_chan );

  // -- Total hack: manually whack the TXINV bit in the LPUART6 block. 
  //    We need inverted TX for Luma-1's hardware, but MIDI library doesn't expose serial port format settings
  
  *(volatile uint32_t *)(IMXRT_LPUART6_ADDRESS + IMXRT_LPUART_CTRL) |= TXINV_FORCE;
  

  // -- modern USB hotness MIDI

  usbMIDI.setHandleNoteOn(          usb_myNoteOn          );
  usbMIDI.setHandleNoteOff(         usb_myNoteOff         );

  usbMIDI.setHandleProgramChange(   usb_myProgramChange   );
  
  usbMIDI.setHandleClock(           usb_myClock           );
  usbMIDI.setHandleStart(           usb_myStart           );
  usbMIDI.setHandleContinue(        usb_myContinue        );
  usbMIDI.setHandleStop(            usb_myStop            );
  
  usbMIDI.setHandleSystemExclusive( usb_mySystemExclusiveChunk  );

  //USB Host init

  usbHOST.begin();
  midiHOST01.setHandleNoteOn(          midiHOST01_myNoteOn           );
  midiHOST01.setHandleNoteOff(         midiHOST01_myNoteOff          );
  midiHOST02.setHandleNoteOn(          midiHOST01_myNoteOn           );
  midiHOST02.setHandleNoteOff(         midiHOST01_myNoteOff          );
  midiHOST03.setHandleNoteOn(          midiHOST01_myNoteOn           );
  midiHOST03.setHandleNoteOff(         midiHOST01_myNoteOff          );
}


/* ---------------------------------------------------------------------------------------
    MIDI Channel (00 = OMNI)
*/

void set_midi_channel( int chan ) {
  midi_chan = chan;
  eeprom_save_midi_channel( midi_chan );
  din_midi_begin( midi_chan );
}

int get_midi_channel() { return midi_chan; }




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


// --- Soft Thru

void set_midi_soft_thru( bool on ) {
  midi_soft_thru = on;   

  Serial.printf("Turning MIDI Soft Thru %s\n", midi_soft_thru?"ON":"off");
  
  if( midi_soft_thru ) {
    midiDIN.turnThruOn();
    //usbMIDI.turnThruOn();                 // doesn't exist for USB
  } else {
    midiDIN.turnThruOff();
    //usbMIDI.turnThruOff();                // doesn't exist for USB
  }

}

bool get_midi_soft_thru()                   {     return midi_soft_thru;        }


// --- Send Velocity

void set_midi_send_vel( bool on ) {
  Serial.printf("Setting MIDI Send Velocity %s\n", midi_send_velocity?"ON":"off");
  midi_send_velocity = on;   
}

bool get_midi_send_vel()                   {     return midi_send_velocity;        }


// --- Sysex Delay

void set_midi_sysex_delay( uint8_t del ) {
  Serial.printf("Setting MIDI Sysex Delay to %d\n", del );
  sysex_chunk_delay = del;  
}

uint8_t get_midi_sysex_delay()             {     return sysex_chunk_delay;         }


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

// ======================================================================
// usbHOST note ON/OFF handlers

void midiHOST01_myNoteOn(byte channel, byte note, byte velocity) {
    myNoteOn( channel, note, velocity );
  //Serial.printf("USB Host data NOTE ON");  
    usbMIDI.sendNoteOn( note, velocity, (midi_chan == 0)?1:midi_chan );
    midiDIN.sendNoteOn( note, velocity, (midi_chan == 0)?1:midi_chan );
    midi_din_out_event();
    midi_usb_in_event();
}

void midiHOST01_myNoteOff(byte channel, byte note, byte velocity) {
    myNoteOff( channel, note, velocity );
  //Serial.printf("USB Host data NOTE OFF");  
    usbMIDI.sendNoteOff( note, MIDI_VEL_LOUD, (midi_chan == 0)?1:midi_chan );
    midiDIN.sendNoteOff( note, MIDI_VEL_LOUD, (midi_chan == 0)?1:midi_chan );
    midi_din_out_event();
    midi_usb_in_event();
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

void din_myContinue() {
  if( get_midi_clock_in_route() & ROUTE_DIN5 ) {
    myContinue();
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

void din_mySystemExclusiveChunk(unsigned char *d, unsigned int len) {
  bool last = false;

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

void usb_myContinue() {
  if( get_midi_clock_in_route() & ROUTE_USB ) {
    myContinue();
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

elapsedMicros midi_in_yield_time;

#define MIDI_IN_YIELD_MICROS    10

#define MIDI_IN_LOOPS_PLAYING   10        // when z-80 sequencer is running
#define MIDI_IN_LOOPS_IDLE      1         // when z-80 sequencer is stopped

void handle_midi_in() {
  int run_loops = MIDI_IN_LOOPS_IDLE;
  int yield_loops;

  if( luma_is_playing() )
    run_loops = MIDI_IN_LOOPS_PLAYING;    // do this for LUI responsiveness when not running z80 sequencer

  for( int loops = 0; loops < run_loops; loops++ ) 
  {    
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

    //Check USB HOST activity
    midiHOST01.read();
    midiHOST02.read();
    midiHOST03.read();

    // -- give some breathing time for messages to come in, while yield()'ing

    midi_in_yield_time = 0;
    yield_loops = 0;

    while( midi_in_yield_time < MIDI_IN_YIELD_MICROS ) {
      yield();

      yield_loops++;
      if( yield_loops > 3 ) {
        
        if( midi_chan == 0 )                // 0 = OMNI
          midiDIN.read();
        else
          midiDIN.read( midi_chan );

        if( midi_chan == 0 )                // 0 = OMNI
          usbMIDI.read();
        else
          usbMIDI.read( midi_chan );
        
        //Check USB HOST activity
        midiHOST01.read();
        midiHOST02.read();
        midiHOST03.read();
      }
    }
  }
}


void myNoteOn(byte channel, byte note, byte velocity) {
  
  //Serial.printf(" %d %d %d\n", channel, note, velocity);

  if( note >= MIDI_NOTE_BASS )
    play_midi_drm( note, velocity );                                    // if it's in our range, play with the provided velocity
  else {
    //Serial.printf(" low note, will play %d\n", note+MIDI_NOTE_SOFT_TRIG_OFFSET);

    play_midi_drm( note+MIDI_NOTE_SOFT_TRIG_OFFSET, MIDI_VEL_SOFT );    // if it's in the soft trig range, play with soft velocity
  }
  
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
  byte note;

  for( int xxx = 0; xxx != 13; xxx++ ) {                                  // check all 13 drums
    
    if( drums[xxx].drum_playing == true ) {                               // is this one playing?

      if ( drums[xxx].drum_time_elapsed > NOF_TIME_MS ) {                 // is it time to send the NOF?
        note = drums[xxx].midi_note;

        if( midi_send_velocity == false ) {                               // IF we are not sending velocity
          if( drums[xxx].drum_soft == true ) {                            // AND this was a soft one
            note -= MIDI_NOTE_SOFT_TRIG_OFFSET;                           // THEN adjust note value
          }
        }

        if( get_midi_note_out_route() & ROUTE_DIN5 ) {
          midiDIN.sendNoteOff( note, MIDI_VEL_LOUD, (midi_chan == 0)?1:midi_chan );
          midi_din_out_event();
        }

        if( get_midi_note_out_route() & ROUTE_USB ) {
          usbMIDI.sendNoteOff( note, MIDI_VEL_LOUD, (midi_chan == 0)?1:midi_chan );
          midi_usb_out_event();
        }

        drums[xxx].drum_playing = false;
      }

    }
  }
}


char *drum_name( int idx, byte vel ) {
  switch( idx ) {
                  case drum_BASS:       if(vel>MIDI_VEL_SOFT) return (char*)"BASS";    else return (char*)"bass";     break;
                  case drum_SNARE:      if(vel>MIDI_VEL_SOFT) return (char*)"SNARE";   else return (char*)"snare";    break;
                  case drum_HIHAT:      if(vel>MIDI_VEL_SOFT) return (char*)"HIHAT";   else return (char*)"hihat";    break;
                  case drum_HIHAT_OPEN: return (char*)"HIHAT OPEN";                                                   break;
                  case drum_CLAPS:      return (char*)"CLAPS";                                                        break;
                  case drum_CABASA:     if(vel>MIDI_VEL_SOFT) return (char*)"CABASA";  else return (char*)"cabasa";   break;
                  case drum_TAMB:       if(vel>MIDI_VEL_SOFT) return (char*)"TAMB";    else return (char*)"tamb";     break;
                  case drum_TOM_UP:     return (char*)"TOM ^";                                                        break;    
                  case drum_TOM_DN:     return (char*)"TOM v";                                                        break;
                  case drum_CONGA_UP:   return (char*)"CONGA ^";                                                      break;
                  case drum_CONGA_DN:   return (char*)"CONGA v";                                                      break;
                  case drum_COWBELL:    return (char*)"COWBELL";                                                      break;
                  case drum_CLAVE:      return (char*)"RIMSHOT";                                                      break;
  
                  default:              return (char*)"UNKNOWN";                                                      break;
  }
}


void send_midi_drm( int drum_idx, byte vel ) {                            // if we are in OMNI mode, send on channel 1
  byte note;

  note = drums[drum_idx].midi_note;

  drums[drum_idx].drum_soft = (vel < MIDI_VEL_LOUD);          // remember this so we can NOF the right way when midi_send_velocity == false

  Serial.printf("%s: %d %d\n", drum_name(drum_idx,vel), note, vel );

  if( midi_send_velocity == false ) {                         // if FALSE, send soft notes on secondary note mapping. can keep low velocity.
    switch( note ) {
      case MIDI_NOTE_BASS:
      case MIDI_NOTE_SNARE:
      case MIDI_NOTE_HIHAT:
      case MIDI_NOTE_CABASA:
      case MIDI_NOTE_TAMB:
                            if( vel < MIDI_VEL_LOUD ) {
                              Serial.printf("no vel, mapped MIDI note %d to %d\n", note, note-MIDI_NOTE_SOFT_TRIG_OFFSET);
                              note -= MIDI_NOTE_SOFT_TRIG_OFFSET;
                            }
                            break;
    }
  }

  if( get_midi_note_out_route() & ROUTE_DIN5 ) {
    midiDIN.sendNoteOn( note, vel, (midi_chan == 0)?1:midi_chan );
    midi_din_out_event();
  }

  if( get_midi_note_out_route() & ROUTE_USB ) {
    usbMIDI.sendNoteOn( note, vel, (midi_chan == 0)?1:midi_chan );
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
  drum_trig_event *dte;

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
  
  while( (dte = pop_trig_event()) != NULL ) {                    // process all that have arrived
    
    if( dte->trigs_a ) {
      if( dte->trigs_a & 0x01 )  send_midi_drm( drum_CABASA,     (dte->trig_mods & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
        
      if( dte->trigs_a & 0x02 )  send_midi_drm( drum_TAMB,       (dte->trig_mods & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );

      if( dte->trigs_a & 0x04 ) {
        if( dte->trig_mods & 0x04 ) 
          send_midi_drm( ( dte->trig_mods & 0x02 ) ? drum_CONGA_UP : drum_CONGA_DN,           MIDI_VEL_LOUD );
        else
          send_midi_drm( ( dte->trig_mods & 0x02 ) ? drum_TOM_UP : drum_TOM_DN,               MIDI_VEL_LOUD );
        }
      
      if( dte->trigs_a & 0x10 )  send_midi_drm( drum_COWBELL,                                 MIDI_VEL_LOUD );
      
      if( dte->trigs_a & 0x20 )  send_midi_drm( drum_CLAVE,                                   MIDI_VEL_LOUD );
    }
  
    
    if( dte->trigs_b ) {
      if( dte->trigs_b & 0x80 )     send_midi_drm( drum_CLAPS,                                MIDI_VEL_LOUD );

      // HIHAT / hihat / HIHAT SPLASH (OPEN)
      if( dte->trigs_b & 0x10 ) {      
        if( dte->trig_mods & 0x04 ) send_midi_drm( drum_HIHAT_OPEN,                           MIDI_VEL_LOUD );
        else                        send_midi_drm( drum_HIHAT,      (dte->trig_mods & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
      }
        
      if( dte->trigs_b & 0x40 )     send_midi_drm( drum_BASS,       (dte->trig_mods & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
        
      if( dte->trigs_b & 0x20 )     send_midi_drm( drum_SNARE,      (dte->trig_mods & 0x02) ? MIDI_VEL_LOUD : MIDI_VEL_SOFT );
    }  
  }  
}


void didProgramChange( byte pgm ) {
  Serial.printf("Sending MIDI Program Change: %02d\n", pgm);

  midiDIN.sendProgramChange( pgm, (midi_chan == 0)?1:midi_chan );
  usbMIDI.sendProgramChange( pgm, (midi_chan == 0)?1:midi_chan );
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

    We do this becuase the DECODED_TAPE_SYNC_CLK signal is used for 2 things:

      - General Purpose Output to drive /CE on EPROM socket for EPROM dumping
      - Clock to LM-1 firmware to simulate Tape Sync Clock
*/

void set_tape_sync_clk_gpo( bool state ) {
  digitalWriteFast( DECODED_TAPE_SYNC_CLK, state );
}

bool get_tape_sync_clk_gpo() {
  return digitalReadFast( DECODED_TAPE_SYNC_CLK );
}


/* ---------------------------------------------------------------------------------------
    MIDI Clock time calculation
*/

elapsedMicros sinceLastMIDIClk;         // use this to keep track of time since previous MIDI clk

elapsedMicros sinceMIDIStart;           // used to detect if quick or slower MIDI Clock after MIDI Start

IntervalTimer tempoTimer;               // interrupt-driven timer to generate tempo clock pulses

bool tempo_clock_state;

int tempo_clock_count;

bool started = false;

bool first_tempo_clock = false;

uint32_t last_time;


void tempo_clock_output() {                               // INTERRUPT CONTEXT, be careful, don't do too much
  if( tempo_clock_count < 3 ) {                           // EDGES 2, 3, 4

    tempo_clock_state = !tempo_clock_state;               // just flip it

    if( started )                                         // normally hold tempo clock high until we get our first MIDI clock
      set_tape_sync_clk_gpo( tempo_clock_state );         // we are running, let it wiggle
    else
      set_tape_sync_clk_gpo( false );                     // inverted, this holds it high

    tempo_clock_count++;
                                                          // total hacks, Ableton sometimes shortens the MIDI clock period -- a lot!
                                                          // these hacks squeeze things to add more room for error in the 24ppqn clocks

    if( tempo_clock_count == 2 )
      tempoTimer.begin( tempo_clock_output, last_time*0.7 );    // shorten the second pulse 30%
  }
  else
    tempoTimer.end();
}


void init_tempo_clock_timer( uint32_t t, bool first_clock ) {
  tempoTimer.end();

  /*
      SOME NOTES

      LM-1 internal tempo clock runs from 35bpm to 165bpm.

      We have tested this with tempos (driven from Ableton) of 30bpm to 150bpm, above 150bpm you lose enough time for the z80 to process.

  */

  if( t > 50000 ) {                                       // if it's so big it looks like we haven't been getting clocks...
    t = 3500;                                             // ...fake it with something fast enough to fit, but not too fast
  }

  //Serial.printf("init: --> %d\n", t);

  sinceLastMIDIClk = 0;                                   // time to start a new measurement

  if( first_clock && (sinceMIDIStart > (t+1000)) ) {      // first one, AND more than 1/4 period between MIDI Clocks passed (with 1ms fudge)?
   tempo_clock_state = false;                             // inverted, drive tempo clock HIGH for first MIDI clock
   tempo_clock_count = 1;                                 // only 3 edges for the first clock
  }
  else {
    tempo_clock_state = true;                             // inverted, drive tempo clock LOW for subsequent MIDI clocks
    tempo_clock_count = 0;
  }

  if( first_clock ) {
   first_tempo_clock = false;                             // first one is special
  }

  if( started )
    set_tape_sync_clk_gpo( tempo_clock_state );           // start in known state at each new MIDI clock, EDGE 1 of 4 (or 3, for first one)

  last_time = t;

  tempoTimer.begin( tempo_clock_output, t );              // we will do 4 (3 for first one) tempo clock flips between each MIDI clock message
  tempoTimer.priority( 0 );                               // highest priority
}


// -- Called when we get a MIDI Clock

/*
  NOTE! MIDI Start means start the sequencer at the NEXT MIDI CLOCK.
        Since we double the MIDI Clock to 48ppqn, we need to make sure we only start it (push the footpedal)
         on a MIDI Clock boundary (not an interpolated 48ppqn clock pulse).


  Hold tempo clock HIGH
  Get MIDI Start
    Drop tempo clock
  Get MIDI Clock
    Raise tempo clock
    Drive tempo clock with interpolated MIDI clock timing
*/

void myClock() {  
  init_tempo_clock_timer( sinceLastMIDIClk/4, first_tempo_clock );
}



bool honorMIDIStartStopState() {
  return honor_start_stop;
}

bool honorMIDIStartStop( bool honor ) {
  bool prev = honor_start_stop;

  honor_start_stop = honor;

  Serial.printf("%s honor MIDI Start/Stop messages\n", honor?"Will":"Will NOT");

  return prev;
}



// -- Called when we get a MIDI Start

/*
    When we get a MIDI Start, the NEXT MIDI CLOCK marks the initial downbeat.

    BUT! We are generating 2 LM-1 clocks for each MIDI Clock.

    We need to wait for the next MIDI Clock to actually start the LM-1 sequencer.

    MIDI Clock  1000 1000 1000
    LM-1 Clock  1010 1010 1010
*/

void myStart() {
  Serial.println("received MIDI Start");

  if( !started ) {                                          // if we hadn't started yet
    if( honorMIDIStartStopState() ) {
      Serial.printf("Honoring MIDI Start, pressing foot pedal\n");
      z80_seq_ctl( Z80_SEQ_START );                           // simulate foot pedal press
    }

    tempoTimer.end();                                       // make sure this isn't running

    tempo_clock_state = true;                               // inverted, drive tempo clock LOW when we get MIDI Start
    set_tape_sync_clk_gpo( tempo_clock_state );

    started = true;                                         // now we've started
    sinceMIDIStart = 0;                                     // used to figure out how much time has passed by first MIDI Clock

    first_tempo_clock = true;                               // first tempo clock is funny, just 3 edges
  }
  else {
    Serial.println("DUPLICATE MIDI Start, ignoring");
  }
}


// -- Called when we get a MIDI Continue

void myContinue() {
/*
  Serial.println("received MIDI Continue");

  if( honor_start_stop ) {

    got_midi_start = true;                              // we don't really honor Continue, we treat it like Start

    z80_seq_ctl( Z80_SEQ_START );                       // simulate foot pedal press
  }
  else {
    Serial.printf("MIDI Start (Continue) honor disabled\n");
  }
*/
}


// -- Called when we get a MIDI Stop

void myStop() {
  Serial.println("received MIDI Stop");

  if( started ) {
    first_tempo_clock = false;                          // shouldn't need to do this, but it doesn't hurt (in case we got no MIDI clocks)

    if( honorMIDIStartStopState() ) {
      Serial.printf("Honoring MIDI Stop, pressing foot pedal\n");
      z80_seq_ctl( Z80_SEQ_STOP );                      // simulate foot pedal press

      init_tempo_clock_timer( 18000/4, false );
      delay( 100 );
      tempoTimer.end();                                       // make sure this isn't running
    }
    
    started = false;                                    // now we've stopped
  }
  else {
    Serial.println("DUPLICATE MIDI Stop, ignoring");
  }
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


bool reboot_after_sysex = false;                                        // set to TRUE to reboot z-80 after current sysex operation

void sysex_store_prologue();                                            // take / release / reboot z-80 for certain sysex messages
void sysex_store_epilogue();


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
    00 / 08   *** LEGACY, DEPRECATED *** SAMPLE Down / Upload (to/from active bank), 24-char sample name, 7-byte pad (to 32 bytes), followed by uLaw sample data
    01 / 09   BANK SAMPLE Down / Upload (to/from SD bank/drum), 1 byte bank#, 1 byte drum sel, 24 byte name, pad 5, uLaw sample data
    02 / 0a   BANK RAM Down / Upload (to/from SD bank), 1 byte bank#, 24 byte name, 6 byte pad, followed by 8KB RAM data
    03 / 0b   
    04 / 0c   8-bit PARAMETER (see param table below), 8-bit param, 8-bit val
    05 / 0d   NAME UTILITY, used to set/get Voice and RAM bank names, see description below
    06        ERROR REPLY
    07 / 0f   ESCAPE, next byte is an extended cmd, currently unused
 
    In an error situation, Luma-1 will reply with a cmd = 06 message with one of the following strings in the name field:

    SD ERROR        SD Card error
    NOT FOUND       no file in that bank                                       
*/

#define CMD_SAMPLE          0x00        // LEGACY, avoid using, doesn't allow specifying specific drum
#define CMD_SAMPLE_BANK     0x01
#define CMD_RAM_BANK        0x02

#define CMD_PARAM           0x04
#define CMD_NAME_UTIL       0x05

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
  uint8_t bank;                         // 00 - 99, 0xff for current bank (STAGING)
  uint8_t drum_sel;                     // drum select, 0xff for last triggered drum
  uint16_t sample_len;                  // sample length in bytes
  uint8_t pad[3];                       // unused, pad
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

// === PARAMETER UTILITIES

//  cmd = 0x04 / 0x0c is set / get parameter
//  Used to get/set the same parameters you can get/set with the MENU key, plus some extras like "simulate key press"

#define SX_PARAM_FAN              0x00      // Fan Control: Auto / On / Off

#define SX_PARAM_MIDI_CHAN        0x01      // MIDI Channel: 0 (Omni), 1-16

#define SX_PARAM_MIDI_NOTE_OUT    0x02      // MIDI Notes Out: DIN-5, USB, DIN-5 & USB
#define SX_PARAM_MIDI_NOTE_IN     0x03      // MIDI Notes In: DIN-5, USB, DIN-5 & USB
#define SX_PARAM_MIDI_CLOCK_OUT   0x04      // MIDI Clock Out: DIN-5, USB, DIN-5 & USB
#define SX_PARAM_MIDI_CLOCK_IN    0x05      // MIDI Clock In: DIN-5, USB, DIN-5 & USB
#define SX_PARAM_MIDI_SYSEX       0x06      // MIDI SysEx: DIN-5, USB, DIN-5 & USB

#define SX_PARAM_MIDI_SOFT_THRU   0x07      // MIDI Soft Thru: On / Off
#define SX_PARAM_MIDI_START_EN    0x08      // MIDI Start Enable: Enabled / Disabled
#define SX_PARAM_MIDI_SEND_VEL    0x09      // MIDI Send Velocity: Enabled / Disabled

#define SX_PARAM_REBOOT           0xf0      // Reboot: Just Reboot / Reset to Factory Default Settings    WRITE-ONLY
#define SX_PARAM_KEYPRESS         0xfe      // jam in a key                                               WRITE-ONLY

typedef struct {
  uint8_t cmd;                          // 0x04 or 0x0c
  uint8_t param;                        
  uint8_t val;                          // data or empty for fetch
  uint8_t pad[29];                      // unused, pad
} __attribute__((packed)) sx_parm_hdr_t;
                                        // nothing after, all data in struct


// === NAME UTILITIES

//  cmd = 0x05 / 0x0d is name write / read

//  cmd = 0x05 / 0x0d, name type = 0x00, Voice Bank Name write / read
//  cmd = 0x05 / 0x0d, name type = 0x01, RAM Pattern Bank Name write / read
//  cmd = 0x05 / 0x0d, name type = 0x02, Teensy code version read-only

// Name Types:
#define SX_VOICE_BANK_NAME    0x00
#define SX_RAM_BANK_NAME      0x01
#define SX_TEENSY_VERSION     0x02      // this one can only be read
#define SX_SERIAL_NUMBER      0x03      // 8 char ASCII string, null terminated

typedef struct {
  uint8_t cmd;                          // 0x05 or 0x0d
  char name[24];                        // bank name, C string, 24 chars max
  uint8_t bank;                         // bank number, 00 - 99, 0xff = currently active
  uint8_t name_type;                    // name type
  uint8_t pad[5];                       // unused, pad
} __attribute__((packed)) sx_name_util_hdr_t;
                                        // nothing after, all data in struct



/* ---------------------------------------------------------------------------------------
    SysEx Pattern RAM transfers

    Use bank # 0xff for current RAM
*/

void send_pattern_RAM_sysex( uint8_t banknum ) {
  int encoded_size = 0;
  uint8_t *ram;
  sx_ram_bank_hdr_t *hdr = (sx_ram_bank_hdr_t*)sysex_decode_buf;

  Serial.printf("SysEx send Pattern RAM\n");
  
  // -- go get it, either from SD bank # or STAGING

  ram = get_ram_bank( banknum );                                    // if banknum == 255, return active z-80 RAM

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




void sysex_ram_store( uint8_t *se, int len ) {
  char vname[24];
  sx_ram_bank_hdr_t *hdr = (sx_ram_bank_hdr_t*)se;

  Serial.printf("\n-- Sysex Pattern RAM Store\n");
  
  // --- sanity check the name
  
  if( strlen( hdr->name ) == 0 )                                                                        // no name?
    snprintf( vname, 24, "RAM_BANK_%04X.BIN", checksum((uint8_t*)&se[SYSEX_HEADER_SIZE],8192) );        // default name w/checksum
  else
    snprintf( vname, 24, "%s", hdr->name );

  Serial.printf("   Pattern RAM len: %d\n", len - SYSEX_HEADER_SIZE );
  Serial.printf("   Pattern RAM name: %s\n", hdr->name );
  if( hdr->bank == 255 )
    Serial.printf("   Pattern RAM bank: ACTIVE Z-80 RAM\n" );
  else
    Serial.printf("   Pattern RAM bank: %02d\n", hdr->bank );

  // --- if bank 00-99, write to SD card. if bank == 255, write to active ram, save filename, and reboot

  if( hdr->bank == 255 ) {
    sysex_store_prologue();                                     // take the bus, pause hihat

    // Z-80 is paused

    load_z80_ram( (uint8_t*)&se[SYSEX_HEADER_SIZE] );           // copy the payload into the z-80 RAM
                                                                
    set_active_ram_bank_name( vname );                          // save the filename

    reboot_after_sysex = true;                                  // this will reboot the z-80 on the way out

    sysex_store_epilogue();                                     // release the bus, reboot z-80
  }
  else {
    store_ram_bank( (uint8_t*)&se[SYSEX_HEADER_SIZE], hdr->bank, hdr->name );
  }
}


void sysex_ram_request( uint8_t *se, int len ) {
  sx_ram_bank_hdr_t *hdr = (sx_ram_bank_hdr_t*)se;

  Serial.printf("\n-- Sysex Pattern RAM Request\n");
  Serial.printf("   Bank %02d\n", hdr->bank);

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
    case DRUM_SEL_BASS:     return (char*)"BASS";
    case DRUM_SEL_SNARE:    return (char*)"SNARE";
    case DRUM_SEL_HIHAT:    return (char*)"HIHAT";
    case DRUM_SEL_CLAPS:    return (char*)"CLAPS";
    case DRUM_SEL_CABASA:   return (char*)"CABASA";
    case DRUM_SEL_TAMB:     return (char*)"TAMB";
    case DRUM_SEL_TOM:      return (char*)"TOMS";
    case DRUM_SEL_CONGA:    return (char*)"CONGAS";
    case DRUM_SEL_COWBELL:  return (char*)"COWBELL";
    case DRUM_SEL_CLAVE:    return (char*)"CLAVE/RIMSHOT";
    case 0xff:              return (char*)"- LAST DRUM -";     // last active drum
    default:                return (char*)"* ERROR *";         // error, should not happen
  }
}


void send_sample_sysex( uint8_t banknum, uint8_t drum_sel ) {
  char vname[24];
  int vlen;
  int encoded_size = 0;
  uint8_t *sample;
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)sysex_decode_buf;

  Serial.printf("--- SysEx send Sample request\n");
  
  // -- go get it from SD card

  sample = get_voice( banknum, drum_sel_2_voice( drum_sel ), vname, &vlen );    // will fill in vname and vlen

  if( sample ) {                                                                // valid?
  
    // -- zero working buf
  
    memset( sysex_decode_buf, 0, sizeof(sysex_decode_buf) );
    
    // --- build the unencoded message
  
    hdr->cmd = CMD_SAMPLE_BANK;
    hdr->bank = banknum;                                                        // 0xff for current working pattern RAM
    hdr->drum_sel = drum_sel;
  
    sprintf( hdr->name, vname );                                                // put the filename in the header

    hdr->sample_len = vlen;                                                     // put the # of bytes in the sample in the header
  
    /*
    Serial.printf("send_sample_sysex: sysex_decode_buf @ %08x, header size = %d (%02x), sample start @ %08x\n",
                                    sysex_decode_buf, sizeof(sx_sample_bank_hdr_t), sizeof(sx_sample_bank_hdr_t), &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)]);
    */

    memcpy( &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)], sample, vlen );    // copy the sample data into the sysex message buffer
  
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
    
    // encode (no hi bits set) the header and sample data, right after the MIDI Manufacturer ID, which is not encoded (hi bit always 0)
    
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


void sysex_sample_store( uint8_t *se, int len ) {
  char vname[24];
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)se;

  Serial.printf("-- Sysex Sample Store\n");

  Serial.printf("   Sample len: %d\n", len - sizeof(sx_sample_bank_hdr_t) );
  Serial.printf("   Sample name: %s\n", hdr->name );
  
  // sanity check the name
  
  if( strlen( hdr->name ) == 0 )                                                // any string there?
    snprintf( vname, 24, "NONAME.BIN" );
  else
    snprintf( vname, 24, "%s", hdr->name );

  // now store it into the hardware and/or SD card

  sysex_store_prologue();                                                        // take the bus, pause hihat

  if( hdr->cmd == CMD_SAMPLE ) {                                                    // legacy cmd 0 sample?                            
    set_voice( last_drum, &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)], 
                  (len - sizeof(sx_sample_bank_hdr_t)), vname );                    // load into last active drum
  }
  else {                                                                            // must be cmd 1, with bank and drum selects
    if( hdr->bank == BANK_STAGING ) {
      set_voice( drum_sel_2_voice(hdr->drum_sel), 
                  &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)], 
                  (len - sizeof(sx_sample_bank_hdr_t)), vname );                    // load into selected drum card, will also put in STAGING
    }
    else {
      write_sd_bank_voice( hdr->bank, hdr->drum_sel, hdr->name,                     // copy into selected SD card bank and voice directory (and delete any old one there)
                  &sysex_decode_buf[sizeof(sx_sample_bank_hdr_t)], 
                  (len - sizeof(sx_sample_bank_hdr_t)) );                           // sample data is right after sx_sample_bank_hdr_t struct
    }
  }

  sysex_store_epilogue();                                                        // release the bus

  Serial.printf("-- Sysex Sample Store complete\n\n");
}


void sysex_sample_request( uint8_t *se, int len ) {
  sx_sample_bank_hdr_t *hdr = (sx_sample_bank_hdr_t*)se;

  Serial.printf("-- Sysex Sample Request\n");

  Serial.printf("   Bank %02d, Drum %02d (%s)\n", hdr->bank, hdr->drum_sel, drum_sel_2_name(hdr->drum_sel));

  send_sample_sysex( hdr->bank, hdr->drum_sel );
}


/* ---------------------------------------------------------------------------------------
    SysEx Parameter Set / Get
*/

void sysex_param( uint8_t *se, int len ) {
  uint8_t v;
  sx_parm_hdr_t *hdr = (sx_parm_hdr_t*)se;

  v = hdr->val;     // new setting

  // figure out which parameter

  Serial.printf("\n--- Parameter set: \n");

  switch( hdr->param ) {
    case SX_PARAM_FAN:            Serial.printf("   Fan Mode: %02d\n", v );
                                  set_fan_mode( v );
                                  eeprom_save_fan_mode( v );
                                  break;

    case SX_PARAM_MIDI_CHAN:      Serial.printf("   MIDI Channel: %02d\n", v );
                                  set_midi_channel( v );
                                  break;

    case SX_PARAM_MIDI_NOTE_OUT:  Serial.printf("   MIDI Note Out: %02d\n", v );
                                  set_midi_note_out_route( v );
                                  eeprom_save_midi_note_out_route( v );
                                  break;

    case SX_PARAM_MIDI_NOTE_IN:   Serial.printf("   MIDI Note In: %02d\n", v );
                                  set_midi_note_in_route( v );
                                  eeprom_save_midi_note_in_route( v );
                                  break;

    case SX_PARAM_MIDI_CLOCK_OUT: Serial.printf("   MIDI Clock Out: %02d\n", v );
                                  set_midi_clock_out_route( v );
                                  eeprom_save_midi_clock_out_route( v );                                  
                                  break;

    case SX_PARAM_MIDI_CLOCK_IN:  Serial.printf("   MIDI Clock In: %02d\n", v );
                                  set_midi_clock_in_route( v );
                                  eeprom_save_midi_clock_in_route( v );
                                  break;

    case SX_PARAM_MIDI_SYSEX:     Serial.printf("   MIDI SysEx: %02d\n", v );
                                  set_midi_sysex_route( v );
                                  eeprom_save_midi_sysex_route( v );
                                  break;

    case SX_PARAM_MIDI_SOFT_THRU: Serial.printf("   MIDI Soft Thru: %02d\n", v );
                                  set_midi_soft_thru( v ? true:false );
                                  eeprom_save_midi_soft_thru( v ? true:false );    
                                  break;

    case SX_PARAM_MIDI_START_EN:  Serial.printf("   MIDI Start Enable: %02d\n", v );
                                  honorMIDIStartStop( v ? true:false );
                                  eeprom_save_midi_start_honor( v ? true:false );
                                  break;

    case SX_PARAM_MIDI_SEND_VEL:  Serial.printf("   MIDI Start Enable: %02d\n", v );
                                  set_midi_send_vel( v ? true:false );
                                  eeprom_save_midi_send_velocity( v ? true:false );    
                                  break;

    case SX_PARAM_REBOOT:         Serial.printf("   Reboot: %02d\n", v );     reboot( v ? true:false );           break;

    case SX_PARAM_KEYPRESS:       Serial.printf("   Keypress: %02d\n", v );
                                  break;

    default:                      Serial.printf("   *** Unexpected param: %02x, ignoring\n\n",hdr->param);
                                  break;
  }

}


void sysex_param_request( uint8_t *se, int len ) {
  int encoded_size = 0;
  uint8_t v;
  sx_parm_hdr_t *hdr = (sx_parm_hdr_t*)se;

  // figure out which parameter

  Serial.printf("--- Parameter GET: \n");

  switch( hdr->param ) {
    case SX_PARAM_FAN:            v = get_fan_mode();                 Serial.printf("   Fan Mode: %02d\n", v );           break;

    case SX_PARAM_MIDI_CHAN:      v = get_midi_channel();             Serial.printf("   MIDI Channel: %02d\n", v );       break;

    case SX_PARAM_MIDI_NOTE_OUT:  v = get_midi_note_out_route();      Serial.printf("   MIDI Note Out: %02d\n", v );      break;

    case SX_PARAM_MIDI_NOTE_IN:   v = get_midi_note_in_route();       Serial.printf("   MIDI Note In: %02d\n", v );       break;

    case SX_PARAM_MIDI_CLOCK_OUT: v = get_midi_clock_out_route();     Serial.printf("   MIDI Clock Out: %02d\n", v );     break;

    case SX_PARAM_MIDI_CLOCK_IN:  v = get_midi_clock_in_route();      Serial.printf("   MIDI Clock In: %02d\n", v );      break;

    case SX_PARAM_MIDI_SYSEX:     v = get_midi_sysex_route();         Serial.printf("   MIDI SysEx: %02d\n", v );         break;

    case SX_PARAM_MIDI_SOFT_THRU: v = get_midi_soft_thru()?1:0;       Serial.printf("   MIDI Soft Thru: %02d\n", v );     break;

    case SX_PARAM_MIDI_START_EN:  v = honorMIDIStartStopState()?1:0;  Serial.printf("   MIDI Start Enable: %02d\n", v );  break;

    case SX_PARAM_MIDI_SEND_VEL:  v = get_midi_send_vel()?1:0;        Serial.printf("   MIDI Start Enable: %02d\n", v );  break;

    default:                      Serial.printf("   *** Unexpected param: %02x, ignoring\n\n",hdr->param);                break;
  }

  hdr->val = v;

  // --- build & send the response

  hdr->cmd = CMD_PARAM;                           // respond with REQUEST bit cleared
                                                  // param is still valid, val has been filled in
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
  
  encoded_size = pack_sysex_data( sizeof(sx_parm_hdr_t), (unsigned char*)hdr, &sysex_encode_buf[1] );      // len, in*, out*

  encoded_size += 1;                                // for the unencoded mfr ID in location 0

  // --- transmit

  send_sysex( encoded_size, sysex_encode_buf );
}


/* ---------------------------------------------------------------------------------------
    SysEx Name Utility Commands / Requests
*/

// Set a Voice Bank or RAM Pattern Bank name

void sysex_name_util( uint8_t *se, int len ) {
  char vname[24];
  sx_name_util_hdr_t *hdr = (sx_name_util_hdr_t*)se;

  // sanity check the name
  
  if( strlen( hdr->name ) == 0 )                      // any string there?
    snprintf( vname, 24, "NO NAME" );
  else
    snprintf( vname, 24, "%s", hdr->name );

  Serial.printf("-- Set Name Utility: \n");

  // figure out what kind of name it is

  switch( hdr->name_type ) {
    case SX_VOICE_BANK_NAME:  Serial.printf("   Voice Bank name: %s\n", vname );
                              set_voice_bank_name( hdr->bank, vname );
                              break;

    case SX_RAM_BANK_NAME:    Serial.printf("   RAM Pattern Bank name: %s\n", vname );
                              Serial.printf("   *** CAN ONLY *READ* RAM PATTERN BANK NAME FOR NOW\n\n");
                              break;

    case SX_SERIAL_NUMBER:    Serial.printf("   Serial Number: %s\n", vname );
                              eeprom_set_serial_number( vname );                // will only store first 8 chars
                              eeprom_get_serial_number( serial_number );
                              break;

    default:                  Serial.printf("   *** Unexpected name_type: %02x, ignoring\n\n",hdr->name_type);
                              break;
  }
}


void sysex_name_util_request( uint8_t *se, int len ) {
  int encoded_size = 0;
  sx_name_util_hdr_t *hdr = (sx_name_util_hdr_t*)se;

  Serial.printf("-- Get Name Utility: \n");

  // figure out what kind of name we are being asked for

  switch( hdr->name_type ) {
    case SX_VOICE_BANK_NAME:  snprintf( hdr->name, 24, get_voice_bank_name( hdr->bank ) );
                              Serial.printf("   Voice Bank name: %s\n", hdr->name );
                              break;

    case SX_RAM_BANK_NAME:    snprintf( hdr->name, 24, get_ram_bank_name( hdr->bank ) );
                              Serial.printf("   RAM Pattern Bank name: %s\n", hdr->name );
                              break;

    case SX_TEENSY_VERSION:   snprintf( hdr->name, 24, "v%d.%d", LUMA1_FW_VERSION_MAJOR, LUMA1_FW_VERSION_MINOR );
                              Serial.printf("   Teensy Code Version: %s\n", hdr->name );
                              break;

    case SX_SERIAL_NUMBER:    snprintf( hdr->name, 24, serial_number );
                              Serial.printf("   Serial Number: %s\n", hdr->name );
                              break;

    default:                  snprintf( hdr->name, 24, "ERROR INVALID REQUEST" );
                              Serial.printf("   *** Unexpected name_type: %02x, returning ERROR\n\n",hdr->name_type);
                              break;
  }
  
  // --- build & send the response

  hdr->cmd = CMD_NAME_UTIL;                       // respond with REQUEST bit cleared
                                                  // bank is still valid, name_type is still valid, name has been filled in
  
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
  
  encoded_size = pack_sysex_data( sizeof(sx_name_util_hdr_t), (unsigned char*)hdr, &sysex_encode_buf[1] );      // len, in*, out*

  encoded_size += 1;                                // for the unencoded mfr ID in location 0

  // --- transmit

  send_sysex( encoded_size, sysex_encode_buf );
}



/* ---------------------------------------------------------------------------------------
    SysEx utilities
*/

/*
    Some USB MIDI receivers (cough, Windows, cough) have trouble reliably consuming a large sysex message.

    So we chunk the sysex message up into bite-sized SYSEX_CHUNK_BYTES-byte pieces.

    We delay sysex_chunk_delay ms between each chunk.
    MENU 89 can be used to adjust the delay.
*/

#define SYSEX_CHUNK_BYTES         256                             // sysex chunk payload size

uint8_t se_chunk[SYSEX_CHUNK_BYTES+1];                            // make this one byte bigger for the start (0xf0) & end (0xf7) sysex bytes

void send_sysex( int len, uint8_t *b ) {
  int se_chunks;
  int se_remainder;
  uint8_t *in_buf;

  Serial.printf("Sending %d bytes of SysEx ", len );

  if( get_midi_sysex_route() & ROUTE_USB ) {
    Serial.printf("via USB...");

    if( len <= SYSEX_CHUNK_BYTES ) {
      usbMIDI.sendSysEx( len, b );                                // small messages we just send the easy way
    }
    else {
      in_buf = b;
        
      se_chunks = len / SYSEX_CHUNK_BYTES;
    
      se_remainder = len - (se_chunks * SYSEX_CHUNK_BYTES);
    
      Serial.printf("Chunks: %d, Remainder: %d\n", se_chunks, se_remainder);
  
      // --- FIRST CHUNK, includes 0xf0 start byte
      se_chunk[0] = 0xf0;                                           // sysex START
      memcpy( &se_chunk[1], in_buf, SYSEX_CHUNK_BYTES );
      usbMIDI.sendSysEx( SYSEX_CHUNK_BYTES+1, se_chunk, true );     // true -> tell midi lib to NOT add F0/F7
      delay( sysex_chunk_delay );
      Serial.printf("Sent chunk 1/%d\n", se_chunks);
      in_buf += SYSEX_CHUNK_BYTES;
  
      // --- CHUNK LOOP
      for( int xxx = 0; xxx != (se_chunks-1); xxx++ ) {             // already sent the first one
        usbMIDI.sendSysEx( SYSEX_CHUNK_BYTES, in_buf, true );       // true -> tell midi lib to NOT add F0/F7
        delay( sysex_chunk_delay );
        Serial.printf("Sent chunk %d/%d\n", xxx+1, se_chunks);
        in_buf += SYSEX_CHUNK_BYTES;
      }
  
      // --- LAST CHUNK, ends with 0xf7 end byte, could be remainder bytes long, could be just the 0xf7 if no remainder
      if( se_remainder )
        memcpy( &se_chunk, in_buf, se_remainder );
      se_chunk[se_remainder] = 0xf7;                                // sysex END
      usbMIDI.sendSysEx( se_remainder+1, se_chunk, true );          // true -> tell midi lib to NOT add F0/F7
      Serial.printf("Sent remainder chunk\n");
    }
  }
  
  if( get_midi_sysex_route() & ROUTE_DIN5 ) {
    Serial.printf("via DIN-5...\n");
    midiDIN.sendSysEx( len, b );
  }
  
  Serial.printf("...done!\n\n");
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

void sysex_store_prologue() {
  
  teensy_drives_z80_bus( true );                      // grab the bus

  set_rst_hihat( 1 );                                 // XXX FIXME should not need to do this, getting stomped

  z80_bus_write( PATT_DISPLAY, voice_num_map[(last_drum & 0xf) - 4]);     // PATT didsplay shows voice we are loading into
}


void sysex_store_epilogue() {

  set_rst_hihat( 0 );                                 // XXX FIXME should not need to do this, getting stomped

  if( reboot_after_sysex ) {
    z80_reset( true );
    teensy_drives_z80_bus( false );                   // we're done, let the z-80 go again
    z80_reset( false );

    delay( 250 );
    
    reboot_after_sysex = false;  
  }
  else {
    delay(100);
    teensy_drives_z80_bus( false );                   // just release the bus
    delay(100);
  }
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
    case SYSEX_INITIALIZE:  sysex_decode_idx = 0;                               // ensure buffer index is reset
                            sysex_err_abort = false;                            // not in error state
                                                                                // fall thru to the F0 hunter
    case SYSEX_FIND_F0:     if( b == 0xf0 )
                              process_sysex_state = SYSEX_FIND_MFR_ID;    break;
    
    case SYSEX_FIND_MFR_ID: if( b == OUR_MIDI_MFR_ID ) {
                              process_sysex_state = SYSEX_GET_B7S;
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
    
  return r;
}             



// mySystemExclusiveChunk() called for variable-sized sysex chunks as received. We use a state machine to decode a stream of these
//  variable-sized sysex chunks. "last" will be set to true for the final chunk.
//
//  ---> EXCEPT, "last" does not seem to be valid for DIN-5 sysex transfers, only USB.

void mySystemExclusiveChunk(const byte *d, uint16_t len, bool last) {

  Serial.printf("-- Got Sysex chunk: %d bytes, last: %d, err: %d\n", len, last, sysex_err_abort );

  // --- PROCESS EACH BLOCK
  
  for( int xxx = 0; xxx != len; xxx++ ) {                                                         // walk thru all bytes received in this block
    if( !sysex_err_abort                                                                          // if no errors detected
        && process_sysex_byte( *d++ )                                                             // and we processed thru the last byte
        && last ) {                                                                               // and teensy agrees that this is the last block
          
      sysex_decode_idx++;                                                                         // add 1 to make index = len
    
      Serial.printf("   Found Sysex end after %d bytes\n", sysex_decode_idx );

      switch( sysex_decode_buf[0] ) {

        // --- sysex DOWNLOAD types
        
        case CMD_SAMPLE:
        case CMD_SAMPLE_BANK:                   sysex_sample_store( sysex_decode_buf, sysex_decode_idx );         break;

        case CMD_RAM_BANK:                      sysex_ram_store( sysex_decode_buf, sysex_decode_idx );            break;

        case CMD_PARAM:                         sysex_param( sysex_decode_buf, sysex_decode_idx );                break;

        case CMD_NAME_UTIL:                     sysex_name_util( sysex_decode_buf, sysex_decode_idx );            break;

        // --- sysex UPLOAD REQUEST types

        case (CMD_SAMPLE + CMD_REQUEST):        
        case (CMD_SAMPLE_BANK + CMD_REQUEST):   sysex_sample_request( sysex_decode_buf, sysex_decode_idx );       break;

        case (CMD_RAM_BANK + CMD_REQUEST):      sysex_ram_request( sysex_decode_buf, sysex_decode_idx );          break;

        case (CMD_PARAM + CMD_REQUEST):         sysex_param_request( sysex_decode_buf, sysex_decode_idx );        break;

        case (CMD_NAME_UTIL + CMD_REQUEST):     sysex_name_util_request( sysex_decode_buf, sysex_decode_idx );    break;
      }
    }
  }

  // --- IF LAST BLOCK AND THERE WAS AN ERROR AT SOME POINT, PLAY ERROR BEEP

  if( sysex_err_abort && last ) {                                                 // sysex_err_abort will have stopped the processing
    Serial.printf("### ERROR receiving sysex\n");                                 // notify the user, clean up, and give up :-)
    beep_failure();
  }

  // --- RESET STATE AFTER LAST BLOCK FOR NEXT TIME
  
  if( last ) {
    process_sysex_state = SYSEX_INITIALIZE;                                       // done, either success or failure, but get ready for the next one
    sysex_err_abort = false;
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
  
  Serial.printf("== pack_sysex_data: len = %d in = %d / out = %d\n", len, in_idx, out_idx);
  
  return out_idx+1;
}
