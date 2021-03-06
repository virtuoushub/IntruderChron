#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "font5x7.h"

// InvaderChron: Space Invaders modification
// Pretty sweeping replacement.
// CRJones aka Dataman
// InvaderChron@crjones.com
// Revision History
//
// 2010-03-03 First Version
//            Requires: Patch to glcd.c routine glcdWriteCharGr(u08 grCharIdx)
//                      Font data in fontgr.h
//
#define InvaderTimer 5                // Smaller to make them move faster
void WriteInvaders(void);             // Displays Invaders
void WritePlayer(void);
void WriteDigits(uint8_t, uint8_t);   // Displays a Set of Digits
void WriteTime(uint8_t);
uint8_t pPlayer = 61;                 // Player Position
uint8_t pPlayerPrevious = 62;         // Previous Player Position
uint8_t pPlayerDirection = -1;        // Player Direction 1=Right, -1=Left
uint8_t pInvaders=1;                  // Invader's X Position
uint8_t pInvadersPrevious=0;          // Previous Invader's Position
int8_t  pInvadersDirection=1;         // Invader's Direction 1=Right, -1=Left
uint8_t Frame=0;                      // Current Animation Frame 0/1
uint8_t Timer = InvaderTimer;         // Count down timer so they don't move rediculously fast
uint8_t left_score2, right_score2;    // Storage for player2 score
// End Space Invaders Mod 1

extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t old_m, old_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t alarming, alarm_h, alarm_m;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;

uint8_t left_score, right_score;


extern volatile uint8_t minute_changed, hour_changed;

uint8_t redraw_time = 0;
uint8_t last_score_mode = 0;

uint32_t rval[2]={0,0};
uint32_t key[4];

void encipher(void) {  // Using 32 rounds of XTea encryption as a PRNG.
  unsigned int i;
  uint32_t v0=rval[0], v1=rval[1], sum=0, delta=0x9E3779B9;
  for (i=0; i < 32; i++) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
  }
  rval[0]=v0; rval[1]=v1;
}

void init_crand() {
  uint32_t temp;
  key[0]=0x2DE9716E;  //Initial XTEA key. Grabbed from the first 16 bytes
  key[1]=0x993FDDD1;  //of grc.com/password.  1 in 2^128 chance of seeing
  key[2]=0x2A77FB57;  //that key again there.
  key[3]=0xB172E6B0;
  rval[0]=0;
  rval[1]=0;
  encipher();
  temp = alarm_h;
  temp<<=8;
  temp|=time_h;
  temp<<=8;
  temp|=time_m;
  temp<<=8;
  temp|=time_s;
  key[0]^=rval[1]<<1;
  encipher();
  key[1]^=temp<<1;
  encipher();
  key[2]^=temp>>1;
  encipher();
  key[3]^=rval[1]>>1;
  encipher();
  temp = alarm_m;
  temp<<=8;
  temp|=date_m;
  temp<<=8;
  temp|=date_d;
  temp<<=8;
  temp|=date_y;
  key[0]^=temp<<1;
  encipher();
  key[1]^=rval[0]<<1;
  encipher();
  key[2]^=rval[0]>>1;
  encipher();
  key[3]^=temp>>1;
  rval[0]=0;
  rval[1]=0;
  encipher();	//And at this point, the PRNG is now seeded, based on power on/date/time reset.
}

uint16_t crand(uint8_t type) {
  if((type==0)||(type>2))
  {
    wdt_reset();
    encipher();
    return (rval[0]^rval[1])&RAND_MAX;
  } else if (type==1) {
  	return ((rval[0]^rval[1])>>15)&3;
  } else if (type==2) {
  	return ((rval[0]^rval[1])>>17)&1;
  }
}

void setscore(void)
{
  if(score_mode != last_score_mode) {
    redraw_time = 1;
    last_score_mode = score_mode;
    // Default left and right displays
    left_score = time_h;
    right_score = time_m;
    if((region == REGION_US)||(region == DOW_REGION_US)) {
      left_score2 = date_m;
      right_score2 = date_d;
    } else {
      left_score2 = date_d;
      right_score2 = date_m;
    }
  }
  switch(score_mode) {
  	case SCORE_MODE_DOW:
  	  break;
  	case SCORE_MODE_DATELONG:
  	  right_score2 = date_d;
  	  break;
    case SCORE_MODE_TIME:
      if(alarming && (minute_changed || hour_changed)) {
      	if(hour_changed) {
	      left_score = old_h;
	      right_score = old_m;
	    } else if (minute_changed) {
	      right_score = old_m;
	    }
      } else {
        left_score = time_h;
        right_score = time_m;
      }
      break;
    case SCORE_MODE_DATE:
      // we defaulted to this
      //if((region == REGION_US)||(region == DOW_REGION_US)) {
      //  left_score2 = date_m;
      //  right_score2 = date_d;
      //} else {
      //  left_score2 = date_d;
      //  right_score2 = date_m;
      //}
      break;
    case SCORE_MODE_YEAR:
      left_score2 = 20;
      right_score2 = date_y;
      break;
    case SCORE_MODE_ALARM:
      left_score2 = alarm_h;
      right_score2 = alarm_m;
      break;
  }
  if (time_format == TIME_12H && left_score>12) {left_score = left_score % 12;}
}

void initanim(void) {
  DEBUG(putstring("screen width: "));
  DEBUG(uart_putw_dec(GLCD_XPIXELS));
  DEBUG(putstring("\n\rscreen height: "));
  DEBUG(uart_putw_dec(GLCD_YPIXELS));
  DEBUG(putstring_nl(""));

  pPlayer = 61;
  pPlayerPrevious = 62;
  pPlayerDirection = -1;
  pInvaders = 1;
  pInvadersPrevious=0;
  
}

void initdisplay(uint8_t inverted) {
  // clear screen
  glcdFillRectangle(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, inverted);
  // get time & display
  last_score_mode = 99;
  setscore();
  WriteTime(inverted);
  // display players 
  WriteInvaders();
  WritePlayer(); 
  // Show the bases, 1 time only
  uint8_t i;
  for (i=0;i<4;i++)
   {
    glcdSetAddress(20 + (i*24), 7);
	glcdWriteCharGr(6);
   }
  
}

void WriteInvaders(void)
{
  
  uint8_t j;
  uint8_t i;
  // Clear Previous
  if (pInvadersPrevious > pInvaders) {i=pInvaders+96;}
  else {i=pInvaders-1;}
  glcdFillRectangle(i, 8, 1, 48, 0);
  // Draw Current
  for (i=0;i<6;i++){
  for (j=0;j<6;j++){
    glcdSetAddress(pInvaders + (j*16), 1+i);
	glcdWriteCharGr((i/2)+(Frame*3));
   }
   }
}

void WritePlayer(void)
{
  return; // looks bad, skip it for now.
  uint8_t j;
  uint8_t i;
  // Clear Previous
  if (pPlayerPrevious > pPlayer) {i=pPlayer+6; j=pPlayer+4;}
  else {i=pPlayer-1; j=pPlayer+2;}
  glcdFillRectangle(j, 61, 1, 1, 0); // clear nub
  glcdFillRectangle(i, 62, 1, 2, 0); // clear end of panel
  glcdFillRectangle(pPlayer+3, 61, 1, 1, 1); //draw padel
  glcdFillRectangle(pPlayer, 62, 5, 2, 1); // draw nub
 }


void step(void) {
 if (--Timer==0) 
  {
  Timer=InvaderTimer;
  pInvadersPrevious = pInvaders;
  pInvaders += pInvadersDirection;
  if (pInvaders > 31) {pInvadersDirection=-1;}
  if (pInvaders < 1) {pInvadersDirection=1;}
  Frame = !Frame;
  pPlayerPrevious = pPlayer;
  pPlayer += pPlayerDirection;
  if (pPlayer > 63) {pPlayerDirection=-1;}
  if (pPlayer < 1) {pPlayerDirection=1;}
  }
}

void WriteTime(uint8_t inverted) {
 	 glcdSetAddress(0,0);
	 WriteDigits(left_score,inverted);
	 WriteDigits(right_score,inverted);
	 glcdSetAddress(102,0);
	 WriteDigits(left_score2,inverted);
	 WriteDigits(right_score2,inverted);
}

void WriteDigits(uint8_t t, uint8_t inverted)
{
	glcdWriteChar(48 + (t/10),inverted);
	glcdWriteChar(48 + (t%10),inverted);
}


void draw(uint8_t inverted) {
    WriteInvaders();
	WritePlayer();
	setscore();
    //if (minute_changed || hour_changed || alarming || redraw_time)
    //{
     redraw_time = 0;
     WriteTime(inverted); 
    //} 
    return;
}


static unsigned char __attribute__ ((progmem)) MonthText[] = {
	0,0,0,
	'J','A','N',
	'F','E','B',
	'M','A','R',
	'A','P','R',
	'M','A','Y',
	'J','U','N',
	'J','U','L',
	'A','U','G',
	'S','E','P',
	'O','C','T',
	'N','O','V',
	'D','E','C',
};

static unsigned char __attribute__ ((progmem)) DOWText[] = {
	'S','U','N',
	'M','O','N',
	'T','U','E',
	'W','E','D',
	'T','H','U',
	'F','R','I',
	'S','A','T',
};

uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr)
{
  uint16_t month, year;

    // Calculate day of the week
    
    month = mon;
    year = 2000 + yr;
    if (mon < 3)  {
      month += 12;
      year -= 1;
    }
    return (day + (2 * month) + (6 * (month+1)/10) + year + (year/4) - (year/100) + (year/400) + 1) % 7;
}





      
