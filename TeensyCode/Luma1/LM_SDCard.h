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

#ifndef LM_SDCard_
#define LM_SDCard_

#define MAX_LEN_FILE_NAME           24              // filenames can be up to 24 chars         
#define MAX_LEN_PATH_NAME           64              // path can be up to 40 chars

bool init_sd_card();                                // return true if we were able to find the SD card


// -- General purpose file utilities

// used for loading voice files, finds the first file in a directory
// copies up to max_len bytes from that file
// returns # of bytes copied in len
// returns name of file in fn (up to 24 chars)
bool get_first_file_in_dir( char *dirname, char *fn, uint8_t *buf, int *len, int max_len );

bool replace_file( char *fn, uint8_t *buf, int len );     // if "fn" exists, delete it. then create a file named "fn", and write len bytes of buf to it.

void delete_and_copy( char *dst, char * src, char *default_data, int default_len );     // Delete the SD card file at dst, if it exists.
                                                                                        // Then copy the file at src to the filename dst.
                                                                                        // If src doesn't exist, copy default_len bytes from default_data[] to dst.

void delete_all_in_dir( char *dirname );                // delete any/all files in dirname

bool create_file( char *path, uint8_t *d, int len );    // create a file at path, copy len bytes of data d into it


// -- SD Card ROM and RAM file utilities

bool load_z80_rom_file( char *rom_fn );             // normally only called by startup code, typically loads 

void save_ram_bank( uint8_t banknum );              // snapshot Z-80 RAM and save to /RAMBANKS/banknum/RAM_IMAGE_<checksum>.bin

uint8_t *get_ram_bank( uint8_t banknum );           // return buf with 8KB from SD card file (banknum 00-99) or local Z-80 RAM (banknum = 0xff), NULL for error
void load_ram_bank( uint8_t banknum );              // replace Z-80 RAM with first file found in /RAMBANKS/banknum

char *get_ram_bank_name( uint8_t bank_num );        // 0xff for current Z-80 RAM


// --- Miscellaneous SD card routines

uint16_t checksum( uint8_t *d, int len );           // calculate a 16 bit checksum, used for naming RAM save files

void sd_card_ls();                                  // dump the SD card directory

bool format_card();                                 // initialize SD card, create expected dir structure



#endif
