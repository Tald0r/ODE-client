//----------------------------------------------------------------------
// MEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffect.h"
#include "MTopView.h"
#include <fstream>
#include "DebugInfo.h"

//----------------------------------------------------------------------
// Init Static Members
//----------------------------------------------------------------------
TYPE_OBJECTID	MEffect::s_ID	= 0;

#ifdef OUTPUT_DEBUG
	int g_EffectCount = 0;
#endif

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
MEffect::MEffect(BYTE bltType)
: CAnimationFrame(bltType)
{
	// instance ID
	m_ID			= s_ID++;

	m_ObjectType	= TYPE_EFFECT;
//	m_EffectType	= EFFECT_SECTOR;

	m_Direction		= 0;

	m_PixelX		= 0;
	m_PixelY		= 0;
	m_PixelZ		= 0;
	m_StepPixel		= 0;

	// 다음 Effect없음
	m_pEffectTarget = NULL;

	m_Light = 0;

	m_bMulti = false;
	m_bDrawSkip = false;

	// 新增：资源容器初始化
	m_pResources = nullptr;

	#ifdef OUTPUT_DEBUG
		g_EffectCount++;
	#endif
}

// 新构造函数：支持依赖注入
MEffect::MEffect(BYTE bltType, EffectResourceContainer* resources)
: CAnimationFrame(bltType)
{
	// instance ID
	m_ID			= s_ID++;

	m_ObjectType	= TYPE_EFFECT;

	m_Direction		= 0;

	m_PixelX		= 0;
	m_PixelY		= 0;
	m_PixelZ		= 0;
	m_StepPixel		= 0;

	// 다음 Effect없음
	m_pEffectTarget = NULL;

	m_Light = 0;

	m_bMulti = false;
	m_bDrawSkip = false;

	// 新增：资源容器（依赖注入）
	m_pResources = resources;

	#ifdef OUTPUT_DEBUG
		g_EffectCount++;
	#endif
}

MEffect::~MEffect()
{
	if (m_pEffectTarget!=NULL)
	{
		//if (!m_pEffectTarget->IsEnd())
		delete m_pEffectTarget;
		m_pEffectTarget = NULL;
	}

	#ifdef OUTPUT_DEBUG
		g_EffectCount--;
	#endif
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set Count
//----------------------------------------------------------------------
void			
MEffect::SetCount(DWORD last, DWORD linkCount)
{ 
	EffectTiming::SetCount(g_CurrentFrame, last, linkCount);
}
//----------------------------------------------------------------------
// Set Link
//----------------------------------------------------------------------
void			
MEffect::SetLink(TYPE_ACTIONINFO nActionInfo, MEffectTarget* pEffectTarget)
{
	#ifdef OUTPUT_DEBUG
		if (pEffectTarget==NULL)
		{
			DEBUG_ADD_FORMAT("[MEffect]SetLink: ai=%d, effectTarget=NULL", nActionInfo);
		}
		else
		{
			DEBUG_ADD_FORMAT("[MEffect]SetLink: ai=%d, effectTargetID=%d", nActionInfo, (int)pEffectTarget->GetEffectID());
		}
	#endif
	
	if (m_pEffectTarget!=NULL)
	{
		DEBUG_ADD_FORMAT("[MEffect]Delete EffectTarget for New One: eid=%d", (int)m_pEffectTarget->GetEffectID());
		
		delete m_pEffectTarget;
		m_pEffectTarget = NULL;
	}

	m_nActionInfo	= nActionInfo;
	m_pEffectTarget	= pEffectTarget;
}

//----------------------------------------------------------------------
// SetEffectTargetNULL
//----------------------------------------------------------------------
void
MEffect::SetEffectTargetNULL()
{
	m_pEffectTarget = NULL;
}


//----------------------------------------------------------------------
// Set Position(x,y)
//----------------------------------------------------------------------
// pixel좌표를 설정하고
// Zone에서 해당하는 Sector의 좌표도 설정해줘야 한다.
//----------------------------------------------------------------------
void			
MEffect::SetPixelPosition(int x, int y, int z)
{
	m_PixelX = static_cast<float>(x);
	m_PixelY = static_cast<float>(y);
	m_PixelZ = static_cast<float>(z);

	AffectPosition();
}


//----------------------------------------------------------------------
// Affect Position
//----------------------------------------------------------------------
// PixelPositon으로서 Sector좌표를 설정한다.
//----------------------------------------------------------------------
void
MEffect::AffectPosition()
{
	// Pixel좌표를 Sector좌표로 바꾼다.
	m_X = MTopView::PixelToMapX( static_cast<int>(m_PixelX) );
	m_Y = MTopView::PixelToMapY( static_cast<int>(m_PixelY) );
}


//----------------------------------------------------------------------
// SetFrameID
//----------------------------------------------------------------------
// Base class인 CAnimationFrame의 SetFrameID를 overload한다.
//----------------------------------------------------------------------
void			
MEffect::SetFrameID(TYPE_FRAMEID FrameID, BYTE max)
{ 
	CAnimationFrame::SetFrameID(FrameID, max);

	// EffectFrame의 밝기에 따라서 빛의 크기를 정한다.
	m_Light = g_pTopView->GetEffectLight((BLT_TYPE)m_BltType, FrameID, m_Direction, 0);
}

//----------------------------------------------------------------------
// SetPosition(x,y)
//----------------------------------------------------------------------
void		
MEffect::SetPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	m_X = x; 
	m_Y = y; 
	
	m_PixelX = static_cast<float>(MTopView::MapToPixelX(x)); 
	m_PixelY = static_cast<float>(MTopView::MapToPixelY(y));
}

//----------------------------------------------------------------------
// SetX
//----------------------------------------------------------------------
void		
MEffect::SetX(TYPE_SECTORPOSITION x)
{ 
	m_X = x; 
	m_PixelX = static_cast<float>(MTopView::MapToPixelX(x)); 
}

//----------------------------------------------------------------------
// SetY
//----------------------------------------------------------------------
void		
MEffect::SetY(TYPE_SECTORPOSITION y)
{ 
	m_Y = y; 
	m_PixelY = static_cast<float>(MTopView::MapToPixelY(y)); 
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// m_Count가 0일때까지 -1 해주면서 Frame을 바꾼다.
//----------------------------------------------------------------------
bool
MEffect::Update()
{
	// Frame을 바꿔준다.
	NextFrame();

	m_Light = g_pTopView->GetEffectLight((BLT_TYPE)m_BltType, m_FrameID, m_Direction, m_CurrentFrame);
		
	// 계속 Update해도 되는가?
	return !EffectTiming::IsEnd(g_CurrentFrame);
}

void
MEffect::SetDelayFrame(DWORD frame)
{
	EffectTiming::SetDelayFrame(g_CurrentFrame, frame);
}

bool
MEffect::IsDelayFrame() const
{
	return EffectTiming::IsDelayFrame(g_CurrentFrame);
}

void
MEffect::SetWaitFrame(DWORD frame)
{
	EffectTiming::SetWaitFrame(g_CurrentFrame, frame);
}

bool
MEffect::IsWaitFrame() const
{
	return EffectTiming::IsWaitFrame(g_CurrentFrame);
}

//----------------------------------------------------------------------
// SetResourceContainer - 设置资源容器（新增）
//----------------------------------------------------------------------
void
MEffect::SetResourceContainer(EffectResourceContainer* resources)
{
	m_pResources = resources;
}
