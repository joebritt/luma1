
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

#ifndef LM_Z80Keys_H_
#define LM_Z80Keys_H_


#define KB_ROW_1          0xdc01
                                          // Row 1's bitmap
#define   KEY_NUM_7         0x07
#define   KEY_NUM_6         0x06
#define   KEY_NUM_5         0x05
#define   KEY_NUM_4         0x04
#define   KEY_NUM_3         0x03
#define   KEY_NUM_2         0x02
#define   KEY_NUM_1         0x01
#define   KEY_NUM_0         0x00

#define KB_ROW_2          0xdc02
                                          // Row 2's bitmap
#define   KEY_ADJ_SHUFL     0x0f        
#define   KEY_AUTO_CORR     0x0e
#define   KEY_LENGTH        0x0d
#define   KEY_ERASE         0x0c
#define   KEY_COPY          0x0b
#define   KEY_REC           0x0a
#define   KEY_NUM_9         0x09
#define   KEY_NUM_8         0x08

#define KB_ROW_3          0xdc04
                                          // Row 3's bitmap
#define   KEY_PLAY_STOP     0x17        
#define   KEY_DEL           0x16
#define   KEY_INS           0x15
#define   KEY_LAST_ENTRY    0x14
#define   KEY_R_ARROW       0x13
#define   KEY_L_ARROW       0x12
#define   KEY_CHAIN_NUM     0x11
#define   KEY_CHAIN_ON_OFF  0x10

#define KB_ROW_4          0xdc08
                                          // Row 4's bitmap
#define   KEY_DRM_TOM_UP    0x1f        
#define   KEY_DRM_TOM_DN    0x1e
#define   KEY_DRM_CONGA_UP  0x1d
#define   KEY_DRM_CONGA_DN  0x1c
#define   KEY_LOAD          0x1b          // UNUSED on Luma-1
#define   KEY_VERIFY        0x1a          // UNUSED on Luma-1
//#define   KEY_SAVE          0x19
#define   KEY_MENU          0x19          // SAVE/STORE repurposed for MENU on Luma-1
#define   KEY_TEMPO         0x18

#define KB_ROW_5          0xdc10
                                          // Row 5's bitmap
#define   KEY_DRM_HIHAT_OP  0x27        
#define   KEY_DRM_COWBELL   0x26
#define   KEY_DRM_HIHAT_LO  0x25
#define   KEY_DRM_HIHAT_HI  0x24
#define   KEY_DRM_BASS_LO   0x23
#define   KEY_DRM_BASS_HI   0x22
#define   KEY_DRM_SNARE_LO  0x21
#define   KEY_DRM_SNARE_HI  0x20

#define KB_ROW_6          0xdc20
                                          // Row 6's bitmap
#define   KEY_UNUSED_2F     0x2f        
#define   KEY_UNUSED_2E     0x2e
#define   KEY_DRM_CABASA_LO 0x2d
#define   KEY_DRM_CABASA_HI 0x2c
#define   KEY_DRM_TAMB_LO   0x2b
#define   KEY_DRM_TAMB_HI   0x2a
#define   KEY_DRM_CLAPS     0x29
#define   KEY_DRM_CLAVE     0x28


void init_keyboard();
void scan_keyboard();

uint8_t get_keycode();

#endif
