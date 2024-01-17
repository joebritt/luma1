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

#ifndef LM_PINMAP_H_
#define LM_PINMAP_H_

/* ---------------------------------------------------------------------------------------
    Hardware Specifics
 */

#define HW_MIDI                Serial1           // pins 0 (rx) & 1 (tx)
                                                  // MIDI_SERIAL.begin( 31250, SERIAL_8N1_TXINV );
                                                  // our hardware is inverted on the transmit side only!

#define ADDR_13                 2                 // Z-80 bus Address bit
#define EXP_IRQ_A               3                 // IRQ from i2c expander on CPU board

#define ADDR_11                 4                 // Z-80 bus Address bit

#define nBUSRQ                  5                 // active low Bus Request to Z_80
#define nBUSAK                  6                 // active low Bus Acknowledge from Z-80

#define ADDR_9                  7                 // Z-80 bus Address bit
#define ADDR_8                  8                 // Z-80 bus Address bit
#define ADDR_7                  9                 // Z-80 bus Address bit

#define DATA_4                  10                // Z-80 bus Data bit
#define DATA_5                  11                // Z-80 bus Data bit
#define DATA_6                  12                // Z-80 bus Data bit

#define nRST                    13                // Z-80 /RST signal and LED on Teensy board

#define ADDR_10                 14                // Z-80 bus Address bit

#define nMREQ                   15                // active low Memory Request to Z-80 bus

#ifdef ARDUINO_TEENSY41
  #define nRD                   37                // active low Read strobe to Z-80 bus
  #define nWR                   38                // active low Write strobe to Z-80 bus
#else
  #define nRD                   16                // active low Read strobe to Z-80 bus
  #define nWR                   17                // active low Write strobe to Z-80 bus
#endif

#define I2C_SDA_0               18                // i2c bus 0 Data
#define I2C_SCL_0               19                // i2c bus 0 Clock

#define ADDR_12                 20                // Z-80 bus Address bit

#define TAPE_FSK_TTL            21                // Tempo clock out
#define DECODED_TAPE_SYNC_CLK   22                // From MIDI Clock, replaces old Tape Sync Clock

#define LM1_LED_A               23                // STORE LED

#define DATA_7                  24                // Z-80 bus Data bit
#define DATA_1                  25                // Z-80 bus Data bit
#define DATA_0                  26                // Z-80 bus Data bit
#define DATA_2                  27                // Z-80 bus Data bit
#define DATA_3                  28                // Z-80 bus Data bit

#define EXP_IRQ_B               29
#define nVOICE_WR               30                // voice board write strobe

#define ADDR_5                  31                // Z-80 bus Address bit
#define ADDR_4                  32                // Z-80 bus Address bit
#define ADDR_0                  33                // Z-80 bus Address bit
#define ADDR_1                  34                // Z-80 bus Address bit
#define ADDR_2                  35                // Z-80 bus Address bit
#define ADDR_3                  36                // Z-80 bus Address bit

#ifdef ARDUINO_TEENSY41
  #define I2C_SCL_1             16                // i2c bus 1 Clock
  #define I2C_SDA_1             17                // i2c bus 1 Data
#else
  #define I2C_SCL_1             37                // i2c bus 1 Clock
  #define I2C_SDA_1             38                // i2c bus 1 Data
#endif

#define ADDR_6                  39                // Z-80 bus Address bit

#ifdef ARDUINO_TEENSY41
  #define nDRIVE_DATA           40                // drive Teensy Data bus to Z-80 bus
  #define nDRIVE_ADDR           41                // drive Teensy Address bus to Z-80 bus
#endif

#endif
