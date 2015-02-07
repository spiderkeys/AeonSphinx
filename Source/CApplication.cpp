// Includes
#include "CApplication.h"

CApplication::CApplication( int argc, char* argv[] ) : m_engine( argc, argv )
{
}

CApplication::~CApplication()
{

}

bool CApplication::Initialize()
{
	m_engine.Initialize();

	return true;
}

void CApplication::Update()
{
	m_engine.ProcessorLoop();
}

void CApplication::Cleanup()
{
	m_engine.Cleanup();
}
