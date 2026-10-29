/*-----------------------------------------------------------------------------

	VS_UI_util.h

	Utility methods.

	2000.2.2. KJTINC

-----------------------------------------------------------------------------*/

#ifndef __VS_UI_UTIL_H__
#define __VS_UI_UTIL_H__

#include <limits.h>
#include "SP.h"
#include "framelib/FR.h"
//#include "rbt.h"
#include "Basics.h"
#include "MonotonicClock.h"
#include "SpriteLib/CSpritePack.h"
#include "SpriteLib/CIndexSpritePack.h"

typedef WORD SPRITE_ID;

// The filled length of a bar `length` long showing `value` of `maximum`.
// Both are the server's (HP, MP, experience past the table's last level):
// the product is 64 bits, and a maximum of 0 or below fills nothing where
// the division by it trapped.
inline int BarFill(int length, long long value, long long maximum)
{
	if (maximum <= 0 || value <= 0)
		return 0;
	if (value >= maximum)
		return length;
	return int(length * value / maximum);
}

// sprite id가 아니다.
#define NO_SPRITE_ID						ULONG_MAX

// data id <-> spk index
//class C_CONVERT_TABLE
//{
//private:
//	struct UNIT
//	{
//		data_id_t	id;
//		SPRITE_ID	index;
//	};
//
//	UNIT *		m_p_unit_list;
//	int			m_size;
//
//public:
//	C_CONVERT_TABLE();
//	~C_CONVERT_TABLE();
//
//	void			Add(data_id_t id, SPRITE_ID index);
//	SPRITE_ID	Index(data_id_t id) const;
//};

/*-----------------------------------------------------------------------------
  Class Sprite Pack
  `DX가 초기화되어서 555, 565 여부를 판별하므로 바드시 DX를 초기화시킨 후에 
   생성(정의)해야 한다.
-----------------------------------------------------------------------------*/
// RGB 중 어느 하나.
#define rgb_RED							0
#define rgb_GREEN							1
#define rgb_BLUE							2

class C_SPRITE_PACK
{
private:
	//CSpritePackList *	m_pC_spk_list; // 565, 555를 결정해야하기 때문에 pointer로.
	CSpritePack			m_SPK;	// by sigi

	// Disable copy constructor and copy assignment to prevent issues with m_file pointer
	C_SPRITE_PACK(const C_SPRITE_PACK&) = delete;
	C_SPRITE_PACK& operator=(const C_SPRITE_PACK&) = delete;

	// Disable move constructor and move assignment to prevent m_file pointer from being moved
	C_SPRITE_PACK(C_SPRITE_PACK&&) = delete;
	C_SPRITE_PACK& operator=(C_SPRITE_PACK&&) = delete;

public:
	C_SPRITE_PACK(const char *sz_filename = NULL);
	~C_SPRITE_PACK();

	void	BltClip(int x, int y, Rect &rect, SPRITE_ID sprite_id=0);

	int		GetWidth(SPRITE_ID sprite_id = 0);
	int		GetHeight(SPRITE_ID sprite_id = 0);
	void	Open(const char *sz_filename);
	void	BltOutline(int x, int y, int color, SPRITE_ID sprite_id = 0);
	void	Blt(POINT &point, SPRITE_ID sprite_id=0);
	void	Blt(int x=0, int y=0, SPRITE_ID sprite_id=0);
	void	BltColor(int x, int y, SPRITE_ID sprite_id, int rgb);
	void	BltColor(POINT &point, SPRITE_ID sprite_id, int rgb);
	void	BltDarkness(int x, int y, SPRITE_ID sprite_id, int dark);
	void	BltDarkness(POINT &point, SPRITE_ID sprite_id, int dark);
	void	BltAlpha(POINT &point, SPRITE_ID sprite_id, int alpha);
	void	BltAlpha(int x, int y, SPRITE_ID sprite_id, int alpha);
	void	BltOffscreen(POINT &point, SPRITE_ID sprite_id = 0);
	void	BltOffscreen(int x=0, int y=0, SPRITE_ID sprite_id = 0);

	// 이미 Surface가 Lock된 상태에서 부르는 함수..
	void	BltLockedClip(int x, int y, Rect &rect, SPRITE_ID sprite_id=0);
	void	BltLockedOutline(int x, int y, int color, SPRITE_ID sprite_id = 0);
	void	BltLocked(POINT &point, SPRITE_ID sprite_id=0);
	void	BltLocked(int x=0, int y=0, SPRITE_ID sprite_id=0);
	void	BltLockedColor(int x, int y, SPRITE_ID sprite_id, int rgb);
	void	BltLockedDarkness(int x, int y, SPRITE_ID sprite_id, int dark);
	void	BltLockedAlpha(POINT &point, SPRITE_ID sprite_id, int alpha);
	void	BltLockedAlpha(int x, int y, SPRITE_ID sprite_id, int alpha);
	void	BltLockedOffscreen(POINT &point, SPRITE_ID sprite_id = 0);
	void	BltLockedOffscreen(int x=0, int y=0, SPRITE_ID sprite_id = 0);
	void	BltLockedColorSet(int x, int y, SPRITE_ID sprite_id, WORD colorset);
	void	BltLockedColorSet(POINT &point, SPRITE_ID sprite_id, WORD colorset);

	bool	IsPixel(int x, int y, SPRITE_ID sprite_id = 0);

	int		GetSize()	{ return m_SPK.GetSize(); }
	CSprite &GetSprite(SPRITE_ID id) { return m_SPK[id]; }
};

/*-----------------------------------------------------------------------------
  Class FRR
  `객체를 Animation시켜주기 위한 Frame array object.
-----------------------------------------------------------------------------*/
class C_FRR
{
private:
	FRAME_ARRAY 	m_C_frame_array;

public:
	C_FRR(const char *sz_filename = NULL);
	~C_FRR();

	bool	Open(const char *sz_filename);
	int	Size() const;
	int	GetRX(int frame) const;
	int	GetRY(int frame) const;
};

/*-----------------------------------------------------------------------------
  Class animation object
  `animation할 object. 이것은 한 쌍의 SPK, FRR 이다. 깥은 SPK, FRR을 출력위치만
   다르게 할 경우, 그리고 독립된 Timer를 사용할 경우가 있기 때문에 만들었다. 
	이것을 C_ANIMATION에 넣으면 animation된다.
-----------------------------------------------------------------------------*/
class C_ANI_OBJECT
{
private:
	//
	// m_pC_spk
	//
	// m_pC_frr과 대응하는 SPK이다. 하나의 Animation object는 하나씩의 SPK, FRR을
	// 갖는다. 하나의 SPK에 다수의 FRR이 대응할 수 없다. 이렇게 하는 이유는, 그러한
	// 쓰임새는 Animation object의 일 경우 드믈거나 없고, 또한 혼란(?)을 방지하며,
	// 단순한 Interface를 제공하기 위함이다. 물론 그럴 필요가 추후에 생긴다면,
	// 또다른 Class를 만들면 그만이다.
	//
	C_SPRITE_PACK *		m_pC_spk;
	C_FRR *					m_pC_frr;

public:
	C_ANI_OBJECT(const char *sz_spkfile, const char *sz_frrfile);
	~C_ANI_OBJECT();

	int	FrrSize() const;
	int	FrrRX(int index) const;
	int	FrrRY(int index) const;
	void	Blt(POINT &point, SPRITE_ID sprite_id);
	void	BltColor(POINT &point, SPRITE_ID sprite_id, int rgb);
	void	BltDarkness(POINT &point, SPRITE_ID sprite_id, int dark_bit);
	int	GetWidth(SPRITE_ID sprite_id) const;
	int	GetHeight(SPRITE_ID sprite_id) const;
};

/*-----------------------------------------------------------------------------
  Class animation object
  `Object를 Animation시키는 object.

  `SPK file과 Frr file은 animation을 하기 위한 필요조건이다. Animation Object는
   이 두개의 정보를 참조하여, 임의 위치에 play, 곧 animation시켜준다. 물론
	Timer를 내장하고 Timer를 설정할 수 있다. 또한 역으로 animation할 수도 있다.
-----------------------------------------------------------------------------*/
class C_ANIMATION
{
private:
	//
	// m_pC_ani_object
	//
	// 1. Animation할 object pointer이다. 이 object는 다른 Animation object에서도
	//    사용할 수 있기 때문에 pointer로 해야 효율적이다.
	//
	// 2. 하나의 Animation object에서는 하나의 ani object를 갖는다. 그러나 그 역은
	//    반드시 그러하지 않아도 된다.
	//
	C_ANI_OBJECT *			m_pC_ani_object;

	//
	// Timer
	//
	// Each animation object plays on a timer of its own, kept inside the
	// object: the timer library needs a global callback, so it cannot be
	// used here.
	//
	MonotonicClock::IntervalTimer	m_interval_timer;

	UINT						m_current_frame;
	int						m_x, m_y;

	bool						m_bl_reverse;

public:
	enum PLAY_ORDER
	{
		STOP,					// stop
		PLAY,					// 0번 frame부터 끝까지 한 번만.
		PLAY_LOOP,			// play를 계속 반복.
		PLAY_BACK,			// 끝 frame부터 0번까지 한 번만.
		PLAY_BACKLOOP,		// play back 계속 반복.
		PLAY_LOOPBACK,		// play -> back -> play (반복)
	};

private:
	PLAY_ORDER				m_play_order;
	PLAY_ORDER				m_play_order_next; // play order가 끝나고 해주는 것.

public:
	C_ANIMATION(C_ANI_OBJECT *p_object);
	~C_ANIMATION();

	PLAY_ORDER	GetAnimationState() const;
	void	SetSpeed(DWORD millisec);
	void	SetPlayPosition(int x, int y);
	void	Play();
	void	Stop();
	void	PlayLoopBack();
	void	PlayBack();
	void	PlayLoop();
	void	Show(bool enable = true);
	void	SetNextPlayOrder(PLAY_ORDER order);
	void	RunNextPlayOrder();
	PLAY_ORDER GetNextPlayOrder() const;
	void	Refresh();

	void	Timer();
};

//----------------------------------------------------------------------------
// Class SpriteFrame
//----------------------------------------------------------------------------
class C_SPRITE_FRAME
{
private:
	int		m_rx;
	int		m_ry;

public:
	C_SPRITE_FRAME();
	~C_SPRITE_FRAME();

//----------------------------------------------------------------------------
// Open/Save
//----------------------------------------------------------------------------
	bool	Open(const char * pathfile);
	bool	Save(const char * pathfile);

//----------------------------------------------------------------------------
// Get
//----------------------------------------------------------------------------
	int	GetRX() const { return m_rx; }
	int	GetRY() const { return m_ry; }

//----------------------------------------------------------------------------
// Set
//----------------------------------------------------------------------------
	void	SetRX(int rx) { m_rx = rx; }
	void	SetRY(int ry) { m_ry = ry; }
};

//----------------------------------------------------------------------------
// Class SpriteFrame Array
//----------------------------------------------------------------------------
class C_SPRITE_FRAME_ARRAY
{
private:
	int						m_size;
	C_SPRITE_FRAME *		m_pC_sprite_frame_buf;

public:
	C_SPRITE_FRAME_ARRAY();
	~C_SPRITE_FRAME_ARRAY();

	void	AddSpriteFrame(C_SPRITE_FRAME * pC_sf);
	int	GetRX(int index);
	int	GetRY(int index);
};

//----------------------------------------------------------------------------
// Public
//----------------------------------------------------------------------------
//void Read_rbt(const char * rbt_file, CSpritePackList * p_spk_list, C_CONVERT_TABLE * p_ct);
//void Read_rbt(const char * rbt_file, C_SPRITE_FRAME_ARRAY * p_sf_array, C_CONVERT_TABLE * p_ct);

#endif