/* ---------------------------------------------------------------------------------------
    Z-80 Bus Control

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

#ifndef LM_Z80BUS_H_
#define LM_Z80BUS_H_


#define D802_SHADOW       0xa016        // RAM shadow of write-only D802 register
                                        // XXX NOTE THAT THIS IS IN V3.1 of the Z-80 ROM, others may be different 

extern uint8_t led_set_2_shadow;        // a place to park that shadow when the Teensy is playing with the D802 register

                                                
// Z-80 address space locations

#define PATT_DISPLAY      0xd800      // 2 BCD digits
#define LINK_DISPLAY      0xd801      // 2 BCD digits


#define LED_SET_2         0xd802      // bit 7 is active-low enable for D[2:0] to drum generators

#define LED_STORE         0x01        // LED_A
#define LED_VERIFY        0x02        // LED_B
#define LED_LOAD          0x04        // LED_C
#define LED_PLAY_STOP     0x08
#define BEEP_OUT          0x10
#define CLOCK_OUT         0x20
#define TAPE_FSK_OUT      0x40
#define DRUM_DO_ENABLE    0x80

#define LED_A             0x01        // LED_STORE
#define LED_B             0x02        // LED_VERIFY
#define LED_C             0x04        // LED_LOAD

#define INPUT_JACKS       0xd803

#define INP_TEMPO_CLK     0x01
#define INP_FOOTSWITCH    0x02
#define INP_REC_SAFE      0x04
#define INP_TAPE_FSK      0x08
#define INP_CLK_SW_4      0x10
#define INP_CLK_SW_5      0x20
#define INP_CLK_SW_6      0x40
#define INP_CLK_SW_7      0x80

// table to normalize voice numbers when we display them
uint8_t voice_num_map[10] = { 0xf4, 0xf3, 0xf5, 0xf7, 0x10, 0xf9, 0xf2, 0xf1, 0xf6, 0xf8 };

#define STB_BASS          0xd804      // drum strobes
#define STB_SNARE         0xd805      
#define STB_HIHAT         0xd806      
#define STB_CLAPS         0xd807      
#define STB_CABASA        0xd808      
#define STB_TAMB          0xd809      
#define STB_TOMS          0xd80a      
#define STB_CONGAS        0xd80b      
#define STB_COWBELL       0xd80c      
#define STB_CLAVE         0xd80d      
#define STB_CLICK         0xd80e      


#define LED_SET_1         0xd80f   

#define SHUFFLE_MASK      0x07        // low 3 bits select which shuffle % LED to illuminate
#define SHUFFLE_SHIFT     0

#define QUANTIZE_MASK     0x38        // these 3 bits select which quantization LED to illuminate
#define QUANTIZE_SHIFT    3



// Z-80 bus utilities

void init_z80_if( void );

void z80_reset( bool inreset );
bool z80_in_reset = true;

void teensy_drives_z80_bus( bool drive );         // acquire or release the Z-80 bus, set up pin modes appropriately

bool teensy_driving_bus = false;                  // use this to detect if bus is already owned by teensy

void set_z80_addr( uint16_t a );

void set_z80_data( uint8_t d );
uint8_t get_z80_data( void );

void z80_drive_data( bool d );

void z80_bus_write( uint16_t a, uint8_t d );
uint8_t z80_bus_read( uint16_t a );


void load_z80_rom( uint8_t *rom_image );          // copy 6KB from rom_image to Z-80 ROM memory (really SRAM)

void copy_z80_ram( uint8_t *img );                // copy 8KB Z-80 RAM to img
void load_z80_ram( uint8_t *img );                // copy 8KB img to z-80 RAM

void dump_z80_mem( uint16_t startAddr, uint16_t len );


void set_LED_SET_2( uint8_t val );
void clr_LED_SET_2( uint8_t val );

uint8_t save_LED_SET_2();
void restore_LED_SET_2();  

#endif
