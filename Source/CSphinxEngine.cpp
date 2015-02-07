// Includes
#include "CSphinxEngine.h"

const arg_t CSphinxEngine::cont_args_def[] =
	{
		POCKETSPHINX_OPTIONS,
		/* Argument file. */
		{ "-argfile",
		  ARG_STRING,
		  NULL,
		  "Argument file giving extra arguments."
		},
		{ "-adcdev",
		  ARG_STRING,
		  NULL,
		  "Name of audio device to use for input."
		},
		{ "-infile",
		  ARG_STRING,
		  NULL,
		  "Audio file to transcribe."
		},
		{ "-time",
		  ARG_BOOLEAN,
		  "no",
		  "Print word times in file transcription."
		},
		CMDLN_EMPTY_OPTION
	};

jmp_buf CSphinxEngine::jbuf = {};

CSphinxEngine::~CSphinxEngine()
{

}

CSphinxEngine::CSphinxEngine(int argc, char* argv[])
{
	m_argc = argc;
	m_argv = argv;
}

bool CSphinxEngine::Initialize()
{
	LoadConfiguration();
}

void CSphinxEngine::UpdateLoop()
{
	signal( SIGINT, &sighandler );

	if( setjmp( jbuf ) == 0 )
	{
		RecognizeFromMicrophone();
	}
}

void CSphinxEngine::Cleanup()
{
	 ps_free( ps );
}

bool CSphinxEngine::LoadConfiguration()
{
	char const *cfg;

	err_set_logfile( "/dev/null" );

	if( m_argc == 2 )
	{
		config = cmd_ln_parse_file_r( NULL, cont_args_def, m_argv[1], TRUE );
	}
	else
	{
		config = cmd_ln_parse_r( NULL, cont_args_def, m_argc, m_argv, FALSE );
	}
	/* Handle argument file as -argfile. */
	if( config && ( cfg = cmd_ln_str_r( config, "-argfile" ) ) != NULL )
	{
		config = cmd_ln_parse_file_r( config, cont_args_def, cfg, FALSE );
	}

	if( config == NULL )
	{
		return false;
	}

	ps = ps_init( config );

	if( ps == NULL )
	{
		return false;
	}

	return true;
}

void CSphinxEngine::RecognizeFromMicrophone()
{
	ad_rec_t *ad;
	int16 adbuf[4096];
	int32 k, ts, rem;
	char const *hyp;
	char const *uttid;
	cont_ad_t *cont;
	char word[256];

	if( ( ad = ad_open_dev( cmd_ln_str_r( config, "-adcdev" ), (int)cmd_ln_float32_r( config, "-samprate" ) ) ) == NULL )
	{
		LOG( ERROR ) << "Failed to open audio device.";
	}

	/* Initialize continuous listening module */
	if( ( cont = cont_ad_init( ad, ad_read ) ) == NULL )
	{
		LOG( ERROR ) << "Failed to initialize voice activity detection.";
	}

	if( ad_start_rec( ad ) < 0 )
	{
		LOG( ERROR ) << "Failed to start recording.";
	}

	if( cont_ad_calib( cont ) < 0 )
	{
		LOG( ERROR ) << "Failed to calibrate voice activity detection.";
	}

	for (;;)
	{
		/* Indicate listening for next utterance */
		LOG( INFO ) << "READY...";

		/* Wait data for next utterance */
		while( ( k = cont_ad_read( cont, adbuf, 4096 ) ) == 0 )
		{
			Msleep(100);
		}

		if( k < 0 )
		{
			LOG( ERROR ) << "Failed to read audio.";
		}

		/*
		 * Non-zero amount of data received; start recognition of new utterance.
		 * NULL argument to uttproc_begin_utt => automatic generation of utterance-id.
		 */
		if( ps_start_utt( ps, NULL ) < 0 )
		{
			LOG( ERROR ) << "Failed to start utterance.";
		}

		ps_process_raw( ps, adbuf, k, FALSE, FALSE );

		LOG( INFO ) << "Listening...";

		/* Note timestamp for this first block of data */
		ts = cont->read_ts;

		/* Decode utterance until end (marked by a "long" silence, >1sec) */
		for(;;)
		{
			/* Read non-silence audio data, if any, from continuous listening module */
			if( ( k = cont_ad_read( cont, adbuf, 4096 ) ) < 0 )
			{
				LOG( ERROR ) << "Failed to read audio.";
			}
			if( k == 0 )
			{
				/*
				 * No speech data available; check current timestamp with most recent
				 * speech to see if more than 1 sec elapsed.  If so, end of utterance.
				 */
				if( ( cont->read_ts - ts ) > DEFAULT_SAMPLES_PER_SEC )
				{
					break;
				}
			}
			else
			{
				/* New speech data received; note current timestamp */
				ts = cont->read_ts;
			}

			/*
			 * Decode whatever data was read above.
			 */
			rem = ps_process_raw( ps, adbuf, k, FALSE, FALSE );

			/* If no work to be done, sleep a bit */
			if( ( rem == 0 ) && ( k == 0 ) )
			{
				Msleep( 20 );
			}
		}

		/*
		 * Utterance ended; flush any accumulated, unprocessed A/D data and stop
		 * listening until current utterance completely decoded
		 */
		ad_stop_rec( ad );

		while( ad_read( ad, adbuf, 4096 ) >= 0 );
		{
			cont_ad_reset(cont);
		}

		LOG( INFO ) << "Stopped listening, please wait...";

		/* Finish decoding, obtain and print result */
		ps_end_utt( ps );

		hyp = ps_get_hyp( ps, NULL, &uttid );
		LOG( INFO ) << uttid << ": " << hyp;

		/* Exit if the first word spoken was GOODBYE */
		if( hyp )
		{
			sscanf( hyp, "%s", word );

			if( strcmp( word, "goodbye" ) == 0 )
			{
				break;
			}
		}

		/* Resume A/D recording for next utterance */
		if (ad_start_rec(ad) < 0)
		{
			LOG( ERROR ) << "Failed to start recording.";
		}
	}

	cont_ad_close( cont );
	ad_close( ad );
}


void CSphinxEngine::Msleep( int32 msIn )
{
	struct timeval tmo;

	tmo.tv_sec = 0;
	tmo.tv_usec = msIn * 1000;

	select( 0, NULL, NULL, NULL, &tmo );
}

void CSphinxEngine::sighandler( int signo )
{
	longjmp( jbuf, 1 );
}
