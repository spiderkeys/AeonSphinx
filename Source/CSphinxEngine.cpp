// Includes
#include "CSphinxEngine.h"

// Initialize static members
const arg_t CSphinxEngine::ARGUMENT_DEFINITIONS[] =
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

jmp_buf CSphinxEngine::m_sJumpBuffer = {};


CSphinxEngine::CSphinxEngine( int argCountIn, char* argsIn[] )
{
	m_pPocketSphinxDecoder 	= nullptr;
	m_pConfig 				= nullptr;

	m_argumentCount 		= argCountIn;
	m_commandLineArgs 		= argsIn;
}

CSphinxEngine::~CSphinxEngine()
{

}

bool CSphinxEngine::Initialize()
{
	LOG( INFO ) << "Initializing SphinxEngine...";

	// Load configuration based on command line inputs
	if( !LoadConfiguration() )
	{
		return false;
	}

	// Initialize Pocketsphinx decoder with loaded configurations
	m_pPocketSphinxDecoder = ps_init( m_pConfig );

	if( m_pPocketSphinxDecoder == NULL )
	{
		LOG( ERROR ) << "Failed to initialize Pocketsphinx with provided configuration.";
		return false;
	}

	LOG( INFO ) << "Sphinx Engine Initialized.";

	return true;
}

void CSphinxEngine::ProcessorLoop()
{
	LOG( INFO ) << "Sphinx Engine Processing Started.";

	// Special interrupt handler breaks the recording loop and frees memory before exiting
	signal( SIGINT, &sighandler );

	if( setjmp( m_sJumpBuffer ) == 0 )
	{
		// Run processing loop
		ProcessMicrophoneInput();
	}

	LOG( INFO ) << "Sphinx Engine Processing Complete.";
}

void CSphinxEngine::Cleanup()
{
	// Free pocket sphinx memory
	ps_free( m_pPocketSphinxDecoder );
}

bool CSphinxEngine::LoadConfiguration()
{
	LOG( INFO ) << "Loading Pocketsphinx configuration...";

	char const *configFile;

	// Disable extra debug info in terminal
	err_set_debug_level( 0 );
	err_set_logfp( NULL );

	// Disable annoying stderr messages in libs
	freopen( "/dev/null", "w", stderr );

	// Parse the command line arguments
	if( m_argumentCount == 2 )
	{
		m_pConfig = cmd_ln_parse_file_r( NULL, ARGUMENT_DEFINITIONS, m_commandLineArgs[1], TRUE );
	}
	else
	{
		m_pConfig = cmd_ln_parse_r( NULL, ARGUMENT_DEFINITIONS, m_argumentCount, m_commandLineArgs, FALSE );
	}

	// If an additional config file was specified on the command line, load it and append it to the existing config
	if( m_pConfig && ( configFile = cmd_ln_str_r( m_pConfig, "-argfile" ) ) != NULL )
	{
		// Parse the file and add it to the existing config
		m_pConfig = cmd_ln_parse_file_r( m_pConfig, ARGUMENT_DEFINITIONS, configFile, FALSE );
	}
	else
	{
		LOG( WARNING ) << "Failed to load additional config file.";
	}

	if( m_pConfig == NULL )
	{
		LOG( ERROR ) << "Failed to load configuration";
		return false;
	}

	LOG( INFO ) << "Loaded Pocketsphinx configuration.";

	return true;
}

void CSphinxEngine::Msleep( int32 msIn )
{
	struct timeval tmo;

	tmo.tv_sec = 0;
	tmo.tv_usec = msIn * 1000;

	// Sleep for specified ms
	select( 0, NULL, NULL, NULL, &tmo );
}

void CSphinxEngine::sighandler( int signo )
{
	// Jump out of the processing loop
	longjmp( m_sJumpBuffer, 1 );
}

void CSphinxEngine::ProcessMicrophoneInput()
{
	ad_rec_t *audioDevice;
	int16 audioData[ 4096 ];
	int32 samplesRead, timestamp, rem;
	char const *hyp;
	char const *uttid;
	cont_ad_t *listeningModule;

	// Open the specified audio device with the specified settings
	if( ( audioDevice = ad_open_dev( cmd_ln_str_r( m_pConfig, "-adcdev" ), (int)cmd_ln_float32_r( m_pConfig, "-samprate" ) ) ) == NULL )
	{
		LOG( ERROR ) << "Failed to open audio device.";
		return;
	}

	// Initialize continuous listening module
	if( ( listeningModule = cont_ad_init( audioDevice, ad_read ) ) == NULL )
	{
		LOG( ERROR ) << "Failed to initialize voice activity detection.";
		return;
	}

	// Start recording
	if( ad_start_rec( audioDevice ) < 0 )
	{
		LOG( ERROR ) << "Failed to start recording.";
		return;
	}

	// Calibrate the silence filter - this happens every once in a while. Can be called whenever
	if( cont_ad_calib( listeningModule ) < 0 )
	{
		LOG( ERROR ) << "Failed to calibrate voice activity detection.";
		return;
	}

	// Processing loop
	for( ;; )
	{
		LOG( INFO ) << "Please issue a command:";

		// Wait for next utterance to break the silence
		while( ( samplesRead = cont_ad_read( listeningModule, audioData, 4096 ) ) == 0 )
		{
			Msleep( 100 );
		}

		if( samplesRead < 0 )
		{
			LOG( ERROR ) << "Failed to read audio.";
			return;
		}

		// Non-zero amount of data received; start recognition of new utterance.
		// NULL argument to uttproc_begin_utt => automatic generation of utterance-id.
		if( ps_start_utt( m_pPocketSphinxDecoder, NULL ) < 0 )
		{
			LOG( ERROR ) << "Failed to start utterance.";
			return;
		}

		// Decode raw utterance
		ps_process_raw( m_pPocketSphinxDecoder, audioData, samplesRead, FALSE, FALSE );

		LOG( INFO ) << "Listening...";

		// Note timestamp for this first block of data
		timestamp = listeningModule->read_ts;

		// Keep decoding utterances until end (marked by a "long" silence, >1sec)
		for(;;)
		{
			// Read non-silence audio data, if any, from continuous listening module
			if( ( samplesRead = cont_ad_read( listeningModule, audioData, 4096 ) ) < 0 )
			{
				LOG( ERROR ) << "Failed to read audio.";
			}

			if( samplesRead == 0 )
			{
				// No speech data available. Check current timestamp with most recent speech to see if more than 1 sec elapsed.
				if( ( listeningModule->read_ts - timestamp ) > DEFAULT_SAMPLES_PER_SEC )
				{
					// End utterance
					break;
				}
			}
			else
			{
				// New speech data received. Note current timestamp
				timestamp = listeningModule->read_ts;
			}


			// Decode whatever data was read above.
			rem = ps_process_raw( m_pPocketSphinxDecoder, audioData, samplesRead, FALSE, FALSE );

			// If no work to be done, sleep a bit
			if( ( rem == 0 ) && ( samplesRead == 0 ) )
			{
				Msleep( 20 );
			}
		}

		// Complete command received. Stop recording.
		ad_stop_rec( audioDevice );

		// Flush any accumulated, unprocessed A/D data
		while( ad_read( audioDevice, audioData, 4096 ) >= 0 );
		{
			cont_ad_reset( listeningModule );
		}

		LOG( INFO ) << "Command received, please wait...";

		// End utterance processing
		ps_end_utt( m_pPocketSphinxDecoder );

		// Get the hypothesized string
		hyp = ps_get_hyp( m_pPocketSphinxDecoder, NULL, &uttid );
		std::string command( hyp );

		LOG( INFO ) << "Formulated command: (" << uttid << ") " << command;



		// DO PROCESSING ON THE COMMAND HERE (pass it to another thread for this processing, probably)



		// Resume A/D recording for next utterance
		if( ad_start_rec( audioDevice ) < 0 )
		{
			LOG( ERROR ) << "Failed to restart recording.";
		}
	}

	// Close listening module and audio device
	cont_ad_close( listeningModule );
	ad_close( audioDevice );
}
