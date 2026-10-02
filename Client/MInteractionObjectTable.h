//----------------------------------------------------------------------
// MInteractionObjectTable.h
//----------------------------------------------------------------------
// InteractionObject의 종류에 따른 정보를 저장하는 class
//----------------------------------------------------------------------
//
// [ 종류별로 있는 정보 ] 
//
// - Type ID
// - Action에 대응하는 FrameID
// - 이름... 등등...
// 
//----------------------------------------------------------------------

#ifndef	__MINTERACTIONOBJECTTABLE_H__
#define	__MINTERACTIONOBJECTTABLE_H__


#include "MObject.h"
#include "CTypeTable.h"
#include "DrawTypeDef.h"
#include "MTypeDef.h"

#include <fstream>

//----------------------------------------------------------------------
//
// 한 종류의 InteractionObject에 대한 정보
//
//----------------------------------------------------------------------
class INTERACTIONOBJECTTABLE_INFO {
	public :
		BYTE					Type = 0;		// InteractionObject Type
		TYPE_FRAMEID			FrameID = 0;	// FrameID
		int						Property = 0;	// 속성
		TYPE_SOUNDID			SoundID = 0;	// 상태를 변경했을 때의 SoundID

	public :
		//-------------------------------------------------------
		// File I/O
		//-------------------------------------------------------
		// Eleven bytes, little-endian. The legacy sound slot is four bytes:
		// read its low word and write its upper word as zero, never padding.
		// An incomplete read leaves the previous record intact.
		void			SaveToFile(std::ofstream& file);		
		void			LoadFromFile(std::ifstream& file);
		
};


typedef CTypeTable<INTERACTIONOBJECTTABLE_INFO>		INTERACTIONOBJECT_TABLE;
extern INTERACTIONOBJECT_TABLE* 	g_pInteractionObjectTable;

#endif

