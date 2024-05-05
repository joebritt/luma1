
/* ---------------------------------------------------------------------------------------
    VOICE LOADING

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

#ifndef LM_Voices_H_
#define LM_Voices_H_

void set_voice( uint16_t voice, uint8_t *s, int len, char *vname );     // use this to load voices (also handles size constraints and staging)

void trig_voice( uint16_t voice, uint8_t val );                         // select voice with Z-80 address

void set_load_data( uint8_t d );                                        // puts d on LOAD bus

#define kLOAD                   0
#define kPLAY                   1

void set_play_load( bool mode );                        // sets PLAY/LOAD to mode

void set_voice_wr( bool wr );                           // sets nVOICE_WR to wr

void set_rst_hihat( bool s );                           // pulse high to reset hihat addr counters

void set_hat_loading_led( bool s );                     // s = 1 -> nHAT_LOADING = input, s = 1 -> nHAT_LOADING = output, drive to 0


#define SAMPLE_LEN_2K           0x00
#define SAMPLE_LEN_4K           0x01
#define SAMPLE_LEN_8K           0x02
#define SAMPLE_LEN_32K          0x03

// voice selectors are STB_ addresses, e.g., STB_SNARE

void set_sample_length( uint16_t v, uint8_t len );      // set desired sample length for voice

void load_sample( uint16_t v, uint8_t *s, int len );    // copy sample s into voice v SRAM
                                                        // XXX - add hook for status

void load_voice_prologue( uint16_t v );
void load_voice_epilogue( uint16_t v );

void stage_voice( char *dirname, char *fn, uint8_t *s, int len );
void stage_bank_name( uint8_t cur_bank_num );

void load_voice( uint16_t voice, uint8_t *s, int len );

void load_voice_file( uint16_t voice, char *filename );

uint8_t *get_voice( uint8_t bank_num, uint16_t voice, char *voice_name, int *voice_len );     // pull into buffer, get info, don't load into voice hw (this is for sysex)


#define BANK_LOAD_CONGAS    0x0001
#define BANK_LOAD_TOMS      0x0002
#define BANK_LOAD_SNARE     0x0004
#define BANK_LOAD_BASS      0x0008
#define BANK_LOAD_HIHAT     0x0010
#define BANK_LOAD_COWBELL   0x0020
#define BANK_LOAD_CLAPS     0x0040
#define BANK_LOAD_CLAVE     0x0080
#define BANK_LOAD_TAMB      0x0100
#define BANK_LOAD_CABASA    0x0200

#define BANK_LOAD_ALL       0x03ff

#define BANK_STAGING        0xff            // pass in for bank # to reference STAGING bank

void build_voice_filename( uint16_t voice, uint8_t bank_num, char *fn );

extern uint16_t voice_load_bm;

void load_voice_bank( uint16_t voice_selects, uint8_t bank_num );
void store_voice_bank( uint16_t voice_selects, uint8_t bank_num );      // copies STAGING to bank_num on SD

char *get_sd_voice_bank_name( uint8_t bank_num );

char *get_cur_bank_name();

void set_bank_name( uint8_t bank_num, char *name );

void voice_bank_dirty( bool d );

#endif
