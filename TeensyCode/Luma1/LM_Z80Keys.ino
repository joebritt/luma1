
/* ---------------------------------------------------------------------------------------
    Z-80 Bus Keyboard Scanning


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

#include "LM_Z80Keys.h"


uint8_t key_map[6];
uint8_t key_prev_map[6];
uint8_t key_change_map[6];


void init_keyboard() {
  key_map[0] = key_prev_map[0] = key_change_map[0] = 0;
  key_map[1] = key_prev_map[1] = key_change_map[1] = 0;
  key_map[2] = key_prev_map[2] = key_change_map[2] = 0;
  key_map[3] = key_prev_map[3] = key_change_map[3] = 0;
  key_map[4] = key_prev_map[4] = key_change_map[4] = 0;
  key_map[5] = key_prev_map[5] = key_change_map[5] = 0;
}


void scan_keyboard() {
  key_prev_map[0] = key_map[0];
  key_map[0]        = ~z80_bus_read( KB_ROW_1 );
  key_change_map[0] = key_prev_map[0] ^ key_map[0];

  key_prev_map[1] = key_map[1];  
  key_map[1]        = ~z80_bus_read( KB_ROW_2 );
  key_change_map[1] = key_prev_map[1] ^ key_map[1];

  key_prev_map[2] = key_map[2];  
  key_map[2]        = ~z80_bus_read( KB_ROW_3 );
  key_change_map[2] = key_prev_map[2] ^ key_map[2];

  key_prev_map[3] = key_map[3];  
  key_map[3]        = ~z80_bus_read( KB_ROW_4 );
  key_change_map[3] = key_prev_map[3] ^ key_map[3];

  key_prev_map[4] = key_map[4];  
  key_map[4]        = ~z80_bus_read( KB_ROW_5 );
  key_change_map[4] = key_prev_map[4] ^ key_map[4];

  key_prev_map[5] = key_map[5];  
  key_map[5]        = ~z80_bus_read( KB_ROW_6 );
  key_change_map[5] = key_prev_map[5] ^ key_map[5];

  delay( 10 );      // debounce keys. OK to spin here, system is halted while in local UI mode
}


// returns LM-1 style keycode or 0xff if no keys down
// starts at row 1, returns first one found

uint8_t find_first( uint8_t base, uint8_t row );

uint8_t find_first( uint8_t base, uint8_t row ) {
  uint8_t code = 0xff;
  uint8_t walker = 0x01;
  
  for( int xxx = 0; xxx < 8; xxx++ ) {
    if( (key_change_map[row] & walker) && (key_map[row] & walker) ) {             // if it changed AND it's down, report it. otherwise, keep looking
      code = base + xxx;
      break;
    }

    walker <<= 1;       // next bit
  }
  
  return code;
}

uint8_t get_keycode() {
  uint8_t kc = 0xff;

  kc = find_first( 0x00, 0 ); if( kc != 0xff ) return kc;
  kc = find_first( 0x08, 1 ); if( kc != 0xff ) return kc;

  kc = find_first( 0x10, 2 ); if( kc != 0xff ) return kc;
  kc = find_first( 0x18, 3 ); if( kc != 0xff ) return kc;

  kc = find_first( 0x20, 4 ); if( kc != 0xff ) return kc;
  kc = find_first( 0x28, 5 ); if( kc != 0xff ) return kc;
    
  kc = find_first( 0x00, 0 ); if( kc != 0xff ) return kc;
  
  return kc;
}
