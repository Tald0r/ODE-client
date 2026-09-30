//----------------------------------------------------------------------
// 
// Filename    : LCPCList.cpp 
// Written By  : Reiot
// Description :
// 
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "LCPCList.h"
#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"
#include "PCOustersInfo.h"

#include <memory>

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
LCPCList::LCPCList ()
{
	for ( uint i = 0 ; i < SLOT_MAX ; i ++ )
		m_pPCInfos[i] = NULL;
}


//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
LCPCList::~LCPCList ()
{
	// heap 에 생성된 PC Type 변수들을 삭제해야 한다.
	for ( uint i = 0 ; i < SLOT_MAX ; i ++ ) {
		if ( m_pPCInfos[i] != NULL ) {
			delete m_pPCInfos[i];
			m_pPCInfos[i] = NULL;
		}
	}
}


//----------------------------------------------------------------------
// Reads the SLOT_MAX PC type tags, then one PC info per character, each
// stored in the slot its own slot byte names.
//----------------------------------------------------------------------
namespace {

// Reads one Info and stores it in the slot it names. Info::read refuses
// a slot byte of SLOT_MAX or more with InvalidProtocolException, so the
// index is in range. The info is owned here until it is stored, so a
// refused or short read frees it. A slot named twice keeps the later
// info, as it always has, and frees the earlier one.
template <class Info>
void	ReadPCInfo ( SocketInputStream & iStream , PCInfo * pPCInfos[SLOT_MAX] )
{
	std::unique_ptr<Info> pInfo( new Info() );
	pInfo->read( iStream );

	const Slot slot = pInfo->getSlot();
	delete pPCInfos[ slot ];
	pPCInfos[ slot ] = pInfo.release();
}

} // namespace

void LCPCList::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	//--------------------------------------------------
	// The PC type of each slot, one byte each: 'S', 'V'
	// or 'O' for a character, '0' for an empty slot.
	//--------------------------------------------------
	char pcTypes[SLOT_MAX];

	for ( uint i = 0 ; i < SLOT_MAX ; i ++ )
		iStream.read(pcTypes[i]);

	//--------------------------------------------------
	// Then the PC info of each character, in tag order.
	//--------------------------------------------------
	for ( uint j = 0 ; j < SLOT_MAX ; j ++ ) {

		switch ( pcTypes[j] ) {

			case 'S' :
				ReadPCInfo<PCSlayerInfo>( iStream , m_pPCInfos );
				break;

			case 'V' :
				ReadPCInfo<PCVampireInfo>( iStream , m_pPCInfos );
				break;

			case 'O' :
				ReadPCInfo<PCOustersInfo>( iStream , m_pPCInfos );
				break;
				
			case '0' :
				break;

			default :
				throw InvalidProtocolException("invalid pc type");
		}

	}

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void LCPCList::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	//--------------------------------------------------
	// 일단 PC 타입을 쓴다. 
	//
	// 나중에는 이 정보를 1 바이트에 넣어서 비트 연산을 하도록 한다.
	//
	// ex>
	// 	S0V : Slayer-EMPTY-VAMPIRE
	// 	00S : EMPTY-EMPTY-SLAYER
	//
	//--------------------------------------------------
	for ( uint i = 0 ; i < SLOT_MAX ; i ++ ) {

		if ( m_pPCInfos[i] ) {	// m_pPCInfos[i] != NULL

			switch(m_pPCInfos[i]->getPCType())
			{
			case PC_SLAYER:
				oStream.write( 'S' );
				break;

			case PC_VAMPIRE:
				oStream.write( 'V' );
				break;

			case PC_OUSTERS:
				oStream.write( 'O' );
				break;
			}
		} else {				// m_pPCInfos[i] == NULL
			oStream.write( '0' );
		}
	}

	//--------------------------------------------------
	// 그다음 PCType 객체 본체를 쓴다.
	//--------------------------------------------------
	for ( uint j = 0 ; j < SLOT_MAX ; j ++ ) {
		if ( m_pPCInfos[j] != NULL ) {
			m_pPCInfos[j]->write( oStream );
		}
	}

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
PacketSize_t LCPCList::getPacketSize () const
{
	PacketSize_t packetSize = 0;

	packetSize = SLOT_MAX*sizeof(char);
	
	for ( uint i = 0 ; i < SLOT_MAX ; i ++ ) {
		if ( m_pPCInfos[i] ) { // m_pPCInfos[i] != NULL
			packetSize += m_pPCInfos[i]->getSize();
		}
	}
	return packetSize;
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
std::string LCPCList::toString () const
	{
		__BEGIN_TRY

		StringStream msg;

		msg << "LCPCList(\n";

		for ( uint i = 0 ; i < SLOT_MAX ; i ++ )
			if ( m_pPCInfos[i] != NULL )
				msg << m_pPCInfos[i]->toString() << "\n";
			else
				msg << "EMPTY SLOT\n";

		msg << ")";

		return msg.toString();

		__END_CATCH
	}


#endif