   //----------------------------------------------------------------------
// MItemTable.cpp
//----------------------------------------------------------------------
// Item떨어지는걸 하고 싶으면..
// 각 info에다가 DropFrameID( ...)를 제대로 설정하면 된다.
// 물론 .. MTopView에는 ItemDropFPK와 ItemDropFPK가 제대로 된게 있어야 겠지..
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MItemTable.h"
#include "SoundDef.h"
#include "MGameStringTable.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
ITEMCLASS_TABLE	*	g_pItemTable = NULL;
COLORREF g_ELEMENTAL_COLOR[5] = { RGB(255, 100, 100), RGB(100, 100, 255), RGB(255, 180, 100), RGB(100, 100, 255), RGB(192, 192, 255) };
int g_ELEMENTAL_STRING_ID[5] = { UI_STRING_MESSAGE_ELEMENTAL_FIRE, UI_STRING_MESSAGE_ELEMENTAL_WATER, UI_STRING_MESSAGE_ELEMENTAL_EARTH, UI_STRING_MESSAGE_ELEMENTAL_WIND, UI_STRING_MESSAGE_ELEMENTAL_SUM};

//----------------------------------------------------------------------
//
//				ITEMTABLE_INFO
//
//----------------------------------------------------------------------
ITEMTABLE_INFO::ITEMTABLE_INFO()
{
	// Frame ID
	TileFrameID			= FRAMEID_NULL;		// Tile에서의 FrameID
	InventoryFrameID	= FRAMEID_NULL;		// Inventory에서의 Frame ID
	GearFrameID			= FRAMEID_NULL;		// Gear에서의 Frame ID
	AddonMaleFrameID	= FRAMEID_NULL;		// 장착했을 때의 동작 FrameID - 남자
	AddonFemaleFrameID	= FRAMEID_NULL;		// 장착했을 때의 동작 FrameID - 여자
	DropFrameID			= FRAMEID_NULL;

	// Sound ID
	UseSoundID			= SOUNDID_NULL;		// Item 사용 SoundID			
	TileSoundID			= SOUNDID_NULL;		// Item 줍기 SoundID
	InventorySoundID	= SOUNDID_NULL;		// Inventory에서의 Sound
	GearSoundID			= SOUNDID_NULL;		// Gear에서의 Sound

	bMaleOnly			= false;
	bFemaleOnly			= false;
	
	// inventory에서의 Grid크기
	GridWidth			= 1;
	GridHeight			= 1;

	// item 자체에 대한 고정된 정보
	Weight				= 0;				// 무게
	Price				= 0;

	// 값들.. --> Protection, 공격력, 사정거리
	Value1				= 0;
	Value2				= 0;
	Value3				= 0;
	Value4				= 0;
	Value5				= 0;
	Value6				= 0;
	Value7				= 0;

	// 필요능력
	RequireSTR			= 0;
	RequireDEX			= 0;
	RequireINT			= 0;
	RequireSUM			= 0;
	RequireAdvancementLevel = 0;
	RequireLevel		= 0;

	// 기본 공격 ActionInfo
	UseActionInfo		= ACTIONINFO_NULL;

	// silver coating
	SilverMax			= 0;

	ToHit				= 0;

	MaxNumber			= 1;

	CriticalHit			= 0;

	ItemStyle			= 0;

	ElementalType		= ELEMENTAL_TYPE_ANY;
	Elemental			= 0;
	Race				= 0;

	DescriptionFrameID = 0;
}

ITEMTABLE_INFO::~ITEMTABLE_INFO()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set SoundID
//----------------------------------------------------------------------
void	
ITEMTABLE_INFO::SetSoundID(TYPE_SOUNDID tile, TYPE_SOUNDID inventory, TYPE_SOUNDID gear, TYPE_SOUNDID use)
{
	TileSoundID = tile;
	InventorySoundID = inventory;
	GearSoundID = gear;
	UseSoundID = use;	
}

//----------------------------------------------------------------------
// Set FrameID
//----------------------------------------------------------------------
void	
ITEMTABLE_INFO::SetFrameID(TYPE_FRAMEID tile, TYPE_FRAMEID inventory, TYPE_FRAMEID gear)
{
	TileFrameID = tile;
	InventoryFrameID = inventory;
	GearFrameID = gear;	
}

//----------------------------------------------------------------------
// Set DropFrameID
//----------------------------------------------------------------------
void	
ITEMTABLE_INFO::SetDropFrameID(TYPE_FRAMEID drop)
{
	DropFrameID	= drop;
}

//----------------------------------------------------------------------
// Set Addon FrameID
//----------------------------------------------------------------------
void
ITEMTABLE_INFO::SetAddonFrameID(TYPE_FRAMEID male, TYPE_FRAMEID female)
{
	AddonMaleFrameID = male;
	AddonFemaleFrameID = female;
}

//----------------------------------------------------------------------
// Set Grid
//----------------------------------------------------------------------
void	
ITEMTABLE_INFO::SetGrid(BYTE width, BYTE height)
{
	GridWidth = width;
	GridHeight = height;
}

//----------------------------------------------------------------------
// Set Values
//----------------------------------------------------------------------
void	
ITEMTABLE_INFO::SetValue(int v1, int v2, int v3, int v4, int v5, int v6, int v7)
{
	Value1 = v1;
	Value2 = v2;
	Value3 = v3;
	Value4 = v4;
	Value5 = v5;
	Value6 = v6;
	Value7 = v7;
}

//----------------------------------------------------------------------
// Save
//----------------------------------------------------------------------
void			
ITEMTABLE_INFO::SaveToFile(std::ofstream& file)
{
	// 이름 저장
	EName.SaveToFile( file );
	HName.SaveToFile( file );
	Description.SaveToFile( file );

	// Frame ID
	file.write((const char*)&TileFrameID, SIZE_FRAMEID);
	file.write((const char*)&InventoryFrameID, SIZE_FRAMEID);
	file.write((const char*)&GearFrameID, SIZE_FRAMEID);
	file.write((const char*)&DropFrameID, SIZE_FRAMEID);
	file.write((const char*)&AddonMaleFrameID, SIZE_FRAMEID);
	file.write((const char*)&AddonFemaleFrameID, SIZE_FRAMEID);

	
	// Sound ID
	file.write((const char*)&UseSoundID, SIZE_SOUNDID);
	file.write((const char*)&TileSoundID, SIZE_SOUNDID);
	file.write((const char*)&InventorySoundID, SIZE_SOUNDID);
	file.write((const char*)&GearSoundID, SIZE_SOUNDID);

	// inventory에서의 Grid크기
	file.write((const char*)&GridWidth, 1);
	file.write((const char*)&GridHeight, 1);

	// 가격
	file.write((const char*)&Price, SIZE_ITEM_PRICE);

	// 무게
	file.write((const char*)&Weight, SIZE_ITEM_WEIGHT);

	// 값들
	file.write((const char*)&Value1, 4);
	file.write((const char*)&Value2, 4);
	file.write((const char*)&Value3, 4);
	file.write((const char*)&Value4, 4);
	file.write((const char*)&Value5, 4);
	file.write((const char*)&Value6, 4);
	file.write((const char*)&Value7, 4);
	
	// 필요능력
	file.write((const char*)&RequireSTR, 1);
	file.write((const char*)&RequireDEX, 1);
	file.write((const char*)&RequireINT, 1);		
	file.write((const char*)&RequireSUM, 2);
	file.write((const char*)&RequireLevel, 1);
	file.write((const char*)&RequireAdvancementLevel, 1);
	file.write((const char*)&bMaleOnly, 1);
	file.write((const char*)&bFemaleOnly, 1);

	// UseActionInfo
	file.write((const char*)&UseActionInfo, 4);
	

	file.write((const char*)&SilverMax, 4);

	file.write((const char*)&ToHit, 4);

	file.write((const char*)&MaxNumber, SIZE_ITEM_NUMBER);

	file.write((const char*)&CriticalHit, 4);	

	BYTE DefaultOptionListSize = static_cast<BYTE>(DefaultOptionList.size());
	file.write((const char*)&DefaultOptionListSize, 1);
	std::list<TYPE_ITEM_OPTION>::iterator itr = DefaultOptionList.begin();

	while(itr != DefaultOptionList.end())
	{
		TYPE_ITEM_OPTION Option = *itr;
		file.write((const char*)&Option, sizeof(TYPE_ITEM_OPTION));

		itr++;
	}

	file.write((const char*)&ItemStyle, 4);

	file.write((const char*)&ElementalType, 4);
	file.write((const char*)&Elemental, 2);

	file.write((const char*)&Race, 1);

	// 2005, 1, 14, sobeit add start - ItemDescription.spk 에서 쓰는 frameID
	file.write((const char*)&DescriptionFrameID, SIZE_FRAMEID);
	// 2005, 1, 14, sobeit add end
}

//----------------------------------------------------------------------
// Load
//----------------------------------------------------------------------
void			
ITEMTABLE_INFO::LoadFromFile(std::ifstream& file)
{
	EName.LoadFromFile( file );
	HName.LoadFromFile( file );
	Description.LoadFromFile( file );

	// Frame ID
	file.read((char*)&TileFrameID, SIZE_FRAMEID);
	file.read((char*)&InventoryFrameID, SIZE_FRAMEID);
	file.read((char*)&GearFrameID, SIZE_FRAMEID);
	file.read((char*)&DropFrameID, SIZE_FRAMEID);
	file.read((char*)&AddonMaleFrameID, SIZE_FRAMEID);
	file.read((char*)&AddonFemaleFrameID, SIZE_FRAMEID);

	// Sound ID
	file.read((char*)&UseSoundID, SIZE_SOUNDID);
	file.read((char*)&TileSoundID, SIZE_SOUNDID);
	file.read((char*)&InventorySoundID, SIZE_SOUNDID);
	file.read((char*)&GearSoundID, SIZE_SOUNDID);	

	// grid 크기
	file.read((char*)&GridWidth, 1);	
	file.read((char*)&GridHeight, 1);	

	// 가격
	file.read((char*)&Price, SIZE_ITEM_PRICE);

	// 무게
	file.read((char*)&Weight, SIZE_ITEM_WEIGHT);	
	
	// 값들 
	file.read((char*)&Value1, 4);	
	file.read((char*)&Value2, 4);
	file.read((char*)&Value3, 4);
	file.read((char*)&Value4, 4);
	file.read((char*)&Value5, 4);
	file.read((char*)&Value6, 4);
	file.read((char*)&Value7, 4);

	// 필요능력
	file.read((char*)&RequireSTR, 1);
	file.read((char*)&RequireDEX, 1);
	file.read((char*)&RequireINT, 1);		
	file.read((char*)&RequireSUM, 2);
	file.read((char*)&RequireLevel, 1);
	file.read((char*)&RequireAdvancementLevel, 1);
	file.read((char*)&bMaleOnly, 1);
	file.read((char*)&bFemaleOnly, 1);
	
	// UseActionInfo
	file.read((char*)&UseActionInfo, 4);

	file.read((char*)&SilverMax, 4);

	file.read((char*)&ToHit, 4);

	file.read((char*)&MaxNumber, SIZE_ITEM_NUMBER);

	file.read((char*)&CriticalHit, 4);

	BYTE DefaultOptionListSize = 0;
	file.read((char*)&DefaultOptionListSize, 1);
	
	for(int i = 0; i < DefaultOptionListSize; i++)
	{
		TYPE_ITEM_OPTION TempOptionType;
		file.read((char*)&TempOptionType, sizeof(TYPE_ITEM_OPTION));
		DefaultOptionList.push_back(TempOptionType);
	}

	file.read((char*)&ItemStyle, 4);

	file.read((char*)&ElementalType, 4);
	file.read((char*)&Elemental, 2);

	file.read((char*)&Race, 1);

	// 2005, 1, 14, sobeit add start - ItemDescription.spk 에서 쓰는 frameID
	file.read((char*)&DescriptionFrameID, SIZE_FRAMEID);
	// 2005, 1, 14, sobeit add end
}

//----------------------------------------------------------------------
//
//							ITEMTYPE_TABLE
//
//----------------------------------------------------------------------
void
ITEMTYPE_TABLE::LoadFromFile(std::ifstream& file)
{
	CTypeTable<ITEMTABLE_INFO>::LoadFromFile(file);

	
	m_AveragePrice = 0;

	int count = 0;
	for (int i=0; i<m_Size; i++)
	{
		if(m_pTypeInfo[i].DefaultOptionList.empty())
		{
			m_AveragePrice += m_pTypeInfo[i].Price;
			count ++;
		}
	}
	if(count)
	{
		m_AveragePrice /= count;
		m_AveragePrice /= 1000;
		m_AveragePrice *= 100;
	}
}
//----------------------------------------------------------------------
//
//							ITEMCLASS_TABLE
//
//----------------------------------------------------------------------
ITEMCLASS_TABLE::ITEMCLASS_TABLE()
{
	// The 23,000-line in-code item table that used to sit here was the
	// server's (__INIT_ITEM__, never defined for the client, which loads
	// Item.inf); deleted with docs/RESTRUCTURING.md task 5.2, in git
	// history if a data-authoring path ever wants it back.
}

ITEMCLASS_TABLE::~ITEMCLASS_TABLE()
{
}

//---------------------------------------------------------------------
// c class를 size개만큼 초기화한다.
//---------------------------------------------------------------------
void
ITEMCLASS_TABLE::InitClass( int c, int size )
{
	// class에 size개만큼 type을 생성	
	m_pTypeInfo[c].Init( size );
}