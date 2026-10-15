//----------------------------------------------------------------------
// MLoadingSPKWorkNode.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MLoadingSPKWorkNode.h"

namespace {
const MLoadingSPKHost* s_pHost = NULL;

bool
OpenBinary(const char* filename, std::ifstream& file)
{
	return s_pHost != NULL && s_pHost->OpenBinary(filename, file);
}
}

void
SetLoadingSPKHost(const MLoadingSPKHost* pHost)
{
	s_pHost = pHost;
}

//----------------------------------------------------------------------
// MLoadingSPKWorkNode1 :: Execute
//----------------------------------------------------------------------
//		= reads the SFPArray at FilePosition and loads the SPK by it.
//		CSpritePack*	SPK
//		long			FilePosition
//
//		[Remain] --> Node 2
//----------------------------------------------------------------------
BOOL
MLoadingSPKWorkNode1::Execute(MWorkNode*& pRemainNode)
{
	// Running
	//m_bExecute = TRUE;

	//---------------------------------------------------
	//
	// Load SpriteFilePositionArray 
	//
	//---------------------------------------------------
	std::ifstream file;
	if (!OpenBinary(m_SFPAFilename.c_str(), file))
	{
		// -_-;;
		m_bExecute = FALSE;
		pRemainNode = NULL;
		return FALSE;
	}
	CSpriteFilePositionArray SFPA;
	SFPA.LoadFromFile( file );
	file.close();

	//---------------------------------------------------
	//
	// SPK Load
	//
	//---------------------------------------------------
	std::ifstream spkFile;
	if (!OpenBinary(m_SPKFilename.c_str(), spkFile))
	{
		// -_-;;
		m_bExecute = FALSE;
		pRemainNode = NULL;
		return FALSE;
	}

	int size = SFPA.GetSize();
	for (int i=0; i<size; i++)
	{
		//---------------------------------------------------
		// Still loading
		//---------------------------------------------------
		if (m_bExecute)
		{
			// Seek to the sprite's position.
			spkFile.seekg( SFPA[i].FilePosition, ios::beg );

			CSprite& sprite = (*m_pSPK)[SFPA[i].SpriteID];

			//---------------------------------------------------
			// Load the sprite.
			//---------------------------------------------------
			// Only if it is not loaded yet.
			if (sprite.IsInit())
				continue;

			sprite.LoadFromFile( spkFile );
		}
		//---------------------------------------------------
		// Stopped
		//---------------------------------------------------
		else
		{
			//---------------------------------------------------
			// It was stopped.
			//---------------------------------------------------
			// The rest of the SFPA becomes a new SFPA.
			// --> Node2	
			CSpriteFilePositionArray* pRemainSFPA = new CSpriteFilePositionArray;

			int remainSize = size - i;

			pRemainSFPA->Init( remainSize );			

			// Copy the rest of the SFPA.
			for (int j=0; j<remainSize; j++)
			{
				(*pRemainSFPA)[j] = SFPA[i + j];
			}

			MLoadingSPKWorkNode2* pNode = new MLoadingSPKWorkNode2( pRemainSFPA );

			pNode->SetSPK( m_pSPK, m_SPKFilename.c_str() );
			pNode->SetType( m_Type );

			// Hand over the rest.
			spkFile.close();
			pRemainNode = pNode;
			return FALSE;
		}
	}
	spkFile.close();

	// Done
	m_bExecute = FALSE;

	// Finished normally
	pRemainNode = NULL;
	return TRUE;
}


//----------------------------------------------------------------------
// MLoadingSPKWorkNode2 :: Execute
//----------------------------------------------------------------------
//		= loads the SPK by the SFPArray.
//
//		SFPArray*		SFPA
//
//		[Remain] --> Node 2
//----------------------------------------------------------------------
BOOL
MLoadingSPKWorkNode2::Execute(MWorkNode*& pRemainNode)
{
	//m_bExecute = TRUE;

	//---------------------------------------------------
	//
	// SPK Load
	//
	//---------------------------------------------------
	std::ifstream spkFile;
	if (!OpenBinary(m_SPKFilename.c_str(), spkFile))
	{
		// -_-;;
		m_bExecute = FALSE;
		pRemainNode = NULL;
		return FALSE;
	}

	int size = m_pSFPA->GetSize();
	for (int i=0; i<size; i++)
	{
		SPRITE_FILEPOSITION_NODE& node = (*m_pSFPA)[i];

		//---------------------------------------------------
		// Still loading
		//---------------------------------------------------
		if (m_bExecute)
		{
			// Seek to the sprite's position.
			spkFile.seekg( node.FilePosition, ios::beg );

			CSprite& sprite = (*m_pSPK)[node.SpriteID];

			//---------------------------------------------------
			// Load the sprite.
			//---------------------------------------------------
			// Only if it is not loaded yet.
			if (sprite.IsInit())
				continue;

			sprite.LoadFromFile( spkFile );
		}
		//---------------------------------------------------
		// Stopped
		//---------------------------------------------------
		else
		{
			CSpriteFilePositionArray* pRemainSFPA = new CSpriteFilePositionArray;

			int remainSize = size - i;

			pRemainSFPA->Init( remainSize );			

			// Copy the rest of the SFPA.
			for (int j=0; j<remainSize; j++)
			{
				(*pRemainSFPA)[j] = (*m_pSFPA)[i + j];
			}

			MLoadingSPKWorkNode2* pNode = new MLoadingSPKWorkNode2( pRemainSFPA );

			pNode->SetSPK( m_pSPK, m_SPKFilename.c_str() );
			pNode->SetType( m_Type );

			// Hand over the rest.
			spkFile.close();
			pRemainNode = pNode;
			return FALSE;
		}
	}
	spkFile.close();

	// Done
	m_bExecute = FALSE;

	// Finished normally
	pRemainNode = NULL;
	return TRUE;
}

//----------------------------------------------------------------------
// MLoadingSPKWorkNode3 :: Execute
//----------------------------------------------------------------------
//		TYPE_SPRITEID	SpriteID
//		long			FilePosition
//
//		[Remain] --> NULL
//----------------------------------------------------------------------
BOOL
MLoadingSPKWorkNode3::Execute(MWorkNode*& pRemainNode)
{
	//m_bExecute = TRUE;

	CSprite& sprite = (*m_pSPK)[m_SpriteID];

	//-------------------------------------------------------------
	// Only if it is not loaded yet.
	//-------------------------------------------------------------
	if (sprite.IsNotInit())
	{	
		std::ifstream spkFile;
		if (!OpenBinary(m_SPKFilename.c_str(), spkFile))
		{
			// -_-;;
			m_bExecute = FALSE;
			pRemainNode = NULL;
			return FALSE;
		}

		// The sprite to load
		spkFile.seekg(m_FilePosition, ios::beg);
		
		//--------------------------------------------------
		// Load one sprite
		//--------------------------------------------------
		if (sprite.LoadFromFile( spkFile ))
		{
		}
		else
		{
			// -_-;;
			// What? Can this happen?
		}
		spkFile.close();
	}

	m_bExecute = FALSE;	

	pRemainNode = NULL;
	return TRUE;
}


//----------------------------------------------------------------------
// MLoadingSPKWorkNode4 :: Execute
//----------------------------------------------------------------------
//		TYPE_SPRITEID	FirstSpriteID
//		TYPE_SPRITEID	LastSpriteID
//		long			FilePosition
//
//		[Remain] --> Node 4
//----------------------------------------------------------------------
BOOL
MLoadingSPKWorkNode4::Execute(MWorkNode*& pRemainNode)
{
	//m_bExecute = TRUE;

	std::ifstream spkFile;
	if (!OpenBinary(m_SPKFilename.c_str(), spkFile))
	{
		// -_-;;
		m_bExecute = FALSE;
		pRemainNode = NULL;
		return FALSE;
	}

	//-------------------------------------------------------
	// To the first sprite
	//-------------------------------------------------------
	spkFile.seekg(m_FilePosition, ios::beg);

	//-------------------------------------------------------
	// Load FirstSpriteID to LastSpriteID.
	//-------------------------------------------------------
	for (TYPE_SPRITEID i=m_FirstSpriteID; i<m_LastSpriteID; i++)
	{
		//---------------------------------------------------
		// Still loading
		//---------------------------------------------------
		if (m_bExecute)
		{
			CSprite& sprite = (*m_pSPK)[i];

			//---------------------------------------------------
			// Only if it is not loaded yet...
			// (!) No: the file position has to advance,
			// so always load!
			//---------------------------------------------------
			//if (sprite.IsNotInit())
			{
				sprite.LoadFromFile( spkFile );
			}
		}
		//---------------------------------------------------
		// Stopped
		//---------------------------------------------------
		else
		{
			//---------------------------------------------------
			// A node from the current i as its first sprite ID.
			//---------------------------------------------------
			MLoadingSPKWorkNode4* pNode = new MLoadingSPKWorkNode4( 
													i,					// first SpriteID
													m_LastSpriteID,		// last SpriteID
													static_cast<long>(spkFile.tellg())		// file position
												);
			pNode->SetSPK( m_pSPK, m_SPKFilename.c_str() );
			pNode->SetType( m_Type );

			spkFile.close();
			
			pRemainNode = pNode;
			return FALSE;
		}
	}

	m_bExecute = FALSE;

	spkFile.close();
	pRemainNode = NULL;
	return TRUE;
}

