//----------------------------------------------------------------------
// MEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffect.h"
#include "MViewDef.h"
#include "DebugLog.h"

#include <cmath>
#include <limits>

namespace {
int PixelCoordinate(float pixel)
{
	const double coordinate = pixel;
	if (std::isnan(coordinate)) return 0;
	if (coordinate >= (std::numeric_limits<int>::max)())
		return (std::numeric_limits<int>::max)();
	if (coordinate <= (std::numeric_limits<int>::min)())
		return (std::numeric_limits<int>::min)();
	return static_cast<int>(coordinate);
}
} // namespace

const MEffectHost* MEffect::s_pHost = nullptr;

const MEffectHost* MEffect::SetHost(const MEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MEffect::ReadCurrentFrame(DWORD& frame)
{
	frame = 0;
	if (!s_pHost || !s_pHost->CurrentFrame) return false;
	frame = s_pHost->CurrentFrame();
	return true;
}

char MEffect::ReadLight(BYTE bltType, TYPE_FRAMEID frameID, BYTE direction, BYTE frame)
{
	return s_pHost && s_pHost->Light
		? static_cast<char>(s_pHost->Light(bltType, frameID, direction, frame)) : 0;
}

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
	DWORD now;
	ReadCurrentFrame(now);
	EffectTiming::SetCount(now, last, linkCount);
}
//----------------------------------------------------------------------
// Set Link
//----------------------------------------------------------------------
void			
MEffect::SetLink(TYPE_ACTIONINFO nActionInfo, MEffectTarget* pEffectTarget)
{
	if (m_pEffectTarget == pEffectTarget)
	{
		m_nActionInfo = nActionInfo;
		return;
	}

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
int MEffect::GetPixelX() const { return PixelCoordinate(m_PixelX); }
int MEffect::GetPixelY() const { return PixelCoordinate(m_PixelY); }
int MEffect::GetPixelZ() const { return PixelCoordinate(m_PixelZ); }

//----------------------------------------------------------------------
// PixelPositon으로서 Sector좌표를 설정한다.
//----------------------------------------------------------------------
void
MEffect::AffectPosition()
{
	// Pixel좌표를 Sector좌표로 바꾼다.
	m_X = PixelCoordinate(m_PixelX) / TILE_X;
	m_Y = PixelCoordinate(m_PixelY) / TILE_Y;
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
	RefreshLight();
}

//----------------------------------------------------------------------
// SetPosition(x,y)
//----------------------------------------------------------------------
void		
MEffect::SetPosition(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	m_X = x; 
	m_Y = y; 
	
	m_PixelX = static_cast<float>(x * TILE_X);
	m_PixelY = static_cast<float>(y * TILE_Y);
}

//----------------------------------------------------------------------
// SetX
//----------------------------------------------------------------------
void		
MEffect::SetX(TYPE_SECTORPOSITION x)
{ 
	m_X = x; 
	m_PixelX = static_cast<float>(x * TILE_X);
}

//----------------------------------------------------------------------
// SetY
//----------------------------------------------------------------------
void		
MEffect::SetY(TYPE_SECTORPOSITION y)
{ 
	m_Y = y; 
	m_PixelY = static_cast<float>(y * TILE_Y);
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

	RefreshLight();
		
	// 계속 Update해도 되는가?
	return !IsEnd();
}

void
MEffect::RefreshLight()
{
	m_Light = ReadLight(m_BltType, m_FrameID, m_Direction, m_CurrentFrame);
}

bool
MEffect::IsEnd() const
{
	return !IsBeforeFrame(m_EndFrame);
}

bool
MEffect::IsBeforeFrame(DWORD deadline) const
{
	DWORD now;
	return ReadCurrentFrame(now) && now < deadline;
}

void
MEffect::LimitRemainingFrames(DWORD frames)
{
	DWORD now;
	if (!ReadCurrentFrame(now))
	{
		m_EndFrame = 0;
		return;
	}
	const DWORD latest = now + frames;
	if (m_EndFrame > latest) m_EndFrame = latest;
}

void
MEffect::SetDelayFrame(DWORD frame)
{
	DWORD now;
	ReadCurrentFrame(now);
	EffectTiming::SetDelayFrame(now, frame);
}

bool
MEffect::IsDelayFrame() const
{
	DWORD now;
	return ReadCurrentFrame(now) && EffectTiming::IsDelayFrame(now);
}

void
MEffect::SetWaitFrame(DWORD frame)
{
	DWORD now;
	ReadCurrentFrame(now);
	EffectTiming::SetWaitFrame(now, frame);
}

bool
MEffect::IsWaitFrame() const
{
	DWORD now;
	return ReadCurrentFrame(now) && EffectTiming::IsWaitFrame(now);
}

//----------------------------------------------------------------------
// SetResourceContainer - 设置资源容器（新增）
//----------------------------------------------------------------------
void
MEffect::SetResourceContainer(EffectResourceContainer* resources)
{
	m_pResources = resources;
}
