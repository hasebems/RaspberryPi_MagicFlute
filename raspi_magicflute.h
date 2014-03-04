//
//  raspi_magicflute.h
//  ToneGenerator
//
//  Created by 長谷部 雅彦 on 2014/02/22.
//  Copyright (c) 2014年 長谷部 雅彦. All rights reserved.
//

#ifndef __ToneGenerator__raspi_magicflute__
#define __ToneGenerator__raspi_magicflute__

void eventLoopInit( void );
void eventLoop( void );

void initHw( void );
void quitHw( void );

#endif /* defined(__ToneGenerator__raspi_magicflute__) */
