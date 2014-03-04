//
//  raspi_cwrap.h
//  ToneGenerator
//
//  Created by 長谷部 雅彦 on 2013/04/07.
//  Copyright (c) 2013年 長谷部 雅彦. All rights reserved.
//

#ifndef raspi_cwrap_h
#define raspi_cwrap_h

//--------------------------------------------------------
//		extern "C"
//--------------------------------------------------------
#if __cplusplus
extern "C" {
#endif
	void raspiaudio_Init( void );
	void raspiaudio_End( void );
	int	raspiaudio_Process( int16_t* buf, int bufsize );
	void raspiaudio_ReduceResource( void );
	int	raspiaudio_Message( unsigned char* message, int msgsize );
#if __cplusplus
}
#endif

#endif
