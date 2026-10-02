//-----------------------------------------------------------------------------
// MShopTemplateTable.h
//-----------------------------------------------------------------------------
// ShopTemplateTable은 ShopTemplate의 array - map을 사용하자...
//-----------------------------------------------------------------------------

#ifndef __MSHOPTEMPLATE_H__
#define __MSHOPTEMPLATE_H__

#include "CTypeMap.h"
#include "Platform.h"

//-----------------------------------------------------------------------------
//		MShopTemplate
//-----------------------------------------------------------------------------
class MShopTemplate {
	public :
		MShopTemplate();
		~MShopTemplate();

		void		SaveToFile(std::ofstream& file);
		void		LoadFromFile(std::ifstream& file);

	public :
		//UINT				m_ID;

		BYTE				Type;				// 상점 type

		// item class
		int					Class;
		
		// item type
		WORD				MinType;			
		WORD				MaxType;

		// item option
		BYTE				MinOption;
		BYTE				MaxOption;
		
};

//-----------------------------------------------------------------------------
//	MShopTemplateTable
//-----------------------------------------------------------------------------
class MShopTemplateTable : public CTypeMap<MShopTemplate> {
	public :
		MShopTemplateTable();
		~MShopTemplateTable() override;
		MShopTemplateTable(const MShopTemplateTable&) = delete;
		MShopTemplateTable& operator=(const MShopTemplateTable&) = delete;

		// Reloads clear old rows and publish only a complete table. Duplicate
		// keys retain their first row, matching the legacy file reader.
		void LoadFromFile(std::ifstream& file) override;
		void SaveToFile(std::ofstream& file) override;

};

extern MShopTemplateTable*		g_pShopTemplateTable;

#endif

