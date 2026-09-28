//----------------------------------------------------------------------
// CShadowSprite.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CSpriteSurface.h"
#include "CFilter.h"
#include "CShadowSprite.h"
#include "SpriteScanline.h"
#include <cstring>
#include <memory>
#include <vector>

#ifdef SPRITELIB_BACKEND_SDL
#include "SpriteLibBackend.h"
#endif


//----------------------------------------------------------------------
// Static member
//----------------------------------------------------------------------
WORD	CShadowSprite::s_Colorkey = 0;
BYTE	CShadowSprite::s_Value1 = 1;
BYTE	CShadowSprite::s_Value2 = 31;
WORD	CShadowSprite::s_wValue1 = 1;


//----------------------------------------------------------------------
// Init Static member
//----------------------------------------------------------------------


//----------------------------------------------------------------------
//
// constructor/destructore
//
//----------------------------------------------------------------------

CShadowSprite::CShadowSprite()
{
	m_Width		= 0;
	m_Height	= 0;
	m_Pixels	= NULL;
	m_bInit		= false;

#ifdef SPRITELIB_BACKEND_SDL
	m_backend_sprite = SPRITECTL_INVALID_SPRITE;
	m_backend_dirty = false;
#endif
}

CShadowSprite::CShadowSprite(const CShadowSprite& sprite) : CShadowSprite()
{
	*this = sprite;
}

CShadowSprite::~CShadowSprite()
{
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Release all rows and reset even a sprite with empty geometry.
//----------------------------------------------------------------------
void
CShadowSprite::Release()
{
#ifdef SPRITELIB_BACKEND_SDL
	if (m_backend_sprite != SPRITECTL_INVALID_SPRITE) {
		spritectl_destroy_sprite(m_backend_sprite);
		m_backend_sprite = SPRITECTL_INVALID_SPRITE;
	}
	m_backend_dirty = false;
#endif
	if (m_Pixels != nullptr) {
		for (int i = 0; i < m_Height; ++i) delete[] m_Pixels[i];
		delete[] m_Pixels;
	}
	m_Pixels = nullptr;
	m_Width = 0;
	m_Height = 0;
	m_bInit = false;
	m_PixelLengths.clear();
}

//----------------------------------------------------------------------
// operator = 
//----------------------------------------------------------------------
void
CShadowSprite::operator = (const CShadowSprite& sprite)
{
	if (this == &sprite) return;
	Release();
	if (!sprite.m_bInit) return;
	if (sprite.m_Width == 0 || sprite.m_Height == 0) {
		m_Width = sprite.m_Width;
		m_Height = sprite.m_Height;
		m_bInit = true;
		return;
	}
	std::vector<std::unique_ptr<WORD[]>> rows(sprite.m_Height);
	for (WORD y = 0; y < sprite.m_Height; ++y) {
		const auto line = sprite.GetPixelLineSpan(y);
		if (!ValidateShadowScanline(line, sprite.m_Width)) return;
		rows[y] = std::make_unique<WORD[]>(line.size());
		memcpy(rows[y].get(), line.data(), line.size_bytes());
	}
	auto pointers = std::make_unique<WORD*[]>(sprite.m_Height);
	m_PixelLengths = sprite.m_PixelLengths;
	for (WORD y = 0; y < sprite.m_Height; ++y) pointers[y] = rows[y].release();
	m_Pixels = pointers.release();
	m_Width = sprite.m_Width;
	m_Height = sprite.m_Height;
	m_bInit = true;
}

//----------------------------------------------------------------------
// fstream에 save 한다.    ( file에는 5:6:5로 저장한다. )
//----------------------------------------------------------------------
bool	
CShadowSprite::SaveToFile(ofstream& file)
{
	// width와 height를 저장한다.
	file.write((const char*)&m_Width , 2);
	file.write((const char*)&m_Height, 2);

	// NULL이면 저장하지 않는다. 길이만 저장되는 것이다.
	if (m_Pixels==NULL || m_Width==0 || m_Height==0)
		return false;
	
	// 압축 된 것 저장
	WORD index;	

	//--------------------------------
	// 5:6:5
	//--------------------------------
	for (int i=0; i<m_Height; i++)
	{
		// 반복수 + 반복수*2byte
		index	= 1 + m_Pixels[i][0]*2;	

		// byte수와 실제 data를 저장한다.
		file.write((const char*)&index, 2);
		file.write((const char*)m_Pixels[i], index<<1);
	}


	return true;
}

//----------------------------------------------------------------------
// Read and validate a complete shadow record before publishing its rows.
//----------------------------------------------------------------------
bool
CShadowSprite::LoadFromFile(ifstream& file)
{
	Release();
	try {
		WORD width = 0, height = 0;
		if (!file.read(reinterpret_cast<char*>(&width), sizeof(width)) ||
			!file.read(reinterpret_cast<char*>(&height), sizeof(height))) return false;
		if (width == 0 || height == 0) {
			m_Width = width;
			m_Height = height;
			m_bInit = true;
			return true;
		}
		std::vector<std::unique_ptr<WORD[]>> rows(height);
		std::vector<size_t> lengths(height);
		for (WORD y = 0; y < height; ++y) {
			WORD length = 0;
			if (!file.read(reinterpret_cast<char*>(&length), sizeof(length))) return false;
			lengths[y] = length;
			if (length == 0) continue;
			rows[y] = std::make_unique<WORD[]>(length);
			if (!file.read(reinterpret_cast<char*>(rows[y].get()), length * sizeof(WORD))) return false;
		}
		// Consume every row before rejecting a complete malformed record, so
		// the next packed sprite remains readable. Extra padding is permitted.
		for (WORD y = 0; y < height; ++y) {
			if (!ValidateShadowScanline({rows[y].get(), lengths[y]}, width)) return false;
		}
		auto pointers = std::make_unique<WORD*[]>(height);
		for (WORD y = 0; y < height; ++y) pointers[y] = rows[y].release();
		m_PixelLengths = std::move(lengths);
		m_Pixels = pointers.release();
		m_Width = width;
		m_Height = height;
		m_bInit = true;
		return true;
	} catch (...) {
		return false;
	}
}

//----------------------------------------------------------------------
// CDirectDrawSurface의 (x,y)+(width, height)영역을 읽어서 m_Pixels에 저장한다.
//----------------------------------------------------------------------
// m_Pixels를 0번 압축 Format으로 바꾼다.
//
// 각 line마다 다음과 같은 구조를 가진다.
//
//    [반복수] (투명수,색깔수)(투명수,색깔수)......
//
// 반복수는 2 bytes이고
// 투명수와 색깔수는 각각 2 byte
//
//----------------------------------------------------------------------
void
CShadowSprite::SetPixel(WORD *pSource, WORD pitch, WORD width, WORD height)
{
	// memory해제
	Release();

	m_Width = width;
	m_Height = height;

	// 일단 memory를 적당히 잡아둔다.	
	std::vector<WORD> data(size_t(m_Width) + 2);

	int	index;				// data의 index로 사용			
	int	count;				// 반복수
	int	trans,				// 투명색 개수
			color;				// 투명이 아닌색 개수

	BOOL	bCheckTrans;		// 최근에 검사한게 투명색인가?

	WORD	*pSourceTemp;


	// height줄 만큼 memory잡기
	m_PixelLengths.assign(height, 0);
	m_Pixels = new WORD* [height]{};

	int j;

	for (int i=0; i<height; i++)
	{
		index = 0;
		count = 0;
		trans = 0;
		color = 0;
		bCheckTrans = TRUE;

		pSourceTemp = pSource;

		// 각 line에 대해서 압축~
		for (j=0; j<width; j++)
		{
			// 0번 color에 대해서 압축
			if (*pSourceTemp==s_Colorkey)
			{
				// 최근에 검사한게 투명색이 아니었다면
				if (!bCheckTrans)
				{
					// ' (투명,색깔수) '의 한 set가 끝났음을 의미하므로
					count++;
					
					data[index++] = color;
					color = 0;

					bCheckTrans = TRUE;
				}
				
				trans++;				
			}
			else
			{
				// 최근에 검사한게 투명색이었다면..
				if (bCheckTrans)
				{						
					data[index++] = trans;		// 상위 byte에 투명수를 넣는다.
					trans = 0;

					bCheckTrans = FALSE;
				}

				color++;								
			}

			pSourceTemp++;
		}
		
		// 한 줄의 마지막 점이 투명색인가?
		if (bCheckTrans)
		{
			// 투명색이면 별다른 처리를 안해줘도 될거 같다.
		}	
		// 투명색이 아닌 경우, 점의 개수를 저장시켜줘야 한다.
		else
		{			
			count++;
			data[index++] = color;
		}
		
		// memory를 다시 잡는다.
		m_Pixels[i] = new WORD [index+1];
		m_PixelLengths[i] = size_t(index) + 1;

		// m_Pixels[i]를 압축했으므로 data로 대체한다.
		// m_Pixels[i][0]에는 count를 넣어야 한다.
		m_Pixels[i][0] = count;
		memcpy(m_Pixels[i]+1, data.data(), index<<1);

		pSource = (WORD*)((BYTE*)pSource + pitch);
	}


	m_bInit = true;
}

//----------------------------------------------------------------------
// SetPixel
//----------------------------------------------------------------------
// IndexSprite --> ShadowSprite
//----------------------------------------------------------------------
void		
CShadowSprite::SetPixel(CIndexSprite& ispr)
{
	// memory해제
	Release();

	m_Width = ispr.GetWidth();
	m_Height = ispr.GetHeight();

	int		transPair,	
			indexCount,			
			colorCount,
			index;
	
	WORD	*pPixels;

	int j;	

	// height줄 만큼 memory잡기
	m_PixelLengths.assign(m_Height, 0);
	m_Pixels = new WORD* [m_Height]{};

	for (int i=0; i<m_Height; i++)
	{			
		pPixels		= ispr.GetPixelLine( i );
		
		// (투명수,Index색,색깔들)의 반복 수		
		transPair	= *pPixels++;

		// 반복수 + (투명수,그림자수)*반복수
		m_Pixels[i] = new WORD [1 + (transPair<<1)];
		m_PixelLengths[i] = 1 + (size_t(transPair) * 2);
		index = 0;
		m_Pixels[i][index++] = transPair;
		
 		// 한 줄 출력
		for (j=0; j<transPair; j++)
		{			
			//------------------------------------
			// 투명색 수
			//------------------------------------
			m_Pixels[i][index++] = *pPixels;	
			pPixels++;			

			indexCount = *pPixels++;	// Index반복 수			
			pPixels += indexCount;		// Index색들만큼 증가
			
			colorCount = *pPixels++;	// color 색깔 수			
			pPixels		+= colorCount;	// Color 색만큼 증가

			//------------------------------------
			// 그림자 수 = Index + Color수
			//------------------------------------
			m_Pixels[i][index++] = indexCount + colorCount;
		}
	}

	m_bInit = true;
}

//----------------------------------------------------------------------
// SetPixel
//----------------------------------------------------------------------
// Sprite --> ShadowSprite
//----------------------------------------------------------------------
void		
CShadowSprite::SetPixel(CSprite& spr)
{
	// memory해제
	Release();

	m_Width = spr.GetWidth();
	m_Height = spr.GetHeight();

	int		transPair,	
			colorCount,
			index;
	
	WORD	*pPixels;

	int j;	

	// height줄 만큼 memory잡기
	m_PixelLengths.assign(m_Height, 0);
	m_Pixels = new WORD* [m_Height]{};

	for (int i=0; i<m_Height; i++)
	{			
		pPixels		= spr.GetPixelLine( i );
		
		// (투명수,색깔들)의 반복 수		
		transPair	= *pPixels++;

		// 반복수 + (투명수,그림자수)*반복수
		m_Pixels[i] = new WORD [1 + (transPair<<1)];
		m_PixelLengths[i] = 1 + (size_t(transPair) * 2);
		index = 0;
		m_Pixels[i][index++] = transPair;
		
 		// 한 줄 출력
		for (j=0; j<transPair; j++)
		{			
			//------------------------------------
			// 투명색 수
			//------------------------------------
			m_Pixels[i][index++] = *pPixels;	
			pPixels++;			

			colorCount = *pPixels++;	// color 색깔 수			
			pPixels		+= colorCount;	// Color 색만큼 증가

			//------------------------------------
			// 그림자 수 = Index + Color수
			//------------------------------------
			m_Pixels[i][index++] = colorCount;
		}
	}

	m_bInit = true;
}

/*
void		
CShadowSprite::Uncompress()
{
	m_bCompress = FALSE;
}
*/

//----------------------------------------------------------------------
// Is ColorPixel ?
//----------------------------------------------------------------------
// Sprite안에서 (x,y)는 색깔이 있는가?(투명색이 아닌 경우)
//----------------------------------------------------------------------
bool		
CShadowSprite::IsColorPixel(short x, short y)
{
	// 아직 초기화 되지 않은 경우
	if (m_Pixels==NULL)
		return false; 

	// Sprite의 영역을 벗어나면 false
	if (x<0 || y<0 || x>=m_Width || y>=m_Height)
		return false;

	// y번째 줄
	WORD	*pPixels = m_Pixels[y];

	// y번째 줄의 반복 수
	int	count = *pPixels++;

	int	transCount, 
			colorCount,
			index = 0;

	for (int i=0; i<count; i++)
	{
		transCount = *pPixels++;
		colorCount = *pPixels++;

		index += transCount;

		// 이번 loop안에 존재하는 점
		if (x < index+colorCount)
		{
			// 투명색까지보다 적은 경우
			if (x < index)
			{
				return false;
			}

			// 색깔에 속한다.
			return true;
		}

		//pPixels += colorCount;
		index += colorCount;
	}

	return false;
}

//----------------------------------------------------------------------
// Blt
//----------------------------------------------------------------------
// Clipping하지 않는다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt(WORD *pDest, WORD pitch)
{
	int	count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;

	int j;

	for (int i=0; i<m_Height; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수)의 반복 수		
		count	= *pPixels++;		
 		// 한 줄 출력
		if (count > 0)
		{			
			j = count;
			do
			{
				pDestTemp += *pPixels++;			// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;		// 투명 아닌 색 수				

				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memset((void*)pDestTemp, 0, colorCount<<1);
		
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt ClipLeft
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltClipLeft(WORD* pDest, WORD pitch, RECT* pRect)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += index - pRect->left;

						// 이번 단계는 모두 출력
						//memcpy(pDestTemp, pPixels, colorCount<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{
						dist = pRect->left - index;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, (colorCount-dist)<<1);

						pDestTemp += colorCount-dist;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 이제부터는 계속 출력한다.		
			//---------------------------------------------		
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 건너 뛴다.
					pDestTemp += transCount;			
					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memset((void*)pDestTemp, 0, colorCount<<1);

					// memory addr 증가
					pDestTemp += colorCount;
					//pPixels += colorCount;			
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt ClipRight
//----------------------------------------------------------------------
// 오른쪽 clipping.  
// pRect->right개 까지의 점만 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltClipRight(WORD* pDest, WORD pitch, RECT* pRect)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index;

	int	j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
			
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가
				index += transCount;
				
				// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
				// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

				// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

				//---------------------------------------------
				// 오른쪽 끝까지 도달했을 경우
				//---------------------------------------------			
				if (index+colorCount > pRect->right)
				{
					// 투명색만으로 더 출력할 필요가 없을 때
					if (index > pRect->right)
					{
						break;
					}
					// 투명색 아닌 것을 조금 출력해야 할 경우
					else
					{
						pDestTemp += transCount;
					
						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, (pRect->right - index)<<1);

						break;
					}
				}

				// 투명색만큼 건너띄고
				pDestTemp += transCount;

				// 출력
				//memcpy(pDestTemp, pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memset((void*)pDestTemp, 0, colorCount<<1);

				pDestTemp += colorCount;
				//pPixels += colorCount;			
				index += colorCount;
			} while (--j);
		}
		
		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt ClipWidth
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력하다가
// pRect->Right까지만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltClipWidth(WORD* pDest, WORD pitch, RECT* pRect)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int		count,
			transCount, 
			colorCount,
			index,
			dist;

	short j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += index - pRect->left;

						// 이번 단계는 모두 출력?
						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{							
							// 투명색만으로 오른쪽 끝 넘어가는 경우
							if (index > pRect->right)
							{
							}
							else
							{
								memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
							}

							j = count;
							
							break;
						}
					
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;
						index += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{		
						dist = pRect->left - index;

						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{						
							memset((void*)pDestTemp, 0, pRect->right - pRect->left);

							j = count;						
							break;
						}

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, (colorCount-dist)<<1);

						pDestTemp += colorCount-dist;
						//pPixels += colorCount;		
						index += colorCount;

						// 이제부터는 계속 출력한다.					
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 각 줄마다 Clipping을 해줘야 하는데...		
			// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
			//---------------------------------------------
			// OOOOOOOOOOOOOO까지만 출력해주면 된다.
			//---------------------------------------------
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 index증가
					index += transCount;
					
					// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
					// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

					// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

					//---------------------------------------------
					// 오른쪽 끝까지 도달했을 경우
					//---------------------------------------------			
					if (index+colorCount > pRect->right)
					{
						// 투명색만으로 더 출력할 필요가 없을 때
						if (index > pRect->right)
						{
							break;
						}
						// 투명색 아닌 것을 조금 출력해야 할 경우
						else
						{
							pDestTemp += transCount;
						
							// 투명이 아닌 색들을 Surface에 출력한다.
							//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
							// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
							memset((void*)pDestTemp, 0, (pRect->right - index)<<1);

							break;
						}
					}

					// 투명색만큼 건너띄고
					pDestTemp += transCount;

					// 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memset((void*)pDestTemp, 0, colorCount<<1);

					pDestTemp += colorCount;
					//pPixels += colorCount;			
					index += colorCount;
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt Clip Height
//----------------------------------------------------------------------
// pRect->top, pRect->bottom만큼만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltClipHeight(WORD *pDest, WORD pitch, RECT* pRect)
{
	int		count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;


	int j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수,색깔들)의 반복 수		
		count	= *pPixels++;		

		// 한 줄 출력
		if (count > 0)
		{			
			j = count;
			do
			{
				pDestTemp += *pPixels++;		// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;		// 투명 아닌 색 수				

				// 투명이 아닌 색들을 Surface에 출력한다.
				//memcpy((void*)pDestTemp, (void*)pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memset((void*)pDestTemp, 0, colorCount<<1);
				
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}


//----------------------------------------------------------------------
// BltDarkness
//----------------------------------------------------------------------
// Clipping하지 않는다.
//----------------------------------------------------------------------
void
CShadowSprite::BltDarkness(WORD *pDest, WORD pitch, BYTE DarkBits)
{
	s_Value1 = DarkBits;

	int	count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;

	int j;

	for (int i=0; i<m_Height; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수)의 반복 수		
		count	= *pPixels++;		
 		// 한 줄 출력
		if (count > 0)
		{			
			j = count;
			do
			{						
				pDestTemp += *pPixels++;			// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;		// 투명 아닌 색 수				

				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				//memset((void*)pDestTemp, 0, colorCount<<1);
				memcpyShadowDarkness(pDestTemp, colorCount);
		
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltDarkness ClipLeft
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltDarknessClipLeft(WORD* pDest, WORD pitch, RECT* pRect, BYTE DarkBits)
{
	s_Value1 = DarkBits;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += index - pRect->left;

						// 이번 단계는 모두 출력
						//memcpy(pDestTemp, pPixels, colorCount<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						//memset((void*)pDestTemp, 0, colorCount<<1);
						memcpyShadowDarkness(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{
						dist = pRect->left - index;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						//memset((void*)pDestTemp, 0, (colorCount-dist)<<1);
						memcpyShadowDarkness(pDestTemp, colorCount-dist);

						pDestTemp += colorCount-dist;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 이제부터는 계속 출력한다.		
			//---------------------------------------------		
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 건너 뛴다.
					pDestTemp += transCount;			
					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, colorCount<<1);
					memcpyShadowDarkness(pDestTemp, colorCount);

					// memory addr 증가
					pDestTemp += colorCount;
					//pPixels += colorCount;			
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltDarkness ClipRight
//----------------------------------------------------------------------
// 오른쪽 clipping.  
// pRect->right개 까지의 점만 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltDarknessClipRight(WORD* pDest, WORD pitch, RECT* pRect, BYTE DarkBits)
{
	s_Value1 = DarkBits;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index;

	int	j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
			
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가
				index += transCount;
				
				// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
				// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

				// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

				//---------------------------------------------
				// 오른쪽 끝까지 도달했을 경우
				//---------------------------------------------			
				if (index+colorCount > pRect->right)
				{
					// 투명색만으로 더 출력할 필요가 없을 때
					if (index > pRect->right)
					{
						break;
					}
					// 투명색 아닌 것을 조금 출력해야 할 경우
					else
					{
						pDestTemp += transCount;
					
						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
						memcpyShadowDarkness(pDestTemp, static_cast<WORD>(pRect->right - index));

						break;
					}
				}

				// 투명색만큼 건너띄고
				pDestTemp += transCount;

				// 출력
				//memcpy(pDestTemp, pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				//memset((void*)pDestTemp, 0, colorCount<<1);
				memcpyShadowDarkness(pDestTemp, colorCount);

				pDestTemp += colorCount;
				//pPixels += colorCount;			
				index += colorCount;
			} while (--j);
		}
		
		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltDarkness ClipWidth
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력하다가
// pRect->Right까지만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltDarknessClipWidth(WORD* pDest, WORD pitch, RECT* pRect, BYTE DarkBits)
{
	s_Value1 = DarkBits;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += index - pRect->left;

						// 이번 단계는 모두 출력?
						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{							
							// 투명색만으로 오른쪽 끝 넘어가는 경우
							if (index > pRect->right)
							{
							}
							else
							{
								//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
								memcpyShadowDarkness(pDestTemp, static_cast<WORD>(pRect->right - index));
							}

							j = count;
							
							break;
						}

						// 이번 단계는 모두 출력
						//memcpy(pDestTemp, pPixels, colorCount<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						//memset((void*)pDestTemp, 0, colorCount<<1);
						memcpyShadowDarkness(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;
						index += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{
						dist = pRect->left - index;

						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{
							memcpyShadowDarkness(pDestTemp, static_cast<WORD>(pRect->right - pRect->left));

							j = count;						
							break;
						}

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						//memset((void*)pDestTemp, 0, (colorCount-dist)<<1);
						memcpyShadowDarkness(pDestTemp, colorCount-dist);

						pDestTemp += colorCount-dist;
						//pPixels += colorCount;		
						index += colorCount;

						// 이제부터는 계속 출력한다.					
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 각 줄마다 Clipping을 해줘야 하는데...		
			// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
			//---------------------------------------------
			// OOOOOOOOOOOOOO까지만 출력해주면 된다.
			//---------------------------------------------
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 index증가
					index += transCount;
					
					// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
					// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

					// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

					//---------------------------------------------
					// 오른쪽 끝까지 도달했을 경우
					//---------------------------------------------			
					if (index+colorCount > pRect->right)
					{
						// 투명색만으로 더 출력할 필요가 없을 때
						if (index > pRect->right)
						{
							break;
						}
						// 투명색 아닌 것을 조금 출력해야 할 경우
						else
						{
							pDestTemp += transCount;
						
							// 투명이 아닌 색들을 Surface에 출력한다.
							//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
							// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
							//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
							memcpyShadowDarkness(pDestTemp, static_cast<WORD>(pRect->right-index));

							break;
						}
					}

					// 투명색만큼 건너띄고
					pDestTemp += transCount;

					// 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, colorCount<<1);
					memcpyShadowDarkness(pDestTemp, colorCount);

					pDestTemp += colorCount;
					//pPixels += colorCount;			
					index += colorCount;
				} while (--j);
			}
		}
			

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltDarkness Clip Height
//----------------------------------------------------------------------
// pRect->top, pRect->bottom만큼만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltDarknessClipHeight(WORD *pDest, WORD pitch, RECT* pRect, BYTE DarkBits)
{
	s_Value1 = DarkBits;

	int	count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;


	int j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수,색깔들)의 반복 수		
		count	= *pPixels++;		

		// 한 줄 출력
		if (count > 0)
		{			
			j = count;
			do
			{
				pDestTemp += *pPixels++;		// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;		// 투명 아닌 색 수				

				// 투명이 아닌 색들을 Surface에 출력한다.
				//memcpy((void*)pDestTemp, (void*)pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				//memset((void*)pDestTemp, 0, colorCount<<1);
				memcpyShadowDarkness(pDestTemp, colorCount);
				
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// ShadowDarkness Copy
//----------------------------------------------------------------------
// Darkens `pixels` pixels at dest in place, each colour channel shifted
// right by s_Value1 bits. Every pixel of the run is darkened once.
//
// A run starts at any pixel, so the one, two and four pixels handled at
// a time are read and written through memcpy, which assumes no
// alignment; compilers turn each into a single load or store.
//----------------------------------------------------------------------
namespace {

template <typename T>
inline void
DarkenPixels(BYTE* pDest, T mask, BYTE shift)
{
	T value;
	std::memcpy(&value, pDest, sizeof(value));
	value = static_cast<T>((value >> shift) & mask);
	std::memcpy(pDest, &value, sizeof(value));
}

}

void	
CShadowSprite::memcpyShadowDarkness(WORD* pDest, WORD pixels)
{
	BYTE*	pBytes	= (BYTE*)pDest;

	const int qTimes = pixels >> 2;	// pixels / 4

	// The pixels left over from whole groups of four go first: one, two,
	// or one and then two.
	switch ( pixels & 0x03 )	// pixels % 4
	{
		case 1 :
			DarkenPixels<WORD>(pBytes, ColorDraw::s_wMASK_SHIFT[s_Value1], s_Value1);
			pBytes += sizeof(WORD);
		break;

		case 2 :
			DarkenPixels<DWORD>(pBytes, ColorDraw::s_dwMASK_SHIFT[s_Value1], s_Value1);
			pBytes += sizeof(DWORD);
		break;

		case 3 :
			DarkenPixels<WORD>(pBytes, ColorDraw::s_wMASK_SHIFT[s_Value1], s_Value1);
			pBytes += sizeof(WORD);

			DarkenPixels<DWORD>(pBytes, ColorDraw::s_dwMASK_SHIFT[s_Value1], s_Value1);
			pBytes += sizeof(DWORD);
		break;
	}

	// Four pixels at a time
	for (int j=0; j<qTimes; j++)
	{
		DarkenPixels<uint64_t>(pBytes, ColorDraw::s_qwMASK_SHIFT[s_Value1], s_Value1);
		pBytes += sizeof(uint64_t);
	}
}

//----------------------------------------------------------------------
// Blt4444
//----------------------------------------------------------------------
// Clipping하지 않는다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt4444(WORD *pDest, WORD pitch, WORD pixel)
{
	s_wValue1 = pixel;

	int	count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;

	for (int i=0; i<m_Height; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수)의 반복 수		
		count	= *pPixels++;		
 		// 한 줄 출력
		for (int j=0; j<count; j++)
		{				
			pDestTemp += *pPixels++;			// 투명색만큼 건너 뛴다.
			colorCount = *pPixels++;		// 투명 아닌 색 수				

			// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
			//memset((void*)pDestTemp, 0, colorCount<<1);
			memcpyShadow4444(pDestTemp, colorCount);
	
			pDestTemp	+= colorCount;
			//pPixels		+= colorCount;
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt4444 ClipLeft
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt4444ClipLeft(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel)
{
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		for (j=0; j<count; j++)
		{
			transCount = *pPixels++;		// 투명색 수			
			colorCount = *pPixels++;		// 투명 아닌 색 수			
					
			// 투명색만큼 index증가			
			index += transCount;
			
		
			//---------------------------------------------
			// xxxx범위를 넘어가게 되는 경우
			//---------------------------------------------
			if (index+colorCount > pRect->left)
			{
				//---------------------------------------------
				// 투명색만으로 xxxx범위를 넘어갔을 경우
				//---------------------------------------------
				if (index > pRect->left)
				{	
					// 투명색부분 건너띔
					pDestTemp += index - pRect->left;

					// 이번 단계는 모두 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, colorCount<<1);
					memcpyShadow4444(pDestTemp, colorCount);

					pDestTemp += colorCount;
					//pPixels += colorCount;

					// 이제부터는 계속 출력한다.
					break;
				}
				//---------------------------------------------
				// 투명색+투명아닌색의 일부까지 출력하면 
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				else
				{
					dist = pRect->left - index;

					// 투명이 아닌 색들을 Surface에 출력한다.
					//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, (colorCount-dist)<<1);
					memcpyShadow4444(pDestTemp, colorCount-dist);

					pDestTemp += colorCount-dist;
					//pPixels += colorCount;

					// 이제부터는 계속 출력한다.
					break;
				}
			}					

			// 투명이 아닌 색만큼 index증가				
			//pPixels += colorCount;
			index += colorCount;
		}

		//---------------------------------------------
		// 이제부터는 계속 출력한다.		
		//---------------------------------------------		
		for (j++; j<count; j++)
		{
			transCount = *pPixels++;		// 투명색 수			
			colorCount = *pPixels++;		// 투명 아닌 색 수			
					
			// 투명색만큼 건너 뛴다.
			pDestTemp += transCount;			
			
			// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
			//memset((void*)pDestTemp, 0, colorCount<<1);
			memcpyShadow4444(pDestTemp, colorCount);

			// memory addr 증가
			pDestTemp += colorCount;
			//pPixels += colorCount;			
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt4444 ClipRight
//----------------------------------------------------------------------
// 오른쪽 clipping.  
// pRect->right개 까지의 점만 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt4444ClipRight(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel)
{
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index;

	int	j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
			
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		for (j=0; j<count; j++)
		{
			transCount = *pPixels++;		// 투명색 수			
			colorCount = *pPixels++;		// 투명 아닌 색 수			
					
			// 투명색만큼 index증가
			index += transCount;
			
			// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
			// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

			// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

			//---------------------------------------------
			// 오른쪽 끝까지 도달했을 경우
			//---------------------------------------------			
			if (index+colorCount > pRect->right)
			{
				// 투명색만으로 더 출력할 필요가 없을 때
				if (index > pRect->right)
				{
					break;
				}
				// 투명색 아닌 것을 조금 출력해야 할 경우
				else
				{
					pDestTemp += transCount;
				
					// 투명이 아닌 색들을 Surface에 출력한다.
					//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
					memcpyShadow4444(pDestTemp, static_cast<WORD>(pRect->right - index));

					break;
				}
			}

			// 투명색만큼 건너띄고
			pDestTemp += transCount;

			// 출력
			//memcpy(pDestTemp, pPixels, colorCount<<1);
			// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
			//memset((void*)pDestTemp, 0, colorCount<<1);
			memcpyShadow4444(pDestTemp, colorCount);

			pDestTemp += colorCount;
			//pPixels += colorCount;			
			index += colorCount;
		}
		
		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt4444 ClipWidth
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력하다가
// pRect->Right까지만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt4444ClipWidth(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel)
{
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	for (int i=pRect->top; i<pRect->bottom; i++)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		for (j=0; j<count; j++)
		{
			transCount = *pPixels++;		// 투명색 수			
			colorCount = *pPixels++;		// 투명 아닌 색 수			
					
			// 투명색만큼 index증가			
			index += transCount;
			
		
			//---------------------------------------------
			// xxxx범위를 넘어가게 되는 경우
			//---------------------------------------------
			if (index+colorCount > pRect->left)
			{
				//---------------------------------------------
				// 투명색만으로 xxxx범위를 넘어갔을 경우
				//---------------------------------------------
				if (index > pRect->left)
				{	
					// 투명색부분 건너띔
					pDestTemp += index - pRect->left;

					// 이번 단계는 모두 출력?
					// 오른쪽 끝을 넘어가는 경우..
					if (index+colorCount > pRect->right)
					{							
						// 투명색만으로 오른쪽 끝 넘어가는 경우
						if (index > pRect->right)
						{
						}
						else
						{
							//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
							memcpyShadow4444(pDestTemp, static_cast<WORD>(pRect->right - index));
						}

						j = count;
						
						break;
					}

					// 이번 단계는 모두 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, colorCount<<1);
					memcpyShadow4444(pDestTemp, colorCount);

					pDestTemp += colorCount;
					//pPixels += colorCount;
					index += colorCount;

					// 이제부터는 계속 출력한다.
					break;
				}
				//---------------------------------------------
				// 투명색+투명아닌색의 일부까지 출력하면 
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				else
				{
					dist = pRect->left - index;

					// 오른쪽 끝을 넘어가는 경우..
					if (index+colorCount > pRect->right)
					{
						memcpyShadow4444(pDestTemp, static_cast<WORD>(pRect->right - pRect->left));

						j = count;						
						break;
					}

					// 투명이 아닌 색들을 Surface에 출력한다.
					//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, (colorCount-dist)<<1);
					memcpyShadow4444(pDestTemp, colorCount-dist);

					pDestTemp += colorCount-dist;
					//pPixels += colorCount;		
					index += colorCount;

					// 이제부터는 계속 출력한다.					
					break;
				}
			}					

			// 투명이 아닌 색만큼 index증가				
			//pPixels += colorCount;
			index += colorCount;
		}

		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		for (j++; j<count; j++)
		{
			transCount = *pPixels++;		// 투명색 수			
			colorCount = *pPixels++;		// 투명 아닌 색 수			
					
			// 투명색만큼 index증가
			index += transCount;
			
			// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
			// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

			// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

			//---------------------------------------------
			// 오른쪽 끝까지 도달했을 경우
			//---------------------------------------------			
			if (index+colorCount > pRect->right)
			{
				// 투명색만으로 더 출력할 필요가 없을 때
				if (index > pRect->right)
				{
					break;
				}
				// 투명색 아닌 것을 조금 출력해야 할 경우
				else
				{
					pDestTemp += transCount;
				
					// 투명이 아닌 색들을 Surface에 출력한다.
					//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//memset((void*)pDestTemp, 0, (pRect->right - index)<<1);
					memcpyShadow4444(pDestTemp, static_cast<WORD>(pRect->right-index));

					break;
				}
			}

			// 투명색만큼 건너띄고
			pDestTemp += transCount;

			// 출력
			//memcpy(pDestTemp, pPixels, colorCount<<1);
			// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
			//memset((void*)pDestTemp, 0, colorCount<<1);
			memcpyShadow4444(pDestTemp, colorCount);

			pDestTemp += colorCount;
			//pPixels += colorCount;			
			index += colorCount;
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt4444 Clip Height
//----------------------------------------------------------------------
// pRect->top, pRect->bottom만큼만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::Blt4444ClipHeight(WORD *pDest, WORD pitch, RECT* pRect, WORD pixel)
{
	s_wValue1 = pixel;

	int	count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;


	int j;

	for (int i=pRect->top; i<pRect->bottom; i++)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수,색깔들)의 반복 수		
		count	= *pPixels++;		

		// 한 줄 출력
		for (j=0; j<count; j++)
		{				
			pDestTemp += *pPixels++;		// 투명색만큼 건너 뛴다.
			colorCount = *pPixels++;		// 투명 아닌 색 수				

			// 투명이 아닌 색들을 Surface에 출력한다.
			//memcpy((void*)pDestTemp, (void*)pPixels, colorCount<<1);
			// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
			//memset((void*)pDestTemp, 0, colorCount<<1);
			memcpyShadow4444(pDestTemp, colorCount);
			
			pDestTemp	+= colorCount;
			//pPixels		+= colorCount;
		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// ShadowDarkness Copy
//----------------------------------------------------------------------
// dest의 pixels개를 s_wValue1로 한다.
//----------------------------------------------------------------------
void	
CShadowSprite::memcpyShadow4444(WORD* pDest, WORD pixels)
{
	int i = pixels;

	if (i>0)
	{
		do {	
			*pDest = s_wValue1;
			pDest++;
		} while (--i);
	}
}


//----------------------------------------------------------------------
// Blt Small
//----------------------------------------------------------------------
// 2^shift byte수당 한 점씩만 출력한다.
//
// 100*100 Sprite일 경우 
// shift = 0 , 100*100
// shift = 1 , 50*50
// shift = 2 , 25*25
// shift = 3 , 12*12
// shift = 4 , 6*6
// ...
// Clipping하지 않는다.
//----------------------------------------------------------------------
// 길이 보정해야되는데...
// clipping 함수들에서는 안한다. - -; 귀찮아서.. 음하하.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall(WORD *pDest, WORD pitch, BYTE shift)
{
	int		count,		
			transCount,
			transCountShift,
			colorCount,
			colorCountShift,
			totalCount,
			totalShiftCount,
			pixelGap,
			pixelGapShift;

	WORD	*pDestTemp,
			*pPixels;

	int i;
	int j;

	if (m_Height > 0)
	{
		i = m_Height-1;
		int stepY = 1 << shift;		// y줄 건너띄는 pixel수
		pDest = (WORD*)((BYTE*)pDest + (i>>shift)*pitch);

		do {	
			pPixels		= m_Pixels[i];
			pDestTemp	= pDest;

			// (투명수,색깔수)의 반복 수		
			count	= *pPixels++;		
 			// 한 줄 출력
			totalCount = 0;
			totalShiftCount = 0;
			if (count > 0)
			{			
				j = count;
				do
				{
					transCount = *pPixels++;
					colorCount = *pPixels++;				// 투명 아닌 색 수
					
					transCountShift = transCount >> shift;
					colorCountShift = colorCount >> shift;

					//--------------------------------------------------
					//				투명 부분 길이 보정
					//--------------------------------------------------
					// 실제 size의 pixel수..
					//--------------------------------------------------
					totalCount += transCount;
					totalShiftCount += transCountShift;

					// 실제pixel - shift해서 늘린 pixel(-_-;)
					pixelGap = totalCount - (totalShiftCount << shift);

					// gap을 다시 shift해서 더해준다.
					pixelGapShift = pixelGap >> shift;
					transCountShift += pixelGapShift;
					totalShiftCount += pixelGapShift;

					//--------------------------------------------------
					// 투명색만큼 건너 뛴다.
					//--------------------------------------------------
					pDestTemp += transCountShift;		

					
					//--------------------------------------------------
					//				그림자 부분 길이 보정
					//--------------------------------------------------
					totalCount += colorCount;
					totalShiftCount += colorCountShift;
					
					//--------------------------------------------------
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//--------------------------------------------------
					memset((void*)pDestTemp, 0, colorCount);
			
					pDestTemp	+= colorCountShift;
					//pPixels		+= colorCount;
				} while (--j);
			}

			pDest = (WORD*)((BYTE*)pDest - pitch);
			
			i-=stepY;

		} while (i>0);
	}
}

//----------------------------------------------------------------------
// BltSmall ClipLeft
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmallClipLeft(WORD* pDest, WORD pitch, RECT* pRect, BYTE shift)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += (index - pRect->left) >> shift;

						// shift만큼 줄여버린다.
						colorCount >>= shift;

						// 이번 단계는 모두 출력
						//memcpy(pDestTemp, pPixels, colorCount<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{
						dist = pRect->left - index;

						// shift만큼 줄여버린다.
						colorCount = (colorCount-dist) >> shift;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 이제부터는 계속 출력한다.		
			//---------------------------------------------		
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 건너 뛴다.
					pDestTemp += transCount>>shift;			

					// shift만큼 줄여버린다.
					colorCount >>= shift;
					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memset((void*)pDestTemp, 0, colorCount<<1);

					// memory addr 증가
					pDestTemp += colorCount;
					//pPixels += colorCount;			
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall ClipRight
//----------------------------------------------------------------------
// 오른쪽 clipping.  
// pRect->right개 까지의 점만 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmallClipRight(WORD* pDest, WORD pitch, RECT* pRect, BYTE shift)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,			
			index;

	int	j;

	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
			
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++ >> shift;		// 투명색 수			
				colorCount = *pPixels++ >> shift;		// 투명 아닌 색 수		
						
				// 투명색만큼 index증가
				index += transCount;
				
				// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
				// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

				// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

				//---------------------------------------------
				// 오른쪽 끝까지 도달했을 경우
				//---------------------------------------------			
				if (index+colorCount > pRect->right)
				{
					// 투명색만으로 더 출력할 필요가 없을 때
					if (index > pRect->right)
					{
						break;
					}
					// 투명색 아닌 것을 조금 출력해야 할 경우
					else
					{
						pDestTemp += transCount;
					
						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, (pRect->right - index)<<1);

						break;
					}
				}

				// 투명색만큼 건너띄고
				pDestTemp += transCount;

				index += colorCount;
				
				// 출력
				//memcpy(pDestTemp, pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memset((void*)pDestTemp, 0, colorCount<<1);

				pDestTemp += colorCount;
				//pPixels += colorCount;							
			} while (--j);
		}
		
		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall ClipWidth
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력하다가
// pRect->Right까지만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmallClipWidth(WORD* pDest, WORD pitch, RECT* pRect, BYTE shift)
{
	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int		count,
			transCount, 
			colorCount,
			index,
			dist;

	short j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += (index - pRect->left)>>shift;

						// 이번 단계는 모두 출력?
						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{							
							// 투명색만으로 오른쪽 끝 넘어가는 경우
							if (index > pRect->right)
							{
							}
							else
							{
								memset((void*)pDestTemp, 0, ((pRect->right - index)>>shift)<<1);
							}

							j = count;
							
							break;
						}

						// shift만큼 줄여버린다.
						colorCount >>= shift;
					
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;
						index += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{		
						dist = pRect->left - index;

						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{						
							memset((void*)pDestTemp, 0, (pRect->right - pRect->left)>>shift);

							j = count;						
							break;
						}

						// shift만큼 줄여버린다.
						index += colorCount;
						colorCount = (colorCount-dist) >> shift;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memset((void*)pDestTemp, 0, colorCount<<1);

						pDestTemp += colorCount;
						//pPixels += colorCount;		
						

						// 이제부터는 계속 출력한다.					
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 각 줄마다 Clipping을 해줘야 하는데...		
			// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
			//---------------------------------------------
			// OOOOOOOOOOOOOO까지만 출력해주면 된다.
			//---------------------------------------------
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 index증가
					index += transCount;
					
					// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
					// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

					// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

					//---------------------------------------------
					// 오른쪽 끝까지 도달했을 경우
					//---------------------------------------------			
					if (index+colorCount > pRect->right)
					{
						// 투명색만으로 더 출력할 필요가 없을 때
						if (index > pRect->right)
						{
							break;
						}
						// 투명색 아닌 것을 조금 출력해야 할 경우
						else
						{
							pDestTemp += transCount>>shift;
						
							// 투명이 아닌 색들을 Surface에 출력한다.
							//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
							// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
							memset((void*)pDestTemp, 0, ((pRect->right - index)>>shift)<<1);

							break;
						}
					}

					// 투명색만큼 건너띄고
					pDestTemp += transCount>>shift;


					index += colorCount;
					// shift만큼 줄여준다.
					colorCount >>= shift;

					// 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memset((void*)pDestTemp, 0, colorCount<<1);

					pDestTemp += colorCount;
					//pPixels += colorCount;								
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall Clip Height
//----------------------------------------------------------------------
// pRect->top, pRect->bottom만큼만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmallClipHeight(WORD *pDest, WORD pitch, RECT* pRect, BYTE shift)
{
	int		count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;


	int j;
	
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수,색깔들)의 반복 수		
		count	= *pPixels++;		

		if (count > 0)
		{			
			j = count;
			do
			{
				pDestTemp += ((*pPixels++)>>shift);		// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;				// 투명 아닌 색 수
				
				colorCount >>= shift;

				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memset((void*)pDestTemp, 0, colorCount<<1);
		
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// Blt Small4444
//----------------------------------------------------------------------
// 2^shift byte수당 한 점씩만 출력한다.
//
// 100*100 Sprite일 경우 
// shift = 0 , 100*100
// shift = 1 , 50*50
// shift = 2 , 25*25
// shift = 3 , 12*12
// shift = 4 , 6*6
// ...
// Clipping하지 않는다.
//----------------------------------------------------------------------
// 길이 보정해야되는데...
// clipping 함수들에서는 안한다. - -; 귀찮아서.. 음하하.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall4444(WORD *pDest, WORD pitch, WORD pixel, BYTE shift)
{
	// Shadow색깔 설정
	s_wValue1 = pixel;

	int		count,		
			transCount,
			transCountShift,
			colorCount,
			colorCountShift,
			totalCount,
			totalShiftCount,
			pixelGap,
			pixelGapShift;


	WORD	*pDestTemp,
			*pPixels;

	int j;

	/*
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수		
	int m_Height_1 = m_Height - 1;

	if (m_Height > 0)
	{
		i = m_Height_1;
		pDest = (WORD*)((BYTE*)pDest + (i>>shift)*pitch);

		do {	
			pPixels		= m_Pixels[i];
			pDestTemp	= pDest;

			// (투명수,색깔수)의 반복 수		
			count	= *pPixels++;		
 			// 한 줄 출력
			totalCount = 0;
			totalShiftCount = 0;
			if (count > 0)
			{			
				j = count;
				do
				{
					transCount = *pPixels++;
					colorCount = *pPixels++;				// 투명 아닌 색 수
					
					transCountShift = transCount >> shift;
					colorCountShift = colorCount >> shift;

					//--------------------------------------------------
					//				투명 부분 길이 보정
					//--------------------------------------------------
					// 실제 size의 pixel수..
					//--------------------------------------------------
					totalCount += transCount;
					totalShiftCount += transCountShift;

					// 실제pixel - shift해서 늘린 pixel(-_-;)
					pixelGap = totalCount - (totalShiftCount << shift);

					// gap을 다시 shift해서 더해준다.
					pixelGapShift = pixelGap >> shift;
					transCountShift += pixelGapShift;
					totalShiftCount += pixelGapShift;

					//--------------------------------------------------
					// 투명색만큼 건너 뛴다.
					//--------------------------------------------------
					pDestTemp += transCountShift;		

					
					//--------------------------------------------------
					//				그림자 부분 길이 보정
					//--------------------------------------------------
					totalCount += colorCount;
					totalShiftCount += colorCountShift;
					
					//--------------------------------------------------
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					//--------------------------------------------------
					memcpyShadow4444(pDestTemp, colorCountShift);
			
					pDestTemp	+= colorCountShift;
					//pPixels		+= colorCount;
				} while (--j);
			}

			pDest = (WORD*)((BYTE*)pDest - pitch);
			
			i-=stepY;

		} while (i>0);
	}
	*/

	int stepY = 1 << shift;		// y줄 건너띄는 pixel수		
	
	for (int i=0; i<m_Height; i+=stepY)
	{
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수)의 반복 수		
		count	= *pPixels++;		
 		// 한 줄 출력
		totalCount = 0;
		totalShiftCount = 0;


		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;
				colorCount = *pPixels++;				// 투명 아닌 색 수
				
				transCountShift = transCount >> shift;
				colorCountShift = colorCount >> shift;

				//--------------------------------------------------
				//				투명 부분 길이 보정
				//--------------------------------------------------
				// 실제 size의 pixel수..
				//--------------------------------------------------
				totalCount += transCount;
				totalShiftCount += transCountShift;

				// 실제pixel - shift해서 늘린 pixel(-_-;)
				pixelGap = totalCount - (totalShiftCount << shift);

				// gap을 다시 shift해서 더해준다.
				pixelGapShift = pixelGap >> shift;
				transCountShift += pixelGapShift;
				totalShiftCount += pixelGapShift;

				//--------------------------------------------------
				// 투명색만큼 건너 뛴다.
				//--------------------------------------------------
				pDestTemp += transCountShift;		
			
				//--------------------------------------------------
				//				그림자 부분 길이 보정
				//--------------------------------------------------
				totalCount += colorCount;
				totalShiftCount += colorCountShift;
				
				//--------------------------------------------------
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				//--------------------------------------------------
				memcpyShadow4444(pDestTemp, colorCountShift);

				pDestTemp	+= colorCountShift;

				//pPixels		+= colorCount;
			} while (--j);

		}

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall4444 ClipLeft
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall4444ClipLeft(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel, BYTE shift)
{
	// Shadow색깔 설정
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,
			index,
			dist;

	int j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += (index - pRect->left) >> shift;

						// shift만큼 줄여버린다.
						colorCount >>= shift;

						// 이번 단계는 모두 출력
						//memcpy(pDestTemp, pPixels, colorCount<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memcpyShadow4444(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{
						dist = pRect->left - index;

						// shift만큼 줄여버린다.
						colorCount = (colorCount-dist) >> shift;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memcpyShadow4444(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 이제부터는 계속 출력한다.		
			//---------------------------------------------		
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 건너 뛴다.
					pDestTemp += transCount>>shift;			

					// shift만큼 줄여버린다.
					colorCount >>= shift;
					
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memcpyShadow4444(pDestTemp, colorCount);

					// memory addr 증가
					pDestTemp += colorCount;
					//pPixels += colorCount;			
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall4444 ClipRight
//----------------------------------------------------------------------
// 오른쪽 clipping.  
// pRect->right개 까지의 점만 pDest에 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall4444ClipRight(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel, BYTE shift)
{
	// Shadow색깔 설정
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int	count,
			transCount, 
			colorCount,			
			index;

	int	j;

	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
			
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...		
		// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
		//---------------------------------------------
		// OOOOOOOOOOOOOO까지만 출력해주면 된다.
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++ >> shift;		// 투명색 수			
				colorCount = *pPixels++ >> shift;		// 투명 아닌 색 수		
						
				// 투명색만큼 index증가
				index += transCount;
				
				// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
				// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

				// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

				//---------------------------------------------
				// 오른쪽 끝까지 도달했을 경우
				//---------------------------------------------			
				if (index+colorCount > pRect->right)
				{
					// 투명색만으로 더 출력할 필요가 없을 때
					if (index > pRect->right)
					{
						break;
					}
					// 투명색 아닌 것을 조금 출력해야 할 경우
					else
					{
						pDestTemp += transCount;
					
						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memcpyShadow4444(pDestTemp, static_cast<WORD>(pRect->right - index));

						break;
					}
				}

				// 투명색만큼 건너띄고
				pDestTemp += transCount;

				index += colorCount;
				
				// 출력
				//memcpy(pDestTemp, pPixels, colorCount<<1);
				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memcpyShadow4444(pDestTemp, colorCount);

				pDestTemp += colorCount;
				//pPixels += colorCount;							
			} while (--j);
		}
		
		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall4444 ClipWidth
//----------------------------------------------------------------------
// 왼쪽 clipping.  
// pRect->left개의 점을 건너띈 다음부터 pDest에 출력하다가
// pRect->Right까지만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall4444ClipWidth(WORD* pDest, WORD pitch, RECT* pRect, WORD pixel, BYTE shift)
{
	// Shadow색깔 설정
	s_wValue1 = pixel;

	WORD	*pPixels,
			*pDestTemp;

	//--------------------------------------------
	// pRect만큼의 점을 출력한다.
	//--------------------------------------------
	int		count,
			transCount, 
			colorCount,
			index,
			dist;

	short j;

	//---------------------------------------------
	// 출력해야하는 모든 줄에 대해서..
	//---------------------------------------------
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{
		pPixels = m_Pixels[i];
		pDestTemp = pDest;		

		// (투명수,색깔수,색깔들)의 반복 수
		count = *pPixels++;		

		// 한 줄 출력		
		index = 0;
		
		//---------------------------------------------
		// 각 줄마다 Clipping을 해줘야 하는데...
		// xxxxOOOOOOOOOOOOOO인 경우이므로..
		//---------------------------------------------
		// xxxx부분까지 check해주는 루틴
		//---------------------------------------------
		if (count > 0)
		{			
			j = count;
			do
			{		
				transCount = *pPixels++;		// 투명색 수			
				colorCount = *pPixels++;		// 투명 아닌 색 수			
						
				// 투명색만큼 index증가			
				index += transCount;
				
			
				//---------------------------------------------
				// xxxx범위를 넘어가게 되는 경우
				//---------------------------------------------
				if (index+colorCount > pRect->left)
				{
					//---------------------------------------------
					// 투명색만으로 xxxx범위를 넘어갔을 경우
					//---------------------------------------------
					if (index > pRect->left)
					{	
						// 투명색부분 건너띔
						pDestTemp += (index - pRect->left)>>shift;

						// 이번 단계는 모두 출력?
						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{							
							// 투명색만으로 오른쪽 끝 넘어가는 경우
							if (index > pRect->right)
							{
							}
							else
							{
								memcpyShadow4444(pDestTemp, static_cast<WORD>((pRect->right - index)>>shift));
							}

							j = count;
							
							break;
						}

						// shift만큼 줄여버린다.
						colorCount >>= shift;
					
						memcpyShadow4444(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;
						index += colorCount;

						// 이제부터는 계속 출력한다.
						break;
					}
					//---------------------------------------------
					// 투명색+투명아닌색의 일부까지 출력하면 
					// xxxx범위를 넘어가게 되는 경우
					//---------------------------------------------
					else
					{		
						dist = pRect->left - index;

						// 오른쪽 끝을 넘어가는 경우..
						if (index+colorCount > pRect->right)
						{						
							memcpyShadow4444(pDestTemp, static_cast<WORD>((pRect->right - pRect->left)>>shift));

							j = count;						
							break;
						}

						// shift만큼 줄여버린다.
						index += colorCount;
						colorCount = (colorCount-dist) >> shift;

						// 투명이 아닌 색들을 Surface에 출력한다.
						//memcpy(pDestTemp, pPixels+dist, (colorCount-dist)<<1);					
						// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
						memcpyShadow4444(pDestTemp, colorCount);

						pDestTemp += colorCount;
						//pPixels += colorCount;		
						

						// 이제부터는 계속 출력한다.					
						break;
					}
				}					

				// 투명이 아닌 색만큼 index증가				
				//pPixels += colorCount;
				index += colorCount;
			} while (--j);

			//---------------------------------------------
			// 각 줄마다 Clipping을 해줘야 하는데...		
			// OOOOOOOOOOOOOOxxxxx 이런 경우이다.
			//---------------------------------------------
			// OOOOOOOOOOOOOO까지만 출력해주면 된다.
			//---------------------------------------------
			if (--j > 0)
			{
				do
				{
					transCount = *pPixels++;		// 투명색 수			
					colorCount = *pPixels++;		// 투명 아닌 색 수			
							
					// 투명색만큼 index증가
					index += transCount;
					
					// 출력하고 있다가 오른쪽부분부터 출력하지 말아야 할 경우가 있다.
					// 현재 출력하는 줄은 모두 출력한 것이므로 break해야 한다.

					// 투명색까지 출력하는것만으로 더 이상 출력할 필요가 없을 경우

					//---------------------------------------------
					// 오른쪽 끝까지 도달했을 경우
					//---------------------------------------------			
					if (index+colorCount > pRect->right)
					{
						// 투명색만으로 더 출력할 필요가 없을 때
						if (index > pRect->right)
						{
							break;
						}
						// 투명색 아닌 것을 조금 출력해야 할 경우
						else
						{
							pDestTemp += transCount>>shift;
						
							// 투명이 아닌 색들을 Surface에 출력한다.
							//memcpy(pDestTemp, pPixels, (pRect->right - index)<<1);
							// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
							memcpyShadow4444(pDestTemp, static_cast<WORD>((pRect->right - index)>>shift));

							break;
						}
					}

					// 투명색만큼 건너띄고
					pDestTemp += transCount>>shift;


					index += colorCount;
					// shift만큼 줄여준다.
					colorCount >>= shift;

					// 출력
					//memcpy(pDestTemp, pPixels, colorCount<<1);
					// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
					memcpyShadow4444(pDestTemp, colorCount);

					pDestTemp += colorCount;
					//pPixels += colorCount;								
				} while (--j);
			}
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

//----------------------------------------------------------------------
// BltSmall4444 Clip Height
//----------------------------------------------------------------------
// pRect->top, pRect->bottom만큼만 출력한다.
//----------------------------------------------------------------------
void
CShadowSprite::BltSmall4444ClipHeight(WORD *pDest, WORD pitch, RECT* pRect, WORD pixel, BYTE shift)
{
	// Shadow색깔 설정
	s_wValue1 = pixel;

	int		count,			
			colorCount;

	WORD	*pDestTemp,
			*pPixels;


	int j;
	
	int stepY = 1 << shift;		// y줄 건너띄는 pixel수
	int endY = pRect->bottom << shift;
	for (int i=pRect->top; i<endY; i+=stepY)
	{			
		pPixels		= m_Pixels[i];
		pDestTemp	= pDest;

		// (투명수,색깔수,색깔들)의 반복 수		
		count	= *pPixels++;		

		if (count > 0)
		{			
			j = count;
			do
			{
				pDestTemp += ((*pPixels++)>>shift);		// 투명색만큼 건너 뛴다.
				colorCount = *pPixels++;				// 투명 아닌 색 수
				
				colorCount >>= shift;

				// 0번(검정색, 5:5:5나 5:6:5나 똑같다)색으로 출력
				memcpyShadow4444(pDestTemp, colorCount);
		
				pDestTemp	+= colorCount;
				//pPixels		+= colorCount;
			} while (--j);
		}
		

		pDest = (WORD*)((BYTE*)pDest + pitch);
	}
}

