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

#include "LM_DebugCmds.h"

// commands over the USB serial port
// this can block, and has no bounds checking, it's only intended for debug commands


byte ram_backup[8192];

extern uint8_t filebuf[32768];
extern bool prev_drum_trig_int_enable;


bool end_test_time();


char dbg_buf[1024];
char dbg_cmd[8];
char dbg_param[256];

#define LED_TEST_DELAY      200


/* ===========================================================================================================
    LED TESTS
*/

void test_7_seg( uint16_t a, uint8_t d ) {
  z80_bus_write( a, d );
  delay( LED_TEST_DELAY );
}


/* ===========================================================================================================
    VOICE TESTS
*/

// u-Law sine wave data for voice test

uint8_t vtest_256[256] = {
  0x00, 0x2C, 0x3A, 0x43, 0x49, 0x4F, 0x52, 0x55, 0x58, 0x5B, 0x5E, 0x61, 0x62, 0x63, 0x65, 0x66, 
  0x68, 0x69, 0x6B, 0x6C, 0x6E, 0x6F, 0x70, 0x71, 0x71, 0x72, 0x72, 0x73, 0x74, 0x74, 0x75, 0x75,
  0x76, 0x77, 0x77, 0x78, 0x78, 0x79, 0x79, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7D, 
  0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 
  0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 
  0x7D, 0x7D, 0x7C, 0x7C, 0x7C, 0x7B, 0x7B, 0x7A, 0x7A, 0x7A, 0x79, 0x79, 0x78, 0x78, 0x77, 0x77, 
  0x76, 0x75, 0x75, 0x74, 0x74, 0x73, 0x72, 0x72, 0x71, 0x71, 0x70, 0x6F, 0x6E, 0x6C, 0x6B, 0x69, 
  0x68, 0x66, 0x65, 0x63, 0x62, 0x61, 0x5E, 0x5B, 0x58, 0x55, 0x52, 0x4F, 0x49, 0x43, 0x3A, 0x2C, 
  0x00, 0xB2, 0xBE, 0xC5, 0xCB, 0xD0, 0xD3, 0xD6, 0xD9, 0xDC, 0xDF, 0xE1, 0xE3, 0xE4, 0xE5, 0xE7, 
  0xE8, 0xEA, 0xEB, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 0xF4, 0xF5, 0xF5, 0xF6,
  0xF6, 0xF7, 0xF7, 0xF8, 0xF8, 0xF9, 0xF9, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFD, 0xFD, 
  0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFE, 0xFE, 0xFD, 
  0xFD, 0xFD, 0xFD, 0xFC, 0xFC, 0xFB, 0xFB, 0xFB, 0xFA, 0xFA, 0xF9, 0xF9, 0xF8, 0xF8, 0xF7, 0xF7, 
  0xF6, 0xF6, 0xF5, 0xF5, 0xF4, 0xF3, 0xF3, 0xF2, 0xF1, 0xF1, 0xF0, 0xEF, 0xEE, 0xED, 0xEB, 0xEA, 
  0xE8, 0xE7, 0xE5, 0xE4, 0xE3, 0xE1, 0xDF, 0xDC, 0xD9, 0xD6, 0xD3, 0xD0, 0xCB, 0xC5, 0xBE, 0xB2
};

uint8_t vtest_512[512] = {
  0x00, 0x18, 0x2C, 0x32, 0x3A, 0x3E, 0x43, 0x45, 0x49, 0x4D, 0x4F, 0x51, 0x52, 0x54, 0x55, 0x57, 0x58, 0x5A, 0x5B, 0x5D, 0x5E, 0x60, 0x61, 0x61, 0x62, 0x63, 0x63, 0x64, 0x65, 0x66, 0x66, 0x67, 
  0x68, 0x69, 0x69, 0x6A, 0x6B, 0x6B, 0x6C, 0x6D, 0x6E, 0x6E, 0x6F, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x75, 0x76,
  0x76, 0x76, 0x77, 0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7D, 0x7D,
  0x7D, 0x7D, 0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
  0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 0x7D, 0x7D,
  0x7D, 0x7D, 0x7D, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x79, 0x79, 0x79, 0x79, 0x78, 0x78, 0x78, 0x78, 0x77, 0x77, 0x77, 0x77, 0x76,
  0x76, 0x76, 0x75, 0x75, 0x75, 0x75, 0x74, 0x74, 0x74, 0x73, 0x73, 0x73, 0x72, 0x72, 0x72, 0x72, 0x71, 0x71, 0x71, 0x70, 0x70, 0x70, 0x6F, 0x6E, 0x6E, 0x6D, 0x6C, 0x6B, 0x6B, 0x6A, 0x69, 0x69,
  0x68, 0x67, 0x66, 0x66, 0x65, 0x64, 0x63, 0x63, 0x62, 0x61, 0x61, 0x60, 0x5E, 0x5D, 0x5B, 0x5A, 0x58, 0x57, 0x55, 0x54, 0x52, 0x51, 0x4F, 0x4D, 0x49, 0x45, 0x43, 0x3E, 0x3A, 0x32, 0x2C, 0x18,
  0x00, 0xA4, 0xB2, 0xB6, 0xBE, 0xC1, 0xC5, 0xC7, 0xCB, 0xCF, 0xD0, 0xD2, 0xD3, 0xD5, 0xD6, 0xD8, 0xD9, 0xDB, 0xDC, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE3, 0xE4, 0xE5, 0xE5, 0xE6, 0xE7, 0xE8,
  0xE8, 0xE9, 0xEA, 0xEB, 0xEB, 0xEC, 0xED, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF2, 0xF2, 0xF2, 0xF3, 0xF3, 0xF3, 0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6,
  0xF6, 0xF7, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFD,
  0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFD, 0xFD,
  0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8, 0xF8, 0xF8, 0xF8, 0xF7, 0xF7, 0xF7, 0xF7,
  0xF6, 0xF6, 0xF6, 0xF5, 0xF5, 0xF5, 0xF5, 0xF4, 0xF4, 0xF4, 0xF3, 0xF3, 0xF3, 0xF2, 0xF2, 0xF2, 0xF1, 0xF1, 0xF1, 0xF0, 0xF0, 0xF0, 0xEF, 0xEF, 0xEE, 0xED, 0xED, 0xEC, 0xEB, 0xEB, 0xEA, 0xE9,
  0xE8, 0xE8, 0xE7, 0xE6, 0xE5, 0xE5, 0xE4, 0xE3, 0xE3, 0xE2, 0xE1, 0xE0, 0xDF, 0xDE, 0xDC, 0xDB, 0xD9, 0xD8, 0xD6, 0xD5, 0xD3, 0xD2, 0xD0, 0xCF, 0xCB, 0xC7, 0xC5, 0xC1, 0xBE, 0xB6, 0xB2, 0xA4
};

uint8_t vtest_1024[1024] = {
 0x00, 0x00, 0x18, 0x24, 0x2C, 0x2C, 0x32, 0x36, 0x3A, 0x3E, 0x3E, 0x41, 0x43, 0x45, 0x45, 0x47, 0x49, 0x4B, 0x4D, 0x4D, 0x4F, 0x50, 0x51, 0x51, 0x52, 0x53, 0x54, 0x55, 0x55, 0x56, 0x57, 0x58,
 0x58, 0x59, 0x5A, 0x5B, 0x5B, 0x5C, 0x5D, 0x5E, 0x5E, 0x5F, 0x60, 0x60, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x63, 0x63, 0x63, 0x64, 0x64, 0x65, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x68,
 0x68, 0x68, 0x69, 0x69, 0x69, 0x6A, 0x6A, 0x6A, 0x6B, 0x6B, 0x6B, 0x6C, 0x6C, 0x6D, 0x6D, 0x6D, 0x6E, 0x6E, 0x6E, 0x6F, 0x6F, 0x6F, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x71,
 0x71, 0x71, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76,
 0x76, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7A, 0x7A,
 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7A, 0x7A, 0x7A, 0x7A,
 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x76, 0x76, 0x76,
 0x76, 0x76, 0x76, 0x76, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x71,
 0x71, 0x71, 0x71, 0x71, 0x71, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x6F, 0x6F, 0x6F, 0x6E, 0x6E, 0x6E, 0x6D, 0x6D, 0x6D, 0x6C, 0x6C, 0x6B, 0x6B, 0x6B, 0x6A, 0x6A, 0x6A, 0x69, 0x69, 0x69, 0x68,
 0x68, 0x68, 0x67, 0x67, 0x66, 0x66, 0x66, 0x65, 0x65, 0x65, 0x64, 0x64, 0x63, 0x63, 0x63, 0x62, 0x62, 0x62, 0x61, 0x61, 0x61, 0x60, 0x60, 0x5F, 0x5E, 0x5E, 0x5D, 0x5C, 0x5B, 0x5B, 0x5A, 0x59,
 0x58, 0x58, 0x57, 0x56, 0x55, 0x55, 0x54, 0x53, 0x52, 0x51, 0x51, 0x50, 0x4F, 0x4D, 0x4D, 0x4B, 0x49, 0x47, 0x45, 0x45, 0x43, 0x41, 0x3E, 0x3E, 0x3A, 0x36, 0x32, 0x2C, 0x2C, 0x24, 0x18, 0x00,
 0x00, 0x98, 0xA4, 0xAC, 0xB2, 0xB2, 0xB6, 0xBA, 0xBE, 0xC1, 0xC1, 0xC3, 0xC5, 0xC7, 0xC7, 0xC9, 0xCB, 0xCD, 0xCF, 0xCF, 0xD0, 0xD1, 0xD2, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD9,
 0xD9, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 0xE3, 0xE3, 0xE4, 0xE4, 0xE4, 0xE5, 0xE5, 0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE7, 0xE8, 0xE8,
 0xE8, 0xE9, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB, 0xEB, 0xEB, 0xEC, 0xEC, 0xEC, 0xED, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF, 0xEF, 0xEF, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1,
 0xF1, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF3, 0xF3, 0xF3, 0xF3, 0xF3, 0xF3, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6,
 0xF6, 0xF6, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA,
 0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD,
 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFD, 0xFD, 0xFD, 0xFD,
 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFA, 0xFA,
 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF6,
 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF4, 0xF3, 0xF3, 0xF3, 0xF3, 0xF3, 0xF3, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,
 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xEF, 0xEF, 0xEF, 0xEE, 0xEE, 0xEE, 0xED, 0xED, 0xED, 0xEC, 0xEC, 0xEC, 0xEB, 0xEB, 0xEB, 0xEA, 0xEA, 0xE9, 0xE9, 0xE9,
 0xE8, 0xE8, 0xE8, 0xE7, 0xE7, 0xE7, 0xE6, 0xE6, 0xE5, 0xE5, 0xE5, 0xE4, 0xE4, 0xE4, 0xE3, 0xE3, 0xE3, 0xE2, 0xE2, 0xE1, 0xE1, 0xE1, 0xE0, 0xE0, 0xDF, 0xDF, 0xDE, 0xDD, 0xDC, 0xDC, 0xDB, 0xDA,
 0xD9, 0xD9, 0xD8, 0xD7, 0xD6, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD2, 0xD1, 0xD0, 0xCF, 0xCF, 0xCD, 0xCB, 0xC9, 0xC7, 0xC7, 0xC5, 0xC3, 0xC1, 0xC1, 0xBE, 0xBA, 0xB6, 0xB2, 0xB2, 0xAC, 0xA4, 0x98
};

 
int voice_test_sample_len= 32768;

uint8_t *voice_test_data = vtest_256;
int voice_test_data_len = sizeof(vtest_256);

void voice_test( char *n, uint16_t strobe );

void run_voice_test(  char *n, uint16_t strobe ) {
  bool prev = prev_drum_trig_int_enable;
    
  Serial.printf("%s Voice test (SEND x TO CANCEL)...", n);
  
  teensy_drives_z80_bus( true );
  
  disable_drum_trig_interrupt();            // XXX should not need to do this, fix properly
  prev_drum_trig_int_enable = false;
  
  Serial.printf("Building test waveform\n");
  
  for( int xxx = 0; xxx < 32768; xxx += voice_test_data_len ) {
    memcpy( &filebuf[xxx], voice_test_data, voice_test_data_len );
  }
  
  Serial.printf("Loading waveform into voice board\n");
  
  set_voice(  strobe, 
              filebuf, 
              (((strobe == STB_CONGAS) || (strobe == STB_TOMS)) && (voice_test_sample_len == 32768))?16384:voice_test_sample_len, 
              (char*)"DIAG" );
  
  Serial.printf("Playing waveform\n");

  trig_voice( strobe, 0x00 );
  trig_voice( strobe, 0x01 );

  Serial.printf("Delay\n");
  delay( 1500 );
  
  if( prev )
    enable_drum_trig_interrupt();
    
  teensy_drives_z80_bus( false );
  
  Serial.printf("done.\n");
}


void voice_test( char *n, uint16_t strobe ) {
  while( !end_test_time() )
    run_voice_test( n, strobe );
}


/* ===========================================================================================================
    FRAM TEST
*/

uint8_t pattern_start, readback, data;
uint16_t addr = 0xa000;

  
void run_fram_test() {
  Serial.printf("FRAM Test (SEND x TO CANCEL)...\n");
  
  teensy_drives_z80_bus( true );
  
  // -- SAVE
  
  Serial.printf("---> Saving current FRAM\n");
  
  addr = 0xa000;
  
  for( int xxx = 0; xxx != (8*1024); xxx++ )
    ram_backup[xxx] = z80_bus_read( addr++ );
  
  Serial.printf("     Done, beginning test\n");
        
  Serial.printf("---> Filling FRAM with pattern\n");

  // -- FILL
  
  addr = 0xa000;
  data = pattern_start;

  for( int xxx = 0; xxx != (8*1024); xxx++ )
    z80_bus_write( addr++, data++ );

  // -- READBACK
  
  Serial.printf("---  Reading back FRAM\n");

  addr = 0xa000;
  data = pattern_start;

  for( int xxx = 0; xxx != (8*1024); xxx++ ) {
    readback = z80_bus_read( addr++ );
    if( readback != data++ ) {
      Serial.printf("###  MISMATCH: %04x read back as %02x, expected %02x\n", addr-1, readback, data-1);
      dump_z80_mem( addr-1, 256 );
      Serial.printf("     HALTED.\n");
      while( 1 )
        delay( 100 );
    }
  }
  
  pattern_start++;

  Serial.printf("     Readback OK! Pattern start = %02x\n", pattern_start);
  delay( 1000 );
  
  // -- RESTORE
  
  Serial.printf("done, restoring RAM...");
  
  addr = 0xa000;
  
  for( int xxx = 0; xxx != (8*1024); xxx++ )
    z80_bus_write( addr++, ram_backup[xxx] );
  
  Serial.printf("resuming where we left off.\n");
  
  teensy_drives_z80_bus( false );
}


/* ===========================================================================================================
    ROM SRAM TEST
*/

void run_rom_sram_test() {
  Serial.printf("Z-80 ROM SRAM Test (SEND x TO CANCEL)...\n");
  
  teensy_drives_z80_bus( true );                  
  
  Serial.printf("Beginning test\n");
      
  Serial.printf("---> Filling ROM SRAM with pattern, start = %02x\n", pattern_start);

  // -- FILL
  
  data = pattern_start;

  for( int xxx = 0; xxx != (3*2048); xxx++ ) {      // copy the ROM code to the SRAM that the Z-80 sees as ROM
    z80_bus_write( xxx, data++ );                   // ROM base addr is 0000
  }

  // -- READBACK
  
  Serial.printf("---  Reading back ROM SRAM\n");

  data = pattern_start;

  for( int xxx = 0; xxx != (3*2048); xxx++ ) {      // did everything stick?
    readback = z80_bus_read( xxx );
    if( readback != data ) {
      Serial.printf("###  MISMATCH: %04x read back as %02x, expected %02x\n", xxx, readback, data);
      dump_z80_mem( xxx, 256 );
      Serial.printf("     HALTED.\n");
      while( 1 )
        delay( 100 );
    }
    data++;
  }
  
  pattern_start++;

  Serial.printf("     Readback OK! Pattern start = %02x\n", pattern_start);
  delay( 1000 );
  
  // -- RESTORE
  
  Serial.printf("Reloading Z-80 ROM code\n");
  load_z80_rom_file( (char*)"Z80_CODE" );
  
  Serial.printf("Applying Z-80 patches\n");
  apply_z80_patches();
  
  Serial.printf("resuming where we left off.\n");
  teensy_drives_z80_bus( false );
}



/* ===========================================================================================================
    TEST COMMANDS
*/

bool end_test_time() {
  if( Serial.available() ) {
    while( Serial.available() )
      Serial.read();

    return true;
  }

  return false;
}

void handle_debug_commands() {
  uint8_t kc;


  if( Serial.available() ) {
    dbg_buf[1023] = 0;
    
    for( int xxx = 0; xxx != 1023; xxx++ ) {
      dbg_buf[xxx] = Serial.read();
      if( dbg_buf[xxx] == 0x0a )
        break;
    }

    Serial.printf( "got: %s\n", dbg_buf );

    // 1-char cmds

    switch( dbg_buf[0] ) {
      case 'S':
      case 's':   eeprom_set_serial_number( &dbg_buf[2] );
                  eeprom_get_serial_number( serial_number );
                  break;

      case 'L':
      case 'l':   teensy_drives_z80_bus( true );                      // grab the bus
                  Serial.printf("PATTERN display test...");
                  test_7_seg( PATT_DISPLAY, 0xf0 );
                  test_7_seg( PATT_DISPLAY, 0xf1 );
                  test_7_seg( PATT_DISPLAY, 0xf2 );
                  test_7_seg( PATT_DISPLAY, 0xf3 );
                  test_7_seg( PATT_DISPLAY, 0xf4 );
                  test_7_seg( PATT_DISPLAY, 0xf5 );
                  test_7_seg( PATT_DISPLAY, 0xf6 );
                  test_7_seg( PATT_DISPLAY, 0xf7 );
                  test_7_seg( PATT_DISPLAY, 0xf8 );
                  test_7_seg( PATT_DISPLAY, 0xf9 );
                  
                  test_7_seg( PATT_DISPLAY, 0x0f );
                  test_7_seg( PATT_DISPLAY, 0x1f );
                  test_7_seg( PATT_DISPLAY, 0x2f );
                  test_7_seg( PATT_DISPLAY, 0x3f );
                  test_7_seg( PATT_DISPLAY, 0x4f );
                  test_7_seg( PATT_DISPLAY, 0x5f );
                  test_7_seg( PATT_DISPLAY, 0x6f );
                  test_7_seg( PATT_DISPLAY, 0x7f );
                  test_7_seg( PATT_DISPLAY, 0x8f );
                  test_7_seg( PATT_DISPLAY, 0x9f );
                  test_7_seg( PATT_DISPLAY, 0xff );
                  Serial.printf("done.\n");

                  Serial.printf("LINK display test...");
                  test_7_seg( LINK_DISPLAY, 0xf0 );
                  test_7_seg( LINK_DISPLAY, 0xf1 );
                  test_7_seg( LINK_DISPLAY, 0xf2 );
                  test_7_seg( LINK_DISPLAY, 0xf3 );
                  test_7_seg( LINK_DISPLAY, 0xf4 );
                  test_7_seg( LINK_DISPLAY, 0xf5 );
                  test_7_seg( LINK_DISPLAY, 0xf6 );
                  test_7_seg( LINK_DISPLAY, 0xf7 );
                  test_7_seg( LINK_DISPLAY, 0xf8 );
                  test_7_seg( LINK_DISPLAY, 0xf9 );
                  
                  test_7_seg( LINK_DISPLAY, 0x0f );
                  test_7_seg( LINK_DISPLAY, 0x1f );
                  test_7_seg( LINK_DISPLAY, 0x2f );
                  test_7_seg( LINK_DISPLAY, 0x3f );
                  test_7_seg( LINK_DISPLAY, 0x4f );
                  test_7_seg( LINK_DISPLAY, 0x5f );
                  test_7_seg( LINK_DISPLAY, 0x6f );
                  test_7_seg( LINK_DISPLAY, 0x7f );
                  test_7_seg( LINK_DISPLAY, 0x8f );
                  test_7_seg( LINK_DISPLAY, 0x9f );
                  test_7_seg( LINK_DISPLAY, 0xff );
                  Serial.printf("done.\n");

                  Serial.printf("LED_SET 2 display test...");
                  z80_bus_write( LED_SET_2, 0x01 );
                  delay( LED_TEST_DELAY );
                  z80_bus_write( LED_SET_2, 0x08 );
                  delay( LED_TEST_DELAY );
                  z80_bus_write( LED_SET_2, 0x00 );
                  Serial.printf("done.\n");

                  Serial.printf("LED_SET 1 display test...");
                  
                  for( int xxx = 1; xxx != 8; xxx++ ) {
                    z80_bus_write( LED_SET_1, xxx );
                    delay( LED_TEST_DELAY*2 );
                  }

                  for( int xxx = 1; xxx != 7; xxx++ ) {
                    z80_bus_write( LED_SET_1, xxx<<3 );
                    delay( LED_TEST_DELAY*2 );
                  }
                  
                  z80_bus_write( LED_SET_1, 0x00 );
                  Serial.printf("done.\n");

                  teensy_drives_z80_bus( false );                     // release the bus
                  break;
                
      case 'K':
      case 'k':   teensy_drives_z80_bus( true );                      // grab the bus
                  Serial.printf("Keyboard test (PRESS EACH KEY -- SEND x TO CANCEL)...\n\n");

                  while( 1 ) {
                    scan_keyboard();
                    kc = get_keycode();                               // if a key is down, return its LM-1-style keycode. returns 0xff if no key down.

                    switch( kc ) {
                      case KEY_NUM_7:         Serial.printf("got 7\n");                           break;
                      case KEY_NUM_6:         Serial.printf("got 6\n");                           break;
                      case KEY_NUM_5:         Serial.printf("got 5\n");                           break;
                      case KEY_NUM_4:         Serial.printf("got 4\n");                           break;
                      case KEY_NUM_3:         Serial.printf("got 3\n");                           break;
                      case KEY_NUM_2:         Serial.printf("got 2\n");                           break;
                      case KEY_NUM_1:         Serial.printf("got 1\n");                           break;
                      case KEY_NUM_0:         Serial.printf("got 0\n");                           break;

                      case KEY_ADJ_SHUFL:     Serial.printf("got ADJ SHFL\n");                    break;
                      case KEY_AUTO_CORR:     Serial.printf("got AUTO CORR\n");                   break;
                      case KEY_LENGTH:        Serial.printf("got LENGTH\n");                      break;
                      case KEY_ERASE:         Serial.printf("got ERASE\n");                       break;
                      case KEY_COPY:          Serial.printf("got COPY\n");                        break;
                      case KEY_REC:           Serial.printf("got RECORD\n");                      break;
                      case KEY_NUM_9:         Serial.printf("got 9\n");                           break;
                      case KEY_NUM_8:         Serial.printf("got 8\n");                           break;

                      case KEY_PLAY_STOP:     Serial.printf("got PLAY/STOP\n");                   break;
                      case KEY_DEL:           Serial.printf("got DELETE\n");                      break;
                      case KEY_INS:           Serial.printf("got INSERT\n");                      break;
                      case KEY_LAST_ENTRY:    Serial.printf("got LAST ENTRY\n");                  break;
                      case KEY_R_ARROW:       Serial.printf("got -->\n");                         break;
                      case KEY_L_ARROW:       Serial.printf("got <--\n");                         break;
                      case KEY_CHAIN_NUM:     Serial.printf("got CHAIN #\n");                     break;
                      case KEY_CHAIN_ON_OFF:  Serial.printf("got ON/OFF\n");                      break;

                      case KEY_DRM_TOM_UP:    Serial.printf("got TOM ^\n");                       break;
                      case KEY_DRM_TOM_DN:    Serial.printf("got TOM v\n");                       break;
                      case KEY_DRM_CONGA_UP:  Serial.printf("got CONGA ^\n");                     break;
                      case KEY_DRM_CONGA_DN:  Serial.printf("got CONGA v\n");                     break;
                      case KEY_LOAD:          Serial.printf("got LOAD (unused on Luma-1)\n");     break;
                      case KEY_VERIFY:        Serial.printf("got VERIFY (unused on Luma-1)\n");   break;
                      case KEY_MENU_LUMA:     Serial.printf("got MENU (SAVE on LM-1) #\n");       break;
                      case KEY_TEMPO:         Serial.printf("got DISPLAY (TEMPO)\n");             break;

                      case KEY_DRM_HIHAT_OP:  Serial.printf("got OPEN HIHAT\n");                    break;
                      case KEY_DRM_COWBELL:   Serial.printf("got COWBELL\n");                     break;
                      case KEY_DRM_HIHAT_LO:  Serial.printf("got hihat\n");                       break;
                      case KEY_DRM_HIHAT_HI:  Serial.printf("got HIHAT\n");                       break;
                      case KEY_DRM_BASS_LO:   Serial.printf("got bass\n");                        break;
                      case KEY_DRM_BASS_HI:   Serial.printf("got BASS\n");                        break;
                      case KEY_DRM_SNARE_LO:  Serial.printf("snare\n");                           break;
                      case KEY_DRM_SNARE_HI:  Serial.printf("SNARE\n");                           break;

                      case KEY_UNUSED_2F:     Serial.printf("got UNUSED KEY 2F");                 break;
                      case KEY_UNUSED_2E:     Serial.printf("got UNUSED KEY 2E\n");               break;
                      case KEY_DRM_CABASA_LO: Serial.printf("got cabasa\n");                      break;
                      case KEY_DRM_CABASA_HI: Serial.printf("got CABASA\n");                      break;
                      case KEY_DRM_TAMB_LO:   Serial.printf("got tamb\n");                        break;
                      case KEY_DRM_TAMB_HI:   Serial.printf("got TAMB\n");                        break;
                      case KEY_DRM_CLAPS:     Serial.printf("CLAPS\n");                           break;
                      case KEY_DRM_CLAVE:     Serial.printf("RIMSHOT (aka CLAVE)\n");             break;                      
                    }

                    if( Serial.available() ) {
                      while( Serial.available() ) {
                        Serial.read();
                        delay( 10 );
                      }
                      break;
                    }
                  }

                  Serial.printf("done.\n");

                  teensy_drives_z80_bus( false );                     // release the bus
                  break;

      case 'M':
      case 'm':   Serial.printf("MIDI loopback test (SEND x TO CANCEL)...");
      
                  HW_MIDI.begin( 31250, SERIAL_8N1_TXINV );         // our hardware is inverted on the transmit side only!

                  while( !Serial.available() ) {                      // send a debug char from teensy terminal to break out of loop
                    HW_MIDI.write( 0xf8 );                            // write a test byte (MIDI clock, shoudl be innocuous if other gear connected) to MIDI port
                    
                    while( !HW_MIDI.available() ) {                   // wait for it to arrive
                      if( Serial.available() )                        // or a "cancel test" byte to arrive on the teensy terminal
                        break;
                    }

                    Serial.printf("MIDI rx: %02x\n", HW_MIDI.read());
                    delay( 250 );
                  }

                  while( Serial.available() )
                    Serial.read();

                  Serial.printf("done.\n");
                  break;
                  

      case 'F':
      case 'f':   pattern_start = 0;
                  while( !end_test_time() )
                    run_fram_test();
                  break;
      

      case 'R':
      case 'r':   pattern_start = 0;
                  while( !end_test_time() )
                    run_rom_sram_test();
                  break;
                  

      case '0':   voice_test( (char*)"RIMSHOT", STB_CLAVE    );                           break;
      case '1':   voice_test( (char*)"COWBELL", STB_COWBELL  );                           break;
      case '2':   voice_test( (char*)"CONGAS",  STB_CONGAS   );                           break;
      case '3':   voice_test( (char*)"TOMS",    STB_TOMS     );                           break;
      case '4':   voice_test( (char*)"CLAPS",   STB_CLAPS    );                           break;
      case '5':   voice_test( (char*)"CABASA",  STB_CABASA   );                           break;
      case '6':   voice_test( (char*)"TAMB",    STB_TAMB     );                           break;
      case '7':   voice_test( (char*)"SNARE",   STB_SNARE    );                           break;
      case '8':   voice_test( (char*)"BASS",    STB_BASS     );                           break;
      case '9':   voice_test( (char*)"HIHAT",   STB_HIHAT    );                           break;

      case '!':   voice_test_data = vtest_256;  voice_test_data_len = sizeof(vtest_256);          
                  Serial.printf("Wave: %d bytes\n", voice_test_data_len);                 break;
      case '@':   voice_test_data = vtest_512;  voice_test_data_len = sizeof(vtest_512);          
                  Serial.printf("Wave: %d bytes\n", voice_test_data_len);                 break;
      case '$':   voice_test_data = vtest_1024;  voice_test_data_len = sizeof(vtest_1024);          
                  Serial.printf("Wave: %d bytes\n", voice_test_data_len);                 break;

      case '%':   voice_test_sample_len = 2048;    Serial.printf("Sample: 2048 bytes\n"); break;
      case '^':   voice_test_sample_len = 4096;    Serial.printf("Sample: 4096 bytes\n"); break;
      case '&':   voice_test_sample_len = 8192;    Serial.printf("Sample: 8192 bytes\n"); break;
      case '*':   voice_test_sample_len = 32768;   Serial.printf("Sample: 32768 bytes\n"); break;


      case 'B':
      case 'b':   pattern_start = 0;
                  while(1) {
                    Serial.printf("\n\n===== VOICE TESTS ===\n\n");
                    
                    run_voice_test( (char*)"CONGAS",  STB_CONGAS   );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"TOMS",    STB_TOMS     );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"SNARE",   STB_SNARE    );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"BASS",    STB_BASS     );
                    if( end_test_time() )
                      break;
                      
                    run_voice_test( (char*)"HIHAT",   STB_HIHAT    );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"COWBELL", STB_COWBELL  );
                    if( end_test_time() )
                      break;
                    
                    run_voice_test( (char*)"CLAPS",   STB_CLAPS    );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"RIMSHOT", STB_CLAVE    );
                    if( end_test_time() )
                      break;

                    run_voice_test( (char*)"TAMB",    STB_TAMB     );
                    if( end_test_time() )
                      break;
                      
                    run_voice_test( (char*)"CABASA",  STB_CABASA   );
                    if( end_test_time() )
                      break;

                    Serial.printf("\n\n===== FRAM TEST ===\n\n");

                    run_fram_test();
                    if( end_test_time() )
                      break;            

                    Serial.printf("\n\n===== ROM SRAM TEST ===\n\n");

                    run_rom_sram_test();
                    if( end_test_time() )
                      break;   
                  }
                  break;

                  
      
      case '?':
      default:    Serial.printf("=== Debug / Diag Commands ===\n\n");

                  Serial.printf("s <8 ASCII chars>        Set Serial Number\n");
                  Serial.printf("\n");

                  Serial.printf("l                        LED Test\n");
                  Serial.printf("k                        Keyboard Test\n");
                  Serial.printf("m                        MIDI Loopback Test\n");

                  Serial.printf("\n -- Z-80 memory tests --\n");
                  Serial.printf("f                        FRAM Test\n");
                  Serial.printf("r                        ROM SRAM Test\n");

                  Serial.printf("\n -- voice card tests --\n");
                  Serial.printf("0                        RIMSHOT Voice Test\n");
                  Serial.printf("1                        COWBELL Voice Test\n");
                  Serial.printf("2                        CONGA   Voice Test\n");
                  Serial.printf("3                        TOM     Voice Test\n");
                  Serial.printf("4                        CLAPS   Voice Test\n");
                  Serial.printf("5                        CABASA  Voice Test\n");
                  Serial.printf("6                        TAMB    Voice Test\n");
                  Serial.printf("7                        SNARE   Voice Test\n");
                  Serial.printf("8                        BASS    Voice Test\n");
                  Serial.printf("9                        HIHAT   Voice Test\n");
                  Serial.printf("\n");
                  Serial.printf("!                        Use  256 byte wave\n");
                  Serial.printf("@                        Use  512 byte wave\n");
                  Serial.printf("$                        Use 1024 byte wave\n");
                  Serial.printf("\n");
                  Serial.printf("%%                        Set voice to 2 KB\n");
                  Serial.printf("^                        Set voice to 4 KB\n");
                  Serial.printf("&                        Set voice to 8 KB\n");
                  Serial.printf("*                        Set voice to 32 KB (16 KB for CONGA and TOM)\n");
                  Serial.printf("\n");
                  Serial.printf("b                        Burn-in test: all voices, then FRAM, then ROM SRAM, then repeat\n");
                  break;
      
    }

  }
}
