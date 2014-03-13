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
//-------------------------------------------------------------------------
static int ExcludeAtmospheric( int value )
{
	int tmpVal;
	
	if ( startCount < 100 ){	//	not calculate at first 100 times
		startCount++;
		if ( startCount == 100 ){
			standardPrs = value;
			printf("Standard Pressure is %d\n",value);
		}
		return 0;
	}
	else {
		if (( startCount > 1000 ) && (( stockPrs-1 <= value ) && ( stockPrs+1 >= value ))){
			startCount++;
			if ( startCount > 1050 ){	//	when pressure keep same value by 50 times
				startCount = 1000;
				standardPrs = stockPrs;
				printf("Change Standard Pressure! %d\n",stockPrs);
			}
		}
		else if (( value >= standardPrs+2 ) || ( value <= standardPrs-2 )){
			stockPrs = value;
			startCount = 1001;
		}
		
		tmpVal = value - standardPrs;
		if (( tmpVal < 2 ) && ( tmpVal > -2 )) tmpVal = 0;
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
//		Touch Sencer Input
//-------------------------------------------------------------------------
static unsigned short newSwdata;
static unsigned char lastNote = 0;
static unsigned short lastSwData = 0;
//	Time Measurement
static long	startTime = 0;
static int noteShift = 0;
//-------------------------------------------------------------------------
const unsigned char tSwTable[64] = {

	//	 ooo   oox   oxo   oxx   xoo   xox   xxo   xxx
	//	0x48, 0x40, 0x41, 0x3e, 0x43, 0x47, 0x45, 0x3c,
	//	0x54, 0x4c, 0x4d, 0x4a, 0x4f, 0x53, 0x51, 0x48,
	//	0x48, 0x40, 0x41, 0x3e, 0x43, 0x47, 0x45, 0x3c,
	//	0x54, 0x4c, 0x4d, 0x4a, 0x4f, 0x53, 0x51, 0x48,
	//	0x47, 0x3f, 0x40, 0x3d, 0x42, 0x46, 0x44, 0x3b,
	//	0x53, 0x4b, 0x4c, 0x49, 0x4e, 0x52, 0x50, 0x47,
	//	0x49, 0x41, 0x42, 0x3f, 0x44, 0x48, 0x46, 0x3d,
	//	0x55, 0x4d, 0x4e, 0x4b, 0x50, 0x54, 0x52, 0x49

//   ooo   oox   oxo   oxx   xoo   xox   xxo   xxx
	0x54, 0x4f, 0x4d, 0x51, 0x4c, 0x53, 0x4a, 0x48,		//	ooo
	0x53, 0x4e, 0x4c, 0x50, 0x4b, 0x52, 0x49, 0x47,		//	oox
	0x54, 0x4f, 0x4d, 0x51, 0x4c, 0x53, 0x4a, 0x48,		//	oxo
	0x55, 0x50, 0x4e, 0x52, 0x4d, 0x54, 0x4b, 0x49,		//	oxx
	0x48, 0x43, 0x59, 0x45, 0x58, 0x47, 0x56, 0x54,		//	xoo
	0x47, 0x42, 0x58, 0x44, 0x57, 0x46, 0x55, 0x53,		//	xox
	0x48, 0x43, 0x59, 0x45, 0x58, 0x47, 0x56, 0x54,		//	xxo
	0x49, 0x44, 0x5a, 0x46, 0x59, 0x48, 0x57, 0x55		//	xxx
};
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
static void analyseTouchSwitch( void )
{
	unsigned char note, vel;
	struct	timeval tstr;

	newSwdata = getTchSwData();
	if ( startTime == 0 ){
		//	first time pushing
		if ( newSwdata != lastSwData ){
			gettimeofday(&tstr, NULL);
			startTime = tstr.tv_sec * 1000 + tstr.tv_usec/1000;
		}
	}
	else {
		gettimeofday(&tstr, NULL);
		long currentTime = tstr.tv_sec * 1000 + tstr.tv_usec/1000;
		long waitTime = 30;

		//	magic algorithm for earier judgement
		if ((~(lastSwData&0x07)&0x07) & (newSwdata&0x07)) waitTime = 60;

		if ( currentTime - startTime > waitTime ){	//	over 50msec
			startTime = 0;
			printf("Switch Data:%04x\n",newSwdata);
			
			note = tSwTable[newSwdata & 0x3f] + noteShift;
			lastSwData = newSwdata;
			if ( note != 0 ){
				vel = 0x7f;
				lastNote = note;
				changeColor((unsigned char*)tNoteToColor[(note-48)%12]);
			}
			else {
				note = lastNote;
				vel = 0x00;
			}
			sendMessageToMsgf( 0x90, note, vel );
		}
	}
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
static unsigned char partVolume = 100;
static unsigned char partModulation = 0;
static unsigned char partPortamento = 0;
//-------------------------------------------------------------------------
static void analyseVolume( void )
{
	unsigned char vol = getVolume(adCh);
	if ( vol > 100 ) vol = 100;
	vol = (unsigned char)(((int)vol*127)/100);

	switch ( adCh ){
		default:
		case 0:{
			if ( vol != partVolume ){
				partVolume = vol;
				sendMessageToMsgf( 0xb0, 0x07, partVolume );
				printf("volume value: %d\n",partVolume);
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