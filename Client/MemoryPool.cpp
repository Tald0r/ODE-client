/********************************************************************
	created:	2003/12/05
	created:	5:12:2003   13:40
	filename: 	MemoryPool.cpp
	file ext:	cpp
	author:		sonee	// 손히승 바보
	
	purpose:	memory pool
				고정된 크기를 빈번하게 new/delete 하는 경우 메모리 풀을 사용하면
				메모리 단편화를 줄일 수 있다.

				메모리 leak 현상을 막을 수 있다.

				Debug 모드인 경우에는 메모리가 MEMORY_POOL_GARBAGE 값으로
				채워진다.
*********************************************************************/
#include "Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <memory.h>
#include "MemoryPool.h"

#include "MNPC.h"
#include "MFakeCreature.h"


// g_CreatureMemoryPool is MCreature.cpp's (gamemodel, task 4.242).
// g_CreatureWearMemoryPool is MCreatureWear.cpp's (gamemodel, task 4.243).
MemoryPool g_NPCCreatureMemoryPool( sizeof( MNPC ), 5 );
MemoryPool g_FakeCreatureMemoryPool( sizeof( MFakeCreature ), 30 );

