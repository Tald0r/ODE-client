//----------------------------------------------------------------------
// MCreatureWear.h
//----------------------------------------------------------------------
// Class for creatures that wear clothes and move
//----------------------------------------------------------------------

#ifndef	__MCREATUREWEAR_H__
#define	__MCREATUREWEAR_H__

#include "MTypeDef.h"
#include "MCreature.h"
#include "MItem.h"
#include "../basic/MemoryPool.h"

// The pool of the creatures that wear gear (MCreatureWear.cpp's).
extern MemoryPool g_CreatureWearMemoryPool;

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------

class MCreatureWear : public MCreature {
	public :
		//----------------------------------------------------
		// Info on the Creature's image and outfit
		//----------------------------------------------------
		struct ADDON_INFO 
		{
			BOOL				bAddon;			// Is it worn?
			TYPE_FRAMEID		FrameID;		// outfit
			ITEM_CLASS	ItemClass;		// item class
			TYPE_ITEMTYPE		ItemType;		// item type
			WORD				ColorSet1;		// colorset1
			WORD				ColorSet2;		// colorset2
			BOOL				bEffectColor;	// Does it use EffectColor?
			WORD				EffectColorSet;	// EffectColor			
		};

	public :
		MCreatureWear();
		virtual ~MCreatureWear();
		
		void* operator new( size_t size )
		{
			return g_CreatureWearMemoryPool.Alloc(size);
		}

		void* operator new(size_t size, std::align_val_t alignment)
		{
			return g_CreatureWearMemoryPool.Alloc(size, static_cast<size_t>(alignment));
		}

		void operator delete(void* pmem, std::align_val_t) noexcept
		{
			g_CreatureWearMemoryPool.Free(pmem);
		}

		void operator delete( void* pmem ) noexcept
		{
			g_CreatureWearMemoryPool.Free( pmem );
		}

		virtual CLASS_TYPE	GetClassType() const	{ return CLASS_CREATUREWEAR; }

		BOOL		IsWear() const					{ return TRUE; }

		//------------------------------------------------------
		// Set the type
		//------------------------------------------------------
		virtual void	SetCreatureType(TYPE_CREATURETYPE type);


		//------------------------------------------------------
		// Outfit changing
		//------------------------------------------------------			
		// Hair
		void			SetAddonHair(TYPE_FRAMEID id, WORD cs1);
		TYPE_FRAMEID	GetHairFrameID()			{ return m_HairFrameID; }
		
		// Taking off the outfit
		bool			RemoveAddon(int Addon);

		// Is there an outfit?
		//BOOL			IsExistAddon(int Addon) const		{ return m_Addon[Addon].bAddon; }
		//TYPE_FRAMEID	GetAddonFrameID(int Addon) const	{ return m_Addon[Addon].FrameID; }
		//ITEM_CLASS	GetAddonItemClass(int Addon) const	{ return m_Addon[Addon].ItemClass; }
		//TYPE_ITEMTYPE	GetAddonItemType(int Addon) const	{ return m_Addon[Addon].ItemType; }
		const ADDON_INFO&	GetAddonInfo(int Addon) const	{ return m_Addon[Addon]; }
		const ADDON_INFO&	GetACAddonInfo(int Addon) const;

		//------------------------------------------------------			
		// Sets the ColorSet.
		//------------------------------------------------------			
		void			SetAddonColorSet1(int Addon, WORD colorSet);
		void			SetAddonColorSet2(int Addon, WORD colorSet);	
		
		//------------------------------------------------------
		// Wears/removes an AddonItem.
		//------------------------------------------------------
		virtual	bool		SetAddonItem(MItem* pItem);
		virtual bool		RemoveAddonItem(MItem* pItem);

		//------------------------------------------------------
		// SetSameWear
		//------------------------------------------------------
		void				SetSameWear(const MCreatureWear* pCreature);

		//------------------------------------------------------
		// Update AttachEffect
		//------------------------------------------------------
		virtual bool		RemoveEffectStatus(EFFECTSTATUS status);
		virtual void		UpdateAttachEffect();

	protected :
		void			ClearAddonInfo(int Addon);		
		MItem*			NewItemFromAddonInfo(int Addon);

	protected :
		// Outfit info
		ADDON_INFO			m_Addon[ ADDON_MAX ];
//		ADDON_INFO			m_ACAddon[ AC_ADDON_MAX ];
		ADDON_INFO			m_ShoulderAddon;

		// So that short hair is not shown when a hat is worn..
		// Only the hair info is saved separately. -- temporary.. but.. --;;
		TYPE_FRAMEID		m_HairFrameID;
		WORD				m_HairColorSet;

	public :
		// Clothes drawing order for the 8 directions
		static int			s_AddonOrder[DIRECTION_MAX][ADDON_MAX];
		static int			s_ACAddonOrder[ DIRECTION_MAX ][ AC_ADDON_MAX ];
};

#endif


