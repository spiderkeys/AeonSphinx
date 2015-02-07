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
	// Pointers

	int m_argc;
	char **m_argv;

	// Attributes
	static const arg_t cont_args_def[];

	ps_decoder_t *ps;
	cmd_ln_t *config;
	static jmp_buf jbuf;

	// Methods
	CSphinxEngine( int argc, char *argv[] );
	virtual ~CSphinxEngine();

	bool Initialize();

	void UpdateLoop();

	void Cleanup();

	bool LoadConfiguration();

	void RecognizeFromMicrophone();


	void Msleep( int32 msIn );

	static void sighandler( int signo );
};
