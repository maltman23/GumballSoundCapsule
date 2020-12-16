/*
GumballSound  --  This plays a bizarre composition with an interesting waveform in a wave tables,
                     pulsing a red, green, and blue LED along the way.
              --  The hardware fits into a 50mm diameter plastic capsule for a gumball machine.
Firmware
for use with ATtiny13a
7-Dec-2020  Mitch Altman

Distributed under Creative Commons 4.0 -- Attib & Share Alike
CC BY-SA

Hacked by Mitch from Brain Machine firmware by Mitch Altman and Limor Fried (Adafruit)

Thanks to Lukasz Podkalicki for his PWM example firmware for the ATtiny13 <https://blog.podkalicki.com/attiny13-hardware-pwm/>
*/


#include <avr/io.h>             // this contains all the IO port definitions
#include <avr/interrupt.h>      // definitions for interrupts
#include <avr/pgmspace.h>       // definitions or keeping constants in program memory

// this macro is needed for binary numbers on some versions of gcc ("0b" prefix is fine for the gcc that comes with WinAVR)
#define HEX__(n) 0x##n##UL

#define B8__(x) ((x&0x0000000FLU)?1:0)  \
               +((x&0x000000F0LU)?2:0)  \
               +((x&0x00000F00LU)?4:0)  \
               +((x&0x0000F000LU)?8:0)  \
               +((x&0x000F0000LU)?16:0) \
               +((x&0x00F00000LU)?32:0) \
               +((x&0x0F000000LU)?64:0) \
               +((x&0xF0000000LU)?128:0)

#define B8(d) ((unsigned char)B8__(HEX__(d)))

/*
The hardware for this project is very simple:
     ATtiny13a has 8 pins:
       pin 1   no connection
       pin 2   PB3 - blue LED -- through a 1K ohm resistor to +3V
       pin 3   no connection
       pin 4   ground
       pin 5   OC1A (PB0) - to speaker -- through a 1000uF cap to +3V
       pin 6   PB1 - green LED -- through a 1K ohm resistor to +3V
       pin 7   PB2 - red LED -- through a 1K ohm resistor to +3V
       pin 8   +3V (CR2032 through a switch)
The hardware fits into a 50mm diameter plastic capsule for a gumball machine.

    This firmware requires that the clock frequency of the ATtiny13a
      is:  9.6MHz internal oscillator
*/


/*
-----------------------------------------------------
   PROGMEM and pgm_read_byte() and pgm_read_word()
-----------------------------------------------------

Unless we go out of our way to tell the avr gcc C compiler otherwise, it
will always create code that transfers all data from program memory into
RAM when the code starts excecuting (at power on or reset).

This firmware has fairly large tables that we do not want to transfer into
RAM:
   gumballWavTab[] -- for storing the waveform to play
   pitchTab        -- for storing the pitches to play
The C compiler needs to be told to keep the tables in program memory.
To do this we use the PROGMEM macro.  Please see the tables, below, to see
PROGMEM in use.

Also, the avr gcc C compiler always assumes that data are in RAM (and not
in program memory).  Since we told the compiler to keep the tables in
program memory, when we access the elements of the tables we need to tell
the compiler to access them from program memory (and not from RAM).  We do
this by using the pgm_read_byte() and pgr_read_word() macros.  These
macros require giving the table names as addresses, i.e., precede the name
with "&".

Examples:
To access the byte in the fourth value in gumballWavTab[] , this is how we
do it:
     pgm_read_byte( &( gumballWavTab[3] ) );

To access the gumballPitch (which is a byte) in the seventh element of the
pitchTab[] , this is how we do it:
     pgm_read_byte( &pitchTab[6].gumballPitch );

To access the pitchDuration (which is a word) in the seventh element of
the pitchTab[] , this is how we do it:
     pgm_read_word( &pitchTab[6].pitchDuration );

To make use of PROGMEM and the pgm_read_byte() and pgm_read_word() macros
this file includes pgmspace.h (you'll see that near the top of this file).
*/



//--------------------
// Gumball waveform table
//   (this is an interesting sound I created using CoolEdit Pro)
const uint8_t gumballWavTabSize = 92;
const uint8_t gumballWavTab[] PROGMEM = {
  0x8a, 0xb1, 0x55, 0x4d, 0xb2, 0x90, 0x43, 0x8f, 0xb7, 0x4f, 0x54, 0xbd,
  0x8c, 0x35, 0x98, 0xb8, 0x3a, 0x70, 0xcb, 0x4c, 0x51, 0xd7, 0x5d, 0x47,
  0xd2, 0x69, 0x3a, 0xde, 0x54, 0x4c, 0xe4, 0x30, 0x7b, 0xcf, 0x0f, 0xc5,
  0x82, 0x2e, 0xf3, 0x13, 0xb2, 0x91, 0x2c, 0xf5, 0x01, 0xe0, 0x45, 0x83,
  0xa8, 0x2e, 0xe9, 0x05, 0xf6, 0x13, 0xd3, 0x47, 0x96, 0x80, 0x61, 0xac,
  0x3e, 0xc9, 0x26, 0xdc, 0x1d, 0xdc, 0x27, 0xc6, 0x43, 0xa8, 0x60, 0x89,
  0x83, 0x65, 0xac, 0x40, 0xc6, 0x30, 0xc2, 0x45, 0xa0, 0x74, 0x6e, 0xa5,
  0x46, 0xba, 0x4b, 0x94, 0x89, 0x56, 0xb7, 0x59
};



//--------------------
// pitchTab[] is a one-dimensional array.
// pitchTab[] is a table of values for pitches to play, and the duration of how long to play the pitch
struct pitchElement {
  // each pitchElement in this table has two values:
  uint8_t gumballPitch;    // this is a number between 10 and 255
                           //    10 is the highest pitch (and is played the fasted)
                           //    255 is the lowest pitch (and takes the longest to play)
  uint16_t pitchDuration;  // length of time to repeat playing this pitch
                           //    useful values are between 200 (very short) and 65535 (very long)
                           // NOTE: for lower-pitch sounds (a high gumballPitch value)
                           //          a given pitchDuration will take longer to play
// the last element in the table has its gumballPitch = 0
} const pitchTab[] PROGMEM = {
  { 100,  280 },  { 150,  250 },  { 180,  300 },  {  90,  800 },  { 120,  500 },
  { 200,   50 },  { 120,  280 },  {  95,  282 },  {  90,  285 },  { 180,  350 },
  { 150,  380 },  { 120,  280 },  {  95,  410 },  {  90,  285 },  {  70,  500 },
  { 200,   50 },  {  70,  180 },  {  65, 1000 },  {  70, 150  },  {  80,  180 },
  {  90,  285 },  {  80,  270 },  {  12,   50 },  {  50, 2000 },  { 200,  500 },
  {  80,  500 },  { 100,  500 },  {  255, 800 },  { 100,  100 },  {  96,  100 },
  {  92,  100 },  {  88,  200 },  {  84,  250 },  {  80,  300 },  {  77,  350 },
  {  74,  400 },  {  71,  200 },  {  68,  200 },  {  65,  200 },  {  62,  200 },
  {  59,  190 },  {  56,  180 },  {  53,  170 },  {  53,  160 },  {  50,  150 },
  {  48,  140 },  {  46,  130 },  {  44,  120 },  {  42,  110 },  {  40,  100 },
  {  38,  100 },  {  36,  100 },  {  34,  100 },  {  32,  100 },  {  30,  100 },
  {  28,  100 },  {  26,  100 },  {  24,  100 },  {  22,   90 },  {  20,   70 },
  {  18,   60 },  {  16,   50 },  {  14,   40 },  {  10,  100 },  {  16,   50 },
  {  20,   70 },  {  36,  100 },  {  50,  150 },  {  62,  200 },  {  71,  200 },
  {  80,  150 },  {  92,  130 },
  {   0,    0 }
};



//--------------------
// This delay function delays units of time.
// The units have longer duration when delCount is bigger.
//   units can be between 0 and 65535
//   delCount can be between 0 and 65535
#define TENTH_MS    112   // when delCount is 112, the units of time are: 1/10 millisecond
#define SAMP         10   // when delCount is 10, it is a good amount for delaying between outputting samples from the waveform wave table
#define ONE_SEC   10000   // when units=ONE_SEC and delCount=TENTH_MS, the delay will be 1 second
void delaySomeTime(unsigned long int units, unsigned long int delayCount) {
  unsigned long int timer;

  while (units != 0) {
    // Toggling PB5 is done here to force the compiler to do this loop, rather than optimize it away
    for (timer=0; timer <= delayCount; timer++) {PINB |= B8(00100000);};
    units--;
  }
}



//--------------------
// This function blinks the LEDs (connected to PB1 (green), PB2 (red), PB3 (blue) )
//   at the rate determined by onTime and offTime
// This function also acts as a delay for the Duration specified (onTime+offTime)
// onTime = time the LEDs are on (divde by 10 to get msec)
// offTime = time the LEDs are off (divde by 10 to get msec)
void blink_LEDs( unsigned long int duration, unsigned long int onTime, unsigned long int offTime) {
  for (int i=0; i<(duration/(onTime+offTime)); i++) {
    PORTB |= B8(00001110);             // turn on LEDs at PB1, PB2, PB3
    delaySomeTime(onTime, TENTH_MS);   //   for onTime
    PORTB &= B8(11110001);             // turn off LEDs at PB1, PB2, PB3
    delaySomeTime(offTime, TENTH_MS);  //   for offTime
  }
}



//--------------------
int main(void) {

  uint8_t  gumIndex;    // index into gumballWavTab[]
  uint8_t  gumWavDat;   // values read from gumballWavTab[]
  uint8_t  pitchIndex;  // index into pitchTab[]
  uint8_t  pitchRate;   // values read from pitchTab[].gumballPitch (the rate at which to play the waveform in gumballWavTab[])
  uint16_t pitchLen;    // values read from pitchTab[].pitchDuration (the length of time to play a pitch)

  // initialize Timer0 in Fast PWM mode (from BOT (0x00) to MAX (0xFF) with Compare Match on OC0A value),
  // with OC0A outputting on PB0 pin, with no prescaling
  DDRB |= _BV(PB0);                // set OC0A PWM pin (PB0) as output
  TCCR0A |= _BV(COM0A1);           // TCCR0A -- COM0A1:COM0A0=10 to clear OC0A on Compare Match, set OC0A at TOP
                                   // TCCR0A -- COM0B1:COM0B0=00 for Timer0 OC0B disconnected
                                   // TCCR0A -- bits 3:2 are unused
  TCCR0A |= _BV(WGM01)|_BV(WGM00); // TCCR0A -- WGM01:WGM00=11 for Fast PWM mode (BOT to MAX, with Compare Match on OC0A value)
                                   // TCCR0B -- FOC0A=0 for no force compare for Timer0 OC0A
                                   // TCCR0B -- FOC0B=0 for no force compare for Timer0 OC0B
                                   // TCCR0B -- bits 5:4 are unused
                                   // TCCR0B -- WGM02=0 for Fast PWM mode (BOT to MAX, with Compare Match on OC0A value)
  TCCR0B |= _BV(CS00);             // TCCR0B -- CS02:CS00=001 for prescaling=1 (no prescaling)

  // initialize PB1 (green), PB2 (red), PB3 (blue) as outputs (for LEDs)
  DDRB |= _BV(PB1)|_BV(PB2)|_BV(PB3);

  // repeat playing all of the pitches in the pitchTab[] forever
  while (1) {
    gumIndex = 0;
    pitchIndex = 0;

    // create Gumball waveform using PWM by continually sequencing through the waveform sample values in gumballWavTab[]
    // vary the playback rate to vary the pitch of the waveform with the values in pitchTab[].gumballPitch
    // vary the lengths of time for playing a pitch with the values in pitchTab[].pitchDuration
    pitchRate = pgm_read_byte( &( pitchTab[pitchIndex].gumballPitch ) );
    pitchLen = pgm_read_word( &( pitchTab[pitchIndex].pitchDuration ) );

    // this "while" loop plays elements in the pitchTab[]
    // each element has a pitch-rate (how fast to play back the waveform) and a pitch-length (how long to play the pitch)
    // (the last element has pitchRate=0, so we keep looping until pitchRate!=0)
    while (pitchRate != 0) {
      // this "for" loop continually plays the samples of the gumball waveform from gumballWavTab[]
      // --the playback rate (the pitch) is determined by pitchRate (which is the Pitch value from pitchTab[] )
      // --the playback duration (how long to play this pitch) is determined by pitchLen (which is the Duration value from pitchTab[] )
      for ( uint32_t j=0; j<pitchLen; j++ ) {
        // send next value to PWM register OCR0A
        gumWavDat = pgm_read_byte( &( gumballWavTab[gumIndex] ) );
        OCR0A = gumWavDat;
        // delay a little
        delaySomeTime(pitchRate, SAMP);
        // increment to next value in gumballWavTab[]
        gumIndex++;
        // reset the index to the beginning if we reached the end of the table
        // and also do something interesting to the LEDs on PB2 (red) and PB1 (green)
        if (gumIndex >= gumballWavTabSize) {
          gumIndex = 0;  // reset the index to the beginning of gumballWavTab
          // make the LEDs light up in cool ways -- PB1 (green), PB2 (red), PB3 (blue)
          if ( ( (pitchRate % 50) == 0 ) || ( (pitchRate % 20) == 0 ) ) {
            PORTB ^= B8(00000100);  // toggle LED at PB2 (red)
          }
          if ( ( (pitchRate % 40) == 0 ) || ( (pitchRate % 10) == 0 ) ) {
            PORTB ^= B8(00000010);  // toggle LED at PB1 (green)
          }
        }
      }
      // get the next values of pitchRate and pitchLen from pitchTab[]
      pitchIndex++;
      pitchRate = pgm_read_byte( &( pitchTab[pitchIndex].gumballPitch ) );
      pitchLen = pgm_read_word( &( pitchTab[pitchIndex].pitchDuration ) );

      // make the LEDs light up in cool ways -- PB1 (green), PB2 (red), PB3 (blue)
      PORTB ^= B8(00001000);  // toggle LED at PB3 (blue)
    }

  }
}

/*
        // use somewhat randomish bit 0 of this value of the wave table to turn make the LED on PB2 (red) fade in and out
        // this creates a sort of PWM effect for the LED, as the (somewhat) randomish duty cycle makes the LED fade in and out in a crazy way
        gumWavDat &= B8(00000001);     // mask out bit 0
        gumWavDat = (gumWavDat << 2);  // shift it to bit 2
        PORTB |= gumWavDat;            // put it in PB2 (without disturbing the other bits of PORTB)
*/
