/* ---------------------------------------------------------------------------------------
    VOICE LOADING

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

#include "LM_SDCard.h"
#include "LM_Voices.h"

uint16_t voice_load_bm = BANK_LOAD_ALL;             // bitmap selects which voices will be loaded/stored

uint8_t cur_bank_num = 0;

char stage_fn[256];

char src_fn[64];

// We maintain some state in these STAGING files

#define STAGING_ORIG_BANKNUM_FN     "/STAGING/ORIGBNUM.BIN"
#define STAGING_BANKNAME_FN         "/STAGING/BANKNAME.TXT"
#define STAGING_DEFAULT_BANKNAME    "NO BANK NAME"


uint8_t *get_voice_file( char *dirname, char *voice_name, int *voice_len );



void copy_voice_SD( char *dirname_src, char *dirname_dst ) {
  Serial.printf("Copying %s -> %s\n", dirname_src, dirname_dst );

  root = SD.open( dirname_src );
  file = root.openNextFile();

  if( file ) {

    // --- read it into memory
    
    filesize = file.size();
    snprintf( src_fn, 63, file.name() );                                      // remember this, will copy to this filename in other dir
    Serial.printf("name: %s\n", src_fn );
    Serial.printf("size: %d\n", filesize );
    if( filesize > sizeof(filebuf) ) {
      Serial.printf("File too big, truncating to %d bytes\n", sizeof(filebuf));
      filesize = sizeof(filebuf);
    }
    file.read( filebuf, filesize );                                           // read it into 32KB working buffer
    file.close();
    Serial.println("Source file read");

    // --- if there is a file at the dest already, delete it
    
    for( int xxx = 0; xxx != 1000; xxx++ ) {                                  // bound it to 1000 files, should be more than enough
      root = SD.open( dirname_dst );
      file = root.openNextFile();

      if( file ) {
        snprintf( stage_fn, 255, "%s/%s", dirname_dst, file.name() );
        Serial.printf("Found file %s, deleting %s\n", file.name(), stage_fn );
        SD.remove( stage_fn );
        Serial.println("Removed, now saving new one");
      }
      else
        break;
    }

    // --- now copy the read in file to the dst directory
    
    snprintf( stage_fn, 255, "%s/%s", dirname_dst, src_fn );
    Serial.print("Saving to: "); Serial.println( stage_fn );
    
    file = SD.open( stage_fn, FILE_WRITE );
  
    if( file ) {
      file.write( filebuf, filesize );
      file.close(); 
    }
    else {
      Serial.print("### Error, could not open file "); Serial.print( stage_fn ); Serial.println(" for writing.");
    }
  
  }
  else {
    Serial.printf("*** ERROR: %s SOURCE file not found\n", dirname_src );
  }

  Serial.printf("\n");
}



void store_voice_file( char *voice_name, uint8_t bank_num ) {
  char srcname[48];
  char dstname[48];

  snprintf( srcname, 48, "/STAGING/%s/",                voice_name );
  snprintf( dstname, 48, "/DRMBANKS/%02d/%s", bank_num, voice_name );
  
  copy_voice_SD( srcname, dstname );
}


/*
    set_voice() loads voice data into a voice board, but also saves that voice data to a corresponding file
    in the STAGING directory of the SD card.

    This is because the voice board memory is write-only. We want to be able to build up a voice bank from multiple
    sources (other SD card banks, SysEx download, etc.), so we make a copy in STAGING of whatever we put in the voice boards.

    Saving or uploading the current bank (or a subset of it) is done using that STAGING directory.

    set_voice() also ensures that Conga and Tom are loaded with voices of the same length.

    We keep track of the sample lengths for Conga and Tom.
    We always push a full 16KB of bytes into each. If the sample uses less than the full 16KB, the empty space is filled with 0.
    We set the Conga/Tom playblack length to the longer length of the two.

    NOTE: Assumes that the sample buffer s is full 32KB, so we can zero bytes from len -> 32K
    
*/

void stage_voice( char *dirname, char *fn, uint8_t *s, int len ) {
  
  Serial.print("Staging "); Serial.println( fn );
  
  root = SD.open( dirname );
  
  file = root.openNextFile();
  
  if( file ) {
    Serial.print("Found staging file: "); Serial.println( file.name() );
    sprintf( stage_fn, "%s%s", dirname, file.name() );
    SD.remove( stage_fn );
    Serial.println("Removed, now saving new one");
  }

  sprintf( stage_fn, "%s%s", dirname, fn );
  Serial.printf("Saving to stage: %s, len = %d\n", stage_fn, len );
  
  file = SD.open( stage_fn, FILE_WRITE );

  if( file ) {
    file.write( s, len );
    file.close(); 
  }
  else {
    Serial.print("### Error, could not open file "); Serial.print( stage_fn ); Serial.println(" for writing.");
  }
}


// if there is a BANKNAME.TXT, move it to STAGING

void stage_bank_name( uint8_t bank_num ) {

  if( bank_num != BANK_STAGING ) {

    sprintf( src_fn, "/DRMBANKS/%02d/BANKNAME.TXT", bank_num );

    delete_and_copy( STAGING_BANKNAME_FN, src_fn, STAGING_DEFAULT_BANKNAME, strlen(STAGING_DEFAULT_BANKNAME) );
 
    Serial.printf("Staged bank name: %s\n", get_voice_bank_name( BANK_STAGING ));
  }
  else
    Serial.printf("stage_bank_name: bank passed in IS staging - %s\n", get_voice_bank_name( BANK_STAGING ));
}     


uint8_t get_orig_bank_num() {
  uint8_t num = 0x7f;                   // error value

  file = SD.open( STAGING_ORIG_BANKNUM_FN );

  if( file ) {
    file.read( &num, 1 );
  }

  return num;
}


void stage_bank_num( uint8_t bank_num ) {

  if( bank_num != BANK_STAGING ) {

    replace_file( STAGING_ORIG_BANKNUM_FN, &bank_num, 1 );

    Serial.printf("Staged Original Bank Number: %02d\n\n", get_orig_bank_num());
  }
  else
    Serial.printf("stage_bank_num: bank passed in IS staging - %02d\n\n", get_orig_bank_num());
} 



// currently loaded voice sizes

uint8_t loaded_toms_len;        // special: max len 16KB, must be same len as congas
uint8_t loaded_congas_len;      // special: max len 16KB, must be same len as toms


uint8_t needed_hw_len( int len ) {
  uint8_t hw_len = SAMPLE_LEN_32K;
  
  if( (len <= 8192) && (len > 4096) ) hw_len = SAMPLE_LEN_8K;
  else
  if( (len <= 4096) && (len > 2048) ) hw_len = SAMPLE_LEN_4K;
  else
  if( len == 2048 )                   hw_len = SAMPLE_LEN_2K;

  return hw_len;
}


int len_round_up( uint16_t voice, int len ) {
  int rounded = 0;

  if( len <= 2048 ) rounded = 2048;
  else
  if( len <= 4096 ) rounded = 4096;
  else
  if( len <= 8192 ) rounded = 8192;
  else
  if( len <= 32768 ) rounded = 32768;

  if( (voice == STB_TOMS) || (voice == STB_CONGAS) ) {                                // always send full 16384 to both CONGA and TOM
    rounded = 16384;
  }

  return rounded;
}

// this should be the only thing that calls load_voice

void set_voice( uint16_t voice, uint8_t *s, int len, char *vname ) {
  char sdir[64];

  Serial.printf("set_voice(): %04X, %d bytes, %s\n", voice, len, vname);
  //Serial.printf("cga: %d, tom: %d\n", loaded_congas_len, loaded_toms_len );
  
  uint8_t hw_len = SAMPLE_LEN_32K;
  
  // --- zero sample buffer from s[len] to s[32767], in case we need to load a padded sample
  
  for( int xxx = len; xxx != 32768; xxx++ )
    s[xxx] = 0;

  // --- figure out needed hardware sample len value

  hw_len = needed_hw_len( len );

  // -- set our new length and prepare our STAGING path

  switch( voice ) {
    case STB_BASS:      snprintf( sdir, 32, "/STAGING/BASS/"    );                                    break;
    case STB_SNARE:     snprintf( sdir, 32, "/STAGING/SNARE/"   );                                    break;
    case STB_HIHAT:     snprintf( sdir, 32, "/STAGING/HIHAT/"   );                                    break;
    case STB_CLAPS:     snprintf( sdir, 32, "/STAGING/CLAPS/"   );                                    break;
    case STB_CABASA:    snprintf( sdir, 32, "/STAGING/CABASA/"  );                                    break;
    case STB_TAMB:      snprintf( sdir, 32, "/STAGING/TAMB/"    );                                    break;
    case STB_CLAVE:     snprintf( sdir, 32, "/STAGING/CLAVE/"   );                                    break;
    case STB_COWBELL:   snprintf( sdir, 32, "/STAGING/COWBELL/" );                                    break;

    case STB_TOMS:      snprintf( sdir, 32, "/STAGING/TOM/"     );  loaded_toms_len     = hw_len;     break;
    case STB_CONGAS:    snprintf( sdir, 32, "/STAGING/CONGA/"   );  loaded_congas_len   = hw_len;     break;
  }

  //if( cur_bank_num != BANK_STAGING ) {                              // are we loading from STAGING?
    stage_voice( sdir, vname, s, len );                             // store SD card shadow copy
  //}

  // --- Store it in STAGING, and copy it to the voice sample RAM

  if( (voice == STB_TOMS) || (voice == STB_CONGAS) ) {              // always send full 16384 to both CONGA and TOM
    len = 16384;
  }

  Serial.printf("Copying staging file to voice hardware\n");

  disable_drum_trig_interrupt();                                    // don't detect drum writes as triggers
  load_voice_prologue( voice );
  
  load_voice( voice, s, len_round_up(voice, len) );                 // send to the voice board

  if( (voice == STB_TOMS) || (voice == STB_CONGAS) ) {                                                          // always send full 16384 to both CONGA and TOM
    set_sample_length( voice, (loaded_toms_len > loaded_congas_len) ? loaded_toms_len : loaded_congas_len );    // and set length to longer one
  }

  load_voice_epilogue( voice );
  restore_drum_trig_interrupt();                                     // back to normal

  Serial.println();
}



// if the requested voice is CONGA, we need to use the TOM strobe and set D[2]
// in the original LM-1, D[2] selected which EPROM set to use.
// in Luma-1, D[2] selects the hi address bit of the SRAM. 
// this puts TOM in the low 16KB, and CONGA in the hi 16KB.
// this also means this voice board supports sample sizes of 2KB, 4KB, 8KB, or 16KB.
// the voice board SRAM is 32KB. other voice boards support 2KB, 4KB, 8KB, or 32KB.

void trig_voice( uint16_t voice, uint8_t val ) {
    
  if( voice == STB_CONGAS ) {                 // CONGAS and TOMS both on TOMS trigger
    voice = STB_TOMS;
    val |= 0x04;                              // DO[2] = 1 selects CONGAS
  }

  save_LED_SET_2();                           // the way we were

    clr_LED_SET_2( DRUM_DO_ENABLE );          // active low enable in bit 7
    z80_bus_write( voice, val );              // write val to trig
    set_LED_SET_2( DRUM_DO_ENABLE );          // active low enable in bit 7

  restore_LED_SET_2();                        // back like we were before
}



// ASSUMES TRIGGER INTERRUPTS DISABLED

void set_play_load( bool mode ) {
  if( mode )
    trig_port_b_shadow |= PLAY_nLOAD;
  else
    trig_port_b_shadow &= ~PLAY_nLOAD;
  
  //disable_drum_trig_interrupt();
  exp_wr_0( GPIOB, trig_port_b_shadow );
  //restore_drum_trig_interrupt();
}


void set_voice_wr( bool wr ) {
  digitalWriteFast( nVOICE_WR, wr );
}


void set_load_data_and_clock( uint8_t d ) {
  z80_bus_write_speed( STB_CONGAS, d, 100 );
}


// pulse high to reset hihat addr counters
// ASSUMES DRUM TRIG INTERRUPTS DISABLED

void set_rst_hihat( bool s ) {
  if( s )
    trig_port_b_shadow |= RST_HIHAT;
  else
    trig_port_b_shadow &= ~RST_HIHAT;
      
  //disable_drum_trig_interrupt();
  exp_wr_0( GPIOB, trig_port_b_shadow );
  //restore_drum_trig_interrupt();
}


// s = 0 -> nHAT_LOADING = input, s = 1 -> nHAT_LOADING = output, drive to 0
// ASSUMES DRUM TRIG INTERRUPTS DISABLED

void set_hat_loading_led( bool s ) {
  trig_port_a_shadow &= ~nHAT_LOADING;            // should always be 0

  if( s )                                         // turn on LED: make output, drive 0
    trig_dir_a_shadow &= ~nHAT_LOADING;
  else                                            // turn off LED: make input
    trig_dir_a_shadow |= nHAT_LOADING;

  //disable_drum_trig_interrupt();
  exp_wr_0( GPIOA, trig_port_a_shadow );
  exp_wr_0( IODIRA, trig_dir_a_shadow );
  //restore_drum_trig_interrupt();
}

  

void print_voice_name( uint16_t addr ) {
  switch( addr ) {
    case STB_BASS:      Serial.print("BASS ");          break;
    case STB_SNARE:     Serial.print("SNARE ");         break;
    case STB_HIHAT:     Serial.print("HIHAT ");         break;
    case STB_CLAPS:     Serial.print("CLAPS ");         break;
    case STB_CABASA:    Serial.print("CABASA ");        break;
    case STB_TAMB:      Serial.print("TAMB ");          break;
    case STB_TOMS:      Serial.print("TOMS ");          break;
    case STB_CONGAS:    Serial.print("CONGAS ");        break;
    case STB_CLAVE:     Serial.print("CLAVE ");         break;
    case STB_COWBELL:   Serial.print("COWBELL ");       break;
    default:            Serial.print("UNKNOWN ");       break;
  }

  Serial.print("(");
  Serial.print( addr, HEX );
  Serial.print(") ");
}


/*
  --> Assumes Teensy has Z-80 bus

  1. Set desired length in LOAD_DATA[1:0]

  2. Set PLAY/LOAD = 0 (LOAD) and /VOICE_WR = 0

  3. Write a 0 (stop) to desired drum strobe (in z-80 address space)

  4. Set /VOICE_WR = 1

  5. Set PLAY/LOAD = 1 (PLAY)
*/

void set_sample_length( uint16_t voice, uint8_t len ) {

  // 1. Set desired length in LOAD_DATA[1:0]

  set_load_data_and_clock( len&0x03 );      // this will pulse the addr clock to the voice boards,
                                            //  but the trig_voice() call below will reset them.

  //Serial.print("Set Load Data to: "); Serial.println( len&0x03, HEX );

  // 2. Set PLAY/LOAD = 0 (LOAD) and /VOICE_WR = 0

  set_play_load( kLOAD );

  set_voice_wr( 0 );                        // on v1.0 voice boards, this will corrupt SRAM, but we're about to overwrite it all anyway
    
  // 3. Write a 0 (stop) to desired drum strobe (in z-80 address space)

  trig_voice( voice, 0x00 );                // this while /VOICE_WR = 0 will latch the length

  // 4. Set /VOICE_WR = 1

  set_voice_wr( 1 );

  // 5. back to PLAY/LOAD = PLAY
  
  set_play_load( kPLAY );

  //Serial.print("Set voice "); print_voice_name( voice ); Serial.print(" to len "); Serial.println(len);
}


/*
  --> Assumes Teensy has Z-80 bus

  1. PLAY/LOAD = PLAY
  2. write first data byte to DRUM_CONGAS, will latch into U3. the strobe will also pulse the addr clk
  3. PLAY/LOAD = LOAD, subsequent data writes to DRUM_CONGAS will pulse the addr clk and advance the voice board addr counters
  4. trig_voice(0) to target voice, this will zero its addr counters
  5. strobe /VOICE_WR to latch the data byte into voice SRAM at the addr selected by the addr counters
  6. write next data byte to DRUM_CONGAS, will latch data into U3 and pulse addr clk, advancing SRAM addr from addr counters
  goto 5 until all data written
  
*/

void load_sample( uint16_t voice, uint8_t *s, int len ) {
  int progress_tick = 0;
  
  Serial.print("Loading sample data to voice "); print_voice_name( voice );

  Serial.print(", sample len = "); Serial.println( len );

  set_LED_SET_2( LED_LOAD );                  // LOAD LED on, it will flash during loading

  set_play_load( kPLAY );                     // 1. PLAY/LOAD = PLAY

  set_load_data_and_clock( *s++ );            // 2. write first data byte, will also pulse the addr clk
  len--;

  set_play_load( kLOAD );                     // 3. PLAY/LOAD = LOAD, now set_load_data_and_clock() will advance addr counters

  // ZERO THE ADDR COUNTERS ON THE SELECTED VOICE BOARD
  
  // if we're loading the HIHAT, pulse the GPIO that resets the address counters

  if( voice == STB_HIHAT ) {
    set_rst_hihat( 0 );
    set_rst_hihat( 1 );
    set_rst_hihat( 0 );

    set_hat_loading_led( true );              // and turn on the "loading" LED
  }

  // write a '0' then a '1' to the run flip-flop. The clock for that flop is the decoded addr strobe, and D = Z-80 D[0]

  trig_voice( voice, 0x00 );                  // stop
  trig_voice( voice, 0x01 );                  // start


  // --- SAMPLE LOAD LOOP

  while( len >= 0 ) {    
    set_voice_wr( 0 );                        // 5. strobe /VOICE_WR to latch the last U3 data byte into voice SRAM 
    delayNanoseconds( 250 );
    set_voice_wr( 1 );                        //                              at the addr selected by the addr counters

    set_load_data_and_clock( *s++ );          // 6. write next data byte, will also pulse the addr clk

    len--;                                    // one less byte to send

    if( progress_tick++ > 4095 ) {
      clr_LED_SET_2( LED_LOAD );
    }

    if( progress_tick++ > 8191 ) {
      set_LED_SET_2( LED_LOAD );
      progress_tick = 0;
    }
  }
  
  // --- END SAMPLE LOAD LOOP
  

  // CLEAN UP, we're done
  
  clr_LED_SET_2( LED_LOAD );                  // stop flashing the LOAD LED


  // if we just loaded the HIHAT, turn off its LED

  if( voice == STB_HIHAT ) {
    set_rst_hihat( 1 );                       // leave with it disabled, so other loads don't stomp on it

    set_hat_loading_led( false );             // turn off the HiHat LED
  }

  // write a '0' to the run flip-flop. The clock for that flop is the decoded addr strobe, and D = Z-80 D[0]
  
  trig_voice( voice, 0x00 );

  set_play_load( kPLAY );                     // let the 556 timer drive the addr clock again

  set_load_data_and_clock( 0 );               // won't clock addr counters since we're in PLAY mode,
                                              //        this leaves the data lines down to the voice boards all set to 0
                                                
  Serial.println("...done!");
}


void load_voice_prologue( uint16_t v ) {
  //if( v == STB_HIHAT )
    set_rst_hihat( 1 );                       // stop it
}

void load_voice_epilogue( uint16_t v ) {
  //if( v == STB_HIHAT )
    set_rst_hihat( 0 );                       // let it run
}


/*
    Set up the sample length, then load the sample data

    --> Assumes Teensy has Z-80 bus
 */


void load_voice( uint16_t voice, uint8_t *s, int len ) {
  uint8_t voice_len = SAMPLE_LEN_32K;     // assume max

  // progress display

  if( len < 16384 )
    z80_bus_write( LINK_DISPLAY, 0xf0 + (len / 1024) );
  else {
    if( len < 32768 )    
      z80_bus_write( LINK_DISPLAY, 0x16 );
    else
      z80_bus_write( LINK_DISPLAY, 0x32 );
  }
  
  z80_bus_write( PATT_DISPLAY, voice_num_map[(voice & 0xf) - 4]);

  // default is 32K, see initializer above
  
  if( (len <= 8192) && (len > 4096) ) voice_len = SAMPLE_LEN_8K;
  else
  if( (len <= 4096) && (len > 2048) ) voice_len = SAMPLE_LEN_4K;
  else
  if( len == 2048 )                   voice_len = SAMPLE_LEN_2K;

  Serial.print("Load voice: hardware len = ");
  switch( voice_len ) {
    case SAMPLE_LEN_32K:  Serial.println("32KB");     break;
    case SAMPLE_LEN_8K:   Serial.println(" 8KB");     break;
    case SAMPLE_LEN_4K:   Serial.println(" 4KB");     break;
    case SAMPLE_LEN_2K:   Serial.println(" 2KB");     break;
    default:              Serial.println("UNKNOWN!"); break;
  }


  // note that CONGA and TOM must have same sample length.
  // both are accessed with the TOM strobe.
  // last one written will set the length for both.

  set_sample_length( (voice == STB_CONGAS) ? STB_TOMS : voice, voice_len ); 
       

  // loading the CONGA requires setting D[2] and using the TOM strobe.
  // load_sample() detects CONGA loads and does the right thing.
  
  // loading the HIHAT requires strobing the RST_HIHAT signal.
  // load_sample() detects HIHAT loads and does the right thing.
    
  load_sample( voice, s, len );
}


void build_voice_filename( uint16_t voice, uint8_t bank_num, char *fn ) {

  if( bank_num == BANK_STAGING )
    switch( voice ) {
      case STB_CONGAS:    sprintf( fn, "/STAGING/CONGA" );      break;
      case STB_TOMS:      sprintf( fn, "/STAGING/TOM" );        break;
      case STB_SNARE:     sprintf( fn, "/STAGING/SNARE" );      break;
      case STB_BASS:      sprintf( fn, "/STAGING/BASS" );       break;
      case STB_HIHAT:     sprintf( fn, "/STAGING/HIHAT" );      break;
      case STB_COWBELL:   sprintf( fn, "/STAGING/COWBELL" );    break;
      case STB_CLAPS:     sprintf( fn, "/STAGING/CLAPS" );      break;
      case STB_CLAVE:     sprintf( fn, "/STAGING/CLAVE" );      break;
      case STB_TAMB:      sprintf( fn, "/STAGING/TAMB" );       break;
      case STB_CABASA:    sprintf( fn, "/STAGING/CABASA" );     break;    }
  else
    switch( voice ) {
      case STB_CONGAS:    sprintf( fn, "/DRMBANKS/%02d/CONGA",   bank_num );   break;
      case STB_TOMS:      sprintf( fn, "/DRMBANKS/%02d/TOM",     bank_num );   break;
      case STB_SNARE:     sprintf( fn, "/DRMBANKS/%02d/SNARE",   bank_num );   break;
      case STB_BASS:      sprintf( fn, "/DRMBANKS/%02d/BASS",    bank_num );   break;
      case STB_HIHAT:     sprintf( fn, "/DRMBANKS/%02d/HIHAT",   bank_num );   break;
      case STB_COWBELL:   sprintf( fn, "/DRMBANKS/%02d/COWBELL", bank_num );   break;
      case STB_CLAPS:     sprintf( fn, "/DRMBANKS/%02d/CLAPS",   bank_num );   break;
      case STB_CLAVE:     sprintf( fn, "/DRMBANKS/%02d/CLAVE",   bank_num );   break;
      case STB_TAMB:      sprintf( fn, "/DRMBANKS/%02d/TAMB",    bank_num );   break;
      case STB_CABASA:    sprintf( fn, "/DRMBANKS/%02d/CABASA",  bank_num );   break;
    } 
}


/*
    for Voice Banks: /DRMBANKS/bb/BANKNAME.TXT
*/
 
char *get_voice_bank_name( uint8_t bank_num ) {
  
  if( bank_num == BANK_STAGING )
    sprintf( src_fn, STAGING_BANKNAME_FN );
  else
    sprintf( src_fn, "/DRMBANKS/%02d/BANKNAME.TXT", bank_num );

  file = SD.open( src_fn );                           // look for that textfile
  
  if( file ) {
    filesize = file.size();
    memset( filebuf, 0, MAX_BANKNAME_CHARS+1 );
    
    file.read( filebuf, (filesize > MAX_BANKNAME_CHARS) ? MAX_BANKNAME_CHARS : filesize );         
    file.close();
  }
  else {    
    strcpy( (char*)filebuf, (char*)"NO BANK NAME" );
  }

  return (char*)filebuf;
}


void set_voice_bank_name( uint8_t bank_num, char *name ) {

  Serial.printf("Setting Bank %02d name to %s\n", bank_num, name);
  if( bank_num == BANK_STAGING )
    sprintf( src_fn, STAGING_BANKNAME_FN );
  else
    sprintf( src_fn, "/DRMBANKS/%02d/BANKNAME.TXT", bank_num );

  delete_and_copy( src_fn, 0, name, strlen(name) );
}


void write_sd_bank_voice( uint8_t bank, uint8_t drum, char *drum_name, uint8_t *sample_data, int len ) {
  Serial.printf("Writing voice data for %s to bank %02d, drum number %02d, len = %d bytes\n", drum_name, bank, drum, len);

  build_voice_filename( drum_sel_2_voice( drum ), bank, src_fn );

  delete_all_in_dir( src_fn );                      // get rid of any other files in the target dir

  make_dir( src_fn );                               // make sure the target dir exists, create_file() won't create needed dirs

  strcat( src_fn, "/" );
  strcat( src_fn, drum_name );

  create_file( src_fn, sample_data, len );          // actually put the file data there
}



/*
    Voice Banks have a 2-digit decimal name, 00-99, which is their BANK NUMBER.
    This is so we can access them easily with the number keys on the LM-1's control panel, and show the bank # on the LED display.

    A Voice Bank is stored in a particular subdirectory in the DRMBANKS top-level directory on the SD card.
    The naming convention for each of those bank subdirectories is:

    XX

    Where "XX" is 2 digits, from 00 to 99.

    Voice Banks can also have a friendly name like "Original LM-1" or "DMX". 
    This is stored in a file called BANKNAME.TXT in the bank subdirectory.

    NOTE: "BANK_STAGING" is a special bank number that refers to the /STAGING voice bank shadow directory on the SD card
*/

char *get_cur_bank_name() {
  char *n = get_voice_bank_name( cur_bank_num );

  return n;
}


void load_voice_bank( uint16_t voice_selects, uint8_t bank_num ) {
  bool prev = prev_drum_trig_int_enable;
  
  Serial.printf("\n--- Loading voice bank # %02d %s\n\n", bank_num, (bank_num==255)?"(STAGING)":" ");

  cur_bank_num = bank_num;
    
  disable_drum_trig_interrupt();            // XXX should not need to do this, fix properly
  prev_drum_trig_int_enable = false;
  
  if( voice_selects & BANK_LOAD_CONGAS )    { build_voice_filename( STB_CONGAS,   bank_num, fn_buf );   load_voice_file( STB_CONGAS,    fn_buf ); }
  if( voice_selects & BANK_LOAD_TOMS )      { build_voice_filename( STB_TOMS,     bank_num, fn_buf );   load_voice_file( STB_TOMS,      fn_buf ); }
  if( voice_selects & BANK_LOAD_SNARE )     { build_voice_filename( STB_SNARE,    bank_num, fn_buf );   load_voice_file( STB_SNARE,     fn_buf ); }
  if( voice_selects & BANK_LOAD_BASS )      { build_voice_filename( STB_BASS,     bank_num, fn_buf );   load_voice_file( STB_BASS,      fn_buf ); }
  if( voice_selects & BANK_LOAD_HIHAT )     { build_voice_filename( STB_HIHAT,    bank_num, fn_buf );   load_voice_file( STB_HIHAT,     fn_buf ); }
  if( voice_selects & BANK_LOAD_COWBELL )   { build_voice_filename( STB_COWBELL,  bank_num, fn_buf );   load_voice_file( STB_COWBELL,   fn_buf ); }
  if( voice_selects & BANK_LOAD_CLAPS )     { build_voice_filename( STB_CLAPS,    bank_num, fn_buf );   load_voice_file( STB_CLAPS,     fn_buf ); }
  if( voice_selects & BANK_LOAD_CLAVE )     { build_voice_filename( STB_CLAVE,    bank_num, fn_buf );   load_voice_file( STB_CLAVE,     fn_buf ); }     // also RIMSHOT
  if( voice_selects & BANK_LOAD_TAMB )      { build_voice_filename( STB_TAMB,     bank_num, fn_buf );   load_voice_file( STB_TAMB,      fn_buf ); }
  if( voice_selects & BANK_LOAD_CABASA )    { build_voice_filename( STB_CABASA,   bank_num, fn_buf );   load_voice_file( STB_CABASA,    fn_buf ); }

  if( prev )
    enable_drum_trig_interrupt();

  stage_bank_name( bank_num );              // if there is a BANKNAME.TXT, copy it to STAGING
  stage_bank_num( bank_num );               // and remember the original bank we loaded into STAGING

  if( bank_num < 100 )                      // 255 = STAGING
    didProgramChange( bank_num );           // send MIDI Program Change
}


// copies STAGING to bank_num on SD

void store_voice_bank( uint16_t voice_selects, uint8_t bank_num ) {
  Serial.print("--- Storing voice bank # "); Serial.println( bank_num );

  if( voice_selects & BANK_LOAD_CONGAS )    { build_voice_filename( STB_CONGAS,   bank_num, fn_buf );   store_voice_file( (char*)"CONGA",    bank_num ); }
  if( voice_selects & BANK_LOAD_TOMS )      { build_voice_filename( STB_TOMS,     bank_num, fn_buf );   store_voice_file( (char*)"TOM",      bank_num ); }
  if( voice_selects & BANK_LOAD_SNARE )     { build_voice_filename( STB_SNARE,    bank_num, fn_buf );   store_voice_file( (char*)"SNARE",    bank_num ); }
  if( voice_selects & BANK_LOAD_BASS )      { build_voice_filename( STB_BASS,     bank_num, fn_buf );   store_voice_file( (char*)"BASS",     bank_num ); }
  if( voice_selects & BANK_LOAD_HIHAT )     { build_voice_filename( STB_HIHAT,    bank_num, fn_buf );   store_voice_file( (char*)"HIHAT",    bank_num ); }
  if( voice_selects & BANK_LOAD_COWBELL )   { build_voice_filename( STB_COWBELL,  bank_num, fn_buf );   store_voice_file( (char*)"COWBELL",  bank_num ); }
  if( voice_selects & BANK_LOAD_CLAPS )     { build_voice_filename( STB_CLAPS,    bank_num, fn_buf );   store_voice_file( (char*)"CLAPS",    bank_num ); }
  if( voice_selects & BANK_LOAD_CLAVE )     { build_voice_filename( STB_CLAVE,    bank_num, fn_buf );   store_voice_file( (char*)"CLAVE",    bank_num ); }     // also RIMSHOT
  if( voice_selects & BANK_LOAD_TAMB )      { build_voice_filename( STB_TAMB,     bank_num, fn_buf );   store_voice_file( (char*)"TAMB",     bank_num ); }
  if( voice_selects & BANK_LOAD_CABASA )    { build_voice_filename( STB_CABASA,   bank_num, fn_buf );   store_voice_file( (char*)"CABASA",   bank_num ); }

  // -- if there is a /STAGING/BANKNAME.TXT, copy it to the dest bank

  sprintf( src_fn, "/DRMBANKS/%02d/BANKNAME.TXT", bank_num );
  delete_and_copy( src_fn, STAGING_BANKNAME_FN, STAGING_DEFAULT_BANKNAME, strlen(STAGING_DEFAULT_BANKNAME) );
}



uint8_t *get_voice_file( char *dirname, char *voice_name, int *voice_len ) {

  memset( filebuf, 0, 32768 );                                                    // zero buffer, so smaller sounds are padded with silence
  
  Serial.printf("get_voice_file: Opening %s\n", dirname );
  
  if( !get_first_file_in_dir( dirname, voice_name, filebuf, voice_len, 32768 ) )  // will return data in filebuf, name in voice_name, len in voice_len
  {
    Serial.printf("### Error opening file, filling voice mem with ramp\n");  
    
    for( int xxx = 0; xxx != 2048; xxx++ )                                        // build a 2KB ramp
      filebuf[xxx] = (uint8_t)xxx;

    strncpy( voice_name, "RAMP2KB.BIN", 24 );
    *voice_len = 2048;    
  }
  
  return( filebuf );
}


  
void load_voice_file( uint16_t voice, char *fn ) {
  char vname[24];
  int vlen;
  uint8_t *vbuf;
  
  vbuf = get_voice_file( fn, vname, &vlen );

  set_voice( voice, vbuf, vlen, vname );                                      // put it in staging, pad it if needed, put it in voice board      
}


uint8_t *get_voice( uint8_t bank_num, uint16_t voice, char *voice_name, int *voice_len ) {
  
  build_voice_filename( voice, bank_num, fn_buf );
  
  return( get_voice_file( fn_buf, voice_name, voice_len ) );
}
