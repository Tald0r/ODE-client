//----------------------------------------------------------------------
// CAlphaSprite565.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CAlphaSprite.h"
#include "CAlphaSprite565.h"
#include <fstream>

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// fstream에 save 한다.    ( file에는 5:6:5로 저장한다. )
//----------------------------------------------------------------------
bool	
CAlphaSprite565::SaveToFile(ofstream& file)
{
	// width와 height를 저장한다.
	file.write((const char*)&m_Width , 2);
	file.write((const char*)&m_Height, 2);

	// NULL이면 저장하지 않는다. 길이만 저장되는 것이다.
	if (m_Pixels==NULL || m_Width==0 || m_Height==0)
		return false;
	
	// 압축 된 것 저장
	WORD index;	

	int j;

	//--------------------------------
	// 5:6:5
	//--------------------------------
	for (int i=0; i<m_Height; i++)
	{
		// 반복 회수의 2 byte
		int	count = m_Pixels[i][0], 
				colorCount;
		index	= 1;

		// 각 line마다 byte수를 세어서 저장해야한다.
		for (j=0; j<count; j++)
		{
			//transCount = m_Pixels[i][index];
			colorCount = m_Pixels[i][index+1];

			index+=2;	// 두 count 만큼

			index += (colorCount<<1);	// 투명색 아닌것만큼 +				
		}

		// byte수와 실제 data를 저장한다.
		file.write((const char*)&index, 2);
		file.write((const char*)m_Pixels[i], index<<1);
	}


	return true;
}

//----------------------------------------------------------------------
// fstream에서 load한다.
//----------------------------------------------------------------------
bool
CAlphaSprite565::LoadFromFile(ifstream& file)
{
	return LoadPixels(file, false);
}

