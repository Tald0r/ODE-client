//----------------------------------------------------------------------
// CSprite555.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CSpriteSurface.h"
#include "CSprite.h"
#include "CSprite555.h"

//extern BYTE	LOADING_STATUS_NONE;
//extern BYTE	LOADING_STATUS_NOW;
//extern BYTE	LOADING_STATUS_LOADING;

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// fstream에 save 한다.    ( file에는 5:6:5로 저장한다. )
//----------------------------------------------------------------------
bool
CSprite555::SaveToFile(ofstream& file)
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
	int k;

	//--------------------------------
	// 5:5:5
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

			// m_Pixels[i][index] ~ m_Pixels[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (k=0; k<colorCount; k++)								
			{
				m_Pixels[i][index] = ColorDraw::Convert555to565(m_Pixels[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}

		// byte수와 실제 data를 저장한다.
		file.write((const char*)&index, 2);			
		file.write((const char*)m_Pixels[i], index<<1);			


		// 다시 5:5:5로 바꿔준다.						
		index	= 1;
			
		for (j=0; j<count; j++)
		{
			//transCount = m_Pixels[i][index];
			colorCount = m_Pixels[i][index+1];				

			index+=2;	// 두 count 만큼

			// m_Pixels[i][index] ~ m_Pixels[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (k=0; k<colorCount; k++)								
			{					
				m_Pixels[i][index] = ColorDraw::Convert565to555(m_Pixels[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}
	}

	return true;
}

//----------------------------------------------------------------------
// fstream에서 load한다.
//----------------------------------------------------------------------
bool	
CSprite555::LoadFromFile(ifstream& file)
{
	return LoadPixels(file, true);
}


//----------------------------------------------------------------------
// fstream에서 load한다.
//----------------------------------------------------------------------
/*
bool	
CSprite555::LoadFromFileToBuffer(ifstream& file)
{
	// width와 height를 저장한다.
	file.read((char*)&s_Width , 2);
	file.read((char*)&s_Height, 2);	

	// 길이가 0이면 더 Load할게 없겠지..
	if (s_Width==0 || s_Height==0) 
		return false;

	//---------------------------------
	// for OLD version of CSprite
	//---------------------------------
	//BOOL dummy;
	//file.read((char*)&dummy, 1);	
	//---------------------------------
	
	//--------------------------------
	// 5:5:5
	//--------------------------------
	// 5:6:5로 저장된걸 읽었기 때문에 5:6:5를 5:5:5로 바꿔줘야 한다.	
	WORD	count, index, colorCount;

	for (int i=0; i<s_Height; i++)
	{			
		// byte수와 실제 data를 Load한다.
		file.read((char*)&s_BufferLen[i], 2);
		
		file.read((char*)s_Buffer[i], s_BufferLen[i]<<1);

		// converter to 5:5:5
		count = s_Buffer[i][0];			
		index = 1;

		for (int j=0; j<count; j++)
		{
			//transCount = s_Buffer[i][index];
			colorCount = s_Buffer[i][index+1];				

			index+=2;	// 두 count 만큼

			// s_Buffer[i][index] ~ s_Buffer[i][index+colorCount-1]
			// 5:5:5를 5:6:5로 바꿔서 저장하고 다시 5:5:5로 바꿔준다.
			for (int j=0; j<colorCount; j++)								
			{					
				s_Buffer[i][index] = ColorDraw::Convert565to555(s_Buffer[i][index]);
				index++;
			}

			//index += colorCount;	// 투명색 아닌것만큼 +				
		}
	}


	return true;
}
*/

