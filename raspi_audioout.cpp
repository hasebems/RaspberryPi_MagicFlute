//
//  raspi_audioout.cpp
//  ToneGenerator
//
//  Created by 長谷部 雅彦 on 2013/04/07.
//  Copyright (c) 2013年 長谷部 雅彦. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#include "raspi_audioout.h"

#include "msgf_audio_buffer.h"
#include "msgf_if.h"

#include "raspi_cwrap.h"

static AudioOutput au;

//--------------------------------------------------------
//		Initialize
//--------------------------------------------------------
void raspiaudio_Init( void )
{
	au.SetTg( new msgf::Msgf() );
}

//--------------------------------------------------------
//		End
//--------------------------------------------------------
void raspiaudio_End( void )
{
	void*	tg = au.GetTg();
	delete reinterpret_cast<msgf::Msgf*>(tg);
}

//--------------------------------------------------------
//		Audio Process
//--------------------------------------------------------
int	raspiaudio_Process( int16_t* buf, int bufsize )
{
	msgf::TgAudioBuffer	abuf;						//	MSGF IF
	msgf::Msgf*	tg = reinterpret_cast<msgf::Msgf*>(au.GetTg());
	abuf.obtainAudioBuffer(bufsize);				//	MSGF IF
	tg->process( abuf );							//	MSGF IF
	
	for ( int j = 0; j < bufsize; j++ ) {
		double wv = abuf.getAudioBuffer(j) * 50000;
		wv += rand()/(RAND_MAX/8);	//	Dither
		buf[j] = static_cast<int16_t>(wv);
    }
	
	abuf.releaseAudioBuffer();						//	MSGF IF
	
	return 1;
}

//--------------------------------------------------------
//		Reduce Resource
//--------------------------------------------------------
void raspiaudio_ReduceResource( void )
{
	msgf::Msgf*	tg = reinterpret_cast<msgf::Msgf*>(au.GetTg());
	tg->reduceResource();
}

//--------------------------------------------------------
//		Receive Message
//--------------------------------------------------------
int	raspiaudio_Message( unsigned char* message, int msgsize )
{
	unsigned char	msg[3];
	msgf::Msgf*	tg = reinterpret_cast<msgf::Msgf*>(au.GetTg());

	msg[0] = *message;
	msg[1] = *(message+1);
	msg[2] = *(message+2);
	tg->sendMessage( 3, msg );						//	MSGF IF

	return 1;
}

