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
			printf("Air Pres.:%d\n",idt);
			currentPressure = idt;
			if ( idt < 0 ) idt = 0;
			else if ( idt >= MAX_EXP_WIDTH ) idt = MAX_EXP_WIDTH-1;
			currentExp = tExpValue[idt];
		}
	}

	if ( currentExp != lastExp ){
		//	Generate Expression Event
		//if ( currentExp > lastExp ) lastExp++;
		//else lastExp--;
		lastExp = currentExp;
		
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
#define		OCT_SW		0x30
#define		CRO_SW		0x08
#define		SX_SW		0x07
#define		ALL_SW		(OCT_SW|CRO_SW|SX_SW)
#define		TAP_FLAG	0x8000
//-------------------------------------------------------------------------
//	Adjustable Value
#define		DEADBAND_POINT_TIME		50		//	[msec]
//-------------------------------------------------------------------------
static unsigned short lastSwData = ALL_SW;
static unsigned short tapSwData = 0;
//	Time Measurement
static long	startTime = 0;	//	!=0 means during deadBand
static int deadBand = 0;
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
static void SendMessage( unsigned short swdata )
{
	unsigned char newNote;
	printf("Switch Data(DeadBand:%d):%04x\n",deadBand,swdata);
	newNote = tSwTable[swdata & ALL_SW];
	blinkLED(newNote);
	sendMessageToMsgf( 0x90, newNote+0x3c, 0x7f );
	deadBand = 0;
	startTime = 0;
	tapSwData = 0;
}
//-------------------------------------------------------------------------
static void analyseTouchSwitch( long crntTime )
{
	unsigned short	newSwData;

	newSwData = getTchSwData();
	if ( newSwData == 0xffff ) return;

	if ( newSwData == lastSwData ){
		if ( deadBand > 0 ){
			if ( startTime != 0 ){
				if ( crntTime-startTime > DEADBAND_POINT_TIME*deadBand ){
					//	KeyOn
					SendMessage( newSwData );
				}
			}
		}
	}

	else {
		if (( deadBand > 0 ) && (tapSwData&TAP_FLAG) && ( tapSwData&(~TAP_FLAG) == newSwData )){
			//	KeyOn
			printf("<<Tapped>>\n");
			SendMessage( newSwData );
		}

		else {
			int diff;
			unsigned char newNote = tSwTable[newSwData & ALL_SW];
			unsigned char lastSwNote = tSwTable[lastSwData & ALL_SW];

			if ( newNote > lastSwNote ) diff = newNote - lastSwNote;
			else diff = lastSwNote - newNote;

			if ( diff > 11 ){
				startTime = crntTime;
				deadBand = 4;
				if ((newSwData^lastSwData)&OCT_SW){
					printf("<<Set Tap>>\n");
					tapSwData = lastSwData|TAP_FLAG;
				}
			}
			else if ( diff > 8 ){
				// 9 - 11
				startTime = crntTime;
				deadBand = 2;
			}
			else if ( diff > 4 ){
				// 5 - 8
				startTime = crntTime;
				deadBand = 1;
			}
			else {
				// 0 - 4
				SendMessage( newSwData );
			}
		}
					 
		//	update lastSwData
		lastSwData = newSwData;
	}
}
//-------------------------------------------------------------------------
//		Settings
//-------------------------------------------------------------------------
#define			MIDI_CENTER			64
static unsigned char partTranspose = MIDI_CENTER;
//-------------------------------------------------------------------------
static void changeTranspose( unsigned char tp )
{
	if ( tp == partTranspose ) return;
	
	if ( tp > MIDI_CENTER+6 ) partTranspose = MIDI_CENTER-6;
	else if ( tp < MIDI_CENTER-6 ) partTranspose = MIDI_CENTER+6;
	else partTranspose = tp;
	
	sendMessageToMsgf( 0xb0, 0x0c, partTranspose );
	printf("Note Shift value: %d\n",partTranspose);
	
	int nsx = partTranspose - MIDI_CENTER;
	if ( nsx < 0 ) nsx += 12; //	0 <= nsx <= 11
	else if ( nsx > 12 ) nsx -= 12;
	
	const int tCnv[12] = {3,12,4,13,5,6,15,7,9,1,10,2};
	writeMark(tCnv[nsx]);
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
				changeTranspose(((int)partNoteShift-64)/10 + 64);
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
#define			FIRST_INPUT_GPIO	9
#define			MAX_SW_NUM			3
#define			MAX_LED_NUM			1
static int		swOld[MAX_SW_NUM] = {1,1,1};
static int		gpioOutputVal[MAX_LED_NUM];
//-------------------------------------------------------------------------
static void ledOn( int num )
{
	write( gpioOutputVal[num], "1", 2 );
}
//-------------------------------------------------------------------------
static void ledOff( int num )
{
	write( gpioOutputVal[num], "0", 2 );
}
//-------------------------------------------------------------------------
static void transposeEvent( int num )
{
	int inc = 1;
	if ( num == 1 ) inc = -1;
	unsigned char tpTemp = partTranspose + inc;
	changeTranspose(tpTemp);
}
//-------------------------------------------------------------------------
static void changeVoiceEvent( int num )
{
	printf("Change Voice!\n");
}
//-------------------------------------------------------------------------
static void (*const tFunc[MAX_SW_NUM])( int num ) =
{
	transposeEvent,
	transposeEvent,
	changeVoiceEvent
};
//-------------------------------------------------------------------------
static void analyseGPIO( void )
{
	unsigned char note, vel;
	int 	i;
	char	gpioPath[64];
	int		fd_in[MAX_SW_NUM], swNew[MAX_SW_NUM];
	
	//	open
	for (i=0; i<MAX_SW_NUM; i++){
		sprintf(gpioPath,"/sys/class/gpio/gpio%d/value",i+FIRST_INPUT_GPIO);
		fd_in[i] = open(gpioPath,O_RDWR);
		if ( fd_in[i] < 0 ) exit(EXIT_FAILURE);
	}
	
	//	read
	for (i=0; i<MAX_SW_NUM; i++){
		char value[2];
		read(fd_in[i], value, 2);
		if ( value[0] == '0' ) swNew[i] = 0;
		else swNew[i] = 1;
	}
	
	//	close
	for (i=0; i<MAX_SW_NUM; i++){
		close(fd_in[i]);
	}

	//	Switch Event Job
	for (i=0; i<MAX_SW_NUM; i++ ){
		if ( swNew[i] != swOld[i] ){
			if ( !swNew[i] ){
				//	When push
				(*tFunc[i])(i);
			}
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
	write(fd_exp,"7",2);
	write(fd_exp,"9",2);
	write(fd_exp,"10",2);
	write(fd_exp,"11",2);
	close(fd_exp);

	//	input
	for ( i=FIRST_INPUT_GPIO; i<FIRST_INPUT_GPIO+MAX_SW_NUM; i++ ){
		sprintf(gpiodrv,"/sys/class/gpio/gpio%d/direction",i);
		fd_dir = open(gpiodrv,O_RDWR);
		if ( fd_dir < 0 ){
			printf("Can't set direction\n");
			exit(EXIT_FAILURE);
		}
		write(fd_dir,"in",3);
		close(fd_dir);
	}

	//	output
	for ( i=7; i<8; i++ ){
		sprintf(gpiodrv,"/sys/class/gpio/gpio%d/direction",i);
		fd_dir = open(gpiodrv,O_RDWR);
		if ( fd_dir < 0 ){
			printf("Can't set direction\n");
			exit(EXIT_FAILURE);
		}
		write(fd_dir,"out",4);
		close(fd_dir);
	}

	for ( i=7; i<8; i++ ){
		sprintf(gpiodrv,"/sys/class/gpio/gpio%d/value",i);
		gpioOutputVal[0] = open(gpiodrv,O_RDWR);
		if ( gpioOutputVal[0] < 0 ){
			printf("Can't set value\n");
			exit(EXIT_FAILURE);
		}
	}
}
//-------------------------------------------------------------------------
//		Inclination Input
//-------------------------------------------------------------------------
#define		MAX_ANGLE_BIT		6		//
#define		MAX_ANGLE			32		//	0x01 << MAX_ANGLE_BIT
static int xaxis = 0;					//	0 means horizontal
static int yaxis = 0;					//	0 means horizontal
static int zaxis = 0;
static int modDpt = 0;
static int prtDpt = 0;
//-------------------------------------------------------------------------
const int tCnvModDpt[MAX_ANGLE] = {
	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	1,	1,	2,	2,
	3,	4,	5,	6,	7,	8,	9,	10,
	12,	14,	16,	19,	22,	25,	28,	31,
};
//-------------------------------------------------------------------------
const int tCnvPrtDpt[MAX_ANGLE] = {
	0,	0,	10,	10,	20,	20,	30,	30,
	40,	40,	50,	50,	60,	60,	70,	70,
	80,	80,	80,	80,	90,	90,	90,	90,
	100,100,100,100,110,110,110,110,
};
//-------------------------------------------------------------------------
static void sendMod( void )
{
	if (( modDpt >= 0 ) && ( modDpt < 128 )){
		sendMessageToMsgf( 0xb0, 0x01, modDpt );
	}
	printf("  Incli. Modulation value: %d\n",modDpt);
}
//-------------------------------------------------------------------------
static void sendPrt( void )
{
	if (( modDpt >= 0 ) && ( modDpt < 128 )){
		sendMessageToMsgf( 0xb0, 0x05, prtDpt );
	}
	printf("  Incli. Portamento value: %d\n",prtDpt);
}
//-------------------------------------------------------------------------
static void analyseAcceleration( void )
{
	signed short accel[3], xVal, modVal, prtVal;

	getAccel( accel );
	xaxis = accel[0];
	yaxis = accel[1];
	zaxis = accel[2];
	
	xVal = xaxis/512;			// make xaxis 6bit
	if ( xVal >= MAX_ANGLE ) xVal = MAX_ANGLE-1;
	
	if ( xVal < 0 ){
		prtVal = xVal * (-1);
		modVal = 0;
	}
	else {
		prtVal = 0;
		modVal = xVal;
	}
	
	//	lessen variation of modulation
	modVal = tCnvModDpt[modVal];
	if ( modVal > modDpt ){
		modDpt++;
		sendMod();
	}
	else if ( modVal < modDpt ){
		modDpt--;
		sendMod();
	}

	prtVal = tCnvPrtDpt[prtVal];
	if ( prtVal != prtDpt ){
		prtDpt = prtVal;
		sendPrt();
	}
}

//-------------------------------------------------------------------------
//		event Loop
//-------------------------------------------------------------------------
#define		AVERAGE_TIMER_CNT		100		//	This times
static long formerTime;
static long timeSumming;
static int	timerCount;
static bool	useAccelSensor;
static unsigned char transposeSetting;
//-------------------------------------------------------------------------
void eventLoopInit( void )
{
	struct	timeval tstr;
	long	crntTime;
	
	sendMessageToMsgf( 0xb0, 0x0b, 0 );
	soundOn = 0;
	timerCount = 0;
	timeSumming = 0;
	changeTranspose( transposeSetting );
	
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
	analyseTouchSwitch(crntTime);
	//analyseVolume();
	analysePressure();
	if ( useAccelSensor == true ) analyseAcceleration();
	analyseGPIO();

	//	Analyse Processing Time
	diff = crntTime - formerTime;
	timeSumming += diff;
	formerTime = crntTime;
	timerCount++;

	//	Measure Main Loop Time & Make heartbeats
	if ( timerCount >= AVERAGE_TIMER_CNT ){
		printf("---Loop Interval value(100times): %d [msec]\n",timeSumming);
		timeSumming = 0;
		timerCount = 0;
		ledOn(0);
	}
	if ( timerCount > (AVERAGE_TIMER_CNT/10) ) ledOff(0);
}
//-------------------------------------------------------------------------
//			Initialize
//-------------------------------------------------------------------------
void settings( INIT_PRM* prm )
{
	transposeSetting = (unsigned char)prm->transpose + MIDI_CENTER;
	useAccelSensor = prm->accelSensor;
	printf("Init Transpose : %d\n",prm->transpose);
	if ( useAccelSensor == true ) printf("Use Acceleration Sensor.\n");
	else printf("Not use Acceleration Sensor.\n");
}
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
	//	initSX1509();	//	GPIO Expander
	initMPR121();
	initBlinkM();
	initAda88();
	//initADS1015();	//	AD Converter
	if ( useAccelSensor == true ) initADXL345();

	//--------------------------------------------------------
	//	initialize Display
	writeMark(3);		// "C"
	ledOff(0);
}
//-------------------------------------------------------------------------
//			Quit
//-------------------------------------------------------------------------
void quitHw( void )
{
	quitI2c();
}
#endif