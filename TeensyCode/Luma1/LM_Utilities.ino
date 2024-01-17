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

#include "LM_Utilities.h"

void printHex2( uint8_t c ) {
  if( c < 0x10 ) Serial.print("0");
  Serial.print( c, HEX );  
}

void printHex4( uint16_t w ) {
  printHex2( w>>8 );
  printHex2( w );
}

uint8_t bcd2dec( uint8_t bcdnum ) {
  return( ((bcdnum>>4)*10) + (bcdnum&0x0f) );
}

uint8_t dec2bcd( uint8_t decnum ) {
  return( ((decnum/10)<<4) + (decnum-((decnum/10)*10)) );
}


void dumpit( uint8_t *b, int idx ) {  
  Serial.printf("Dumping %d bytes: \n", idx);

  for( int zzz = 0; zzz < idx; zzz += 16 ) {
    Serial.printf("%04X: ", zzz);
    
    for( int xxx = 0; xxx != 16; xxx++ )
      Serial.printf("%02x ", b[zzz+xxx]);
  
    Serial.print(" | ");
  
    for( int yyy = 0; yyy != 16; yyy++ )
      Serial.printf("%c ", isprint(b[zzz+yyy]) ? b[zzz+yyy] : ' ');
  
    Serial.println();
  }
}
