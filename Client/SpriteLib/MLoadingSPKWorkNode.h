//----------------------------------------------------------------------
// MLoadingSPKWorkNode.h
//----------------------------------------------------------------------
// Naming the classes was a headache, so they are numbered.
//
// Each one loads an SPK; they differ in how, hence several nodes.
//
// [Remain]: when a node is stopped while it runs, the work left
// over has to become a node again;
// this is the kind of node made then.
/*

  [ Common ]
  		CSpritePack*	SPK
		char			SPKFilename


  [Node 1]
		= reads the SFPArray at FilePosition and loads the SPK by it.
		
		char			SFPFilename
		long			FilePosition

		[Remain] --> Node 2


  [Node 2]
		= loads the SPK by the SFPArray.

		SFPArray*		SFPA

		[Remain] --> Node 2
		

  [Node 3]
		= reads one sprite at FilePosition into the SPK's entry ID.

		TYPE_SPRITEID	SpriteID
		long			FilePosition

		[Remain] --> NULL

  [Node 4]
		= reads the SPK's FirstID to LastID from FilePosition.

		TYPE_SPRITEID	FirstSpriteID
		TYPE_SPRITEID	LastSpriteID
		long			FilePosition

		[Remain] --> Node 4


*/
//----------------------------------------------------------------------

#ifndef __MLOADINSPKWORKNODE_H__
#define __MLOADINSPKWORKNODE_H__

#include "MWorkNode.h"
#include <string>
#include "CSpriteDef.h"
//#include "CSpritePack.h"
#include "CSpritePack.h"
#include <fstream>


#define	MAX_FILENAME		256

// Opening a data file is the program's (FileOpenBinary: the data-path
// spelling, and the init-failure report when it fails). Without a host no
// file opens, and a node that needs one fails (task 4.217).
struct MLoadingSPKHost
{
	bool (*OpenBinary)(const char* filename, std::ifstream& file);
};

void	SetLoadingSPKHost(const MLoadingSPKHost* pHost);

//----------------------------------------------------------------------
// MLoadingSPKWorkNode
//----------------------------------------------------------------------
// Loads the SPK from the SPK file, one way or another.
//----------------------------------------------------------------------
class MLoadingSPKWorkNode : public MWorkNode {
	public :
		MLoadingSPKWorkNode()	{ m_pSPK = NULL; m_bExecute = TRUE; }
		~MLoadingSPKWorkNode() {}

		//---------------------------------------------------
		// Loads something (?) of SPKFilename into pSPK.
		//---------------------------------------------------
		void			SetSPK(CSpritePack* pSPK, const char* filename)
		{
			m_pSPK = pSPK;
			m_SPKFilename = filename ? filename : "";
		}

	protected :
		CSpritePack*			m_pSPK;
		std::string			m_SPKFilename;
};

//----------------------------------------------------------------------
// Node 1
//----------------------------------------------------------------------
class MLoadingSPKWorkNode1 : public MLoadingSPKWorkNode {
	public :
		MLoadingSPKWorkNode1(const char* sfpaFilename, long filePosition) { m_SFPAFilename = sfpaFilename ? sfpaFilename : ""; m_FilePosition = filePosition; }
		~MLoadingSPKWorkNode1() {}

		BOOL		Execute(MWorkNode*& pRemainNode);

	protected :
		std::string			m_SFPAFilename;
		long							m_FilePosition;
};

//----------------------------------------------------------------------
// Node 2
//----------------------------------------------------------------------
// The node deletes m_pSFPA.
//----------------------------------------------------------------------
class MLoadingSPKWorkNode2 : public MLoadingSPKWorkNode {
	public :
		MLoadingSPKWorkNode2(CSpriteFilePositionArray* pSFPA) { m_pSFPA = pSFPA; }
		~MLoadingSPKWorkNode2() { if (m_pSFPA!=NULL) delete m_pSFPA; }

		BOOL		Execute(MWorkNode*& pRemainNode);
		
	protected :
		CSpriteFilePositionArray*		m_pSFPA;
};

//----------------------------------------------------------------------
// Node 3
//----------------------------------------------------------------------
class MLoadingSPKWorkNode3 : public MLoadingSPKWorkNode {
	public :
		MLoadingSPKWorkNode3(TYPE_SPRITEID spriteID, long fp) { m_SpriteID = spriteID; m_FilePosition = fp; }
		~MLoadingSPKWorkNode3() {}

		BOOL		Execute(MWorkNode*& pRemainNode);
		
	protected :	
		TYPE_SPRITEID			m_SpriteID;
		long					m_FilePosition;
};

//----------------------------------------------------------------------
// Node 4
//----------------------------------------------------------------------
class MLoadingSPKWorkNode4 : public MLoadingSPKWorkNode {
	public :
		MLoadingSPKWorkNode4(TYPE_SPRITEID fID, TYPE_SPRITEID lID, long fp) { m_FirstSpriteID = fID; m_LastSpriteID = lID; m_FilePosition = fp; }
		~MLoadingSPKWorkNode4() {}

		BOOL		Execute(MWorkNode*& pRemainNode);
		

	protected :
		TYPE_SPRITEID			m_FirstSpriteID;
		TYPE_SPRITEID			m_LastSpriteID;
		long					m_FilePosition;
};

#endif

