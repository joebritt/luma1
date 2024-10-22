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
#include "LM_EEPROM.h"

/* ---------------------------------------------------------------------------------------
    Files, SD Card
*/


uint32_t cardSize;                  // XXX

File root;
File file;

char fn_buf[256];

char sd_scratch[256];


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


/* ================================================================================================================
    General purpose file utilities
*/

// used for loading voice files, finds the first file in a directory and loads up to max_len bytes of it into buf, returns len in len

bool get_first_file_in_dir( char *dirname, char *fn, uint8_t *buf, int *len, int max_len ) {
  bool status = true;
  int maxdotfiles = 10;

  root = SD.open( dirname );
  
  file = root.openNextFile();                                                 // we will load the 1st file in the dir (should only be 1)
  
  // OSX annoyingly adds these .DS_Store and ._.DS_Store files, skip to next file if we find one

  while( maxdotfiles && file.name()[0] == '.' ) {
    Serial.printf("*** Found %s, skipping to next file\n", file.name());
    file = root.openNextFile();
    maxdotfiles--;
  }

  if( file ) {
    Serial.print("name: "); Serial.println( file.name() );

    filesize = file.size();

    if( filesize > max_len ) {
      Serial.printf("### file too big, it is %d bytes, max is %d bytes, truncating\n", filesize, max_len);
      filesize = max_len;
    }

    Serial.print("size: "); Serial.println( filesize );

    file.read( buf, filesize );                                           // read it into 32KB working buffer

    snprintf( fn, 24, "%s", file.name() );                                // remember what it's called
    *len = filesize;                                                      // and how big it is

    file.close();
  }
  else
    status = false;                                                       // bummer

  return status;
}


// if "fn" exists, delete it. then create a file named "fn", and write len bytes of buf to it.

bool replace_file( char *fn, uint8_t *buf, int len ) {
  bool status = true;

  file = SD.open( fn );                             // see if there is an old one we need to remove

  if( file ) {
    Serial.printf("Found %s, deleting it\n", fn);
    file.close();
    SD.remove( fn );
  }
  else
    Serial.printf("### Did not find %s\n", fn);
  

  file = SD.open( fn, FILE_WRITE );                 // now make a new one

  if( file ) {
    file.write( buf, len );
    file.close();
  }
  else {
    status = false;
    Serial.printf("### Error, could not open %s file for writing.\n", fn);
  }

  return status;
}


// Delete the SD card file at dst, if it exists. Then copy the file at src to the filename dst.
// If src doesn't exist, copy default_len bytes from default_data[] to dst.

void delete_and_copy( char *dst, char *src, char *default_data, int default_len ) {

  Serial.printf("Delete and Copy: %s -> %s\n", src, dst);

  // -- if the dst file exists, delete it

  file = SD.open( dst );

  if( file ) {
    Serial.printf("Found %s, deleting it\n", dst);
    file.close();
    SD.remove( dst );
  }

  // -- now copy the src file to the dst location

  filesize = 0;

  if( src ) {                                         // pass in NULL to always use default_data
    file = SD.open( src );                            // read in the src

    if( file ) {
      filesize = file.size();

      if( filesize > 32768 )
        filesize = 32768;
      
      Serial.printf("src filesize = %d\n", filesize);

      memset( filebuf, 0, filesize );
      
      file.read( filebuf, filesize );         
      file.close();
    }
  }
  else
    Serial.printf("src = NULL, using default data\n");

  file = SD.open( dst, FILE_WRITE );                  // write to the dst

  if( file ) {
    if( filesize )
      file.write( filebuf, filesize );
    else
      file.write( default_data, default_len );

    file.close();
  }
  else {
    Serial.printf("### Error, could not open %s file for writing.\n", dst);
  }
}


// delete any/all files in dirname

void delete_all_in_dir( char *dirname ) {
  bool r;
  Serial.printf("delete_all_in_dir: %s\n", dirname);

  if( root ) {
    for( int xxx = 0; xxx != 20; xxx++ ) {                  // delete up to 20 existing files, more checks needed?
      root = SD.open( dirname );
      file = root.openNextFile();

      if( file ) {
        snprintf( fn_buf, sizeof(fn_buf)-1, dirname );
        strcat( fn_buf, "/" );
        strcat( fn_buf, file.name() );                      // remove() needs full path
        Serial.printf("Found %s, deleting it\n", fn_buf);
        file.close();
        r = SD.remove( fn_buf );
        if( !r )
          Serial.printf("delete_all_in_dir: remove of %s failed\n", fn_buf);
      }
      else {
        Serial.printf("delete_all_in_dir: no files to delete found\n");
        break;
      }
    }
  }
  else
    Serial.printf("delete_all_in_dir: could not open root, this seems bad\n");

  Serial.printf("delete_all_in_dir: done\n");

}


// have this so we can make sure directories exist before trying to use them

bool make_dir( char *path ) {
  Serial.printf("make_dir: %s\n", path);
  return SD.mkdir( path );
}


// create a file at path, dir must already exist, copy len bytes of data d into it

bool create_file( char *path, uint8_t *d, int len ) {
  bool r = true;
  Serial.printf("create_file: %s, len = %d\n", path, len);

  file = SD.open( path, FILE_WRITE );

  if( file ) {
    file.write( d, len );
    file.close();
  }
  else {
    Serial.printf("### create_file, could not open %s file for writing.\n", path);
    r = false;
  }

  return r;
}


/* ================================================================================================================
    SD Card ROM and RAM file utilities
*/

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
  sprintf( fn, "/RAMBANKS/%02d/RAM_IMAGE_%04d.bin",   bank_num, cs );           // change to monotonically increasing # in filename
//  sprintf( fn, "/RAMBANKS/%02d/RAM_IMAGE_%04X.bin",   bank_num, cs );
}


// look for a file, return status.
// if del == true, delete the file that was found.

bool ram_bank_file_exists( uint8_t banknum, bool del ) {
  bool r = false;

  Serial.printf("Checking for ram bank file in bank %02d\n", banknum);

  sprintf( sd_scratch, "/RAMBANKS/%02d/", banknum );

  root = SD.open( sd_scratch );
  
  file = root.openNextFile();

  if ( file ) {
    Serial.printf("### found a ram bank file!\n");
    r = true;

    if( del ) {
      Serial.printf("### DELETING that file\n");
      sprintf( sd_scratch, "/RAMBANKS/%02d/%s", banknum, file.name() );
      Serial.printf("File to delete name = %s\n", sd_scratch);
      SD.remove( sd_scratch );
    }
  }
  else {
    Serial.printf("No ram bank file found for bank %02d\n", banknum);
  }

  return r;
}


/*
  if banknum == 255, snapshot Z-80 RAM and save to /RAMBANKS/banknum/RAM_IMAGE_<checksum>.bin
  if banknum == 00-99, save to /RAMBANKS/banknum/ram_fn (unless NULL, then use RAM_IMAGE_<seqnum>.bin)
*/

void store_ram_bank( uint8_t *ram_image, uint8_t banknum, char *ram_fn ) {

  Serial.printf("store_ram_bank: %s\n", ram_fn );

  // -- clean up any old files in this directory, bound it to 1000 files, should be more than enough

  for( int xxx = 0; xxx != 1000; xxx++ ) {
    if ( ram_bank_file_exists( banknum, true ) == false )       // the "true" means to delete if a file is found, returns false when no file found
      break;
  }

  // -- now write the new file

  if ( file = SD.open( ram_fn, (O_RDWR | O_CREAT | O_TRUNC) ) ) {
    Serial.println("### open OK");

    file.write( ram_image, 8192 );

    file.close();       // this will flush anything pending to the card
  }
  else {
    Serial.printf("### %s: open failed\n", ram_fn);      
  }
}


// Copy active Z-80 RAM to /RAMBANKS/banknum/RAM_IMAGE_xxxx.bin, where xxxx is a checksum

void copy_ram_to_sd_bank( uint8_t banknum ) {
  if( banknum < 100 ) {
    Serial.printf("Saving all of Z-80 RAM to SD Card RAMBANK %02d\n", banknum );

    // -- now copy the current RAM into what should be an empty RAMBANKS directory

    copy_z80_ram( rambuf );

    //build_rambank_filename( banknum, eeprom_next_rambank_num(), fn_buf );     // next monotonically increasing # for filename
    build_rambank_filename( banknum, checksum(rambuf, 8192), fn_buf );
    Serial.printf("Will save to %s\n", fn_buf);

    store_ram_bank( rambuf, banknum, fn_buf );

    Serial.printf("done!\n");
  }
  else {
    Serial.printf("### RAM bank must be between 00 and 99\n");
  }
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

  if( bank_num == 0xff ) {                                                    // special case, currently active RAM
    copy_z80_ram( rambuf );
    if( !get_active_ram_bank_name() )                                         // big hack, if there is a filename, this copies it to filebuf, then we exit and return that
      sprintf( (char*)filebuf, "RAM_BANK_%04x", checksum(rambuf,8192));       // otherwise, put the default filename into filebuf
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

  Serial.printf("get_ram_bank_name: returning %s\n", filebuf);
  
  return (char*)filebuf;
}


// set active RAM bank (bank 255) name

void set_active_ram_bank_name( char *ram_fn ) {
  Serial.printf("set_active_ram_bank_name: %s", ram_fn );

  sprintf( (char*)filebuf, "/RAMBANKS/ACTIVE.TXT" );

  if ( file = SD.open( filebuf, (O_RDWR | O_CREAT | O_TRUNC) ) ) {

    file.write( ram_fn, 24 );

    file.close();       // this will flush anything pending to the card  

    Serial.printf(" - successful\n");
  }
  else
    Serial.printf(" - FAILED\n");
}

char *get_active_ram_bank_name() {
  char *n = 0;

  Serial.printf("get_active_ram_bank_name: ");

  sprintf( (char*)filebuf, "/RAMBANKS/ACTIVE.TXT" );

  file = SD.open( (char*)filebuf );

  if( file ) {
    file.read( filebuf, 24 );
    file.close();
    Serial.printf("opened, found %s\n", filebuf);
    n = filebuf;
  }
  else {
    Serial.printf("could not open /RAMBANKS/ACTIVE.TXT, returning NULL\n");
  }
  
  return n;
}


/* ================================================================================================================
    Miscellaneous SD card routines
*/

uint16_t checksum( uint8_t *d, int len ) {
  uint16_t cs = 0;

  for( int xxx = 0; xxx != len; xxx++ )
    cs += d[xxx];

  return cs;
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
