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

#include <SD.h>
#include "LM_SDCard.h"

/* ---------------------------------------------------------------------------------------
    Files, SD Card
*/


uint32_t cardSize;                  // XXX

File root;
File file;

char fn_buf[256];

unsigned long filesize;
uint8_t filebuf[32768];

uint8_t rambuf[8192];



bool init_sd_card() {
  bool card_ok;
  
  card_ok = SD.sdfs.begin( SdioConfig( DMA_SDIO ) );            // can also use FIFO_SDIO for programmed I/O
  
  if( card_ok ) {
    Serial.println("SD card OK!");
    root = SD.open("/");
    SD.sdfs.ls();  
  }
  else {
    Serial.println("*** ERROR, Could not find SD card!");
  }
  
  return card_ok;
}


bool load_z80_rom_file( char *rom_fn ) {

  Serial.print("Opening "); Serial.println( rom_fn );
  
  root = SD.open( rom_fn );
  
  file = root.openNextFile();
  
  if( file ) {
    filesize = file.size();
    Serial.print("name: "); Serial.println( file.name() );
    Serial.print("size: "); Serial.println( filesize );

    file.read( filebuf, filesize );
    
    load_z80_rom( filebuf ); 
  
    file.close();
  } 
  else {
    Serial.println("### Error opening file ");  
    return false;
  }

  return true;
}


void build_rambank_filename( uint8_t bank_num, uint16_t cs, char *fn ) {
  sprintf( fn, "/RAMBANKS/%02d/RAM_IMAGE_%04X.bin",   bank_num, cs );
}


uint16_t checksum( uint8_t *d, int len );

uint16_t checksum( uint8_t *d, int len ) {
  uint16_t cs = 0;

  for( int xxx = 0; xxx != len; xxx++ )
    cs += d[xxx];

  return cs;
}


void save_ram_bank( uint8_t banknum ) {
  if( banknum < 100 ) {
    Serial.print("Saving all of Z-80 RAM to SD Card RAMBANK "); Serial.println( banknum );

    copy_z80_ram( rambuf );

    build_rambank_filename( banknum, checksum(rambuf, 8192), fn_buf );
    Serial.println( fn_buf );
    
    if ( file = SD.open( fn_buf, (O_RDWR | O_CREAT | O_TRUNC) ) ) {
      Serial.println("### open OK");

      file.write( rambuf, 8192 );

      file.close();       // this will flush anything pending to the card
    }
    else {
      Serial.println("### open failed");      
    }

    Serial.println("done!");
  }
  else
    Serial.println("### RAM bank must be between 00 and 99");
}


// fetch RAM image from SD or Z-80 RAM into working buffer "rambuf"
// use bank 0xff for currently active RAM

uint8_t *get_ram_bank( uint8_t banknum ) {
  uint8_t *b = 0;                               // assume failure case, return 0

  if( banknum == 0xff ) {
    Serial.printf("Loading rambuf from Z-80 RAM\n");
    copy_z80_ram( rambuf );
    b = rambuf;
  }
  else {
    if( banknum < 100 ) {
      Serial.printf("Loading rambuf from SD Card RAMBANK %02d\n", banknum);
  
      //build_rambank_filename( banknum, fn_buf );
  
      sprintf( fn_buf, "/RAMBANKS/%02d/", banknum );
  
      root = SD.open( fn_buf );
      
      file = root.openNextFile();
    
      Serial.println( fn_buf );
  
      // open or create file, truncate existing file
      
      if ( file ) {
        Serial.println("### open OK");
  
        filesize = file.size();
        Serial.print("name: "); Serial.println( file.name() );
        Serial.print("size: "); Serial.println( filesize );
      
        file.read( rambuf, 8192 );                              // ignore the file size, it needs to be 8192
        
        file.close();                                           // this will flush anything pending to the card

        b = rambuf;
      }
      else {
        Serial.println("### open failed");      
      }
  
      Serial.println("done!");
    }
    else
      Serial.println("### RAM bank must be between 00 and 99");
  }

  return b;
}


// use bank 0xff for currently active RAM

void load_ram_bank( uint8_t banknum ) {
  uint8_t *b = get_ram_bank( banknum );

  if( b )
    load_z80_ram( b );
  else
    Serial.printf("### Error loading RAM bank %02d\n", banknum);
}



// RAM Bank Name is just the file name.
// Should only be 1 file per RAMBANKS subdirectory.

#define MAX_DISPLAYED_FN_LEN        24

char *get_ram_bank_name( uint8_t bank_num ) {             // 0xff for current active Z-80 RAM

  if( bank_num == 0xff ) {                                // special case, currently active RAM
    copy_z80_ram( rambuf );
    sprintf( (char*)filebuf, "RAM_BANK_%04x", checksum(rambuf,8192));
  }
  else {
    sprintf( (char*)filebuf, "/RAMBANKS/%02d/", bank_num );
  
    root = SD.open( (char*)filebuf );
    file = root.openNextFile();
  
    if( file ) {
      snprintf( (char*)filebuf, MAX_DISPLAYED_FN_LEN, "%s", file.name() );
      file.close();
    }
    else {
      sprintf( (char*)filebuf, "No RAM Bank %02d found", bank_num );
    }
  }

  return (char*)filebuf;
}



// dump the SD card directory
void sd_card_ls() {
  SD.sdfs.ls( LS_R );
  Serial.println();
}



bool format_card() {
  bool r = false;
      
  r = SD.sdfs.begin( SdioConfig( DMA_SDIO ) );          // can also use FIFO_SDIO for programmed I/O

  if( !r ) {
    Serial.println("SD.sdfs.begin() failed");
  }
  else {
    Serial.println("SD.sdfs.begin() OK!");

                                                        // XXX cardSize not working?
    //cardSize = sdEx.card()->cardSize();
    //Serial.print("Card size = "); Serial.print(cardSize); Serial.println(" bytes");
  

                                                        // XXX FAT formatting not working?
    //Serial.print("Formatting...");
    //r = SD.format();
    //Serial.print("Done!, result = "); Serial.println( r ? "OK!" : "### ERR");
  
    root = SD.open("/");                                // get to the root
    SD.sdfs.ls();
    
    if( r ) {

      r = SD.sdfs.begin( SdioConfig( DMA_SDIO ) );      // can also use FIFO_SDIO for programmed I/O

      // --- build DRMBANKS directories
      
      Serial.println("creating DRMBANKS directories");
      for( int xxx = 0; xxx != 100; xxx++ ) {
        Serial.print("Bank "); Serial.println( xxx );
        
        sprintf( fn_buf, "/DRMBANKS/%02d/BASS",     xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/CABASA",   xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/CLAPS",    xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/CLAVE",    xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/CONGA",    xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/COWBELL",  xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/HIHAT",    xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/SNARE",    xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/TAMB",     xxx );    SD.sdfs.mkdir( fn_buf, true );
        sprintf( fn_buf, "/DRMBANKS/%02d/TOM",      xxx );    SD.sdfs.mkdir( fn_buf, true );
      }
      
      // --- build RAMBANKS directories
      
      Serial.println("creating RAMBANKS directories");
      for( int xxx = 0; xxx != 100; xxx++ ) {
        Serial.print("Bank: "); Serial.println( xxx );
        
        sprintf( fn_buf, "/RAMBANKS/%02d", xxx );
        SD.sdfs.mkdir( fn_buf, true );
      }
      
      // --- build STAGING directory
      
      Serial.println("creating STAGING directory");
      SD.sdfs.mkdir( "/STAGING/BASS",    true );
      SD.sdfs.mkdir( "/STAGING/CABASA",  true );
      SD.sdfs.mkdir( "/STAGING/CLAPS",   true );
      SD.sdfs.mkdir( "/STAGING/CLAVE",   true );
      SD.sdfs.mkdir( "/STAGING/CONGA",   true );
      SD.sdfs.mkdir( "/STAGING/COWBELL", true );
      SD.sdfs.mkdir( "/STAGING/HIHAT",   true );
      SD.sdfs.mkdir( "/STAGING/SNARE",   true );
      SD.sdfs.mkdir( "/STAGING/TAMB",    true );
      SD.sdfs.mkdir( "/STAGING/TOM",     true );

      // --- build Z80_CODE directory
      
      Serial.println("creating Z80_CODE directory");
      SD.sdfs.mkdir( "/Z80_CODE", true );

      // -- RAM bank 0 is special, it's the default to load, and contains the factory patterns

      Serial.println("initializing RAM bank 0, default factory settings");
      //load_z80_ram( factory_ram );
      //save_ram_bank( 0 );
      
      Serial.println("DONE!");
    }
  }
  
  return r;
}
