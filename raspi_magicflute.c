//
//  raspi_magicflute.c
//  ToneGenerator
//
//  Created by 長谷部 雅彦 on 2014/02/22.
//  Copyright (c) 2014年 長谷部 雅彦. All rights reserved.
//

#include	"raspi.h"
#ifdef RASPI

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#include 	"raspi_magicflute.h"

#include	"raspi_cwrap.h"
#include	"raspi_hw.h"

//-------------------------------------------------------------------------
//		event Loop
//-------------------------------------------------------------------------
void sendMessageToMsgf( unsigned char msg0, unsigned char msg1, unsigned char msg2 )
{
	unsigned char msg[3];
	msg[0] = msg0; msg[1] = msg1; msg[2] = msg2;
	//	Call MSGF
	raspiaudio_Message( msg, 3 );
}
//-------------------------------------------------------------------------
#if 0
#define	MAX_EXP_WIDTH		40
const unsigned char tExpValue[MAX_EXP_WIDTH] = {
	0,0,0,0,0,24,38,48,56,62,
	68,72,76,80,83,86,89,92,94,96,
	98,100,102,104,106,107,109,110,112,113,
	115,116,117,118,119,120,121,123,124,125
};

#else
#define	MAX_EXP_WIDTH		250
const unsigned char tExpValue[MAX_EXP_WIDTH] = {
	0,	0,	0,	0,	0,	0,	16,	25,	32,	37,
	41,	45,	48,	51,	53,	55,	57,	59,	61,	62,
	64,	65,	67,	68,	69,	70,	71,	72,	73,	74,
	75,	76,	77,	78,	78,	79,	80,	80,	81,	82,
	82,	83,	84,	84,	85,	85,	86,	87,	87,	88,
	88,	89,	89,	90,	90,	91,	91,	91,	92,	92,
	93,	93,	93,	94,	94,	95,	95,	95,	96,	96,
	96,	97,	97,	97,	98,	98,	98,	99,	99,	99,
	100,	100,	100,	101,	101,	101,	101,	102,	102,	102,
	103,	103,	103,	103,	104,	104,	104,	104,	105,	105,
	105,	105,	106,	106,	106,	106,	106,	107,	107,	107,
	107,	108,	108,	108,	108,	108,	109,	109,	109,	109,
	109,	110,	110,	110,	110,	110,	111,	111,	111,	111,
	111,	112,	112,	112,	112,	112,	112,	113,	113,	113,
	113,	113,	113,	114,	114,	114,	114,	114,	114,	115,
	115,	115,	115,	115,	115,	115,	116,	116,	116,	116,
	
	116,	116,	117,	117,	117,	117,	117,	117,	117,	118,
	118,	118,	118,	118,	118,	118,	118,	119,	119,	119,
	119,	119,	119,	119,	120,	120,	120,	120,	120,	120,
	120,	120,	121,	121,	121,	121,	121,	121,	121,	121,
	121,	122,	122,	122,	122,	122,	122,	122,	122,	123,
	123,	123,	123,	123,	123,	123,	123,	123,	124,	124,
	124,	124,	124,	124,	124,	124,	124,	124,	125,	125,
	125,	125,	125,	125,	125,	125,	125,	125,	126,	126,
	126,	126,	126,	126,	126,	126,	126,	126,	127,	127
};
#endif

//-------------------------------------------------------------------------
//		Pressure Sencer Input
//-------------------------------------------------------------------------
static int startCount = 0;
static int standardPrs = 0;	//	standard pressure value
static int stockPrs = 0;
#define		FIRST_COUNT			100
#define		COUNT_OFFSET		1000
#define		STABLE_COUNT		30
#define		NOISE_WIDTH			1
//-------------------------------------------------------------------------
static int ExcludeAtmospheric( int value )
{
	int tmpVal;
	
	if ( startCount < FIRST_COUNT ){	//	not calculate at first FIRST_COUNT times
		startCount++;
		if ( startCount == FIRST_COUNT ){
			standardPrs = value;
			printf("Standard Pressure is %d\n",value);
		}
		return 0;
	}

	else {
		if (( startCount > COUNT_OFFSET ) &&
			(( stockPrs-NOISE_WIDTH <= value ) &&
			 ( stockPrs+NOISE_WIDTH >= value ))){
			startCount++;
			if ( startCount > COUNT_OFFSET+STABLE_COUNT ){
				//	when pressure keep same value by STABLE_COUNT times
				startCount = COUNT_OFFSET;
				standardPrs = stockPrs;
				printf("Change Standard Pressure! %d\n",stockPrs);
			}
		}
		else if (( value > standardPrs+NOISE_WIDTH ) ||
				 ( value < standardPrs-NOISE_WIDTH )){
			stockPrs = value;
			startCount = COUNT_OFFSET+1;
		}
		
		tmpVal = value - standardPrs;
		if (( tmpVal <= NOISE_WIDTH ) && ( tmpVal >= -NOISE_WIDTH )) tmpVal = 0;
		return tmpVal;
	}
}
//-------------------------------------------------------------------------
static unsigned char currentExp = 0;
static unsigned char lastExp = 0;
static int currentPressure = 0;
//-------------------------------------------------------------------------
static void analysePressure( void )
{
	int tempPrs = getPressure();

	if ( tempPrs != 0 ){
		int idt = ExcludeAtmospheric( tempPrs );
		if ( currentPressure != idt ){
			//	protect trembling
			printf("Pressure:%d\n",idt);
			currentPressure = idt;
			if ( idt < 0 ) idt = 0;
			else if ( idt >= MAX_EXP_WIDTH ) idt = MAX_EXP_WIDTH-1;
			currentExp = tExpValue[idt];
		}
	}

	if ( currentExp != lastExp ){
		if ( currentExp > lastExp ) lastExp++;
		else lastExp--;
		
		//	Generate Expression Event
		sendMessageToMsgf( 0xb0, 0x0b, lastExp );
	}
}

//-------------------------------------------------------------------------
//		Blink LED
//-------------------------------------------------------------------------
const unsigned char tNoteToColor[12][3] = {
	{ 0xff, 0x00, 0x00 },
	{ 0xe0, 0x10, 0x00 },
	{ 0xc0, 0x20, 0x00 },
	{ 0xa0, 0x30, 0x00 },
	{ 0x80, 0x40, 0x00 },
	{ 0x00, 0xff, 0x00 },
	{ 0x00, 0x60, 0x60 },
	{ 0x00, 0x00, 0xff },
	{ 0x10, 0x00, 0xe0 },
	{ 0x20, 0x00, 0xc0 },
	{ 0x30, 0xff, 0xa0 },
	{ 0x40, 0x00, 0x80 }
};
//-------------------------------------------------------------------------
void blinkLED( unsigned char movableDo )
{
	changeColor((unsigned char*)tNoteToColor[(movableDo-48)%12]);
}

//-------------------------------------------------------------------------
//		Touch Sencer Input
//-------------------------------------------------------------------------
static unsigned short newSwData;
static unsigned char lastNote = 0;
static unsigned short lastSwData = 0;
//	Time Measurement
static long	startTime = 0;	//	!=0 means during deadBand
static int noteShift = 0;
static int deadBand = 0;
static unsigned short tapSwData = 0;
//-------------------------------------------------------------------------
const unsigned char tSwTable[64] = {

//   ooo   oox   oxo   oxx   xoo   xox   xxo   xxx	right hand
//	do(up)  so    fa    la    mi    ti    re    do
	0x48, 0x43, 0x41, 0x45, 0x40, 0x47, 0x3e, 0x3c,		//	ooo	left hand
	0x49, 0x44, 0x42, 0x44, 0x3f, 0x46, 0x3f, 0x3d,		//	oox
	0x54, 0x4f, 0x4d, 0x51, 0x4c, 0x53, 0x4a, 0x48,		//	oxo
	0x55, 0x50, 0x4e, 0x50, 0x4b, 0x52, 0x4b, 0x49,		//	oxx
	0x54, 0x4f, 0x4d, 0x51, 0x4c, 0x53, 0x4a, 0x48,		//	xoo
	0x55, 0x50, 0x4e, 0x50, 0x4b, 0x52, 0x4b, 0x49,		//	xox
	0x60, 0x5b, 0x59, 0x5d, 0x58, 0x5f, 0x56, 0x54,		//	xxo
	0x61, 0x5c, 0x5a, 0x5c, 0x57, 0x5e, 0x57, 0x55		//	xxx
};
//-------------------------------------------------------------------------
static void makeKeyOn( unsigned short swdata )
{
	unsigned char note, vel;

	printf("Switch Data:%04x\n",swdata);
	
	note = tSwTable[swdata & 0x3f];
	blinkLED(note);
	
	//	make real note number (fixed Do)
	if ( note != 0 ){
		vel = 0x7f;
		note += noteShift;
		lastNote = note;
	}
	else {
		note = lastNote;
		vel = 0x00;
	}
	sendMessageToMsgf( 0x90, note, vel );
}
//-------------------------------------------------------------------------
#define		OCT_SW		0x30
#define		CRO_SW		0x08
#define		SX_SW		0x07
#define		TAP_FLAG	0x8000
//-------------------------------------------------------------------------
//	Adjustable Value
#define		DEADBAND_POINT_TIME		50		//msec
#define		TAP_DEADBAND_POINT		5
//-------------------------------------------------------------------------
const unsigned char tSx2DoTable[8] = {7,4,3,5,2,6,1,0};
const int tDeadBandPoint[8][8] = {
//		do, re, mi, fa, so, la, ti, do	before
	{	0,	0,	1,	1,	1,	2,	2,	4	},	//	do	after
	{	0,	0,	1,	1,	1,	1,	2,	2	},	//	re
	{	1,	0,	0,	0,	1,	1,	1,	2	},	//	mi
	{	1,	1,	0,	0,	0,	1,	1,	1	},	//	fa
	{	1,	1,	1,	0,	0,	0,	1,	1	},	//	so
	{	2,	1,	1,	1,	1,	0,	0,	1	},	//	la
	{	2,	2,	1,	1,	1,	0,	0,	0	},	//	ti
	{	4,	2,	2,	1,	1,	1,	0,	0	}	//	do
};
//-------------------------------------------------------------------------
static void analyseTouchSwitch( void )
{
	struct	timeval tstr;
	long	crntTime;

	newSwData = getTchSwData();
	if ( newSwData == 0xffff ) return;

	gettimeofday(&tstr, NULL);
	crntTime = tstr.tv_sec * 1000 + tstr.tv_usec/1000;
		
	if ( newSwData != lastSwData ){
		if (((newSwData&OCT_SW) & ((~lastSwData)&OCT_SW)) ||
			(((~newSwData)&OCT_SW) & (lastSwData&OCT_SW))){
			//	oct sw on/off
			if ( tapSwData == 0 ){
				deadBand = TAP_DEADBAND_POINT;
				tapSwData = lastSwData|TAP_FLAG;
			}
			else if ( tapSwData&(~TAP_FLAG) == lastSwData ){
				deadBand = 0;
			}
		}
		else {
			int	newNum = tSx2DoTable[newSwData&SX_SW];
			int oldNum = tSx2DoTable[lastSwData&SX_SW];
			deadBand = tDeadBandPoint[newNum][oldNum];
		}
		
		if ( startTime == 0 ){
			//	for the first time
			if ( deadBand == 0 ){
				//	Direct KeyOn
				makeKeyOn(newSwData);
				startTime = 0;
			}
			else {
				//	enter deadBand
				startTime = crntTime;
			}
		}
	}

	if ((startTime != 0) &&
		( crntTime - startTime > DEADBAND_POINT_TIME*deadBand )){
		//over deadBand
		makeKeyOn(newSwData);
		startTime = 0;
		tapSwData = 0;
	}

	lastSwData = newSwData;
}

//-------------------------------------------------------------------------
//		GPIO Input
//-------------------------------------------------------------------------
#define			MAX_SW_NUM			3
static int		swOld[MAX_SW_NUM] = {1,1,1};
//-------------------------------------------------------------------------
static void analyseGPIO( void )
{
	unsigned char note, vel;
	int 	i;
	char	gpioPath[64];
	int		fd_in[MAX_SW_NUM], swNew[MAX_SW_NUM];
	
	for (i=0; i<MAX_SW_NUM; i++){
		sprintf(gpioPath,"/sys/class/gpio/gpio%d/value",i+9);
		fd_in[i] = open(gpioPath,O_RDWR);
		if ( fd_in[i] < 0 ) exit(EXIT_FAILURE);
	}

	for (i=0; i<MAX_SW_NUM; i++){
		char value[2];
		read(fd_in[i], value, 2);
		if ( value[0] == '0' ) swNew[i] = 0;
		else swNew[i] = 1;
	}

	for (i=0; i<MAX_SW_NUM; i++){
		close(fd_in[i]);
	}
		
	for (i=0; i<MAX_SW_NUM; i++ ){
		if ( swNew[i] != swOld[i] ){
			if ( !swNew[i] ){
				note = 0x3c + 2*i; vel = 0x7f;
				printf("Now KeyOn of %d\n",i);
			}
			else {
				note = 0x3c + 2*i; vel = 0;
				printf("Now KeyOff of %d\n",i);
			}
			//	Call MSGF
			sendMessageToMsgf( 0x90, note, vel );
			swOld[i] = swNew[i];
		}
	}
}

//-------------------------------------------------------------------------
//		Keyboard Input
//-------------------------------------------------------------------------
static int	c=0, d=0, e=0, f=0, g=0, a=0, b=0, q=0;
//-------------------------------------------------------------------------
static void analyseKeyboard( void )
{
	unsigned char note, vel;
	int key;
	
	if (( key = getchar()) != -1 ){
		bool anykey = false;
		switch (key){
			case 'c': note = 0x3c; c?(c=0,vel=0):(c=1,vel=0x7f); anykey = true; break;
			case 'd': note = 0x3e; d?(d=0,vel=0):(d=1,vel=0x7f); anykey = true; break;
			case 'e': note = 0x40; e?(e=0,vel=0):(e=1,vel=0x7f); anykey = true; break;
			case 'f': note = 0x41; f?(f=0,vel=0):(f=1,vel=0x7f); anykey = true; break;
			case 'g': note = 0x43; g?(g=0,vel=0):(g=1,vel=0x7f); anykey = true; break;
			case 'a': note = 0x45; a?(a=0,vel=0):(a=1,vel=0x7f); anykey = true; break;
			case 'b': note = 0x47; b?(b=0,vel=0):(b=1,vel=0x7f); anykey = true; break;
			case 'q':{
				q?(q=0,vel=0):(q=1,vel=0x7f);
				sendMessageToMsgf( 0xc0, vel, 0 );
				break;
			}
			default: break;
		}
		if ( anykey == true ){
			//	Call MSGF
			sendMessageToMsgf( 0x90, note, vel );
		}
	}
}
//-------------------------------------------------------------------------
//		Volume Input
//-------------------------------------------------------------------------
static int adCh = 0;
static unsigned char partNoteShift = 64;
static unsigned char partModulation = 0;
static unsigned char partPortamento = 0;
//-------------------------------------------------------------------------
static void analyseVolume( void )
{
	unsigned char vol = getVolume(adCh);
	if ( vol != 255 ){
		if ( vol > 100 ) vol = 100;
		vol = (unsigned char)(((int)vol*127)/100);
	}
	else adCh = -1;

	switch ( adCh ){
		case 0:{
			if ( vol != partNoteShift ){
				partNoteShift = vol;
				unsigned char ns = ((int)partNoteShift-64)/10 + 64;
				sendMessageToMsgf( 0xb0, 0x0c, ns );
				printf("Note Shift value: %d\n",partNoteShift);
			}
			break;
		}
		case 1:{
			if ( vol != partModulation ){
				partModulation = vol;
				sendMessageToMsgf( 0xb0, 0x01, partModulation );
				printf("Modulation value: %d\n",partModulation);
			}
			break;
		}
		case 2:{
			if ( vol != partPortamento ){
				partPortamento = vol;
				sendMessageToMsgf( 0xb0, 0x05, partPortamento );
				printf("Portamento value: %d\n",partPortamento);
			}
			break;
		}
		default: break;
	}

	adCh++;
	if ( adCh >= 3 ) adCh = 0;
}
		
		
//-------------------------------------------------------------------------
//		event Loop
//-------------------------------------------------------------------------
void eventLoopInit( INIT_PRM* prm )
{
	sendMessageToMsgf( 0xb0, 0x0b, 0 );
	noteShift = prm->transpose;
}
//-------------------------------------------------------------------------
void eventLoop( void )
{
	analyseVolume();
	analysePressure();
	analyseTouchSwitch();
}

//-------------------------------------------------------------------------
//			Initialize GPIO
//-------------------------------------------------------------------------
static void initGPIO( void )
{
	int	fd_exp, fd_dir, i;
	char gpiodrv[64];
	
	fd_exp = open("/sys/class/gpio/export", O_WRONLY );
	if ( fd_exp < 0 ){
		printf("Can't open GPIO\n");
		exit(EXIT_FAILURE);
	}
	write(fd_exp,"9",2);
	write(fd_exp,"10",2);
	write(fd_exp,"11",2);
	close(fd_exp);
	
	for ( i=9; i<12; i++ ){
		sprintf(gpiodrv,"/sys/class/gpio/gpio%d/direction",i);
		fd_dir = open(gpiodrv,O_RDWR);
		if ( fd_dir < 0 ){
			printf("Can't set direction\n");
			exit(EXIT_FAILURE);
		}
		write(fd_dir,"in",3);
		close(fd_dir);
	}
}

//-------------------------------------------------------------------------
//			Initialize
//-------------------------------------------------------------------------
void initHw( void )
{
	//	Initialize GPIO
	initGPIO();
	
	//--------------------------------------------------------
	//	Initialize I2C device
	initI2c();
	initLPS331AP();
	//	initSX1509();
	initMPR121();
	initBlinkM();
	initADS1015();
}
//-------------------------------------------------------------------------
//			Quit
//-------------------------------------------------------------------------
void quitHw( void )
{
	quitI2c();
}
#endif