#pragma once

#include "CSphinxEngine.h"

class CApplication
{
public:
	// Pointers

	// Attributes
	CSphinxEngine m_engine;

	// Methods
	CApplication( int argc, char *argv[] );
	virtual ~CApplication();

	bool Initialize();
	void Update();
	void Cleanup();
};
