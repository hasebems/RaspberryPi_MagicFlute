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

static unsigned char soundOn = 0;

//-------------------------------------------------------------------------
//		Send Message
//-------------------------------------------------------------------------
void sendMessageToMsgf( unsigned char msg0, unsigned char msg1, unsigned char msg2 )
{
	unsigned char msg[3];
	msg[0] = msg0; msg[1] = msg1; msg[2] = msg2;
	//	Call MSGF
	raspiaudio_Message( msg, 3 );
}
//-------------------------------------------------------------------------
//		Blink LED
//-------------------------------------------------------------------------
static unsigned char movableDo = 0;
#define		TURN_OFF_LED		0xff
#define		TURN_ON_LED			0xfe
const unsigned char tNoteToColor[13][3] = {
	//	R	  G		B
	{ 0xff, 0x00, 0x00 },
	{ 0xe0, 0x20, 0x00 },
	{ 0xc0, 0x40, 0x00 },
	{ 0xa0, 0x60, 0x00 },
	{ 0x80, 0x80, 0x00 },
	{ 0x00, 0xff, 0x00 },
	{ 0x00, 0x80, 0x80 },
	{ 0x00, 0x00, 0xff },
	{ 0x20, 0x00, 0xe0 },
	{ 0x40, 0x00, 0xc0 },
	{ 0x60, 0x00, 0xa0 },
	{ 0x80, 0x00, 0x80 },
	{ 0x00, 0x00, 0x00 }
};
//-------------------------------------------------------------------------
void blinkLED( unsigned char mvDo )
{
	if ( mvDo == TURN_OFF_LED ){
		changeColor((unsigned char*)tNoteToColor[12]);
	}
	else if ( mvDo == TURN_ON_LED ){
		changeColor((unsigned char*)tNoteToColor[movableDo]);
	}
	else {
		movableDo = mvDo%12;
		if ( soundOn ){
			changeColor((unsigned char*)tNoteToColor[movableDo]);
		}
	}
}

//-------------------------------------------------------------------------
//		Pressure Sencer Input
//-------------------------------------------------------------------------
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
//-------------------------------------------------------------------------
static int startCount = 0;
static int standardPrs = 0;	//	standard pressure value
static int stockPrs = 0;
#define		FIRST_COUNT			100
#define		COUNT_OFFSET		1000
#define		STABLE_COUNT		30
#define		NOISE_WIDTH			1
//-------------------------------------------------------------------------
static int excludeAtmospheric( int value )
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
		int idt = excludeAtmospheric( tempPrs );
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
		//	Generate Expression Event
		if ( currentExp > lastExp ) lastExp++;
		else lastExp--;
		
		//	controll LED
		if ( lastExp == 0 ){
			soundOn = 0;
			blinkLED( TURN_OFF_LED );
		}
		else if ( soundOn == 0 ){
			blinkLED( TURN_ON_LED );
			soundOn = 1;
		}
		
		//	Send Message
		sendMessageToMsgf( 0xb0, 0x0b, lastExp );
	}
}

//-------------------------------------------------------------------------
//		Touch Sencer Input
//-------------------------------------------------------------------------
static unsigned char lastNote = 0;
static unsigned short lastSwData = 0;
static unsigned short tapSwData = 0;
static int noteShift = 0;
//	Time Measurement
static long	startTime = 0;	//	!=0 means during deadBand
static int deadBand = 0;
//-------------------------------------------------------------------------
#define		OCT_SW		0x30
#define		CRO_SW		0x08
#define		SX_SW		0x07
#define		ALL_SW		(OCT_SW|CRO_SW|SX_SW)
#define		TAP_FLAG	0x8000
//-------------------------------------------------------------------------
//	Adjustable Value
#define		DEADBAND_POINT_TIME		50		//	[msec]
#define		OCT_DEADBAND_POINT		4		//	200msec
//-------------------------------------------------------------------------
const unsigned char tSwTable[64] = {
	
//   ooo   oox   oxo   oxx   xoo   xox   xxo   xxx	right hand
//	do(hi) so    fa    la    mi    ti    re    do
	0x24, 0x1f, 0x1d, 0x21, 0x1c, 0x23, 0x1a, 0x18,		//	ooo	left hand
	0x25, 0x20, 0x1e, 0x20, 0x1b, 0x22, 0x1b, 0x19,		//	oox
	0x18, 0x13, 0x11, 0x15, 0x10, 0x17, 0x0e, 0x0c,		//	oxo
	0x19, 0x14, 0x12, 0x14, 0x0f, 0x16, 0x0f, 0x0d,		//	oxx
	0x18, 0x13, 0x11, 0x15, 0x10, 0x17, 0x0e, 0x0c,		//	xoo
	0x19, 0x14, 0x12, 0x14, 0x0f, 0x16, 0x0f, 0x0d,		//	xox
	0x0c, 0x07, 0x05, 0x09, 0x04, 0x0b, 0x02, 0x00,		//	xxo
	0x0d, 0x08, 0x06, 0x08, 0x03, 0x0a, 0x03, 0x01		//	xxx
};
//-------------------------------------------------------------------------
const unsigned char tSx2DoTable[8] = {7,4,3,5,2,6,1,0};
const int tDeadBandPoint[8][8] = {
//		do, re, mi, fa, so, la, ti, do	before
	{	0,	0,	1,	1,	1,	2,	3,	4	},	//	do	after
	{	0,	0,	1,	1,	1,	1,	2,	3	},	//	re
	{	1,	0,	0,	0,	1,	1,	1,	2	},	//	mi
	{	1,	1,	0,	0,	0,	1,	1,	1	},	//	fa
	{	1,	1,	1,	0,	0,	0,	1,	1	},	//	so
	{	2,	1,	1,	1,	1,	0,	0,	1	},	//	la
	{	3,	2,	1,	1,	1,	0,	0,	0	},	//	ti
	{	4,	3,	2,	1,	1,	1,	0,	0	}	//	do
};
//-------------------------------------------------------------------------
static void judgeSendingMessage( long diffTime, unsigned short swdata )
{
	if ( startTime != 0 ){
		if ( diffTime > DEADBAND_POINT_TIME*deadBand ){
			//over deadBand
			printf("Switch Data(L%d):%04x\n",deadBand,swdata);
			lastNote = tSwTable[swdata & ALL_SW];
			blinkLED(lastNote);
			sendMessageToMsgf( 0x90, lastNote+noteShift+0x3c, 0x7f );
			startTime = 0;
			tapSwData = 0;
		}
	}
}
//-------------------------------------------------------------------------
static void analyseTouchSwitch( long crntTime )
{
	unsigned short	newSwData;

	newSwData = getTchSwData();
	if ( newSwData == 0xffff ) return;
		
	if ( newSwData != lastSwData ){
		if (((newSwData&OCT_SW) & ((~lastSwData)&OCT_SW)) ||
			(((~newSwData)&OCT_SW) & (lastSwData&OCT_SW))){
			//	oct sw on/off
			if ( tapSwData == 0 ){
				deadBand = OCT_DEADBAND_POINT;
				tapSwData = lastSwData|TAP_FLAG;
			}
			else if ( tapSwData&(~TAP_FLAG) == newSwData ){
				//	return past state
				deadBand = 0;
			}
		}
		else {
			//	except oct
			int	newNum = tSx2DoTable[newSwData&SX_SW];
			int oldNum = tSx2DoTable[lastSwData&SX_SW];
			deadBand = tDeadBandPoint[newNum][oldNum];
		}

		// check crossing octave slightly
		unsigned char note = tSwTable[newSwData & ALL_SW];
		if ((( lastNote > note )&&((note%12)>8)&&((lastNote%12)<3)&&((lastNote-note)<4)) ||
			(( lastNote < note )&&((note%12)<3)&&((lastNote%12)>8)&&((note-lastNote)<4))){
			startTime = 0;
			deadBand = 1;
			printf("Cross Octave Slighly\n");
		}
		
		//	no Deadband
		if ( startTime == 0 ){
			if ( deadBand != 0 ){
				//	start Deadband
				startTime = crntTime;
			}
			else {
				//	Direct KeyOn
				printf("Switch Data(D0):%04x\n",newSwData);
				lastNote = tSwTable[newSwData & ALL_SW];
				blinkLED(lastNote);
				sendMessageToMsgf( 0x90, lastNote+noteShift+0x3c, 0x7f );
			}
		}
		
		//	update lastSwData
		lastSwData = newSwData;
	}

	judgeSendingMessage( crntTime-startTime, newSwData );
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
				int nsx = ns - 64;
				if ( nsx < 0 ) nsx += 12; //	0 <= nsx <= 11
				const int tCnv[12] = {3,12,4,13,5,6,15,7,9,1,10,2};
				writeMark(tCnv[nsx]);
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
//		Volume Input
//-------------------------------------------------------------------------
//static int adCh = 0;
//-------------------------------------------------------------------------
static void analyseAcceleration( void )
{
	unsigned char accel[3];
	
	getAccel( accel );
	printf("xxxxxxxxxxx X:%04x Y:%04x Z:%04x\n",*accel,*(accel+1),*(accel+2));
}

//-------------------------------------------------------------------------
//		event Loop
//-------------------------------------------------------------------------
static long formerTime;
static long timeSumming;
static int	timerCount;
#define		AVERAGE_TIMER_CNT		100		//	This times
//-------------------------------------------------------------------------
void eventLoopInit( INIT_PRM* prm )
{
	struct	timeval tstr;
	long	crntTime;
	
	sendMessageToMsgf( 0xb0, 0x0b, 0 );
	soundOn = 0;
	noteShift = prm->transpose;
	timerCount = 0;
	timeSumming = 0;

	//	Time Measurement
	gettimeofday(&tstr, NULL);
	formerTime = tstr.tv_sec * 1000 + tstr.tv_usec/1000;
}
//-------------------------------------------------------------------------
void eventLoop( void )
{
	struct	timeval tstr;
	long	crntTime, diff;

	//	Time Measurement
	gettimeofday(&tstr, NULL);
	crntTime = tstr.tv_sec * 1000 + tstr.tv_usec/1000;

	//	Main Task
	analyseVolume();
	analysePressure();
	analyseTouchSwitch(crntTime);
	analyseAcceleration();

	//	Analyse Processing Time
	diff = crntTime - formerTime;
	timeSumming += diff;
	formerTime = crntTime;
	timerCount++;

	if ( timerCount >= AVERAGE_TIMER_CNT ){
		printf("---Loop Interval value(100times): %d [msec]\n",timeSumming);
		timeSumming = 0;
		timerCount = 0;
	}
}
//-------------------------------------------------------------------------
//			Initialize
//-------------------------------------------------------------------------
void initHw( void )
{
	//--------------------------------------------------------
	//	Initialize GPIO
	initGPIO();
	
	//--------------------------------------------------------
	//	Initialize I2C device
	initI2c();
	initLPS331AP();
	//	initSX1509();
	initMPR121();
	initBlinkM();
	initAda88();
	initADS1015();
	initADXL345();
}
//-------------------------------------------------------------------------
//			Quit
//-------------------------------------------------------------------------
void quitHw( void )
{
	quitI2c();
}
#endif