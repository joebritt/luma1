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

#include "LM_OLED.h"
#include "LM_OLED_Images.h"

#include "LM_LUI.h"


bool display_found = false;

elapsedMillis show_bootscreen_time;

elapsedMillis oled_frame_time;              // just redraw every 16ms

int oled_frame_cnt = 0;                     // counts continuously, and wraps, roughly every 745 days, shouldn't matter

void luma_oled_display(void);                 // more time-sensitive replacement for display.display()


/* ---------------------------------------------------------------------------------------
    i2c OLED Display

    128x64 pixel monochrome display

    SH1106 controller, i2c address 0x3c
*/


bool setup_oled_display() {

  delay(250);                                 // wait for the OLED to power up
  
  display_found = display.begin( DISP_I2C_ADDR, true );

  oled_frame_time = 0;

  if( display_found ) {
    Serial.println("Initializing luma_oled_display() i2c");
    
    // crank up i2c
    
    pinMode( I2C_SDA_1, OUTPUT );             // first drive both lines low
    pinMode( I2C_SCL_1, OUTPUT );
    digitalWrite( I2C_SDA_1, LOW );
    digitalWrite( I2C_SCL_1, LOW );
    
    Wire1.begin();

    Wire1.setClock(1000000);                  // 1 MHz i2c clock to display
  }
  
  return display_found;
}


uint16_t getTextWidth( char *s ) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}


char systxt[256];

void display_oled_bootscreen() {
  display.clearDisplay();

  display.drawBitmap( 0, 0, boot_logos[eeprom_load_boot_screen_select()], 128, 64, 1 );

  display.setFont();
  display.setTextColor( 1 );
  display.setTextSize( 1 );
  
  display.setCursor( 1, 55 );
  sprintf( systxt, "SN %s", serial_number );
  display.print( systxt );

  sprintf( systxt, "v%d.%d", LUMA1_FW_VERSION_MAJOR, LUMA1_FW_VERSION_MINOR );
  display.setCursor( 128-getTextWidth(systxt), 55 );
  display.print( systxt );

  luma_oled_display();                          // optimized display output routine
//  display.display();                          // --> don't use stock one

  bootscreen_showing = true;
  show_bootscreen_time = 0;       // start bootscreen dwell timer
}


// total hack to convert 8bpp PISKEL sprites into 1bpp bitmaps that the graphics library can use

uint8_t ram_bm[16][2];

uint8_t *cvt_bm( uint32_t *bm ) {
  uint32_t h;
  uint8_t o;
  
  for( int yyy = 0; yyy != 16; yyy++ ) {
    
    o = 0;
    
    for( int xxx = 0; xxx != 8; xxx++ ) {
      h = *bm++;
      if( h == 0xff000000 )
        o |= 1;
      if( xxx < 7 ) o <<= 1;
    }

    ram_bm[yyy][0] = o;

    o = 0;
    
    for( int xxx = 0; xxx != 8; xxx++ ) {
      h = *bm++;
      if( h == 0xff000000 )
        o |= 1;
      if( xxx < 7 ) o <<= 1;
    }

    ram_bm[yyy][1] = o;
  }

  return (uint8_t *)ram_bm;
}


elapsedMillis fan_anim_time = 0;

#define FAN_ANIM_FRAME_TIME     250

char fans[4] = { '|', '/', '-', '\\' };
int fan_frame = 0;

void draw_sys_display() {
  display.fillRect( 0, 0, 128, 10, SH110X_BLACK );

  display.setFont();
  display.setTextSize( 1 );

  // -- Draw MIDI info
  
  display.setCursor( 1, 0 );
  if( midi_chan == 0 )
    sprintf( systxt, "MIDI:ALL" );
  else
    sprintf( systxt, "MIDI:%02d", midi_chan );
  display.print( systxt );

  if( midi_din_out_active() ) display.drawChar( 59, 0, 0x18, SH110X_WHITE, SH110X_BLACK, 1 );
  if( midi_din_in_active() )  display.drawChar( 66, 0, 0x19, SH110X_WHITE, SH110X_BLACK, 1 );

  display.setCursor( 73, 0 );
  display.print( "DIN" );

  if( midi_usb_out_active() ) display.drawChar( 116, 0, 0x18, SH110X_WHITE, SH110X_BLACK, 1 );
  if( midi_usb_in_active() )  display.drawChar( 123, 0, 0x19, SH110X_WHITE, SH110X_BLACK, 1 );

  display.setCursor( 97, 0 );
  display.print( "USB" );

  display.drawLine( 93, 10, 93, 0, SH110X_WHITE );
  



  // -- top section dividing line
  
  display.drawLine( 0, 10, 128, 10, SH110X_WHITE );


 // display.drawBitmap(64, 48, cvt_bm(&clock_icons[0][0]), 16, 16, SH110X_WHITE);

  // -- if not in local UI mode, draw other status
  
  if( !lui_is_active() ) {
    display.setCursor( 1, 55 );
    switch( get_fan_mode() ) {
      case FAN_AUTO:  display.print("FAN AUTO");    break;
      case FAN_ON:    display.print("FAN ON");      break;
      case FAN_OFF:   display.print("FAN OFF");     break;
    }
  
    if( fan_running() ) {
      display.setCursor( 40, 55 );
      display.print( fans[fan_frame] );    
      if( fan_anim_time >= FAN_ANIM_FRAME_TIME ) {
        fan_frame++; if( fan_frame > 3 ) fan_frame = 0;
        fan_anim_time = 0;
      }
    }
    else {
      fan_anim_time = 0;
      fan_frame = 0;
    }
  }  
}



/*
  If we are showing the bootscreen, leave it up until the dwell time has passed OR the user hit a key.

  If we are running normally, repaint the OLED every 16ms (60Hz).
*/

void handle_oled_display() {

  if( display_found == true ) {
    if( bootscreen_showing ) {
      if( show_bootscreen_time >= BOOTSCREEN_DWELL_MS )
        bootscreen_showing = false;
    }
    else {
      if( oled_frame_time > MS_PER_OLED_FRAME ) {
        oled_frame_time = 0;
        oled_frame_cnt++;
        
        display.clearDisplay();         // just memset()s the offscreen buffer to 0
      
        if( draw_lui_display() )        // draw local UI, return true if we should draw system display
          draw_sys_display();           // draw system parts

        luma_oled_display();            // optimized display output routine, send bitmap to OLED
        //display.display();            // --> don't use stock one, affects MIDI performance
      }
    }
  }
}



/*
  We use the Adafruit library to initialize the OLED display, and we use its drawing primitives to paint into the framebuffer.

  However, the Adafruit library display.display() method is quite slow, and it blocks while sending data to the display over i2c.

  Luma-1 has its own display routine which:
  
    - sends the offscreen framebuffer to the OLED more quickly
    - calls loop_time_critical() 8 times while painting the display to ensure that time-critical operations like MIDI are not delayed

  NOTE:
        The Adafruit library is very general-purpose, supporting multiple display controllers and resolutions.
        This code is ONLY for an SH1106(G) display with 128x64 pixels.
*/

#define WIDTH               128
#define HEIGHT              64

#define MAX_I2C_BUF         32

#define SH1106G_COL_FUDGE   2                           // from Adafruit driver

void luma_oled_display(void) {
  uint8_t *buf = display.getBuffer();
  uint8_t *ptr;
  
  for (int p = 0; p < 8; p++) {                         // there are 8 pages
    
    ptr = buf + (p*128);                                // offset into our frame buffer for this page

    /*
        A COMMAND WORD consists of: a control byte, which defines Co and D/C (note1), plus a data byte.
        
        The last control byte is tagged with a cleared most significant bit, Co = 0 
        After a control byte with a cleared Co-bit, data bytes will follow

        bit 7 = Co:   O -> The last control byte , only data bytes follow
                      1 -> Next two bytes are a data byte and another control byte

        bit 6 = D/C:  0 -> COMMAND byte
                      1 -> DATA byte
    */

    uint8_t cmd[] = {
        0x00,                                           // control byte (special value, Co = 0, D/C = 0)
        (uint8_t)( SH110X_SETPAGEADDR + p ),            // set page addr, this cmd byte is 0xBp, where p = page. 1011 pppp -> Co = 1, D/C = 0
        (uint8_t)( 0x10 + (0 >> 4) ),                   // set hi nybble of col
        (uint8_t)( 0x00 + (SH1106G_COL_FUDGE & 0xF))    // set lo bybble of col
        };


    // Send the cmd to set the page
    
    Wire1.beginTransmission( DISP_I2C_ADDR );    
    Wire1.write( cmd, 4 );
    Wire1.endTransmission();

    // Send a page worth of data

/*
    Wire1.beginTransmission( DISP_I2C_ADDR );           // Send a dummy column to get to the active pixel area
    Wire1.write( 0x40 );                                // D/C = 1 -> sending DATA (not command)
    Wire1.write( 0 );
    Wire1.endTransmission();
*/

    for( int xxx = 0; xxx != (128/16); xxx++ ) {
      Wire1.beginTransmission( DISP_I2C_ADDR );    
      Wire1.write( 0x40 );                              // D/C = 1 -> sending DATA (not command)
      Wire1.write( ptr, 16 );
      Wire1.endTransmission();

      ptr += 16;
  
      loop_time_critical();                             // i2c writes can take a long time
    }
    
  }
}
