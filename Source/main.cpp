#include "CApplication.h"

// Set up easylogger
INITIALIZE_EASYLOGGINGPP

int main( int argc, char *argv[] )
{
	CApplication app( argc, argv );

	app.Initialize();

	app.Update();

	app.Cleanup();
}
