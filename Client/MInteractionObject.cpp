//----------------------------------------------------------------------
// MInteractionObject.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MInteractionObject.h"



//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
MInteractionObject::MInteractionObject()
{
	// type
	m_ObjectType	= TYPE_INTERACTIONOBJECT;	
	
	m_bAnimation	= true;
}

MInteractionObject::MInteractionObject(TYPE_INTERACTIONOBJECTTYPE iaotype, TYPE_OBJECTID id, TYPE_OBJECTID ImageObjectID, TYPE_SPRITEID SpriteID, int pX, int pY, TYPE_SECTORPOSITION viewpoint, bool trans, BYTE type)
{
	// instance ID발급
	m_ID			= id;

	m_InteractionObjectType	 = iaotype;

	// type
	m_ObjectType	= TYPE_INTERACTIONOBJECT;	
	m_ImageObjectID = ImageObjectID;

	m_bAnimation	= true;

	// data
	m_SpriteID		= SpriteID;
	m_PixelX		= pX;
	m_PixelY		= pY;	
	m_Viewpoint		= viewpoint;
	m_bTrans		= trans;
	m_BltType	= type;
}

MInteractionObject::~MInteractionObject() 
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void	
MInteractionObject::SaveToFile(ofstream& file)
{	
	MAnimationObject::SaveToFile(file);	

	file.write((const char*)&m_InteractionObjectType, SIZE_INTERACTIONOBJECTTYPE);
}
		
//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void	
MInteractionObject::LoadFromFile(ifstream& file)
{
	MAnimationObject::LoadFromFile(file);

	file.read((char*)&m_InteractionObjectType, SIZE_INTERACTIONOBJECTTYPE);
}

//----------------------------------------------------------------------
// Set Action
//----------------------------------------------------------------------
void
MInteractionObject::SetAction(BYTE action)
{ 
	(void)action;
	// Action dispatch is disabled in this client.
}

//----------------------------------------------------------------------
// Set NextAction
//----------------------------------------------------------------------
// action의 다음 action을 설정한다.
//----------------------------------------------------------------------
void			
MInteractionObject::SetNextAction(BYTE action)
{
	(void)action;
	// Action dispatch is disabled in this client.
}

//----------------------------------------------------------------------
// ChangeAction Door
//----------------------------------------------------------------------
// Live door/trap/switch actions are in MInteractionObjectAction.cpp.
