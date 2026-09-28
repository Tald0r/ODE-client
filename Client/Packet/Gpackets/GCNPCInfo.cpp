//-------------------------------------------------------------------------------- // 
// Filename    : GCNPCInfo.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "GCNPCInfo.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PacketAssert.h"

// for client.. by sigi
#ifndef SAFE_DELETE
	#define SAFE_DELETE(p)	if (p!=NULL) { delete p; p=NULL; }
#endif

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCNPCInfo::GCNPCInfo ()
{
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCNPCInfo::~GCNPCInfo ()
{
	std::list<NPCInfo*>::iterator itr = m_NPCInfos.begin();
	for (; itr != m_NPCInfos.end(); itr++)
	{
		NPCInfo* pInfo = *itr;
		SAFE_DELETE(pInfo);
	}
}

//--------------------------------------------------------------------------------
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//--------------------------------------------------------------------------------
void GCNPCInfo::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
		
	//////////////////////////////////////////////////
	// read npc info
	//////////////////////////////////////////////////
	BYTE NPCInfoCount = 0;
	iStream.read(NPCInfoCount);
	for (BYTE nc=0; nc<NPCInfoCount; nc++)
	{
		NPCInfo* pInfo = new NPCInfo;
		pInfo->read(iStream);
		addNPCInfo(pInfo);
	}

	__END_CATCH
}

		    
//--------------------------------------------------------------------------------
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//--------------------------------------------------------------------------------
void GCNPCInfo::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	//////////////////////////////////////////////////
	// write npc info
	//////////////////////////////////////////////////
	BYTE NPCInfoCount = static_cast<BYTE>(m_NPCInfos.size());
	oStream.write(NPCInfoCount);

	std::list<NPCInfo*>::const_iterator itr = m_NPCInfos.begin();
	for (; itr != m_NPCInfos.end(); itr++)
	{
		NPCInfo* pInfo = *itr;
		pInfo->write(oStream);
	}
		
	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
//--------------------------------------------------------------------------------
// get packet's debug std::string
//--------------------------------------------------------------------------------
std::string GCNPCInfo::toString () const
{
	__BEGIN_TRY
	StringStream msg;
	return msg.toString();

	__END_CATCH
}
#endif