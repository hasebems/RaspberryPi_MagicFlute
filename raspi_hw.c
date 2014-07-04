//
//  raspi_hw.c
//  ToneGenerator
//
//  Created by Masahiko Hasebe on 2013/08/13.
//  Copyright (c) 2013 Masahiko Hasebe. All rights reserved.
//

#include	"raspi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>

#ifdef RASPI
 #include <linux/i2c-dev.h>
#endif

#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

#ifdef RASPI
 #define 	I2CSLAVE_	I2C_SLAVE
#else
 #define	I2CSLAVE_	0
#endif

//-------------------------------------------------------------------------
//			Variables
//-------------------------------------------------------------------------
static int i2cDscript;       // file discripter



//-------------------------------------------------------------------------
//			Constants
//-------------------------------------------------------------------------
static unsigned char GPIO_EXPANDER_ADDRESS = 0x3e;
static unsigned char PRESSURE_SENSOR_ADDRESS = 0x5d;
static unsigned char TOUCH_SENSOR_ADDRESS = 0x5a;
static unsigned char LED_BLINKM_ADDRESS = 0x09;
static unsigned char ADC_ADDRESS = 0x48;
static unsigned char LED_ADA88_ADDRESS = 0x70;

//-------------------------------------------------------------------------
//			I2c Device Access Functions
//-------------------------------------------------------------------------
void initI2c( void )
{
    const char	*fileName = "/dev/i2c-1"; // I2C Drive File name
	
	//	Pressure Sensor
    printf("***** start i2c *****\n");
	
    // Open I2C port with Read/Write Attribute
    if ((i2cDscript = open(fileName, O_RDWR)) < 0){
        printf("Faild to open i2c port\n");
        exit(1);
    }
}
//-------------------------------------------------------------------------
void quitI2c( void )
{
     printf("***** quit i2c *****\n");
	
    // Open I2C port with Read/Write Attribute
    if ( close(i2cDscript) < 0){
        printf("Faild to close i2c port\n");
        exit(1);
    }
}
//-------------------------------------------------------------------------
void writeI2c( unsigned char adrs, unsigned char data )
{
	unsigned char buf[2];
	
	// Commands for performing a ranging
	buf[0] = adrs;
	buf[1] = data;
	
	if ((write(i2cDscript, buf, 2)) != 2) {
		// Write commands to the i2c port
		printf("Error writing to i2c slave(I2C)\n");
		//exit(1);
	}
}
//-------------------------------------------------------------------------
unsigned char readI2c( unsigned char adrs )
{
	unsigned char buf[2];
	buf[0] = adrs;						// This is the register we wish to read from
	
	if (write(i2cDscript, buf, 1) != 1) {
		// Send the register to read from
		printf("Error writing to i2c slave(I2C:read)\n");
		//exit(1);
		return 0;
	}
	
	if (read(i2cDscript, buf, 1) != 1) {
		// Read back data into buf[]
		printf("Unable to read from slave(I2C)\n");
		//exit(1);
		return 0;
	}
	
	return buf[0];
}


//-------------------------------------------------------------------------
//			SX1509 (GPIO Expansion Device)
//-------------------------------------------------------------------------
//	for GPIO Expansion
#define		GPIO_EXPNDR_PULL_UP_B		0x06
#define		GPIO_EXPNDR_PULL_UP_A		0x07
#define		GPIO_EXPNDR_DIR_B			0x0e
#define		GPIO_EXPNDR_DIR_A			0x0f
#define		GPIO_EXPNDR_DATA_B			0x10
#define		GPIO_EXPNDR_DATA_A			0x11
//-------------------------------------------------------------------------
void accessSX1509( void )
{
	int		address = GPIO_EXPANDER_ADDRESS;  // I2C

	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(GPIOEX)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void initSX1509( void )
{
	//	Start Access
	accessSX1509();
	
	//	Init Parameter
	writeI2c( GPIO_EXPNDR_PULL_UP_B, 0xFF );
	writeI2c( GPIO_EXPNDR_PULL_UP_A, 0xFF );
	writeI2c( GPIO_EXPNDR_DIR_B, 0xFF );
	writeI2c( GPIO_EXPNDR_DIR_A, 0xFF );
}
//-------------------------------------------------------------------------
unsigned short getSwData( void )
{
	unsigned short dt;
			
	//	Start Access
	accessSX1509();
	
	//	GPIO
	dt = readI2c( GPIO_EXPNDR_DATA_B ) << 8;
	dt |= readI2c( GPIO_EXPNDR_DATA_A );

	return dt;
}

//-------------------------------------------------------------------------
//			LPS331AP (Pressure Sencer : I2c Device)
//-------------------------------------------------------------------------
//	for Pressure Sencer
#define		PRES_SNCR_RESOLUTION		0x10
#define		PRES_SNCR_PWRON				0x20
#define		PRES_SNCR_START				0x21
#define		PRES_SNCR_ONE_SHOT			0x01
#define		PRES_SNCR_RCV_DT_FLG		0x27
#define		PRES_SNCR_RCV_TMPR			0x01
#define		PRES_SNCR_RCV_PRES			0x02
#define		PRES_SNCR_DT_H				0x28
#define		PRES_SNCR_DT_M				0x29
#define		PRES_SNCR_DT_L				0x2a
//-------------------------------------------------------------------------
void accessLPS331AP( void )
{
	int		address = PRESSURE_SENSOR_ADDRESS;  // I2C
	
	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(PRS)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void initLPS331AP( void )
{
	//	Start Access
	accessLPS331AP();
	
	//	Init Parameter
	writeI2c( PRES_SNCR_PWRON, 0x80 );	//	Power On
	writeI2c( PRES_SNCR_RESOLUTION, 0x7A );	//	Resolution
}
//-------------------------------------------------------------------------
int getPressure( void )
{
	unsigned char rdDt, dt[3];
	float	fdata = 0;	//	can not get a value
	
	//	Start Access
	accessLPS331AP();
	
	//	Pressure Sencer
	writeI2c( PRES_SNCR_START, PRES_SNCR_ONE_SHOT );	//	Start One shot
	rdDt = readI2c( PRES_SNCR_RCV_DT_FLG );
	if ( rdDt & PRES_SNCR_RCV_PRES ){
		dt[0] = readI2c( PRES_SNCR_DT_H );
		dt[1] = readI2c( PRES_SNCR_DT_M );
		dt[2] = readI2c( PRES_SNCR_DT_L );
		fdata = (dt[2]<<16)|(dt[1]<<8)|dt[0];
		fdata = fdata*10/4096;
	}
	
	return (int)roundf(fdata);	//	10 times of Pressure(hPa)
	
	//	Temperature
#if 0
	if ( rdDt & PRES_SNCR_RCV_TMPR ){
		dt[0] = readI2c( 0x2b );
		dt[1] = readI2c( 0x2c );
		data = 0x10000 - (dt[1]<<8)|dt[0];
		data = (42.5 - data/480)*10;
	}
#endif
}

//-------------------------------------------------------------------------
//			MPR121 (Touch Sencer : I2c Device)
//-------------------------------------------------------------------------
//	for Touch Sencer
#define		TCH_SNCR_TOUCH_STATUS1		0x00
#define		TCH_SNCR_TOUCH_STATUS2		0x01
#define 	TCH_SNCR_ELE_CFG			0x5e
#define 	TCH_SNCR_MHD_R				0x2b
#define 	TCH_SNCR_MHD_F				0x2f
#define 	TCH_SNCR_ELE0_T				0x41
#define 	TCH_SNCR_FIL_CFG			0x5d
#define 	TCH_SNCR_MHDPROXR			0x36
#define 	TCH_SNCR_EPROXTTH			0x59

// Threshold defaults
#define		E_THR_T      0x0c	// Electrode touch threshold
#define		E_THR_R      0x08	// Electrode release threshold
#define		PROX_THR_T   0x02	// Prox touch threshold
#define		PROX_THR_R   0x02	// Prox release threshold

//-------------------------------------------------------------------------
void accessMPR121( void )
{
	int		address = TOUCH_SENSOR_ADDRESS;  // I2C

	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(TOUCH)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void initMPR121( void )
{
	int	i, j;
	
	//	Start Access
	accessMPR121();
	
	//	Init Parameter
	// Put the MPR into setup mode
    writeI2c(TCH_SNCR_ELE_CFG,0x00);
    
    // Electrode filters for when data is > baseline
    unsigned char gtBaseline[] = {
		0x01,  //MHD_R
		0x01,  //NHD_R
		0x00,  //NCL_R
		0x00   //FDL_R
	};
	for ( i=0; i<4; i++ ) writeI2c(TCH_SNCR_MHD_R+i,gtBaseline[i]);
	
	// Electrode filters for when data is < baseline
	unsigned char ltBaseline[] = {
        0x01,   //MHD_F
        0x01,   //NHD_F
        0xFF,   //NCL_F
        0x02    //FDL_F
	};
	for ( i=0; i<4; i++ ) writeI2c(TCH_SNCR_MHD_F+i,ltBaseline[i]);

    // Electrode touch and release thresholds
    unsigned char electrodeThresholds[] = {
        E_THR_T, // Touch Threshhold
        E_THR_R  // Release Threshold
	};
	
    for( j=0; j<12; j++ ){
		for ( i=0; i<2; i++ ){
        	writeI2c(TCH_SNCR_ELE0_T+(j*2)+i,electrodeThresholds[i]);
    	}
	}
	
    // Proximity Settings
    unsigned char proximitySettings[] = {
        0xff,   //MHD_Prox_R
        0xff,   //NHD_Prox_R
        0x00,   //NCL_Prox_R
        0x00,   //FDL_Prox_R
        0x01,   //MHD_Prox_F
        0x01,   //NHD_Prox_F
        0xff,   //NCL_Prox_F
        0xff,   //FDL_Prox_F
        0x00,   //NHD_Prox_T
        0x00,   //NCL_Prox_T
        0x00    //NFD_Prox_T
	};
    for ( i=0; i<11; i++ ) writeI2c(TCH_SNCR_MHDPROXR+i,proximitySettings[i]);
	
    unsigned char proxThresh[] = {
        PROX_THR_T, // Touch Threshold
        PROX_THR_R  // Release Threshold
	};
    for ( i=0; i<2; i++ ) writeI2c(TCH_SNCR_EPROXTTH+i,proxThresh[i]);
	
    writeI2c(TCH_SNCR_FIL_CFG,0x04);
    
    // Set the electrode config to transition to active mode
    writeI2c(TCH_SNCR_ELE_CFG,0x0c);
}
//-------------------------------------------------------------------------
unsigned short getTchSwData( void )
{
	unsigned char buf[2] = { 0xff, 0xff };

	//	Start Access
	accessMPR121();
	
	if (read(i2cDscript, buf, 2) != 2) {	// Read back data into buf[]
		printf("Unable to read from slave(Touch)\n");
		//exit(1);
		return 0xffff;
	}
	
	return (buf[1]<<8) | buf[0];
}

//-------------------------------------------------------------------------
//			ADS1015 (ADC Sencer : I2c Device)
//-------------------------------------------------------------------------
//	for ADC Sencer

//-------------------------------------------------------------------------
void accessADS1015( void )
{
	int		address = ADC_ADDRESS;  // I2C
	
	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(ADC)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void setNext( int adNum )
{
	unsigned char buf[3];
	
	buf[0] = 0x01;
	buf[1] = 0xc3 + (adNum << 4);
	buf[2] = 0x83;
	
	if ((write(i2cDscript, buf, 3)) != 3) {			// Write commands to the i2c port
		printf("Error writing to i2c slave(ADC)\n");
		//exit(1);
	}
}
//-------------------------------------------------------------------------
unsigned char getValue( void )
{
	unsigned char buf[2];
	buf[0] = 0x00;								// This is the register we wish to read from
	
	if (write(i2cDscript, buf, 1) != 1) {			// Send the register to read from
		printf("Error writing to i2c slave(ADC:read)\n");
		//exit(1);
		return 0xff;
	}
	
	if (read(i2cDscript, buf, 1) != 1) {					// Read back data into buf[]
		printf("Unable to read from slave(ADC)\n");
		//exit(1);
		return 0xff;
	}
	
	return buf[0];
}
//-------------------------------------------------------------------------
void initADS1015( void )
{
	//	Start Access
	accessADS1015();
	
	//	Init Parameter
	setNext(0);
}
//-------------------------------------------------------------------------
unsigned char getVolume( int number )
{
	unsigned char ret;
	
	accessADS1015();
	ret = getValue();
	
	if ( number >= 2) setNext(0);
	else setNext(number+1);
	
	return ret;
}

//-------------------------------------------------------------------------
//			BlinkM ( Full Color LED : I2c Device)
//-------------------------------------------------------------------------
void accessBlinkM( void )
{
	int		address = LED_BLINKM_ADDRESS;  // I2C
	
	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(LED)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void writeBlinkM( unsigned char cmd, unsigned char* color )
{
	unsigned char buf[4];

	buf[0] = cmd;									// Commands for performing a ranging
	buf[1] = *color;
	buf[2] = *(color+1);
	buf[3] = *(color+2);
	
	if ((write(i2cDscript, buf, 4)) != 4) {			// Write commands to the i2c port
		printf("Error writing to i2c slave(LED)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void initBlinkM( void )
{
	unsigned char color[3] = {0x00,0xff,0x00};
	accessBlinkM();
	writeI2c('o', 0 );			//	stop script
	writeBlinkM('n',color);
}
//-------------------------------------------------------------------------
void changeColor( unsigned char* color )
{
	accessBlinkM();
	writeBlinkM('c',color);
}


//-------------------------------------------------------------------------
//			Adafruit88matrix ( 8*8 LED Matrix : I2c Device)
//-------------------------------------------------------------------------
#define HT16K33_BLINK_CMD 0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF 0
#define HT16K33_BLINK_2HZ  1
#define HT16K33_BLINK_1HZ  2
#define HT16K33_BLINK_HALFHZ  3
#define HT16K33_CMD_BRIGHTNESS 0xE0
#define MATRIX_MAX	8

//-------------------------------------------------------------------------
void accessAda88( void )
{
	int		address = LED_ADA88_ADDRESS;  // I2C
	
	// Set Address
	if (ioctl(i2cDscript, I2CSLAVE_, address) < 0){
		printf("Unable to get bus access to talk to slave(LED Matrix)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void writeAda88( unsigned char cmd, unsigned char* bitPtn )
{
	unsigned char buf[MATRIX_MAX+1];
	int		i;
	
	buf[0] = cmd;									// Commands for performing a ranging
	for ( i=0; i<MATRIX_MAX; i++ ){
		buf[i*2+1] = *(bitPtn+i);
		buf[i*2+2] = 0;
	}
	
	if ((write(i2cDscript, buf, MATRIX_MAX+1)) != MATRIX_MAX+1) {	// Write commands to the i2c port
		printf("Error writing to i2c slave(LED)\n");
		exit(1);
	}
}
//-------------------------------------------------------------------------
void initAda88( void )
{
	unsigned char bitPtn[MATRIX_MAX] = {0xaa,0x00,0xaa,0x01,0x55,0x10,0x55,0x80};
	unsigned char cmd;
	
	accessAda88();
	cmd = 0x21;
	if ((write(i2cDscript, &cmd, 1)) != 1) {			// Write commands to the i2c port
		printf("Error writing to i2c slave(LED)\n");
		exit(1);
	}

	cmd = HT16K33_BLINK_CMD|HT16K33_BLINK_DISPLAYON|(HT16K33_BLINK_OFF<<1);
	if ((write(i2cDscript, &cmd, 1)) != 1) {			// Write commands to the i2c port
		printf("Error writing to i2c slave(LED)\n");
		exit(1);
	}

	cmd = HT16K33_CMD_BRIGHTNESS | 0x0f;
	if ((write(i2cDscript, &cmd, 1)) != 1) {			// Write commands to the i2c port
		printf("Error writing to i2c slave(LED)\n");
		exit(1);
	}
	writeAda88(0,bitPtn);
}
//-------------------------------------------------------------------------
void writePicture( unsigned char* bitPtn )
{
	accessAda88();
	writeAda88(0,bitPtn);
}
