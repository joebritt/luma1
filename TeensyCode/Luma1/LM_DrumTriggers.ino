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

#ifdef ARDUINO_TEENSY41
#include <Wire.h>
#else
#include <i2c_t3.h>
#endif

#include "LM_DrumTriggers.h"

//#define WIRE_INTERFACES_COUNT         2


volatile uint8_t drum_triggers_a;           // raw bits read from i2c port expander, non-interrupt code will parse
volatile uint8_t drum_triggers_b;
volatile uint8_t drum_modifiers;

volatile bool check_triggers = false;

volatile uint8_t trig_port_a_shadow;        // for voice loading control bit outputs
volatile uint8_t trig_port_b_shadow;

volatile uint8_t trig_dir_a_shadow;         // for voice loading control bit outputs
volatile uint8_t trig_dir_b_shadow;

// Port A
#define DRUM_CABASA             0x01
#define DRUM_TAMB               0x02
#define DRUM_TOMS               0x04
#define DRUM_CONGAS             0x08
#define DRUM_COWBELL            0x10
#define DRUM_CLAVE              0x20
#define DRUM_CLICK              0x40
#define nHAT_LOADING            0x80        // output

// Port B
#define PLAY_nLOAD              0x01        // output
#define DRM_DO0                 0x02        // input  drum modifier bit 0
#define DRM_DO1                 0x04        // input  drum modifier bit 1
#define RST_HIHAT               0x08        // output
#define DRUM_HIHAT              0x10
#define DRUM_SNARE              0x20
#define DRUM_BASS               0x40
#define DRUM_CLAPS              0x80

// MCP23018 registers

#define IODIRA          0x00      // IO Direction (1 = input, 0xff on reset)
#define IODIRB          0x01
#define IOPOLA          0x02      // IO Polarity (1 = invert, 0x00 on reset)
#define IOPOLB          0x03
#define GPINTENA        0x04      // 1 = enable interrupt on change
#define GPINTENB        0x05
#define DEFVALA         0x06      // default compare values for interrupt on change, 0xoo on reset
#define DEFVALB         0x07
#define INTCONA         0x08      // 1 = pin compared to DEFVAL, 0 = pin compare to previous pin value
#define INTCONB         0x09
#define IOCON           0x0a      // see bit definitions below
//#define IOCON           0x0b    // duplicate
#define GPPUA           0x0c      // 1 = pin pullup enabled, 0x00 on reset
#define GPPUB           0x0d
#define INTFA           0x0e      // interrupt flags, 1 = associated pin caused interrupt
#define INTFB           0x0f
#define INTCAPA         0x10      // interrupt capture, holds GPIO port values when interrupt occurred
#define INTCAPB         0x11
#define GPIOA           0x12      // read: value on port, write: modify OLAT (output latch) register
#define GPIOB           0x13
#define OLATA           0x14      // read: value in OLAT, write: modify OLAT
#define OLATB           0x15

// IOCON bit values

#define IOCON_BANK      0x80      // 1: regs in different banks, 0: regs in same bank (addrs are seq, default)
#define IOCON_MIRROR    0x40      // 1: 2 INT pins are OR'ed, 0: 2 INT pins are independent (default)
#define IOCON_SEQOP     0x20      // 1: sequential ops disabled, addr ptr does NOT increment, 0: addr ptr increments (default)
#define IOCON_ODR       0x04      // 1: INT pin is Open Drain, 0: INT pin is active drive (default)
#define IOCON_INTPOL    0x02      // 1: INT pin is active hi, 0: INT pin is active lo (default)
#define IOCON_INTCC     0x01      // 1: reading INTCAP clears interrupt, 0: reading GPIO reg clears interrupt (default)

/* ---------------------------------------------------------------------------------------
    i2c Port Expander, CPU board drum trigger detection

    XXX THIS IS ON I2C PORT 0

    23018 i2c port expander -> Drum trigger signals
    
    GPA0      CABASA
    GPA1      TAMB
    GPA2      TOMS
    GPA3      CONGAS
    GPA4      COWBELL
    GPA5      CLAVE
    GPA6      CLICK
    GPA7       /HAT_LOADING         output
    
    GPB0       PLAY/LOAD            output
    GPB1      Drum D[0] -> D[1]
    GPB2      Drum D[1] -> D[2]
    GPB3       RST_HIHAT            output
    GPB4      HIHAT
    GPB5      SNARE
    GPB6      BASS
    GPB7      CLAPS
*/



#define TRIGGER_EXP_ADDR     0x20          // what you get with ADDR grounded


/* ---------------------------------------------
   MCP23018 i2c port expander registers
   
   IOCON.BANK = 0 (default)
*/

void exp_irq( void );

bool expander_0_found = false;    // used to bypass i2c operations if we don't find the port expander at boot

void exp_wr_0( uint8_t addr, uint8_t val ) {
  byte err;
  
  Wire.beginTransmission( TRIGGER_EXP_ADDR );
  Wire.write( addr );
  Wire.write( val );
  err = Wire.endTransmission();

  if( err == 0 ) {
    expander_0_found = true;        // excellent, we have the i2c expander!
  }
}

uint8_t exp_rd_0( uint8_t addr ) {
  uint8_t r;
  
  Wire.beginTransmission( TRIGGER_EXP_ADDR );
  Wire.write( addr );
  Wire.endTransmission();  

  Wire.requestFrom( TRIGGER_EXP_ADDR, 1 );
  r = Wire.read();
  
  return r;
}


void init_drum_trig_expander( void ) {
  
  pinMode( EXP_IRQ_A, INPUT );
  pinMode( EXP_IRQ_B, INPUT );
  
  // crank up i2c
  
  pinMode( I2C_SDA_0, OUTPUT );             // first drive both lines low
  pinMode( I2C_SCL_0, OUTPUT );             // this seems to make probe work better when nothing is connected
  digitalWrite( I2C_SDA_0, LOW );
  digitalWrite( I2C_SCL_0, LOW );
  
  Wire.begin();

#ifndef ARDUINO_TEENSY41
  Wire.setOpMode(I2C_OP_MODE_IMM);
#endif

  Wire.setClock(1000000);                   // need to be at least this fast to reliably catch drum trigger modifiers
                                            // verified on scope that we are OK up to 2MHz with 2.4K pullups on SCL/SDA
#ifndef ARDUINO_TEENSY41
  Wire.resetBus();
#endif

  Serial.print("initializing Drum Trigger port expander...");  

  exp_wr_0( IOCON,        IOCON_INTCC );    // INTA/B independent, active low int, read INTCAP to clear interrupt

  Serial.println("back");

  if( expander_0_found ) {
    Serial.println("Found Drum Trigger port expander!");

    // MCP23018 outputs are all open-drain, need to enable pullups for RST_HIHAT and PLAY_nLOAD
    //                                      but NOT for nHAT_LOADING, that's just used as and open drain pulldown

    trig_dir_a_shadow = 0xff;                     // GPA all inputs
    trig_dir_a_shadow &= ~nHAT_LOADING;           //                except for nHAT_LOADING
    exp_wr_0( IODIRA,     trig_dir_a_shadow );

    trig_port_a_shadow = nHAT_LOADING;            // /HAT_LOADING = 1
    exp_wr_0( GPIOA,      trig_port_a_shadow );           

    trig_dir_b_shadow = 0xff;                     // GPB all inputs
    trig_dir_b_shadow &= ~RST_HIHAT;              //                except for RST_HIHAT and PLAY_nLOAD
    trig_dir_b_shadow &= ~PLAY_nLOAD;     
    exp_wr_0( IODIRB,     trig_dir_b_shadow );

    trig_port_b_shadow = PLAY_nLOAD;              // RST_HIHAT = 0, PLAY_LOAD = 1
    exp_wr_0( GPIOB,      trig_port_b_shadow );     

    exp_wr_0( GPPUB,      0xff );                 // all outputs have pullups, outputs are RST_HIHAT and PLAY_nLOAD


    exp_wr_0( IOPOLA,     0x7f );                 // strobes are active low, let the MCP23018 flip them for us
    exp_wr_0( IOPOLB,     0xf0 );

    exp_wr_0( DEFVALA,    0x00 );                 // the strobes natually fall, but since we invert with IOPOL,
    exp_wr_0( DEFVALB,    0x00 );                 //  the DEFVALs need to be inverted too

    exp_wr_0( INTCONA,    0xff );                 // interrupt from compare against DEFVAL
    exp_wr_0( INTCONB,    0xff );

    exp_wr_0( GPINTENA,   0x77 );                 // all but GPIOA[7], which is /HAT_LOADING, and GPIOA[3], which is CONGAS (see loading scheme)
    exp_wr_0( GPINTENB,   0xf0 );                 // all but GPIOB[3:0], which is: drum data[1:0], PLAY_LOAD, and RST_HIHAT

    exp_rd_0( INTCAPA );                          // clear the interrupt
    exp_rd_0( INTCAPB );
  }
  else {
    Serial.println("### Drum Trigger port expander NOT FOUND!");
  } 
}



bool drum_trig_int_enabled = false;

/*
int trig_irq_counting_semaphore = 0;
void hw_en_drum_trig_interrupt( bool en );

void enable_drum_trig_interrupt( bool en ) {

  Serial.printf("--> IRQ %s: %d\n", en?"ENA":"DIS", trig_irq_counting_semaphore );
  
  if( en ) {                                       // if we are being asked to enable drum trigger interrupts
    if( trig_irq_counting_semaphore == 0 ) {         // and we are currently NOT
      trig_irq_counting_semaphore++;                 //   mark that we are now enabled
      hw_en_drum_trig_interrupt( true );             //   and do so
    }
    else
      trig_irq_counting_semaphore++;                 // we ARE already enabled, just mark that there is another layer of request
  }
  else {                                           // if we are being asked to disable drum trigger interrupts
    if( trig_irq_counting_semaphore > 0 )            // and we currently ARE enabled
      trig_irq_counting_semaphore--;                 // one less request to disable

    if( trig_irq_counting_semaphore == 0 ) {         // if that got us to 0, it's time to disable
      hw_en_drum_trig_interrupt( false );            //   there'll be no bargin' in and there'll be no dissin'
    }
  }
}

void hw_en_drum_trig_interrupt( bool en ) {

  Serial.printf("--> HW IRQ %s: %d\n", en?"ENA":"DIS", trig_irq_counting_semaphore );

  if( en ) {
    exp_rd_0( INTCAPA );                          // clear any pending interrupt
    exp_rd_0( INTCAPB );

    drum_triggers_a = 0;
    drum_triggers_b = 0;
    drum_modifiers = 0;
    check_triggers = false;
    
    Wire.beginTransmission( TRIGGER_EXP_ADDR );  // set us up to read the GPIOB reg as soon as we come in next time
    Wire.write( GPIOB );
    Wire.endTransmission(); 
    
    attachInterrupt( digitalPinToInterrupt(EXP_IRQ_A),  exp_irq_a, FALLING );
    attachInterrupt( digitalPinToInterrupt(EXP_IRQ_B),  exp_irq_b, FALLING );
  
    drum_trig_int_enabled = true;
  }
  else {
    detachInterrupt( digitalPinToInterrupt(EXP_IRQ_A) );
    detachInterrupt( digitalPinToInterrupt(EXP_IRQ_B) );
  
    drum_trig_int_enabled = false;
  }
}
*/


bool prev_drum_trig_int_enable = false;

void enable_drum_trig_interrupt() {

  //Serial.printf("*** Trig IRQ ENABLE: cur = %d, prev = %d\n", drum_trig_int_enabled, prev_drum_trig_int_enable );
  
  if( drum_trig_int_enabled == false ) {
    //Serial.println("enable drum trig interrupt");

    exp_rd_0( INTCAPA );                          // clear any pending interrupt
    exp_rd_0( INTCAPB );

    drum_triggers_a = 0;
    drum_triggers_b = 0;
    drum_modifiers = 0;
    check_triggers = false;
    
    Wire.beginTransmission( TRIGGER_EXP_ADDR );  // set us up to read the GPIOB reg as soon as we come in next time
    Wire.write( GPIOB );
    Wire.endTransmission(); 
    
    attachInterrupt( digitalPinToInterrupt(EXP_IRQ_A),  exp_irq_a, FALLING );
    attachInterrupt( digitalPinToInterrupt(EXP_IRQ_B),  exp_irq_b, FALLING );
  
    drum_trig_int_enabled = true;

    prev_drum_trig_int_enable = true;
  }
}


void disable_drum_trig_interrupt() {

  //Serial.printf("*** Trig IRQ Disable: cur = %d, prev = %d\n", drum_trig_int_enabled, prev_drum_trig_int_enable );

  if( drum_trig_int_enabled == true ) {
    prev_drum_trig_int_enable = drum_trig_int_enabled;        // remember for restore
    
    if( drum_trig_int_enabled == true ) {
      
      //Serial.println("disable drum trig interrupt");
    
      detachInterrupt( digitalPinToInterrupt(EXP_IRQ_A) );
      detachInterrupt( digitalPinToInterrupt(EXP_IRQ_B) );
    
      drum_trig_int_enabled = false;
    }
  }
  
}


void restore_drum_trig_interrupt() {
  //Serial.printf("*** Trig IRQ RESTORE: cur = %d, prev = %d\n", drum_trig_int_enabled, prev_drum_trig_int_enable );

  if( prev_drum_trig_int_enable )
    enable_drum_trig_interrupt();
}




void exp_irq_a( void ) {  
  Wire.requestFrom( TRIGGER_EXP_ADDR, 1 );         // we left the address pointer -> GPIOB so we can grab it fast
  drum_modifiers = Wire.read();

  Wire.beginTransmission( TRIGGER_EXP_ADDR );
  Wire.write( INTCAPA );
  Wire.endTransmission();  
  Wire.requestFrom( TRIGGER_EXP_ADDR, 1 );
  drum_triggers_a = Wire.read();

  Wire.beginTransmission( TRIGGER_EXP_ADDR );      // set us up to read the GPIOB reg as soon as we come in next time
  Wire.write( GPIOB );
  Wire.endTransmission();

  drum_triggers_b = 0;
  
  check_triggers = true;
}


void exp_irq_b( void ) {  
  Wire.requestFrom( TRIGGER_EXP_ADDR, 1 );         // we left the address pointer -> GPIOB so we can grab it fast
  drum_modifiers = Wire.read();

  Wire.beginTransmission( TRIGGER_EXP_ADDR );
  Wire.write( INTCAPB );
  Wire.endTransmission();  
  Wire.requestFrom( TRIGGER_EXP_ADDR, 1 );
  drum_triggers_b = Wire.read();

  Wire.beginTransmission( TRIGGER_EXP_ADDR );      // set us up to read the GPIOB reg as soon as we come in next time
  Wire.write( GPIOB );
  Wire.endTransmission(); 

  drum_triggers_a = 0;

  check_triggers = true;
}
