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

#ifndef LM_LUI_H_
#define LM_LUI_H_

/* ---------------------------------------------------------------------------------------
    Local UI

    Enter Local UI mode by pressing the STORE key
    Then we wait for a 2-digit command: <- key undoes digit entries, his PLAY/STOP once 2 digits entered
    Then some commands with gather parameters (2-digit number, drum key)

    For the local UI, common routine to get 2 BCD digits from the user.
    Backspace is honored.
    Pressing STORE again will cancel entry and return FALSE.
    Pressing PLAY/STOP will accept entry (if 2 digits have been entered) and return TRUE.

*/

bool draw_lui_display();                              // called by OLED code to draw current LUI display

bool lui_is_active();

extern bool forced_local_ui_mode;                     // remember if we are forced in, don't let z-80 run until cmd 99 (reboot)

#define LUI_INACTIVE                0                 // not in Local UI mode
#define LUI_GET_CMD                 1                 // entered Local UI mode (STORE KEY), awaiting command
#define LUI_GOT_CMD                 2
#define LUI_GET_VAL                 3
#define LUI_GOT_VAL                 4
#define LUI_CMD_COMPLETE            5

#define CURSOR_BLINK_RATE           150               // milliseconds

#define CMD_INVALID                 0xff              // init value before a command has been entered

/* ---------------------------------------------------------------------------------------
    Local UI, entered with STORE button OR if memory card error on boot
 */

void handle_local_ui();

bool in_local_ui();

void draw_info_screen( uint8_t c );                   // OLED drawing routine for DISPLAY mode


/* ---------------------------------------------------------------------------------------
    Local UI ACTIONS
 */

char *voice_bank_name( uint8_t bank );
char *pattern_bank_name( uint8_t bank );

bool logic_ui_eprom_dump( uint8_t lastkey );
bool display_ui_eprom_dump();



/* ---------------------------------------------------------------------------------------
    Local UI INFO
 */

void draw_serial_version_screen();

void draw_voice_bank_info_screen();

void draw_drum_bitmap();

void input_drum_bitmap( uint8_t keycode );


/* ---------------------------------------------------------------------------------------
    Voice Operations
 */
 
#define VOP_SEL_COPY          0
#define VOP_SEL_REVERSE       1


#endif
