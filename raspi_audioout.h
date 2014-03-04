//
//  raspi_audioout.h
//  ToneGenerator
//
//  Created by 長谷部 雅彦 on 2013/04/07.
//  Copyright (c) 2013年 長谷部 雅彦. All rights reserved.
//

#ifndef __raspi_audioout__
#define __raspi_audioout__

#include <iostream>

class AudioOutput {

public:
	void	SetTg( void* tg ){ _tg = tg; }
	void*	GetTg( void ){ return _tg; }
	
private:
	void*	_tg;
};
#endif /* defined(__ToneGenerator__raspi_audioout__) */
