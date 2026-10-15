//-----------------------------------------------------------------------------
// Alignment.cpp
//-----------------------------------------------------------------------------
// The alignment steps (task 4.235: PacketFunction.cpp's until then).
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "Alignment.h"
#include "DebugLog.h"

//-----------------------------------------------------------------------------
// Convert Alignment
//-----------------------------------------------------------------------------
int
ConvertAlignment(int alignment2)
{
	int alignment = alignment2;

	// A negative value comes as 16 bits.
	if (alignment > 10000)
	{	
		alignment = - (short)(~alignment + 1);

		DEBUG_ADD_FORMAT("Modify AlignmentValue = %d --> %d", alignment2, alignment);
	}

	const int numAlignment = 5;
	int alignmentValue[numAlignment] =
	{
		-7501,		// evil+
		-2501,		// evil
		2500,		// neutral
		7500,		// good
		10000		// good+
	};

	for (int i=0; i<numAlignment; i++)
	{
		if (alignment <= alignmentValue[i])
		{
			return i;
		}
	}

	return 0;
}
