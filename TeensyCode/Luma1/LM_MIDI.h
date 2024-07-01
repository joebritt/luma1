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

#ifndef LM_MIDI_H_
#define LM_MIDI_H_

#include <MIDI.h>
#include "LM_EEPROM.h"

int midi_chan = 1;

// MIDI note mapping used in the Kenton LM-1 MIDI retrofit kit

#define MIDI_NOTE_BASS              36              // C1
#define MIDI_NOTE_SNARE             37              // C#1
#define MIDI_NOTE_HIHAT             38              // D1
#define MIDI_NOTE_HIHAT_OPEN        39              // D#1
#define MIDI_NOTE_CLAPS             40              // E1
#define MIDI_NOTE_CABASA            41              // F1
#define MIDI_NOTE_TAMB              42              // F#1
#define MIDI_NOTE_TOM_UP            43              // G1
#define MIDI_NOTE_TOM_DN            44              // G#1
#define MIDI_NOTE_CONGA_UP          45              // A1
#define MIDI_NOTE_CONGA_DN          46              // A#1
#define MIDI_NOTE_COWBELL           47              // B1
#define MIDI_NOTE_CLAVE             48              // C2       AKA RIMSHOT

#define MIDI_VEL_LOUD               127
#define MIDI_VEL_SOFT               63

void init_midi();

void handle_midi_in();
void handle_midi_out();

bool enable_midi_start_stop_clock( bool en );       // enable/disable interrupt used to detect FSK TTL clock
                                                    // --> this signal comes from a register shared with other control signals, we need to disable/enable to prevent false triggers

bool luma_is_playing();                             // used to display "Awaiting Tempo Clock" and control when fan can run


bool midi_din_out_active();                         // used to display MIDI traffic indicators
bool midi_din_in_active();

bool midi_usb_out_active();
bool midi_usb_in_active();


// configure MIDI interface routing

#define ROUTE_NONE                0x00
#define ROUTE_DIN5                0x01
#define ROUTE_USB                 0x02
#define ROUTE_DIN5_USB            0x03


// --- Notes

void set_midi_note_out_route( uint8_t r );
void set_midi_note_in_route( uint8_t r );

uint8_t get_midi_note_out_route();
uint8_t get_midi_note_in_route();

// --- Start / Stop / Clock

void set_midi_clock_out_route( uint8_t r );
void set_midi_clock_in_route( uint8_t r );

uint8_t get_midi_clock_out_route();
uint8_t get_midi_clock_in_route();

bool honorMIDIStartStopState();
bool honorMIDIStartStop( bool honor );

// --- Sysex

void set_midi_sysex_route( uint8_t r );

uint8_t get_midi_sysex_route();

// --- Soft Thru

void set_midi_soft_thru( bool on );
bool get_midi_soft_thru();


// drum trigger to/from MIDI

void send_midi_drm( byte note, byte vel );
void play_midi_drm( byte note, byte vel );


// MIDI handlers

void myNoteOn(byte channel, byte note, byte velocity);
void myNoteOff(byte channel, byte note, byte velocity);
void myProgramChange(byte channel, byte pgm);
void myClock();
void myStart();
void myContinue();
void myStop();
void mySystemExclusiveChunk(const byte *d, uint16_t len, bool last);

void didProgramChange( byte pgm );                  // send Program Change MIDI message


// manage the TAPE_SYNC_CLK signal

void set_tape_sync_clk_gpo( bool state );           // drive it hi or lo
bool get_tape_sync_clk_gpo();                       // return current state


// clock work

elapsedMillis sinceLastFSKClk;                      // zeroed in interrupt handler -- keep pulling it back to 0
bool song_is_started = false;                       // set when we first see the FSK clock go, stopped when we see time pass without an FSK clock

bool send_midi_clk = false;                         // set to true by the clock edge interrupt handler when it's time to send one

void reset_FSK_clock_check();                       // call if we haven't been able to check the FSK clock for a while to prevent spurious MIDI start/stop

void internal_tempo_clock( void );                  // interrupt handler for internal tempo clock edge

#define MS_TO_NO_CLK                500             // if 1/2 sec passes with no clocks, trigger "no clk" warning
extern elapsedMillis no_tempo_clk_detect;


// externally visible sysex routines

#define DRUM_SEL_BASS           0
#define DRUM_SEL_SNARE          1
#define DRUM_SEL_HIHAT          2
#define DRUM_SEL_CLAPS          3
#define DRUM_SEL_CABASA         4
#define DRUM_SEL_TAMB           5
#define DRUM_SEL_TOM            6
#define DRUM_SEL_CONGA          7
#define DRUM_SEL_COWBELL        8
#define DRUM_SEL_CLAVE          9                               // AKA RIMSHOT

void send_sample_sysex( uint8_t banknum, uint8_t drum_sel );    // banknum 00-99, or 0xff -> send currently active RAM
                                                                // see above for drum_sel, or 0xff -> send currently selected drum

uint16_t drum_sel_2_voice( uint8_t drum_sel );                  // map sysex drum_sel to drum strobe

void send_pattern_RAM_sysex( uint8_t banknum );                 // banknum 00-99, or 0xff -> send currently active RAM

#endif
