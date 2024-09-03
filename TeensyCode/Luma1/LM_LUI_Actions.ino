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
#include "LM_Utilities.h"
#include "LM_Voices.h"


/* ================================================================================================================
    BANKS
*/

char *voice_bank_name( uint8_t bank ) {
  return get_voice_bank_name( bcd2dec(bank) );
}


char *pattern_bank_name( uint8_t bank ) {
  return get_ram_bank_name( bcd2dec(bank) );
}




/* ================================================================================================================   
     __      __   _               ____                       _   _                 
     \ \    / /  (_)             / __ \                     | | (_)                
      \ \  / /__  _  ___ ___    | |  | |_ __   ___ _ __ __ _| |_ _  ___  _ __  ___ 
       \ \/ / _ \| |/ __/ _ \   | |  | | '_ \ / _ \ '__/ _` | __| |/ _ \| '_ \/ __|
        \  / (_) | | (_|  __/   | |__| | |_) |  __/ | | (_| | |_| | (_) | | | \__ \
         \/ \___/|_|\___\___|    \____/| .__/ \___|_|  \__,_|\__|_|\___/|_| |_|___/
                                       | |                                         
                                       |_|                                         
    Doing Stuff With Voices
    Copy, Reverse, etc.
    
    There are 2 main parts:
    
      bool logic_ui_vop( uint8_t type ) - get keyboard input and run the UI state machine, return TRUE when done
      
      bool display_ui_vop() - render the current UI screen, return TRUE to also show system display (bar at top of screen)

*/

/*
  Once the user has selected a VAL (operation type, like copy, reverse, etc.), this is called repeatedly to drive the state machine.
  This will return TRUE when we're all done.

  Steps this state machine takes:

  1. Ask the user to select a drum voice to operate on
  2. If another drum voice is needed (e.g., a target voice for copy), ask the user to select it
  3. Perform the operation, return TRUE

*/


#define VOP_SEL_COPY          0
#define VOP_SEL_REVERSE       1


#define VOP_START             0             // prompt user to select drum to operate on, wait for drum key
#define VOP_TARGET            1             // prompt user to select a target drum (e.g., for copy)
#define VOP_COPY              2             // copy voice from start to target
#define VOP_REVERSE           3             // reverse voice selected for start
#define VOP_FINISHED          4             // DONE, call epilogue and return true

int vop_state = VOP_START;

char *vop_prompt = (char*)"Voice Ops";      // this gets set by the state machine below to the UI message we need to show

char vop_textbuf[256];                      // scratch string

uint16_t vop_start_voice;                   // voice card we should start with
uint16_t vop_target_voice;                  // target voice for some ops


char *stb_2_name( uint16_t stb ) {
  switch( stb ) {
    case STB_CONGAS:          return (char*)"CONGAS";
    case STB_TOMS:            return (char*)"TOMS";
    case STB_SNARE:           return (char*)"SNARE";
    case STB_BASS:            return (char*)"BASS";
    case STB_HIHAT:           return (char*)"HIHAT";
    case STB_COWBELL:         return (char*)"COWBELL";
    case STB_CLAPS:           return (char*)"CLAPS";
    case STB_CLAVE:           return (char*)"RIMSHOT";
    case STB_TAMB:            return (char*)"TAMB";
    case STB_CABASA:          return (char*)"CABASA";
  }
  return (char*)"ERROR";
}


uint16_t key_2_stb( uint8_t lastkey ) {
  switch( lastkey ) {
    case KEY_DRM_CONGA_UP:
    case KEY_DRM_CONGA_DN:    return STB_CONGAS;
    
    case KEY_DRM_TOM_UP:
    case KEY_DRM_TOM_DN:      return STB_TOMS;
    
    case KEY_DRM_SNARE_LO:
    case KEY_DRM_SNARE_HI:    return STB_SNARE;
    
    case KEY_DRM_BASS_LO:                              
    case KEY_DRM_BASS_HI:     return STB_BASS;
    
    case KEY_DRM_HIHAT_OP:                              
    case KEY_DRM_HIHAT_LO:                              
    case KEY_DRM_HIHAT_HI:    return STB_HIHAT;
                              
    case KEY_DRM_COWBELL:     return STB_COWBELL;

    case KEY_DRM_CLAPS:       return STB_CLAPS;

    case KEY_DRM_CLAVE:       return STB_CLAVE;
    
    case KEY_DRM_TAMB_LO:                              
    case KEY_DRM_TAMB_HI:     return STB_TAMB;
    
    case KEY_DRM_CABASA_LO:                              
    case KEY_DRM_CABASA_HI:   return STB_CABASA;

    default:                  return 0;
  }
}


void reverse_buffer( uint8_t *b, int len ) {
  uint8_t s;
  
  if( len & 1 ) len--;              // hacky

  for( int xxx = 0; xxx != (len/2); xxx++ ) {
    s = b[xxx];
    b[xxx] = b[len-xxx];
    b[len-xxx] = s;
  }
}



bool logic_ui_vop( uint8_t lastkey, int operation_select ) {
  uint8_t *v;
  int vlen;

  switch( vop_state ) {
    case VOP_START:         
                            vop_start_voice = key_2_stb( lastkey );

                            if( vop_start_voice != 0 ) {
                              Serial.printf("Start voice = %04x\n", vop_start_voice );
                              vop_state = VOP_TARGET;
                            }
                            break;

    case VOP_TARGET:        vop_target_voice = key_2_stb( lastkey );

                            if( vop_target_voice != 0 ) {
                              Serial.printf("Target voice = %04x\n", vop_target_voice );
                              switch( operation_select ) {
                                case VOP_SEL_COPY:      vop_state = VOP_COPY;         break;
                                case VOP_SEL_REVERSE:   vop_state = VOP_REVERSE;      break;
                              }
                            }
                            break;
    
    case VOP_COPY:          Serial.printf("VOP Copy %04x to %04x\n", vop_start_voice, vop_target_voice);
                            v = get_voice( BANK_STAGING, vop_start_voice, vop_textbuf, &vlen );
                            set_voice( vop_target_voice, v, vlen, vop_textbuf );
                            vop_state = VOP_FINISHED;
                            break;
    
    case VOP_REVERSE:       Serial.printf("VOP Reverse %04x to %04x\n", vop_start_voice, vop_target_voice);
                            v = get_voice( BANK_STAGING, vop_start_voice, vop_textbuf, &vlen );
                            reverse_buffer( v, vlen );
                            set_voice( vop_target_voice, v, vlen, vop_textbuf );
                            vop_state = VOP_FINISHED;
                            break;
        
    case VOP_FINISHED:    
                            if( (lastkey == KEY_PLAY_STOP) || (lastkey == KEY_MENU_LUMA) ) {
                              vop_state = VOP_START;
                              return true;
                            }
                            break;           
  }
  
  return false;
}
  

bool display_ui_vop() {
  
  switch( vop_state ) {
    case VOP_START:         vop_prompt = (char*)"Select Drum Voice";
                            show_top_banner((char*)"Tap Drum");      
                            break;

    case VOP_TARGET:        vop_prompt = (char*)"Select Drum Voice";
                            show_top_banner( stb_2_name(vop_start_voice) );    
                            show_middle_banner((char*)"Tap Drum");      
                            break;

    case VOP_COPY:          vop_prompt = (char*)"DONE, hit PLAY/STOP";
                            show_top_banner( stb_2_name(vop_start_voice) );    
                            show_middle_banner( stb_2_name(vop_target_voice) );    
                            break;

    case VOP_REVERSE:       vop_prompt = (char*)"DONE, hit PLAY/STOP";
                            show_top_banner( stb_2_name(vop_start_voice) );    
                            show_middle_banner( stb_2_name(vop_target_voice) );    
                            break;    

    case VOP_FINISHED:      show_top_banner(vop_prompt);
                            show_middle_banner( stb_2_name(vop_start_voice) );
                            show_bottom_banner( stb_2_name(vop_target_voice) );
                            break;           
  }

  return true;
}




/* ================================================================================================================   
     ______ _____  _____   ____  __  __    _____                        
    |  ____|  __ \|  __ \ / __ \|  \/  |  |  __ \                       
    | |__  | |__) | |__) | |  | | \  / |  | |  | |_   _ _ __ ___  _ __  
    |  __| |  ___/|  _  /| |  | | |\/| |  | |  | | | | | '_ ` _ \| '_ \ 
    | |____| |    | | \ \| |__| | |  | |  | |__| | |_| | | | | | | |_) |
    |______|_|    |_|  \_\\____/|_|  |_|  |_____/ \__,_|_| |_| |_| .__/ 
                                                                 | |    
                                                                 |_|    
    EPROM Dumping UI
    
    There are 2 main parts:
    
      bool logic_ui_eprom_dump( uint8_t type ) - get keyboard input and run the UI state machine, return TRUE when done
      
      bool display_ui_eprom_dump() - render the current UI screen, return TRUE to also show system display (bar at top of screen)

*/

// select one of the valid EPROM sizes, note how many bytes we have to copy

char *handle_val_eprom_size( uint8_t val, bool val_is_valid ) {
  if( val_is_valid ) {
    switch( val ) {
      case 00:  eprom_size = 2048;      return (char*)"2716 (2 KB)";
      case 01:  eprom_size = 4096;      return (char*)"2732 (4 KB)";
      case 02:  eprom_size = 8192;      return (char*)"2764 (8 KB)";
      case 03:  eprom_size = 16384;     return (char*)"27128 (16 KB)";
      case 04:  eprom_size = 32768;     return (char*)"27256 (32 KB)";
      default:  eprom_size = 0;         return (char*)"Invalid Type";
    }
  }

  return (char*)"EPROM Type";
}


/*
    This gets called from the main handle_local_ui processing loop.
    It drives through the steps of EPROM reading:
      - Prompt user to select drum to load with EPROM data, wait for drum key
      - Prompt user to set 24/28 switch the right way, wait for PLAY/STOP
      - Prompt user to insert EPROM, wait for PLAY/STOP
      - Dump data, show user checksum, push to selected voice
      - Prompt user to remove EPROM, wait for PLAY/STOP
      - DONE, return TRUE
*/

#define EDUMP_START           0             // prompt user to select drum to load, wait for drum key
#define EDUMP_SET_SWITCH      1             // call read prologue to set up for read
#define EDUMP_SET_SWITCH_2    2             // prompt user to set the 24/28 switch the right way for the EPROM type
#define EDUMP_INSERT          3             // prompt user to insert the EPROM and hit PLAY/STOP
#define EDUMP_DUMP            4             // actually read the EPROM
#define EDUMP_DUMP_2          5             // Show checksum and prompt
#define EDUMP_REMOVE          6             // prompt user to REMOVE the EPROM and hit PLAY/STOP
#define EDUMP_FINISHED        7             // DONE, call epilogue and return true

int eprom_dump_state = EDUMP_START;

char *eprom_dump_prompt = (char*)"EPROM Dump";  // this gets set by the state machine below to the UI message we need to show

char eprom_dump_textbuf[256];               // scratch string


/*
  Once the user has selected a VAL (eprom type), this is called repeatedly to drive the eprom dumping state machine.
  This will return TRUE when we're all done.

  Steps this state machine takes:

  1. Ask the user to select a drum voice to dump this into
  2. Prompt the user to set the "EPROM Pins" switch based on the eprom type (which is in val)
  3. Prompt the user to put the eprom in the socket, noting orientation
  4. Read the eprom:
     - Copy eprom data to eprom_dump_buf[32768], show checksum
     - Load eprom_buf to selected voice (which also saves it in the corresponding STAGING slot)
  5. Prompt the user to remove the eprom from the socket and then hit PLAY/STOP
  6. Done, return TRUE

*/

bool logic_ui_eprom_dump( uint8_t lastkey ) {

  switch( eprom_dump_state ) {
    case EDUMP_START:       eprom_dump_prompt = (char*)"Select Drum Voice";
    
                            switch( lastkey ) {
                              case KEY_DRM_CONGA_UP:
                              case KEY_DRM_CONGA_DN:    eprom_dump_prompt = (char*)"CONGA";
                                                        eprom_dump_voice = STB_CONGAS;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                              
                              case KEY_DRM_TOM_UP:
                              case KEY_DRM_TOM_DN:      eprom_dump_prompt = (char*)"TOM";
                                                        eprom_dump_voice = STB_TOMS;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                          
                              case KEY_DRM_SNARE_LO:
                              case KEY_DRM_SNARE_HI:    eprom_dump_prompt = (char*)"SNARE";
                                                        eprom_dump_voice = STB_SNARE;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                          
                              case KEY_DRM_BASS_LO:                              
                              case KEY_DRM_BASS_HI:     eprom_dump_prompt = (char*)"BASS";
                                                        eprom_dump_voice = STB_BASS;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                          
                              case KEY_DRM_HIHAT_OP:                              
                              case KEY_DRM_HIHAT_LO:                              
                              case KEY_DRM_HIHAT_HI:    eprom_dump_prompt = (char*)"HIHAT";
                                                        eprom_dump_voice = STB_HIHAT;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                                                        
                              case KEY_DRM_COWBELL:     eprom_dump_prompt = (char*)"COWBELL";
                                                        eprom_dump_voice = STB_COWBELL;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                              case KEY_DRM_CLAPS:       eprom_dump_prompt = (char*)"CLAPS";
                                                        eprom_dump_voice = STB_CLAPS;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                              case KEY_DRM_CLAVE:       eprom_dump_prompt = (char*)"RIMSHOT";
                                                        eprom_dump_voice = STB_CLAVE;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                          
                              case KEY_DRM_TAMB_LO:                              
                              case KEY_DRM_TAMB_HI:     eprom_dump_prompt = (char*)"TAMBOURINE";
                                                        eprom_dump_voice = STB_TAMB;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                          
                              case KEY_DRM_CABASA_LO:                              
                              case KEY_DRM_CABASA_HI:   eprom_dump_prompt = (char*)"CABASA";
                                                        eprom_dump_voice = STB_CABASA;
                                                        eprom_dump_state = EDUMP_SET_SWITCH;
                                                        break;
                            }
                            break;

    case EDUMP_SET_SWITCH:       
                            dump_eprom_prologue();
                            eprom_dump_state = EDUMP_SET_SWITCH_2;
                            break;
                                                  
    case EDUMP_SET_SWITCH_2:                             
                            if( lastkey == KEY_PLAY_STOP ) {
                              eprom_dump_state = EDUMP_INSERT;
                            }
                            break;

    case EDUMP_INSERT:
                            if( lastkey == KEY_PLAY_STOP ) {
                              eprom_dump_state = EDUMP_DUMP;
                            }
                            break;
                          
    case EDUMP_DUMP:
                            dump_eprom_data();
                            eprom_dump_state = EDUMP_DUMP_2;
                            break;
                            
    case EDUMP_DUMP_2:
                            eprom_dump_state = EDUMP_REMOVE;
                            break;

    case EDUMP_REMOVE:
                            if( lastkey == KEY_PLAY_STOP ) {
                              eprom_dump_state = EDUMP_FINISHED;
                            }
                            break;

    case EDUMP_FINISHED:    
                            dump_eprom_epilogue();
                            eprom_dump_state = EDUMP_START;           // we're done, reset eprom dumping state
                            eprom_size = 0;
                            eprom_dump_voice = 0;
                            return true;
                            break;           
  }

  
  return false;
}
  

bool display_ui_eprom_dump() {
  
  switch( eprom_dump_state ) {
    case EDUMP_START:       
                            eprom_dump_prompt = (char*)"Select Drum Voice";
                            
                            show_top_banner((char*)"Tap Drum to Load");      

                            flashrate_bottom( FLASH_VERY_FAST );
                            show_bottom_banner((char*)"DO NOT INSERT EPROM!");
                            break;

    case EDUMP_SET_SWITCH:                              
    case EDUMP_SET_SWITCH_2:                              
                            sprintf( eprom_dump_textbuf, "%d bytes to %s", eprom_size, eprom_dump_prompt );                            
                            show_top_banner( eprom_dump_textbuf );
                            
                            switch( eprom_size ) {
                              case 2048:  show_middle_banner((char*)"Set EPROM pins to 24");  break;
                              case 4096:  show_middle_banner((char*)"Set EPROM pins to 24");  break;
                              case 8192:  show_middle_banner((char*)"Set EPROM pins to 28");  break;
                              case 16384: show_middle_banner((char*)"Set EPROM pins to 28");  break;
                              case 32768: show_middle_banner((char*)"Set EPROM pins to 28");  break;
                            }

                            flashrate_bottom( 0 );
                            show_bottom_banner( (char*)"PLAY/STOP to continue" );
                            break;

    case EDUMP_INSERT:
                            show_top_banner( (char*)"Insert EPROM" );      
                            flashrate_middle( FLASH_FAST );
                            show_middle_banner( (char*)"NOTE ORIENTATION!" );
                            show_bottom_banner( (char*)"PLAY/STOP to continue" );
                            break;
                          
    case EDUMP_DUMP:
    case EDUMP_DUMP_2:
                            show_top_banner( (char*)"Reading..." );      
                            break;

    case EDUMP_REMOVE:
                            flashrate_top( FLASH_FAST );
                            show_top_banner( (char*)"REMOVE EPROM NOW!" );      

                            sprintf( eprom_dump_textbuf, "Checksum: %04X", eprom_checksum );
                            flashrate_middle( 0 );
                            show_middle_banner( eprom_dump_textbuf );
                            
                            show_bottom_banner( (char*)"PLAY/STOP to continue" );
                            break;

    case EDUMP_FINISHED:    
                            flashrate_top( 0 );
                            flashrate_middle( 0 );
                            flashrate_bottom( 0 );
                            break;           
  }

  
  return true;
}
