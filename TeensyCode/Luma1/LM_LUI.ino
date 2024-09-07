/* ================================================================================================================
    Use LM-1 Keyboard & LINK/PATT LEDs for Local UI

    MENU (LM-1 STORE): Enter/Exit Local UI Mode

      --> Enters into "Settings" Mode. Press DISPLAY to switch to "Info" Mode.

    PLAY/STOP: Like an "enter" key, accept Command or Param entered

    0-9: Used to enter Command and Parameter values

    Left Arrow: Show previous valid Command or Param

    Right Arrow: Show next valid Command or Param

    Drum Keys: Toggle that drum on/off for loading/saving
               When reading EPROM, select target drum voice

    DISPLAY: Toggle between Settings and Info modes.

    If you manually enter an invalid command, it does nothing.
    If you manually enter an invalid value, it rejects it and lets you enter it again.

    OLED display code calls draw_lui_display() (below) to draw the appropriate OLED screen for the current LUI state.



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
#include "LM_Utilities.h"


uint16_t  local_ui_state = LUI_INACTIVE;

bool forced_local_ui_mode = false;                  // remember if we are forced in, don't let z-80 run until cmd 99 (reboot)


// use this to decide which LUI UI to draw

#define LUI_DM_ACTION           0
#define LUI_DM_INFO             1

int lui_display_mode = LUI_DM_ACTION;


elapsedMillis cursor_blink_cnt;
bool cursor_on;

uint8_t entered_num;
uint8_t num_digits_entered;

uint8_t cmd;
uint8_t val;

#define INPUT_DIGITS_PENDING    0
#define INPUT_DIGITS_COMPLETE   1
#define INPUT_DIGITS_CANCEL     2

void input_digits_init();
uint8_t input_digits( uint8_t *digits );


bool lui_is_active() { return local_ui_state != LUI_INACTIVE; }


/* ================================================================================================================
    Draw Local UI on OLED

    What gets drawn depends on the LUI state machine
    
    LUI_INACTIVE

    LUI_GET_CMD
      Echoes PATT display, shows Command description

    LUI_GOT_CMD
    LUI_GET_VAL
      Shows Command description
      Echoes LINK display, shows info about the proposed VAL (e.g., for sound banks, the description, for RAM banks, the checksum)

    Touching a Drum key toggles that key on/off in the drum selection bitmap.
    This causes the display to change to the Drum Bitmap display with the latest status.
    That status display is shown for 3 seconds, then times out back to the previous display (Command or Val entry).
    
*/

#define FLASH_SLOW          (1000/MS_PER_OLED_FRAME*2))
#define FLASH_FAST          (500/(MS_PER_OLED_FRAME*2))
#define FLASH_VERY_FAST     (300/(MS_PER_OLED_FRAME*2))

int flashrate_t = 0;                        // 0 = no flash
int flashrate_m = 0;
int flashrate_b = 0;

int flasher_top = 0;
int flasher_middle = 0;
int flasher_bottom = 0;

void show_top_banner( char *s );            // UI helper, shows text below system UI
void flashrate_top( int rate );

void show_middle_banner( char *s );         // UI helper, shows text in middle of screen
void flashrate_middle( int rate );

void show_bottom_banner( char *s );         // UI helper, shows text at bottom of screen
void flashrate_bottom( int rate );

void show_beside_2digits( char *s );        // UI helper, shows text beside 2-digit cmd/val input field

uint8_t min_valid_val;
uint8_t max_valid_val;

char oled_text_buf[256];

char *lui_cmd_str;
char *lui_cmd_desc;

char *lui_actions( uint8_t cmd ) {
  switch( cmd ) {
    case 0x00:  return (char*)"Load Voices";
    case 0x01:  return (char*)"Store Voices";
    case 0x02:  return (char*)"Load Patterns";
    case 0x03:  return (char*)"Store Patterns";
    case 0x10:  return (char*)"Copy Voice";
    case 0x11:  return (char*)"Reverse Voice";
    case 0x55:  return (char*)"EPROM Dump";
    case 0x66:  return (char*)"Fan Control";
    case 0x70:  return (char*)"Send Sample";
    case 0x71:  return (char*)"Send Sample Bank";    
    case 0x72:  return (char*)"Send Patterns";
    case 0x73:  return (char*)"Send Patt Bank";
    case 0x80:  return (char*)"MIDI Channel";
    case 0x81:  return (char*)"MIDI Notes OUT";
    case 0x82:  return (char*)"MIDI Notes IN";
    case 0x83:  return (char*)"MIDI Clock OUT";
    case 0x84:  return (char*)"MIDI Clock IN";
    case 0x85:  return (char*)"MIDI SysEx";
    case 0x86:  return (char*)"MIDI Soft Thru";
    case 0x87:  return (char*)"MIDI Start Enabl";
    case 0x88:  return (char*)"MIDI Send Vel";
    case 0x89:  return (char*)"MIDI SysEx Delay";
    case 0x90:  return (char*)"Boot Screen";
    case 0x97:  return (char*)"SD Dir";
    case 0x98:  return (char*)"Format SD";
    case 0x99:  return (char*)"Reboot";

    default:    return (char*)"Unused";
  }
}



char *lui_infos( uint8_t cmd ) {
  switch( cmd ) {
    case 0x00:  return (char*)"Drum Bitmap";
    case 0x01:  return (char*)"Versions";
    case 0x02:  return (char*)"Bank Name";
    case 0x03:  return (char*)"Patterns Name";

    default:    return (char*)"Unused";
  }
}


void valid_range( uint8_t lo, uint8_t hi ) {
  min_valid_val = lo;
  max_valid_val = hi;
}

uint8_t max_val_check( uint8_t v ) {
  return (v > max_valid_val) ? max_valid_val : v;
}


uint8_t next_valid_cmd( uint8_t c ) {
  do {
    if( (c & 0x0f) == 0x09 ) {                                                  // time to increment the left digit?
      c = (c & 0xf0) + 0x10;
      if( (c & 0xf0) > 0x90 )
        c = 0x00;                                                               // 99, roll over to 00
    } else {
      c = (c & 0xf0) + ((((c & 0x0f) + 1)>0x09) ? 0x00 : ((c & 0x0f) + 1));     // preserve left digit, increment right digit, roll over after 9
    }    
  } while( strcmp( (lui_display_mode == LUI_DM_ACTION)?lui_actions( c ):lui_infos(c), "Unused" ) == 0 );

  return c;
}


uint8_t next_valid_val( uint8_t c ) {
  if( (c & 0x0f) == 0x09 ) {                                                  // time to increment the left digit?
    c = (c & 0xf0) + 0x10;
    if( (c & 0xf0) > 0x90 )
      c = 0x00;                                                               // 99, roll over to 00
  } else {
    c = (c & 0xf0) + ((((c & 0x0f) + 1)>0x09) ? 0x00 : ((c & 0x0f) + 1));     // preserve left digit, increment right digit, roll over after 9
  }    

  if( bcd2dec( c ) > max_valid_val ) {
    c = 0;
  }
    
  return c;
}


uint8_t prev_valid_cmd( uint8_t c ) {
  do {
    if( (c & 0x0f) == 0x00 ) {                                                  // right digit == 0, so need to decrement left digit and make right digit 9
      
      if( (c & 0xf0) == 0x00 )
        c = 0x90;                                                               // left digit was 0, make it 9
      else
        c = (c & 0xf0) - 0x10;                                                  // left digit was 1-9, decrement it

      c += 0x09;                                                                // right digit was 0, decremented left digit, so right digit goes to 9
      
    } else {                                                                    // not time to decrement left digit
      c = c - 0x01;                                                             // right digit != 0, so must be 1-9, so we can just decrement right digit      
    }
  } while( strcmp( (lui_display_mode == LUI_DM_ACTION)?lui_actions(c):lui_infos(c), "Unused" ) == 0 );

  return c;
}


uint8_t prev_valid_val( uint8_t c ) {
  if( (c & 0x0f) == 0x00 ) {                                                  // right digit == 0, so need to decrement left digit and make right digit 9
    
    if( (c & 0xf0) == 0x00 )
      c = 0x90;                                                               // left digit was 0, make it 9
    else
      c = (c & 0xf0) - 0x10;                                                  // left digit was 1-9, decrement it

    c += 0x09;                                                                // right digit was 0, decremented left digit, so right digit goes to 9
    
  } else {                                                                    // not time to decrement left digit
    c = c - 0x01;                                                             // right digit != 0, so must be 1-9, so we can just decrement right digit      
  }  

  if( bcd2dec( c ) > max_valid_val ) {
    c = dec2bcd(max_valid_val);
  }
    
  return c;
}



  
/* ================================================================================================================
*/

// !!! by this point, the entered BCD command has been converted to decimal

char *lui_val_info( uint8_t cmd, uint8_t val, bool val_is_valid ) {
  switch( cmd ) {
    case 00:  if( val_is_valid ) show_top_banner( voice_bank_name( val ) );
              return (char*)"Load Bank #";
              break;
    
    case 01:  if( val_is_valid ) show_top_banner( voice_bank_name( val ) );
              return (char*)"Store Bank #";
              break;
    
    case 02:  if( val_is_valid ) show_top_banner( pattern_bank_name( val ) );
              return (char*)"Load Patterns #";
              break;
    
    case 03:  if( val_is_valid ) show_top_banner( pattern_bank_name( val ) );
              return (char*)"Store Patterns #";
              break;
    
    case 55:  show_top_banner( (char*)"Select EPROM Type" );
              return handle_val_eprom_size( val, val_is_valid );
              break;
              
    case 66:  show_top_banner( (char*)"Choose Fan Mode" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"AUTO";
                  case 01:  return (char*)"ON";
                  case 02:  return (char*)"OFF";
                }
              }
              break;

    case 71:  if( val_is_valid ) show_top_banner( voice_bank_name( val ) );
              return (char*)"Send Voice Bank";
              break;
              
    case 73:  if( val_is_valid ) show_top_banner( pattern_bank_name( val ) );
              return (char*)"Send Patt Bank";
              break;
    
    case 80:  show_top_banner( (char*)"Choose MIDI Channel" );
              return (char*)"00 is OMNI";
              break;

    case 81:  show_top_banner( (char*)"MIDI Notes OUT" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"DIN-5";
                  case 01:  return (char*)"USB";
                  case 02:  return (char*)"DIN-5 & USB";
                }
              }
              break;

    case 82:  show_top_banner( (char*)"MIDI Notes IN" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"DIN-5";
                  case 01:  return (char*)"USB";
                  case 02:  return (char*)"DIN-5 & USB";
                }
              }
              break;

    case 83:  show_top_banner( (char*)"MIDI Clock OUT" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"DIN-5";
                  case 01:  return (char*)"USB";
                  case 02:  return (char*)"DIN-5 & USB";
                }
              }
              break;

    case 84:  show_top_banner( (char*)"MIDI Clock IN " );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"DIN-5";
                  case 01:  return (char*)"USB";
                }
              }
              break;

    case 85:  show_top_banner( (char*)"MIDI Sysex " );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"DIN-5";
                  case 01:  return (char*)"USB";
                }
              }
              break;

    case 86:  show_top_banner( (char*)"MIDI Soft Thru " );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"off";
                  case 01:  return (char*)"ON";
                }
              }
              break;

    case 87:  show_top_banner( (char*)"MIDI Start Enable " );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"disabled";
                  case 01:  return (char*)"ENABLED";
                }
              }
              break;

    case 88:  show_top_banner( (char*)"MIDI Send Velocity" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"disabled";
                  case 01:  return (char*)"ENABLED";
                }
              }
              break;

    case 89:  show_top_banner( (char*)"MIDI SysEx Delay" );
              return (char*)"ms";
              break;
              
    case 90:  show_top_banner( (char*)"Choose Boot Screen" );
              return (char*)"Screen";
              break;

    case 99:  show_top_banner( (char*)"Reset all settings?" );
              if( val_is_valid ) {
                switch( val ) {
                  case 00:  return (char*)"no";
                  case 01:  return (char*)"YES";
                }
              }
              break;

    default:    Serial.println( cmd, HEX );
                return (char*)"Unused";
  }

  return (char*)"";
}



bool oled_show_2digits() {            // show 2 digits as they are entered, when 2 are present return TRUE (indicating ok to print info text)
  bool show_info_str = false;
  
  display.setFont();
  display.setCursor( 1, 35 );
  display.setTextSize( 2 );
  
  switch( num_digits_entered ) {
    case 0: oled_text_buf[0] = '_';
            oled_text_buf[1] = '_';
            oled_text_buf[2] = 0;
            display.print( oled_text_buf );
            break;
            
    case 1: oled_text_buf[0] = (entered_num >> 4) + 0x30;
            oled_text_buf[1] = '_';
            oled_text_buf[2] = 0;
            display.print( oled_text_buf );
            break;
    
    case 2: oled_text_buf[0] = (entered_num >> 4) + 0x30;
            oled_text_buf[1] = (entered_num & 0x0f) + 0x30;
            oled_text_buf[2] = 0;
            display.print( oled_text_buf );
            show_info_str = true;
            break;  
  
    default:
            oled_text_buf[0] = '?';
            oled_text_buf[1] = '?';
            oled_text_buf[2] = 0;
            break;
  }

  return show_info_str;
}



/* ================================================================================================================
   Called by OLED display update code to render a screen to show.
  
   Returns TRUE if it wants the system display rendered on top.
*/

bool draw_lui_display() {
  bool drawsys = true;                                                  // draw the system display elements by default

  if( lui_display_mode == LUI_DM_INFO ) {
    drawsys = false;
    draw_info_screen( bcd2dec( entered_num ) );                         // use decimal values to find requested info page
  }
  else {
    switch( local_ui_state ) {
      case LUI_INACTIVE:      draw_idle_display();                      // what to draw when the machine is just sitting there idling
                              break;  
  
      case LUI_GET_CMD:  
      case LUI_GOT_CMD:       switch( cmd ) {
                                case 10:
                                case 11:      display_ui_vop();       
                                              break;

                                default:      
                                              show_top_banner( (char*)"Enter 2-digit Command" );
                                
                                              if( oled_show_2digits() ){
                                                show_beside_2digits( lui_actions( entered_num ) );
                                              }
                  
                                              show_bottom_banner( (char*)"Use     for help" );
                                              display.fillTriangle( 35, 63, 35, 59, 42, 61, 1 );
                                              display.drawLine( 25, 61, 35, 61, 1 );
                                              break;
                              }
                              break;  
  
      case LUI_GET_VAL:  
                              show_beside_2digits( lui_val_info( cmd, entered_num, oled_show_2digits()) );
  
                              show_bottom_banner( (char*)"Use     for help" );
                              display.fillTriangle( 35, 63, 35, 59, 42, 61, 1 );
                              display.drawLine( 25, 61, 35, 61, 1 );
                              break;  
  
      case LUI_GOT_VAL:                                 
                              switch( cmd ) {
                                case 55:  display_ui_eprom_dump();
                                          break;
  
                                default:  show_beside_2digits( lui_val_info( cmd, entered_num, oled_show_2digits()) );
                                          break;
                              }
                              break;
  
  /*
                              if( drum_bitmap_changed ) {
                                show_bitmap_count = 0;
                                show_drum_bitmap = true;
  
                                drum_bitmap_changed = false;
                              }
  
                              if( show_drum_bitmap ) {
                                if( show_bitmap_count < BITMAP_SHOW_MS )
                                  draw_drum_bitmap();
                                else
                                  show_drum_bitmap = false;
                              } else {
                                display.setCursor( 1, 5 );
                                display.setTextColor( 1 );
                                display.setFont();
                                display.setTextSize( 2 );
                                display.print( "GET VAL" );
                              }
                              break;  
  */
  
      
      case LUI_CMD_COMPLETE:         
                              break;  
  
      default:                display.setCursor( 1, 5 );
                              display.setTextColor( 1 );
                              display.setFont();
                              display.setTextSize( 2 );
                              display.print( "* UNEXPECTED *" );
                              break;  
    }
  }

  return drawsys;
}


void banner_common( int x, int y, char *s ) {
  display.setCursor( x, y );
  display.setTextColor( 1 );
  display.setFont();
  display.setTextSize( 1 );
  display.print( s );
}


bool should_show_top() {
  if( (flashrate_t == 0) || (oled_frame_cnt < (flasher_top + (flashrate_t / 2))) )
    return true;
  else
    if( oled_frame_cnt > (flasher_top + flashrate_t) )
      flasher_top = oled_frame_cnt;

  return false;
}

bool should_show_middle() {
  if( (flashrate_m == 0) || (oled_frame_cnt < (flasher_middle + (flashrate_m / 2))) )
    return true;
  else
    if( oled_frame_cnt > (flasher_middle + flashrate_m) )
      flasher_middle = oled_frame_cnt;

  return false;
}

bool should_show_bottom() {
  if( (flashrate_b == 0) || (oled_frame_cnt < (flasher_bottom + (flashrate_b / 2))) )
    return true;
  else
    if( oled_frame_cnt > (flasher_bottom + flashrate_b) )
      flasher_bottom = oled_frame_cnt;

  return false;
}


void show_top_banner( char *s ) {
  if( should_show_top() )
    banner_common( 1, 20, s );
}

void show_middle_banner( char *s ) {
  if( should_show_middle() )
    banner_common( 1, 38, s );
}

void show_bottom_banner( char *s ) {
  if( should_show_bottom() )
    banner_common( 1, 56, s );
}


void show_beside_2digits( char *s ) {
  display.setCursor( 30, 38 );
  display.setTextSize( 1 );
  display.print( s );
}


void flashrate_top( int rate )    { if( flashrate_t != (rate*2) ){ flashrate_t = rate*2;  flasher_top = oled_frame_cnt;     }  }
void flashrate_middle( int rate ) { if( flashrate_m != (rate*2) ){ flashrate_m = rate*2;  flasher_middle = oled_frame_cnt;  }  }
void flashrate_bottom( int rate ) { if( flashrate_b != (rate*2) ){ flashrate_b = rate*2;  flasher_bottom = oled_frame_cnt;  }  }


void draw_idle_display() {
  if( luma_is_playing() == false ) {
    show_top_banner((char*)"Awaiting Tempo Clock");
  }

  if( honorMIDIStartStopState() )
    show_middle_banner((char*)"MIDI Start ENABLED");
  else
    show_middle_banner((char*)"MIDI Start disabled");
}



void input_digits_init() {
  num_digits_entered = 0;
  cursor_blink_cnt = 0;
  cursor_on = true;
  
  scan_keyboard();                                    // mark STORE as not changed
}


void input_digits_init_preload( uint8_t val ) {       // init with a known/current value shown
  input_digits_init();
  num_digits_entered = 2;
  entered_num = val;
}


uint8_t input_digits( uint8_t keycode, uint8_t *digits ) {
  uint8_t retval = INPUT_DIGITS_PENDING;

  if( cursor_blink_cnt > CURSOR_BLINK_RATE ) {
    cursor_on = !cursor_on;
    cursor_blink_cnt = 0;
  }
    
  if( keycode != 0xff ) {
      
    // NUMBER KEY?
    
    if( keycode < 0x10 ) {
      switch( num_digits_entered ) {
        case 0: entered_num = (keycode<<4) + 0x0c;    // key into left digit, cursor in right digit
                num_digits_entered = 1;
                break;
                
        case 1: entered_num &= 0xf0;                  // zero out right digit
                entered_num |= keycode;               // key into right digit
                num_digits_entered = 2;
                break;
                
        case 2: entered_num = (keycode<<4) + 0x0c;    // key into left digit, cursor in right digit
                num_digits_entered = 1;
                break;
      }
    }
    else
    {
      // CHECK FOR PLAY/STOP (enter) or STORE (cancel) or LEFT ARROW or RIGHT ARROW
      
      switch( keycode ) {
        case KEY_PLAY_STOP: if( num_digits_entered == 2 )
                              retval = INPUT_DIGITS_COMPLETE;
                            break;
                            
        case KEY_MENU_LUMA: //Serial.println("got MENU");
                            retval = INPUT_DIGITS_CANCEL;         // AKA STORE
                            break;
                            
        case KEY_L_ARROW:   switch( num_digits_entered ) {
                              case 0: if( local_ui_state == LUI_GET_CMD )                     // nothing entered yet, find the LAST valid value
                                        entered_num = prev_valid_cmd( 0x00 );                 // prev from 00 -> 99 (or other last valid value)
                                      else
                                        entered_num = prev_valid_val( 0x00 );                                        
                                      num_digits_entered = 2;
                                      break;
                              case 1: if( local_ui_state == LUI_GET_CMD )                     // 1 digit entered, find PREVIOUS valid value
                                        entered_num = prev_valid_cmd( entered_num & 0xf0 );
                                      else
                                        entered_num = prev_valid_val( entered_num & 0xf0 );                                        
                                      num_digits_entered = 2;
                                      break;
                              case 2: if( local_ui_state == LUI_GET_CMD )
                                        entered_num = prev_valid_cmd( entered_num );
                                      else
                                        entered_num = prev_valid_val( entered_num );
                                      num_digits_entered = 2;
                                      break;
                            }
                            break;

                            /*
                            if( num_digits_entered == 2 ) num_digits_entered = 0;
                            break;
                            */
                            
        case KEY_R_ARROW:   switch( num_digits_entered ) {
                              case 0: entered_num = 0x00;         // 0 into left digit, 0 into right digit
                                      num_digits_entered = 2;
                                      break;
                              case 1: entered_num &= 0xf0;        // preserve left digit, 0 into right digit
                                      num_digits_entered = 2;
                                      break;
                              case 2: if( local_ui_state == LUI_GET_CMD )
                                        entered_num = next_valid_cmd( entered_num );
                                      else
                                        entered_num = next_valid_val( entered_num );
                                      num_digits_entered = 2;
                                      break;
                            }
                            break;
      }
    }
  }
  
  switch( num_digits_entered ) {
    case 0: *digits = (cursor_on ? 0xcc : 0xfc);                  // no digits entered yet, 2 cursors, auto blink left one
            break;
    case 1:                                                       // see keycode processor above, puts cursor (0xc) in left digit if only 1 entered so far
    case 2: *digits = entered_num;
            *digits |= (cursor_on ? 0x00 : 0x0f );                // cursor blink forces right digit to blank by or'ing in 0x0f
            break;
  }

  return retval;
}



/* ================================================================================================================
    Called from loop().

    Check to see if MENU was pressed, if so go into MENU (LUI) mode.
    
    If we are in MENU (LUI) mode:
        - Get a 2-digit COMMAND, use PLAY/STOP to enter the command
        
        For some commands: 
        - Get a 2-digit VALUE,   use PLAY/STOP to enter the value

        Once a command or command/value pair has been entered, execute the command.

    Complex commands that involve more than a simple value parameter can have state machines to implement the
    needed logic and display rendering. See the EPROM Dump command for an example of a complex command.
    
*/

bool need_z80_reboot = false;                                               // set to TRUE by ops that need the z-80 rebooted (like, loading all of z-80 RAM)


bool in_local_ui() {
  return( local_ui_state != LUI_INACTIVE );
}


void handle_local_ui() {
  uint8_t linkval;
  uint8_t pattval;
  uint8_t kc;
  bool cmd_completed = true;

  // --- Scan keyboard
  
  if( local_ui_state != LUI_INACTIVE ) {
    scan_keyboard();                                                        // should normally only call scan_keyboard() here to keep state sane
    kc = get_keycode();                                                     // if a key is down, return its LM-1-style keycode. returns 0xff if no key down.
  }

  // -- handle DISPLAY key

  if( (local_ui_state != LUI_INACTIVE) && (local_ui_state != LUI_CMD_COMPLETE) && (kc == KEY_TEMPO) ) {
    lui_display_mode = ( lui_display_mode == LUI_DM_ACTION ) ? LUI_DM_INFO : LUI_DM_ACTION;        // DISPLAY toggles between regular and drum bitmap display
    
    cmd = 0;                      // a bit of a hack, switching between ACTION and INFO resets cmd to 00, which is valid in both worlds
    entered_num = 0;
    num_digits_entered = 2;
  }

  // -- handle ON/OFF key

  if( (local_ui_state != LUI_INACTIVE) && (local_ui_state != LUI_CMD_COMPLETE) && (kc == KEY_CHAIN_ON_OFF) ) {
    honorMIDIStartStop( !honorMIDIStartStopState() );
    eeprom_save_midi_start_honor( honorMIDIStartStopState() );
    kc = KEY_MENU_LUMA;                                     // fake pressing MENU again
  }

  // -- handle normal command entry
  
  switch( local_ui_state ) {
    case LUI_INACTIVE:
                      if( digitalRead( LM1_LED_A ) ) {                      // set either by Z-80 when STORE pressed,
                                                                            //  or at boot by Teensy when we detect that STORE is down at power-on
                        teensy_drives_z80_bus( true );                      // grab the bus

                        z80_bus_write( LINK_DISPLAY, 0xff );                // LED displays off
                        z80_bus_write( PATT_DISPLAY, 0xff );
  
                        input_digits_init();

                        voice_load_bm = BANK_LOAD_ALL;                      // default to loading all voices

                        local_ui_state = LUI_GET_CMD;                       // get a command

                        cmd = CMD_INVALID;                                  // invalid command when we start
                      
                        // make sure the bootscreen goes away
                        bootscreen_showing = false;                   
                      }
                      break;

    case LUI_GET_CMD: switch( input_digits( kc, &pattval ) ) {
                        case INPUT_DIGITS_COMPLETE:     if( lui_display_mode == LUI_DM_ACTION )             // PLAY/STOP only takes action if in ACTION mode
                                                          local_ui_state = LUI_GOT_CMD;             break;
                        case INPUT_DIGITS_CANCEL:       local_ui_state = LUI_CMD_COMPLETE;          break;
                      }                        

                      input_drum_bitmap( kc );                              // check for drum load bitmap changes

                      z80_bus_write( PATT_DISPLAY, pattval );
                      break;

    case LUI_GOT_CMD: z80_bus_write( PATT_DISPLAY, entered_num );           // make sure both digits are on
                      
                      //Serial.println("GOT CMD");
                      
                      cmd = bcd2dec( entered_num );                         // CONVERT 2 BCD DIGITS (so now we drop the 0x)

                      switch( cmd ) {
  
                        case 10:  if( logic_ui_vop( kc, VOP_SEL_COPY) )
                                    local_ui_state = LUI_CMD_COMPLETE;
                                  break;
                        
                        case 11:  if( logic_ui_vop( kc, VOP_SEL_REVERSE) )
                                    local_ui_state = LUI_CMD_COMPLETE;
                                  break;
                                                                      
                        case 55:  valid_range( 0, 4 );                      // 5 EPROM types supported                        
                                  input_digits_init();
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 66:  valid_range( 0, 2 );                      // 3 Fan Modes
                                  input_digits_init();
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 70:  send_sample_sysex( 0xff, 0xff );          // bank 0xff, drum_sel 0xff -> send last hit drum
                                  local_ui_state = LUI_CMD_COMPLETE;
                                  break;

                        case 72:  send_pattern_RAM_sysex( 0xff );           // bank 0xff -> send currently loaded RAM
                                  local_ui_state = LUI_CMD_COMPLETE;
                                  break;
                        
                        case 80:  valid_range( 0, 16 );                     // 00 is OMNI, then chans 1 - 16
                                  input_digits_init_preload( dec2bcd( get_midi_channel() ) );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 81:  valid_range( 0, 2 );                      // DIN-5, USB, Both
                                  input_digits_init_preload( get_midi_note_out_route()-1 );       // these go 1,2,3 -- not 0,1,2
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 82:  valid_range( 0, 2 );                      // DIN-5, USB, Both
                                  input_digits_init_preload( get_midi_note_in_route()-1 );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 83:  valid_range( 0, 2 );                      // DIN-5, USB, Both
                                  input_digits_init_preload( get_midi_clock_out_route()-1 );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 84:  valid_range( 0, 1 );                      // DIN-5, USB
                                  input_digits_init_preload( get_midi_clock_in_route()-1 );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 85:  valid_range( 0, 1 );                      // DIN-5, USB
                                  input_digits_init_preload( get_midi_sysex_route()-1 );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 86:  valid_range( 0, 1 );                      // off, ON
                                  input_digits_init_preload( (get_midi_soft_thru() ? 1:0) );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 87:  valid_range( 0, 1 );                      // off, ON
                                  input_digits_init_preload( (honorMIDIStartStopState() ? 1:0) );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 88:  valid_range( 0, 1 );                      // off, ON
                                  input_digits_init_preload( (get_midi_send_vel() ? 1:0) );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 89:  valid_range( 0, 99 );                     // delay in ms
                                  input_digits_init_preload( dec2bcd( get_midi_sysex_delay() ) );
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        case 90:  valid_range( 0, 2 );                      // 3 boot screens
                                  input_digits_init();
                                  local_ui_state = LUI_GET_VAL;
                                  break;
                                                          
                        case 97:  Serial.println("\nCard Directory:\n");
                                  sd_card_ls();
                                  local_ui_state = LUI_CMD_COMPLETE;
                                  break;
                                  
                        case 99:  valid_range( 0, 1 );                      // just reboot, or factory reset & reboot?
                                  input_digits_init_preload( 0 );           // default, just reboot
                                  local_ui_state = LUI_GET_VAL;
                                  break;

                        default:  valid_range( 0, 99 );
                                  input_digits_init();
                                  local_ui_state = LUI_GET_VAL;
                                  break;
                      }
                      break;


    case LUI_GET_VAL: switch( input_digits( kc, &linkval ) ) {
                        case INPUT_DIGITS_COMPLETE:     local_ui_state = LUI_GOT_VAL;         break;
                        case INPUT_DIGITS_CANCEL:       local_ui_state = LUI_CMD_COMPLETE;    break;
                      }                        

                      input_drum_bitmap( kc );                              // check for drum load bitmap changes
                  
                      z80_bus_write( LINK_DISPLAY, linkval );
                      break;

    case LUI_GOT_VAL: val = bcd2dec( entered_num );

                      if( val != max_val_check( val ) ) {                   // invalid value manually entered?
                        input_digits_init();
                        local_ui_state = LUI_GET_VAL;
                      }
                      else {  
                        z80_bus_write( LINK_DISPLAY, entered_num );         // make sure both digits are on
  
                        switch( cmd ) {
                          case 00:  Serial.print("CMD: Load Voice Bank "); Serial.println( val );
                                    load_voice_bank( voice_load_bm, val );
                                    break;
  
                          case 01:  Serial.print("CMD: Store Voice Bank "); Serial.println( val );
                                    store_voice_bank( voice_load_bm, val );
                                    break;
  
                          case 02:  Serial.print("CMD: Load RAM Bank "); Serial.println( val );
                                    load_ram_bank( val );
                                    need_z80_reboot = true;
                                    break;
  
                          case 03:  Serial.print("CMD: Store RAM Bank "); Serial.println( val );
                                    save_ram_bank( val );
                                    break;
  
                          case 55:  //Serial.print("CMD: EPROM Dump "); Serial.println( val );
                                    if( !logic_ui_eprom_dump( kc ) )
                                      cmd_completed = false;                  // keep us here until it's done
                                    break;
                                    
                          case 66:  Serial.print("CMD: Fan Control "); Serial.println( val );
                                    set_fan_mode( val );
                                    eeprom_save_fan_mode( val );
                                    break;


                          case 71:  Serial.print("CMD: SysEx Send Sample Bank # "); Serial.println( val );
                                    if( voice_load_bm & BANK_LOAD_CONGAS )
                                      send_sample_sysex( val, DRUM_SEL_CONGA );
                                      
                                    if( voice_load_bm & BANK_LOAD_TOMS )
                                      send_sample_sysex( val, DRUM_SEL_TOM );
                                    
                                    if( voice_load_bm & BANK_LOAD_SNARE )
                                      send_sample_sysex( val, DRUM_SEL_SNARE );
                                    
                                    if( voice_load_bm & BANK_LOAD_BASS )
                                      send_sample_sysex( val, DRUM_SEL_BASS );
                                    
                                    if( voice_load_bm & BANK_LOAD_HIHAT )
                                      send_sample_sysex( val, DRUM_SEL_HIHAT );
                                    
                                    if( voice_load_bm & BANK_LOAD_COWBELL )
                                      send_sample_sysex( val, DRUM_SEL_COWBELL );
                                    
                                    if( voice_load_bm & BANK_LOAD_CLAPS )
                                      send_sample_sysex( val, DRUM_SEL_CLAPS );
                                    
                                    if( voice_load_bm & BANK_LOAD_CLAVE )
                                      send_sample_sysex( val, DRUM_SEL_CLAVE );                                      
                                    
                                    if( voice_load_bm & BANK_LOAD_TAMB )
                                      send_sample_sysex( val, DRUM_SEL_TAMB );
                                    
                                    if( voice_load_bm & BANK_LOAD_CABASA )
                                      send_sample_sysex( val, DRUM_SEL_CABASA );   
                                    break;

                          case 73:  Serial.print("CMD: SysEx Send RAM Bank # "); Serial.println( val );
                                    send_pattern_RAM_sysex( val );
                                    break;
                                    
                          case 80:  Serial.print("CMD: MIDI Channel "); Serial.println( val );
                                    set_midi_channel( val );
                                    break;

                          case 81:  val++;                                                            // 81-85 are 01,02,03
                                    Serial.printf("CMD: MIDI Notes OUT %02x\n", val );
                                    set_midi_note_out_route( val );
                                    eeprom_save_midi_note_out_route( val );
                                    break;

                          case 82:  val++;                                                            // 81-85 are 01,02,03
                                    Serial.printf("CMD: MIDI Notes IN %02x\n", val );
                                    set_midi_note_in_route( val );
                                    eeprom_save_midi_note_in_route( val );
                                    break;

                          case 83:  val++;                                                            // 81-85 are 01,02,03
                                    Serial.printf("CMD: MIDI Clock OUT %02x\n", val );
                                    set_midi_clock_out_route( val );
                                    eeprom_save_midi_clock_out_route( val );
                                    break;
                                    
                          case 84:  val++;                                                            // 81-85 are 01,02,03
                                    Serial.printf("CMD: MIDI Clock IN %02x\n", val );
                                    set_midi_clock_in_route( val );
                                    eeprom_save_midi_clock_in_route( val );
                                    break;

                          case 85:  val++;                                                            // 81-85 are 01,02,03
                                    Serial.printf("CMD: MIDI Sysex %02x\n", val );
                                    set_midi_sysex_route( val );
                                    eeprom_save_midi_sysex_route( val );
                                    break;

                          case 86:  Serial.printf("CMD: MIDI Soft Thru %02x\n", val );
                                    set_midi_soft_thru( val ? true:false );
                                    eeprom_save_midi_soft_thru( val ? true:false );
                                    break;

                          case 87:  Serial.printf("CMD: MIDI Start Enable %02x\n", val );
                                    honorMIDIStartStop( val ? true:false );
                                    eeprom_save_midi_start_honor( val ? true:false );
                                    break;

                          case 88:  Serial.printf("CMD: MIDI Send Vel %02x\n", val );
                                    set_midi_send_vel( val ? true:false );
                                    eeprom_save_midi_send_velocity( val ? true:false );
                                    break;
                                    
                          case 89:  Serial.printf("CMD: MIDI Sysex Delay %02d\n", val );
                                    set_midi_sysex_delay( val );
                                    eeprom_save_midi_sysex_delay( val );
                                    break;

                          case 90:  Serial.print("CMD: Boot Screen Select "); Serial.println( val );
                                    eeprom_save_boot_screen_select( val );
                                    break;
                                    
                          case 98:  Serial.print("CMD: FORMAT CARD "); Serial.println( val );
                                    if( val == 98 ) {
                                      Serial.println("Calling format");
                                      format_card();
                                    }
                                    else {
                                      Serial.print("Entered parameter != 98, NOT FORMATTING");
                                    }
                                    break;

                          case 99:  reboot( val ? true:false );
                                    local_ui_state = LUI_CMD_COMPLETE;
                                    break;
                        }
                                  
                        if( cmd_completed )
                          local_ui_state = LUI_CMD_COMPLETE;
                      }
                      break;


    case LUI_CMD_COMPLETE:           
                      if( need_z80_reboot ) {
                        z80_reset( true );
                        teensy_drives_z80_bus( false );                       // we're done, let the z-80 go again
                        z80_reset( false );

                        delay( 250 );
                        
                        need_z80_reboot = false;             
                      }
                      else
                        if( !forced_local_ui_mode )                           // if we were forced into local UI mode, stay in it
                          teensy_drives_z80_bus( false );                     // we're done, let the z-80 go again

                      lui_display_mode = LUI_DM_ACTION;                       // make sure we go back to normal (non-INFO) display

                      if( !forced_local_ui_mode ) {                           // if we were forced into local UI mode, stay in it
                        delay( 100 );
  
                        if( digitalRead( LM1_LED_A ) == false )               // wait for the STORE LED to turn off (z-80 back in control)
                          local_ui_state = LUI_INACTIVE;
                      }
                      else
                          local_ui_state = LUI_INACTIVE;

                      break;             
    
  }
}
