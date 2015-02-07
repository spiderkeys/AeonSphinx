#pragma once

// Includes
extern "C"
{
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include <pocketsphinx.h>
}

#include "easylogging.h"

class CSphinxEngine
{
public:
	// Static members
	static const arg_t ARGUMENT_DEFINITIONS[];
	static jmp_buf m_sJumpBuffer;

	// Attributes
	ps_decoder_t 	*m_pPocketSphinxDecoder;
	cmd_ln_t 		*m_pConfig;

	int 			m_argumentCount;
	char 			**m_commandLineArgs;




	// Methods
	CSphinxEngine( int argc, char *argv[] );
	virtual ~CSphinxEngine();

	bool Initialize();

	void ProcessorLoop();

	void Cleanup();

	bool LoadConfiguration();

	void ProcessMicrophoneInput();


	void Msleep( int32 msIn );

	static void sighandler( int signo );
};
