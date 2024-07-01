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


#include "LM_LUI.h"
#include "LM_OLED.h"
#include "LM_Voices.h"
#include "LM_Utilities.h"

void draw_serial_version_screen();

/* ================================================================================================================   
    OLED drawing routine for DISPLAY mode
*/

void draw_info_screen( uint8_t c ) {
  switch( c ) {
    case 00:
    case CMD_INVALID: draw_drum_bitmap();                         break;

    case 01:          draw_voice_bank_info_screen();              break;

    case 02:          draw_pattern_bank_info_screen();            break;

    case 03:          draw_serial_version_screen();               break;
  }
}


/* ================================================================================================================
   Show serial number and firmware version
*/

void draw_serial_version_screen() {
  display.setTextSize( 1 );
  
  display.setCursor( 10, 0 );
  display.print( "Serial Number" );

  display.setCursor( 10, 15 );
  sprintf( systxt, "%s", serial_number );
  display.print( systxt );

  display.drawLine( 0, 30, 128, 30, SH110X_WHITE );
  
  display.setCursor( 10, 40 );
  display.print( "Luma-1 Firmware" );

  display.setCursor( 10, 55 );
  sprintf( systxt, "v%d.%d", LUMA1_FW_VERSION_MAJOR, LUMA1_FW_VERSION_MINOR );
  display.print( systxt );
}

/* ================================================================================================================
   Show current Voice Bank name
*/

void draw_voice_bank_info_screen() {
  display.setTextSize( 1 );
  
  display.setCursor( 10, 0 );
  display.print( "Current Voice Bank");

  display.drawLine( 0, 10, 128, 10, SH110X_WHITE );

  display.setCursor( 10, 30 );
  display.print( "Voice Bank # " );
  display.setCursor( 85, 30 );
  sprintf( systxt, "%02d", get_orig_bank_num() );
  display.print( systxt );

  display.setCursor( 10, 45 );
  display.print( get_cur_bank_name() );
}


/* ================================================================================================================
   Show current Pattern Bank name
*/

void draw_pattern_bank_info_screen() {
  display.setTextSize( 1 );
  
  display.setCursor( 10, 0 );
  display.print( "Current Patterns");

  display.drawLine( 0, 10, 128, 10, SH110X_WHITE );

  display.setCursor( 10, 30 );
  display.print( "- Coming soon -" );
}




/* ================================================================================================================
   Manage Drum Bitmap

   This is used to select which drum(s) to load, from SD or from EPROM.
   
*/

bool drum_bitmap_changed = false;

bool show_drum_bitmap = false;
elapsedMillis show_bitmap_count;
#define BITMAP_SHOW_MS          3000      // hold for 3s


  
void drum_bm_toggle( uint16_t state, uint16_t strobe ) {
  //Serial.print(state); Serial.print(" - "); Serial.println(strobe, HEX );

  disable_drum_trig_interrupt();
  
  if( state ) {
      trig_voice( strobe, 0x00 );
      trig_voice( strobe, 0x01 );
  }
  else {
      trig_voice( strobe, 0x00 );
      trig_voice( strobe, 0x01 );

      delay( 100 );

      trig_voice( strobe, 0x00 );
      trig_voice( strobe, 0x01 );
  }

  restore_drum_trig_interrupt();
}


void input_drum_bitmap( uint8_t keycode ) {
  switch( keycode ) {
    case KEY_DRM_CONGA_UP:
    case KEY_DRM_CONGA_DN:    voice_load_bm ^= BANK_LOAD_CONGAS;      drum_bitmap_changed = true;
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_CONGAS), STB_CONGAS );
                              break;
    
    case KEY_DRM_TOM_UP:
    case KEY_DRM_TOM_DN:      voice_load_bm ^= BANK_LOAD_TOMS;        drum_bitmap_changed = true;            
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_TOMS), STB_TOMS );
                              break;

    case KEY_DRM_SNARE_LO:
    case KEY_DRM_SNARE_HI:    voice_load_bm ^= BANK_LOAD_SNARE;       drum_bitmap_changed = true;
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_SNARE), STB_SNARE );
                              break;

    case KEY_DRM_BASS_LO:                              
    case KEY_DRM_BASS_HI:     voice_load_bm ^= BANK_LOAD_BASS;        drum_bitmap_changed = true;        
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_BASS), STB_BASS );
                              break;

    case KEY_DRM_HIHAT_OP:                              
    case KEY_DRM_HIHAT_LO:                              
    case KEY_DRM_HIHAT_HI:    voice_load_bm ^= BANK_LOAD_HIHAT;       drum_bitmap_changed = true;     
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_HIHAT), STB_HIHAT );
                              break;
                              
    case KEY_DRM_COWBELL:     voice_load_bm ^= BANK_LOAD_COWBELL;     drum_bitmap_changed = true;    
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_COWBELL), STB_COWBELL );
                              break;
    case KEY_DRM_CLAPS:       voice_load_bm ^= BANK_LOAD_CLAPS;       drum_bitmap_changed = true;    
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_CLAPS), STB_CLAPS );
                              break;
    case KEY_DRM_CLAVE:       voice_load_bm ^= BANK_LOAD_CLAVE;       drum_bitmap_changed = true;    
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_CLAVE), STB_CLAVE );
                              break;

    case KEY_DRM_TAMB_LO:                              
    case KEY_DRM_TAMB_HI:     voice_load_bm ^= BANK_LOAD_TAMB;        drum_bitmap_changed = true;    
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_TAMB), STB_TAMB );
                              break;

    case KEY_DRM_CABASA_LO:                              
    case KEY_DRM_CABASA_HI:   voice_load_bm ^= BANK_LOAD_CABASA;      drum_bitmap_changed = true;    
                              drum_bm_toggle( (voice_load_bm & BANK_LOAD_CABASA), STB_CABASA );
                              break;
  }

  //Serial.print("Bitmap: "); printHex4( voice_load_bm ); Serial.println();
}


#define DRM_BTN_YPOS                50
#define DRM_BTN_WIDTH               13
#define DRM_LABEL_LETTER_HEIGHT     8


void print_str_vertical( uint16_t x, uint16_t y, char *s ) {
  while( *s ) {
    display.drawChar( x, y, *s, SH110X_WHITE, SH110X_BLACK, 1 );
    y += DRM_LABEL_LETTER_HEIGHT;
    s++;
  }
}

void draw_drum_button( uint16_t x, uint16_t y, bool en ) {
  display.fillRoundRect( x, y, 10, 10, 3, en ? SH110X_WHITE : SH110X_BLACK );
}


void draw_drum_bitmap() {
  uint16_t xPos, yPos;

  display.setFont();
 
  xPos = 2;
  yPos = 1;
  print_str_vertical( xPos, yPos, (char*)"CONGA" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_CONGAS );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"TOM" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_TOMS );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"SNARE" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_SNARE );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"BASS" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_BASS );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"HIHAT" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_HIHAT );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"COWBEL" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_COWBELL );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"CLAPS" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_CLAPS );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"RIMSHT" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_CLAVE );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"TAMB" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_TAMB );

  xPos += DRM_BTN_WIDTH;
  print_str_vertical( xPos, yPos, (char*)"CABASA" );
  draw_drum_button( xPos-2, DRM_BTN_YPOS, voice_load_bm & BANK_LOAD_CABASA );
}
