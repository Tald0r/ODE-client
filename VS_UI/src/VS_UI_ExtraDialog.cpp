// VS_UI_ExtraDialog.cpp

#include "Client_PCH.h"
#include <cstring>
#undef assert
#define assert(e) ((void)(e))
// Disabled assert for macOS

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "VS_UI_ExtraDialog.h"
#include "VS_UI_filepath.h"
#include "VS_UI_mouse_pointer.h"

#include "MMoneyManager.h"
#include "MItemOptionTable.h"
#include "VS_UI.h"
#include "MPriceManager.h"
#include "MCreatureTable.h"
#include "UtilityFunction.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "ClientConfig.h"
#include "MNPCTable.h"
#include "TextSystem/TextService.h"
#include "TextSystem/RenderTargetSpriteSurface.h"
extern RECT g_GameRect;
#include "DirectoryListing.h"
#include "FileDialogListing.h"
#include "DescriptorText.h"
#include "TextWrap.h"
#include <algorithm>
#include <filesystem>
#include <vector>
#include "MonotonicClock.h"

//-----------------------------------------------------------------------------
// AskString
//
// Use GetGameString for the dialog message arrays because it answers ""
// for an id the table does not hold, where
// the subscript answers a default MString whose GetString() is NULL, and
// these rows are handed to strlen().
//-----------------------------------------------------------------------------
static const char* AskString(int nStringID)
{
	return GetGameString(nStringID);
}

Window* g_desc_dialog_window_id = NULL;
/*

char * C_VS_UI_EDIT_DIALOG::m_sz_question_msg[1] = {"이 아이템            개를 사시겠습니까?"};

// 주석처리 by sigi
//char * C_VS_UI_MONEY_DIALOG::m_sz_question_msg[1] = {"버릴 금액을 입력하세요."};
//char * C_VS_UI_MONEY_DIALOG::m_sz_question_msg_for_storage[1] = {"보관할 금액을 입력하세요."};

char * C_VS_UI_MONEY_DIALOG::m_sz_question_msg[MAX_MONEY_DIALOG_TYPE][1] = {	// by sigi
	{  "버릴 금액을 입력하세요." } ,
	{  "보관할 금액을 입력하세요." } ,
	{  "찾을 금액을 입력하세요." } ,
	{  "거래할 금액을 입력하세요." } ,
	{  "빼낼 금액을 입력하세요." } ,
};

char * C_VS_UI_ASK_DIALOG::m_sz_question_msg[MAX_ASK_DIALOG_TYPE][2] = {	// by sigi
	{  "보관함을 $%d에 사시겠습니까?"  },	
	{  "%s 님과 거래하겠습니까?"  },
	{  "%s 님의 응답을 기다립니다." ,  "취소하시려면 Cancel을 누르세요."   },
//	{  "선택하신 무기는 %s입니다.", "결정하시겠습니까?" },					// by larosel
	{  "%s 님이 당신의 %s에 가입을 요청하고 있습니다."  },
	{  "%s 님이 당신을 %s에 초청하셨습니다."  },
	{  "%s 님의 응답을 기다립니다.",   "취소하시려면 Cancel을 누르세요."  },
	{  "검색 결과를 찾을 수 없습니다."  },
	{  "이 아이템을 Enchant하시겠습니까?",   "취소하시려면 Cancel을 누르세요."  },
};
*/
//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::C_VS_UI_EDIT_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_EDIT_DIALOG::C_VS_UI_EDIT_DIALOG(int _x, int _y, int center_x, int center_y, void (*exec_fp)(C_VS_UI_DIALOG *, id_t), WORD dd_button, int cur_val, int max_val) :
							C_VS_UI_DIALOG(_x, _y, center_x, center_y, exec_fp, dd_button)
{
	m_sz_question_msg[0] = AskString(UI_STRING_MESSAGE_BUY_ITEM_NUM);
	SetMessage(m_sz_question_msg, 1, SMO_NOFIT);

//	if (digit_count < 0)
//		digit_count = 0;
	if(max_val < 0)
		max_val = 0;

	m_max_val = max_val;
	m_lev_value.SetDigitOnlyMode(true);
	int digit_count, number;
	for(digit_count = 0, number = max_val; number > 0; number/=10, digit_count++);
	m_lev_value.SetByteLimit(digit_count);
	Attach(&m_lev_value);
	m_default_val = cur_val;

	m_p_image_spk = new C_SPRITE_PACK(SPK_SLAYER_BUY_DIALOG);

	m_buy_dialog_pt.x = m_client_rect.x + 90;
	m_buy_dialog_pt.y = m_client_rect.y + 13;

	// set scroll button rect
	m_scroll_button_up_rt.Set(m_buy_dialog_pt.x+SCROLL_BUTTON_OFFSET_X, m_buy_dialog_pt.y+SCROLL_BUTTON1_OFFSET_Y, SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT);
	m_scroll_button_down_rt.Set(m_buy_dialog_pt.x+SCROLL_BUTTON_OFFSET_X, m_buy_dialog_pt.y+SCROLL_BUTTON2_OFFSET_Y, SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT);

	m_lev_value.SetPosition(m_buy_dialog_pt.x+20-7*(digit_count-1), m_buy_dialog_pt.y+5);
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::~C_VS_UI_EDIT_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_EDIT_DIALOG::~C_VS_UI_EDIT_DIALOG()
{
	m_lev_value.Unacquire();

	DeleteNew(m_p_image_spk);
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::GetValue
//
// 
//-----------------------------------------------------------------------------
int	C_VS_UI_EDIT_DIALOG::GetValue() const
{
	int cur_val;
	char * p_temp = NULL;
	g_Convert_DBCS_Ascii2SingleByte(m_lev_value.GetStringWide().c_str(), m_lev_value.Size(), p_temp);
	if (p_temp == NULL)
	{
		// string이 없다.
		cur_val = m_default_val;
	}
	else
	{
		cur_val = atoi(p_temp);
	}

	DeleteNewArray(p_temp);

	return cur_val;
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::Start
//
// 
//-----------------------------------------------------------------------------
void C_VS_UI_EDIT_DIALOG::Start()
{
	C_VS_UI_DIALOG::Start();

	m_lev_value.Acquire();
	AttrKeyboardControl(true);

	m_bl_up_pushed = false;
	m_bl_down_pushed = false;
	m_bl_up_focused = false;
	m_bl_down_focused = false;

	char sz_temp[10];	// by sigi (원래는 2였당 - -;)
	SafeFormat::Format(sz_temp, "%d", m_default_val);
	m_lev_value.AddString(sz_temp);

//	m_lev_value.AddString("1");
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::MouseControl
//
// 
//-----------------------------------------------------------------------------
bool C_VS_UI_EDIT_DIALOG::MouseControl(UINT message, int _x, int _y)
{
	C_VS_UI_DIALOG::MouseControl(message, _x, _y);

	switch (message)
	{
		case M_MOVING:
			m_bl_up_focused = false;
			m_bl_down_focused = false;
			if (m_scroll_button_up_rt.IsInRect(_x, _y))
				m_bl_up_focused = true;

			if (m_scroll_button_down_rt.IsInRect(_x, _y))
				m_bl_down_focused = true;
			break;

		case M_LEFTBUTTON_DOWN:
		case M_LB_DOUBLECLICK:
			if (m_bl_up_focused)
			{
				m_bl_up_pushed = true;
				ScrollButton(true);
			}
			if (m_bl_down_focused)
			{
				m_bl_down_pushed = true;
				ScrollButton(false);
			}
			break;

		case M_LEFTBUTTON_UP:
			m_bl_up_pushed = false;
			m_bl_down_pushed = false;
			break;
	
	}

	return true;
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::Show
//
// 
//-----------------------------------------------------------------------------
void C_VS_UI_EDIT_DIALOG::Show()
{
	C_VS_UI_DIALOG::Show();

	if(gpC_base->m_p_DDSurface_back->Lock())
	{
		m_p_image_spk->BltLocked(m_buy_dialog_pt.x, m_buy_dialog_pt.y, BUY_DIALOG);
		
		int vampire_plus = 6;

		switch(g_eRaceInterface)
		{
		case RACE_SLAYER:
			vampire_plus = 0;
			break;
		default:
			break;
		}		
		
		if (m_bl_up_focused)
		{
			if(m_bl_up_pushed)
				m_p_image_spk->BltLocked(m_scroll_button_up_rt.x, m_scroll_button_up_rt.y, SLAYER_BUTTON_UP_HILIGHTED_PUSHED+vampire_plus);
			else
				m_p_image_spk->BltLocked(m_scroll_button_up_rt.x, m_scroll_button_up_rt.y, SLAYER_BUTTON_UP_HILIGHTED+vampire_plus);
		}
		else
			m_p_image_spk->BltLocked(m_scroll_button_up_rt.x, m_scroll_button_up_rt.y, SLAYER_BUTTON_UP+vampire_plus);
		
		if (m_bl_down_focused)
		{
			if(m_bl_down_pushed)
				m_p_image_spk->BltLocked(m_scroll_button_down_rt.x, m_scroll_button_down_rt.y, SLAYER_BUTTON_DOWN_HILIGHTED_PUSHED+vampire_plus);
			else
				m_p_image_spk->BltLocked(m_scroll_button_down_rt.x, m_scroll_button_down_rt.y, SLAYER_BUTTON_DOWN_HILIGHTED+vampire_plus);
		}
		else
			m_p_image_spk->BltLocked(m_scroll_button_down_rt.x, m_scroll_button_down_rt.y, SLAYER_BUTTON_DOWN+vampire_plus);
		gpC_base->m_p_DDSurface_back->Unlock();
	}

	Window::ShowWidget();
/*
#ifndef _LIB
	if (gpC_base->m_p_DDSurface_back->Lock())
	{
		S_SURFACEINFO	surfaceinfo;
		SetSurfaceInfo(&surfaceinfo, gpC_base->m_p_DDSurface_back->GetDDSD());

		rectangle(&surfaceinfo, &m_scroll_button_up_rt, WHITE);
		rectangle(&surfaceinfo, &m_scroll_button_down_rt, BLUE);

		gpC_base->m_p_DDSurface_back->Unlock();
	}
#endif*/
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::ScrollButton
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_EDIT_DIALOG::ScrollButton(bool up)
{
	const int _MAX_NUM = m_max_val;

	int cur_val;
	char * p_temp = NULL;
	g_Convert_DBCS_Ascii2SingleByte(m_lev_value.GetStringWide().c_str(), m_lev_value.Size(), p_temp);
	if (p_temp == NULL)
	{
		// string이 없다. backspace하고 했을 때.
		cur_val = m_default_val;
	}
	else
	{
		cur_val = atoi(p_temp);
		m_lev_value.EraseAll();

		if (up)
		{
			if (cur_val < _MAX_NUM)
				cur_val++;
		}
		else
		{
			if (cur_val > 1)
				cur_val--;
		}
	}

	char sz_new_str[50];
	SafeFormat::Format(sz_new_str, "%d", cur_val);
	m_lev_value.AddString(sz_new_str);

	DeleteNewArray(p_temp);
}

//-----------------------------------------------------------------------------
// C_VS_UI_EDIT_DIALOG::KeyboardControl
//
// 바로 숫자 입력할 수 있게 한다.
//-----------------------------------------------------------------------------
void	C_VS_UI_EDIT_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{
	char num[100] = {0,};
//	if (message == WM_CHAR)
//		if ((char)key >= '1' && (char)key <= '9')
//		{
//			if(m_max_val < 10)m_lev_value.EraseAll();
//			num[0] = (char)key;
//			m_lev_value.AddString(num);
//		}
	int digit_count, number;
	for(digit_count = 0, number = m_max_val; number > 0; number/=10, digit_count++);
	if (message == WM_CHAR)
		if ((char)key >= '0' && (char)key <= '9')
		{
			if(digit_count <= m_lev_value.Size())m_lev_value.EraseCharacterBegin();
//			num[0] = (char)key;
//			m_lev_value.AddString(num);
		}

	m_lev_value.KeyboardControl(message, key, extra);

	for(digit_count = 0, number = GetValue(); number > 0; number/=10, digit_count++);
	while(digit_count < m_lev_value.Size())m_lev_value.EraseCharacterBegin();
	

	if(GetValue() > m_max_val)
	{
		m_lev_value.EraseAll();
		SafeFormat::Format(num, "%d", m_max_val);
		m_lev_value.AddString(num);
	}
	if (message == WM_KEYDOWN)
		switch (key)
		{
			case VK_ESCAPE:
			case VK_RETURN:
				C_VS_UI_DIALOG::KeyboardControl(message, key, extra);
				break;
		}
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::Start
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_MONEY_DIALOG::Start()
{
	C_VS_UI_DIALOG::Start();

	m_lev_value.Acquire();
	AttrKeyboardControl(true);

	m_bPrintMessage = false;
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::KeyboardControl
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_MONEY_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{
	if (message == WM_CHAR)
		if ((char)key == '0')
			if (m_lev_value.Size() == 0)
				return;

	C_VS_UI_DIALOG::KeyboardControl(message, key, extra);

	int _MAX_MONEY = 0;

	// by sigi
	switch (m_type)
	{
		// 보관함에 넣기
		case MONEY_DEPOSIT :
			// 현재 가지고 있는 만큼만 넣을 수 있고 보관함에 돈의 limit이상은 넣을수 없다.
			_MAX_MONEY =  min(g_pMoneyManager->GetMoney(), g_pStorage->GetMoneyManager()->GetMaxAddMoney());
			break;

		case MONEY_EXCHANGE:
			_MAX_MONEY = min(g_pMoneyManager->GetMoney(), g_pTradeManager->GetOtherMoneyManager()->GetMaxAddMoney());
			break;

		// 버리기
		case MONEY_DROP : 
				_MAX_MONEY = g_pMoneyManager->GetMoney();	
			break;
		
		// 보관함에서 꺼내기
		case MONEY_WITHDRAW : 
			// 보관함에 있는 만큼만 찾을 수 있고 인벤토리에 돈의 limit이상은 넣을수 없다.
			_MAX_MONEY = min(g_pStorage->GetMoneyManager()->GetMoney(), g_pMoneyManager->GetMaxAddMoney());
			break;

		case MONEY_WITHDRAW_FROM_EXCHANGE:
			_MAX_MONEY = g_pTradeManager->GetMyMoneyManager()->GetMoney();
			break;
		case MONEY_BRING_FEE :			// 총 세금에서 가지고 있는 돈을 빼야 20억 안넘는다...
			_MAX_MONEY = min(gC_vs_ui.GetTotalFee(),g_pMoneyManager->GetMaxAddMoney());
			break;
		case MODIFY_TAX:
			_MAX_MONEY = 10; // 세율은 10 까지만
			break;
		case MONEY_SELL_ITEM:
			_MAX_MONEY = 2000000000;
			break;
		case MONEY_CAMPAIGN_HELP:
			_MAX_MONEY = g_pMoneyManager->GetMoney()/10000;
			break;
		default:
			break;
	}

	if (GetValue() > _MAX_MONEY)
	{
		char buf[30];
		SafeFormat::Format(buf, "%d", _MAX_MONEY);
		m_lev_value.EraseAll();
		m_lev_value.AddString(buf);

		if(m_bPrintMessage == false)
		{
			m_bPrintMessage = true;
			switch(m_type)
			{
			case MONEY_DEPOSIT:
				gC_vs_ui.RunDepositLimit();
				break;

			case MONEY_WITHDRAW:
				gC_vs_ui.RunWithdrawLimit();
				[[fallthrough]];
			case MONEY_BRING_FEE :
				gC_vs_ui.RunBringFeeLimit();
				break;
			default:
				break;
			}
			
		}
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::Show
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_MONEY_DIALOG::Show()
{
	C_VS_UI_DIALOG::Show();

	gpC_global_resource->m_pC_assemble_box_button_spk->Blt(m_money_dialog_pt.x, m_money_dialog_pt.y, C_GLOBAL_RESOURCE::AB_MONEY_BAR);
//	m_p_image_spk->Blt(m_money_dialog_pt.x, m_money_dialog_pt.y, MONEY_DIALOG);

	if(m_type == MONEY_CAMPAIGN_HELP)
	{
		if(g_FL2_GetDC())
		{		
			switch(g_eRaceInterface)
			{
			case RACE_SLAYER:
				g_PrintColorStr(x+185, y+64, (*g_pGameStringTable)[UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_SLAYER].GetString(), gpC_base->m_item_name_pi, RGB_WHITE);
				break;

			case RACE_VAMPIRE:
				g_PrintColorStr(x+185, y+64, (*g_pGameStringTable)[UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_VAMPIRE].GetString(), gpC_base->m_item_name_pi, RGB_WHITE);
				break;

			case RACE_OUSTERS:
				g_PrintColorStr(x+185, y+64, (*g_pGameStringTable)[UI_STRING_MESSAGE_CAMPAIGN_HELP_UNITS_OUSTERS].GetString(), gpC_base->m_item_name_pi, RGB_WHITE);
				break;		
			default:
				break;
			}
			g_FL2_ReleaseDC();
		}
		int TempValue = GetValue();
		if(TempValue>0)
		{
			Point TempPoint = m_lev_value.GetPosition();
			char TempBuffer[64];
			SafeFormat::Format(TempBuffer, "%d0000",TempValue);
			g_PrintColorStr(TempPoint.x, TempPoint.y, TempBuffer, gpC_base->m_item_name_pi, RGB_WHITE);
		}
		else
			Window::ShowWidget();
	}
	else
		Window::ShowWidget();
/*
#ifndef _LIB
	if (gpC_base->m_p_DDSurface_back->Lock())
	{
		S_SURFACEINFO	surfaceinfo;
		SetSurfaceInfo(&surfaceinfo, gpC_base->m_p_DDSurface_back->GetDDSD());

		rectangle(&surfaceinfo, &m_scroll_button_up_rt, WHITE);
		rectangle(&surfaceinfo, &m_scroll_button_down_rt, BLUE);

		gpC_base->m_p_DDSurface_back->Unlock();
	}
#endif*/
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::GetValue
//
// 
//-----------------------------------------------------------------------------
int	C_VS_UI_MONEY_DIALOG::GetValue() const
{
	__int64 cur_val;
	char * p_temp = NULL;
	g_Convert_DBCS_Ascii2SingleByte(m_lev_value.GetStringWide().c_str(), m_lev_value.Size(), p_temp);
	if (p_temp == NULL)
	{
		// string이 없다.
		cur_val = 0;
	}
	else
	{
		cur_val = _atoi64(p_temp);
	}

	if (cur_val > 0x7FFFFFFF)
	{
		cur_val = 0x7FFFFFFF;
	}

	DeleteNewArray(p_temp);

	return cur_val;	
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::C_VS_UI_MONEY_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_MONEY_DIALOG::C_VS_UI_MONEY_DIALOG(int _x, int _y, int center_x, int center_y, void (*exec_fp)(C_VS_UI_DIALOG *, id_t), WORD dd_button, int digit_count, TYPE_MONEY_DIALOG type) :	// type 추가 by sigi
							C_VS_UI_DIALOG(_x, _y, center_x, center_y, exec_fp, dd_button)
{
	/*	// 주석처리 by sigi
	if (!gC_vs_ui.IsRunningStorage())
		SetMessage(m_sz_question_msg, 1);//, SMO_NOFIT);
	else
		SetMessage(m_sz_question_msg_for_storage, 1);//, SMO_NOFIT);
	*/
	m_sz_question_msg[0][0]=AskString(UI_STRING_MESSAGE_TRHOW_MONEY_IN_DIALOG);
	m_sz_question_msg[1][0]=AskString(UI_STRING_MESSAGE_SAVE_MONEY_IN_DIALOG);
	m_sz_question_msg[2][0]=AskString(UI_STRING_MESSAGE_BRING_MONEY_IN_DIALOG);
	m_sz_question_msg[3][0]=AskString(UI_STRING_MESSAGE_TRADE_MONEY_IN_DIALOG);
	m_sz_question_msg[4][0]=AskString(UI_STRING_MESSAGE_DIVIDE_MONEY_IN_DIALOG);	
	m_sz_question_msg[5][0]=AskString(UI_STRING_MESSAGE_HOLY_LAND_INPUT_BRING_FEE);	
	m_sz_question_msg[6][0]=AskString(UI_STRING_MESSAGE_MODIFY_TAX);	
	m_sz_question_msg[7][0]=AskString(UI_STRING_MESSAGE_SELL_MONEY_IN_DIALOG);	
	m_sz_question_msg[8][0]=AskString(UI_STRING_MESSAGE_CAMPAIGN_HELP_REQUEST);	
	

	m_type = type;										

	SetMessage(m_sz_question_msg[type], 1);//, SMO_NOFIT);		// by sigi

	if (digit_count < 0)
		digit_count = 0;

	m_lev_value.SetDigitOnlyMode(true);
	m_lev_value.SetByteLimit(digit_count);
	Attach(&m_lev_value);

	//	m_p_image_spk = new C_SPRITE_PACK(SPK_MONEY_DIALOG);

	m_money_dialog_pt.x = m_client_rect.x + 0;
	m_money_dialog_pt.y = m_client_rect.y + 25;

	m_lev_value.SetPosition(m_money_dialog_pt.x+5, m_money_dialog_pt.y+3);
}

//-----------------------------------------------------------------------------
// C_VS_UI_MONEY_DIALOG::~C_VS_UI_MONEY_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_MONEY_DIALOG::~C_VS_UI_MONEY_DIALOG()
{
//	DeleteNew(m_p_image_spk);
}


//-----------------------------------------------------------------------------
// AllocAskMessage
//
// Allocates one row of an ask dialog's message and fills it through the
// checked formatter.
//
// Every row is formatted with a String.inf entry as the *format*
// argument (docs/code-health-review-2026-08-29.md finding C19) into a
// block sized from that same entry, and twelve of the call sites below
// pass no arguments at all - so a %s an operator left in an entry made
// the CRT read a stack word as a char* and copy it, unbounded, into a
// buffer that had no room for it. Ratchet R7 never saw any of this,
// because the format is not spelled at the call site: it is read out of
// m_sz_question_msg, which InitString() fills from the table.
//
// Allocating and formatting in one place is what stops the bound and the
// destination from drifting apart, which is the mistake the hand-written
// pairs kept making. nExtra is the room over strlen(pFormat) that the
// arguments need, carried over unchanged from each site's own
// arithmetic; SafeFormat refuses a conversion it has no argument for and
// copies the specification out as text, which can never be longer than
// the format it came from, so the format's own length is always a floor.
//-----------------------------------------------------------------------------
template <typename ...Args>
static char* AllocAskMessage(const char* pFormat, size_t nExtra, Args... args)
{
	if (pFormat == NULL)
	{
		pFormat = "";
	}

	// At least one byte over the format, for the terminator.
	if (nExtra < 1)
	{
		nExtra = 1;
	}

	const size_t	nSize	= strlen(pFormat) + nExtra;
	char* const		pRow	= new char [nSize];

	SafeFormat::Format(pRow, nSize, pFormat, args...);

	return pRow;
}

void	C_VS_UI_ASK_DIALOG::InitString()
{
	/*	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_BUY_STORAGE] = "보관함을 $%d에 사시겠습니까?";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_TRADE_OTHER_PLAYER]= "%s 님과 거래하겠습니까?";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_WAIT_OTHER_PLAYER]= "%s 님의 응답을 기다립니다.";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_CANCEL]	="취소하시려면 Cancel을 누르세요.";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_REQUEST_JOIN]= "%s 님이 당신의 %s에 가입을 요청하고 있습니다.";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ASK_DIALOG_INVITE]= "%s 님이 당신을 %s에 초청하셨습니다.";
	(*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_FIND_RESULT]= "검색 결과를 찾을 수 없습니다." ;
	(*g_pGameStringTable)[UI_STRING_MESSAGE_ENCHANT_CONFIRM]= "이 아이템을 Enchant하시겠습니까?";

  {  "보관함을 $%d에 사시겠습니까?"  },	
	{  "%s 님과 거래하겠습니까?"  },
	{  "%s 님의 응답을 기다립니다." ,  "취소하시려면 Cancel을 누르세요."   },
	{  "%s 님이 당신의 %s에 가입을 요청하고 있습니다."  },
	{  "%s 님이 당신을 %s에 초청하셨습니다."  },
	{  "%s 님의 응답을 기다립니다.",   "취소하시려면 Cancel을 누르세요."  },
	{  "검색 결과를 찾을 수 없습니다."  },
	{  "이 아이템을 Enchant하시겠습니까?",   "취소하시려면 Cancel을 누르세요."  },
*/
	// Every row first, then the ones this dialog knows about. The rows
	// were left indeterminate before: upstream commented the six
	// ASK_FRIEND_* assignments out of this function but left the cases
	// that read them live, so opening a friend dialog ran strlen() over
	// an uninitialised pointer and handed it to sprintf as a format.
	// m_sz_question_msg is an ordinary member array, so nothing else
	// gives it a value.
	for (int nRow = 0; nRow < MAX_ASK_DIALOG_TYPE; ++nRow)
	{
		for (int nCol = 0; nCol < 5; ++nCol)
		{
			m_sz_question_msg[nRow][nCol] = "";
		}
	}

	// GetGameString rather than the table's operator[]: a subscript that
	// misses answers a default MString whose GetString() is NULL, and
	// these rows are handed to strlen().
	//m_sz_question_msg
	m_sz_question_msg[0][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_BUY_STORAGE);
	m_sz_question_msg[0][1]="";

	m_sz_question_msg[1][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_TRADE_OTHER_PLAYER);
	m_sz_question_msg[1][1]="";
	
	m_sz_question_msg[2][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_WAIT_OTHER_PLAYER);
	m_sz_question_msg[2][1]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_CANCEL);

	m_sz_question_msg[3][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_REQUEST_JOIN);
	m_sz_question_msg[3][1]="";

	m_sz_question_msg[4][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_INVITE);
	m_sz_question_msg[4][1]="";

	m_sz_question_msg[5][0]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_WAIT_OTHER_PLAYER);
	m_sz_question_msg[5][1]=AskString(UI_STRING_MESSAGE_ASK_DIALOG_CANCEL);

	m_sz_question_msg[6][0]=AskString(UI_STRING_MESSAGE_CANNOT_FIND_RESULT);
	m_sz_question_msg[6][1]="";

	m_sz_question_msg[7][0]=AskString(UI_STRING_MESSAGE_ENCHANT_CONFIRM);
	m_sz_question_msg[7][1]=AskString(UI_STRING_MESSAGE_ENCHANT_CONFIRM_2);
	m_sz_question_msg[7][2]=AskString(UI_STRING_MESSAGE_PET_RESSURECT);
	m_sz_question_msg[7][3]=AskString(UI_STRING_MESSAGE_REMOVE_PET_OPTION);
	m_sz_question_msg[7][4]=AskString(UI_STRING_MESSAGE_PET_MUTANT);
	
	m_sz_question_msg[8][0]=AskString(UI_STRING_MESSAGE_DEPOSIT_LIMIT);
	m_sz_question_msg[8][1]="";

	m_sz_question_msg[9][0]=AskString(UI_STRING_MESSAGE_WITHDRAW_LIMIT);
	m_sz_question_msg[9][1]="";

	m_sz_question_msg[10][0]=AskString(UI_STRING_MESSAGE_BRING_FEE_LIMIT);
	m_sz_question_msg[10][1]="";
	
	m_sz_question_msg[11][0]=AskString(UI_STRING_MESSAGE_TRANS_ITEM);
	m_sz_question_msg[11][1]="";

	m_sz_question_msg[12][0]=AskString(UI_STRING_MESSAGE_USE_PET_FOOD);
	m_sz_question_msg[12][1]=AskString(UI_STRING_MESSAGE_CANNOT_DETACH);
	
	m_sz_question_msg[13][0]=AskString(STRING_MESSAGE_KEEP_PETITEM);
	m_sz_question_msg[13][1]="";

	m_sz_question_msg[14][0]=AskString(STRING_MESSAGE_GET_KEEP_PETITEM);
	m_sz_question_msg[14][1]="";

	m_sz_question_msg[15][0]=AskString(UI_STRING_MESSAGE_USE_SMSITEM);
	m_sz_question_msg[15][1]="";

	m_sz_question_msg[16][0]=AskString(UI_STRING_MESSAGE_USE_NAMINGITEM);
	m_sz_question_msg[16][1]="";

	m_sz_question_msg[17][0]=AskString(UI_STRING_MESSAGE_REQUEST_GET_EVENT_ITEM);
	m_sz_question_msg[17][1]="";
	//-------------------------------------------------friend strings------------------------------------------------
	// These six were commented out upstream, ids and all, while the six
	// cases that read them stayed live and reachable from a
	// GCFriendChatting packet - so the rows below held whatever the
	// allocation left behind and the constructor ran strlen() over it.
	// Subscripted by the enum rather than by a literal, so a new type
	// inserted above them cannot silently move a row again.
	m_sz_question_msg[ASK_FRIEND_REQUEST][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_REQUEST);
	m_sz_question_msg[ASK_FRIEND_REFUSE][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_REFUSE);
	m_sz_question_msg[ASK_FRIEND_WAIT][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_WAIT);
	m_sz_question_msg[ASK_FRIEND_EXIST][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_EXIST);
	m_sz_question_msg[ASK_FRIEND_BLACK][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_BLACK);
	m_sz_question_msg[ASK_FRIEND_DELETE][0]=AskString(UI_STRING_MESSAGE_ASK_FRIEND_DELETE);
}
//-----------------------------------------------------------------------------
// C_VS_UI_ASK_DIALOG::C_VS_UI_ASK_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_ASK_DIALOG::C_VS_UI_ASK_DIALOG(int _x, int _y, int center_x, int center_y, void (*exec_fp)(C_VS_UI_DIALOG *, id_t), WORD dd_button, TYPE_ASK_DIALOG type, int value, void* pValue) :
							C_VS_UI_DIALOG(_x, _y, center_x, center_y, exec_fp, dd_button)
{
	m_sz_question_msg_temp[0] = NULL;	
	m_sz_question_msg_temp[1] = NULL;	

	m_type = type;										

	m_dwTemporayValue = value;
	m_pTemporayValue = pValue;

	InitString();
	const char *PartyName = GetGameString(UI_STRING_MESSAGE_DESC_PARTY_NAME);

	AttrKeyboardControl(true);

	switch (type)
	{	
		// 숫자가 하나 들어가는 경우.. - -;
		case ASK_STORAGE_BUY :
		{
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 20, value);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		}
		break;

		case ASK_EXCHANGE :
		{
			const char* pName = (const char*)m_pTemporayValue;

			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+5, pName);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		}
		break;

		case ASK_EXCHANGE_CANCEL :
		{
			const char* pName = (const char*)m_pTemporayValue;
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+5, pName);
			m_sz_question_msg_temp[1] = AllocAskMessage(m_sz_question_msg[type][1], 1);
			SetMessage(m_sz_question_msg_temp, 2, SMO_NOFIT);	
		}
		break;

//		case ASK_TUTORIAL_EXIT :	// by larosel
//		{
//			const char* pName = (const char*)m_pTemporayValue;
//
//			m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+12];
//			m_sz_question_msg_temp[1] = new char [strlen(m_sz_question_msg[type][1])+1];
//			sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0], pName);
//			sprintf(m_sz_question_msg_temp[1], m_sz_question_msg[type][1]);
//			SetMessage(m_sz_question_msg_temp, 2, SMO_NOFIT);	
//		}
//		break;

		case ASK_PARTY_REQUEST :
		{
			const char* pName = (const char*)m_pTemporayValue;

			// Both entries carry two %s ("%s is asking to join your %s."),
			// and the allocation this replaces budgeted for pName only.
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+strlen(PartyName)+5, pName, PartyName);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		}
		break;

		case ASK_PARTY_INVITE :
		{
			const char* pName = (const char*)m_pTemporayValue;

			// Both entries carry two %s ("%s is asking to join your %s."),
			// and the allocation this replaces budgeted for pName only.
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+strlen(PartyName)+5, pName, PartyName);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		}
		break;

		case ASK_PARTY_CANCEL :
		{
			const char* pName = (const char*)m_pTemporayValue;

			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+5, pName);
			m_sz_question_msg_temp[1] = AllocAskMessage(m_sz_question_msg[type][1], 1);
			SetMessage(m_sz_question_msg_temp, 2, SMO_NOFIT);	
		}
		break;

		case ASK_ENCHANT :
		{
			switch(value)
			{
			case 0:		// 일반 아이템 인첸트
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
				m_sz_question_msg_temp[1] = AllocAskMessage(m_sz_question_msg[type][1], 1);
				
				SetMessage(m_sz_question_msg_temp, 2, SMO_NOFIT);	
				break;
				
			case 1:		// 펫 인첸트
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
				
				SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
				break;
				
			case 2:		// 펫 부활
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][2], 1);
				SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
				break;

			case 3:		// 펫 퓨리타스
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][3], 1);
				SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
				break;

			case 4:		// 펫 변신
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][4], 1);
				SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
				break;


			}
		}
		break;

		case ASK_TRANS_ITEM :
		{
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT );
		}
		break;		

		case ASK_USE_PET_FOOD :
			{
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
				m_sz_question_msg_temp[1] = AllocAskMessage(m_sz_question_msg[type][1], 1);
				
				SetMessage(m_sz_question_msg_temp, 2, SMO_NOFIT );
			}
			break;		

		case ASK_KEEP_PETITEM:
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT );
			break;
			
		case ASK_GET_KEEP_PETITEM:
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT );
			break;

		case ASK_USE_SMSITEM:
			m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], 1);
			SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT );
			break;

			//add by viva--------------------------------------ASK_FRIEND_REQUEST-----------------------
		case ASK_FRIEND_REQUEST :
		case ASK_FRIEND_REFUSE:
		case ASK_FRIEND_WAIT:
		case ASK_FRIEND_EXIST:
		case ASK_FRIEND_BLACK:
		case ASK_FRIEND_DELETE:
			{
				const char* pName = (const char*)m_pTemporayValue;
				m_sz_question_msg_temp[0] = AllocAskMessage(m_sz_question_msg[type][0], strlen(pName)+5, pName);
				SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
				break;
			}
		//case ASK_FRIEND_REFUSE:
		//	{
		//		const char* pName = (const char*)m_pTemporayValue;
		//		m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+strlen(pName)+5];
		//		sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0], pName);
		//		SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		//		break;
		//	}
		//case ASK_FRIEND_WAIT:
		//	{
		//		const char* pName = (const char*)m_pTemporayValue;
		//		m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+strlen(pName)+5];
		//		sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0], pName);
		//		SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		//		break;
		//	}
		//case ASK_FRIEND_EXIST:
		//	{
		//		const char* pName = (const char*)m_pTemporayValue;
		//		m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+strlen(pName)+5];
		//		sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0], pName);
		//		SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		//		break;
		//	}
		//case ASK_FRIEND_BLACK:
		//	{
		//		const char* pName = (const char*)m_pTemporayValue;
		//		m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+strlen(pName)+5];
		//		sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0], pName);
		//		SetMessage(m_sz_question_msg_temp, 1, SMO_NOFIT);	
		//		break;
		//	}
		default :
			SetMessage(m_sz_question_msg[type], 1, SMO_NOFIT);	
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_ASK_DIALOG::~C_VS_UI_ASK_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_ASK_DIALOG::~C_VS_UI_ASK_DIALOG()
{
	if (m_sz_question_msg_temp[0]!=NULL)
	{
		delete [] m_sz_question_msg_temp[0];
		m_sz_question_msg_temp[0] = NULL;
	}
	if (m_sz_question_msg_temp[1]!=NULL)
	{
		delete [] m_sz_question_msg_temp[1];
		m_sz_question_msg_temp[1] = NULL;
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_ASK_DIALOG::Start
//
// 
//-----------------------------------------------------------------------------
void C_VS_UI_ASK_DIALOG::Start()
{
	C_VS_UI_DIALOG::Start();
}

//-----------------------------------------------------------------------------
// C_VS_UI_ASK_DIALOG::MouseControl
//
// 
//-----------------------------------------------------------------------------
bool C_VS_UI_ASK_DIALOG::MouseControl(UINT message, int _x, int _y)
{
	C_VS_UI_DIALOG::MouseControl(message, _x, _y);
	
	return true;
}

//-----------------------------------------------------------------------------
// C_VS_UI_ASK_DIALOG::KeyboardControl
//
// 바로 숫자 입력할 수 있게 한다.
//-----------------------------------------------------------------------------
void	C_VS_UI_ASK_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{
	if (message == WM_KEYDOWN)
		switch (key)
		{
			case VK_ESCAPE:
			case VK_RETURN:
				C_VS_UI_DIALOG::KeyboardControl(message, key, extra);
				break;
		}
}

//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::C_VS_UI_NPC_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_NPC_DIALOG::C_VS_UI_NPC_DIALOG(void (*exec_fp)(C_VS_UI_DIALOG *, id_t), int sprite_num, const char* pname, int Width, int Height, WORD dd_button, bool IsShowItem):
							C_VS_UI_DIALOG(-1, -1, Width, Height, exec_fp, dd_button)
{
	InitNpcDlg(sprite_num, pname, IsShowItem);
}
//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::C_VS_UI_NPC_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_NPC_DIALOG::C_VS_UI_NPC_DIALOG(void (*exec_fp)(C_VS_UI_DIALOG *, id_t), int sprite_num, const char* pname, WORD dd_button):
							C_VS_UI_DIALOG(-1, -1, 6, 3, exec_fp, dd_button)
{
	InitNpcDlg(sprite_num, pname);
}

//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::~C_VS_UI_NPC_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_NPC_DIALOG::~C_VS_UI_NPC_DIALOG()
{
}
void C_VS_UI_NPC_DIALOG::InitNpcDlg(int sprite_num, const char* pname , bool IsShowItem)
{
	if(pname != NULL && strlen(pname) > 1)
		m_name = pname; 
	else
		m_name = "";
	if(false == IsShowItem)
	{
		const int g_npc_size = 65;
		const int g_npc_id[g_npc_size] = 
		{
			157, 17, 16, 18, 26, 
			21, 97, 100, 24, 14,
			110, 108, 101, 95, 109,
			103, 27, 98, 15, 20,
			96, 11, 102, 23, 158,
			99, 25, 163, 162, 164,
			12, 13, 22, 159, 160,
			161, 19,
			93, 92, 94, 219, 220,
			253, 254, 255,//골레머*2, 레베카
			217,//바토리
			250,251,252,//헬기조종사
			296, //야마모토
			297, //데니스
			362, 363, 364, 365, 366, //라센성 4명+블라드테페즈
			302, 303, 311,//튜토리얼 추가
			368, //리스토어 NPC 브리콜라카스 클론
			370, //드로베타 옷팜 레이븐
			427, 428, 429, 430,	// 유료화 존 그리피스, 로빈벡스터, 머피, 카르멘버만
		};

		[[maybe_unused]] const int g_npc_num[g_npc_size] = 
		{
			1, 2, 3, 4, 5, 
			6, 7, 8, 9, 10,
			11, 12, 13, 14, 15,
			16, 17, 18, 19, 20,
			21, 22, 23, 24, 25,
			26, 27, 28, 29, 30,
			31, 32, 33, 34, 35,
			36, 37,
			38, 39, 40, 41, 42,
			48, 47, 49, //골레머*2, 레베카
			43,//바토리
			44, 44, 44,//헬기조종사
			45, //야마모토
			46, //데니스
			50, 51, 52, 53, 54,//라센성 4명+블라드테페즈
			45, 46, 45,
			39, //리스토어 NPC 브리콜라카스 클론
			35, //드로베타 옷팜 레이븐
			55, 56, 57, 58,	// 유료화 존 그리피스, 로빈벡스터, 머피, 카르멘버만
		};

		m_id = sprite_num;
		for(int i = 0; i < g_npc_size && g_npc_id[i] != sprite_num; i++);

		NPC_INFO* ni=(*g_pNPCTable).GetData((unsigned int)sprite_num);
		
		if(ni == NULL)
			m_iSprite = -1;
		else
			m_iSprite = ni->SpriteID;
		
	//	if(i == g_npc_size)
	//		m_iSprite = -1;
	//	else
	//		m_iSprite = // g_npc_num[i];	// + GUARD
		

		if(m_iSprite != -1)
		{
			m_p_NPCFace.Init( 2 );		// 임시로 1개만 loading.. - -;;

			bool re;
			re = m_p_NPCFace.LoadFromFileData( 0, 0, SPK_NPCFACE, SPKI_NPCFACE );
			assert(re);
			re = m_p_NPCFace.LoadFromFileData( 1, m_iSprite, SPK_NPCFACE, SPKI_NPCFACE );
			assert(re);
			
			m_client_rect.x += m_p_NPCFace[0].GetWidth();
			m_client_rect.w -= m_p_NPCFace[0].GetWidth();
		}
	}
	// 2005, 1, 14, sobeit add start - npc 얼굴 대신 ItemDescription 이미지를보여준다. - 퀘스트 땜시...
	else
	{
		if(sprite_num == 0)
		{
			m_iSprite = -1;
		}
		else
		{
			m_iSprite = sprite_num;

			m_p_NPCFace.Init( 1 );		// 임시로 1개만 loading.. - -;;

			bool re;
			re = m_p_NPCFace.LoadFromFileData( 0, m_iSprite, SPK_ITEM, SPKI_ITEM );
			assert(re);
//			re = m_p_NPCFace.LoadFromFileData( 1, 0, SPK_NPCFACE, SPKI_NPCFACE );
//			assert(re);

			m_client_rect.x += m_p_NPCFace[0].GetWidth();
			m_client_rect.w -= m_p_NPCFace[0].GetWidth();

		}
	}

	// 2005, 1, 14, sobeit add end
}
//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::Start
//
// 
//-----------------------------------------------------------------------------
void C_VS_UI_NPC_DIALOG::Start()
{
	C_VS_UI_DIALOG::Start();
	AttrTopmost(true);
}

//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::MouseControl
//
// 
//-----------------------------------------------------------------------------
bool C_VS_UI_NPC_DIALOG::MouseControl(UINT message, int _x, int _y)
{
	C_VS_UI_DIALOG::MouseControl(message, _x, _y);
	
	return true;
}

//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::KeyboardControl
//
// 바로 숫자 입력할 수 있게 한다.
//-----------------------------------------------------------------------------
void	C_VS_UI_NPC_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{
	if (message == WM_KEYDOWN)
	{
		switch (key)
		{
			case VK_ESCAPE:
			case VK_RETURN:
				if(key == VK_ESCAPE)
				{
					if(m_id == 638 || m_id == 639)
						return;
				}
				C_VS_UI_DIALOG::KeyboardControl(message, key, extra);
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_NPC_DIALOG::KeyboardControl
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_NPC_DIALOG::Show()
{
	C_VS_UI_DIALOG::Show();
	
	if(m_iSprite != -1)
	{
		int _x = m_client_rect.x - m_p_NPCFace[0].GetWidth()-10, _y = m_client_rect.y;

		if (gpC_base->m_p_DDSurface_back->Lock())
		{
			POINT point = {_x, _y};
			gpC_base->m_p_DDSurface_back->BltSprite(&point, &m_p_NPCFace[0]);
			if(m_p_NPCFace.GetSize()>1)
			{
				point.x+=17; point.y+=18;
				gpC_base->m_p_DDSurface_back->BltSprite(&point, &m_p_NPCFace[1]);
			}			
			gpC_base->m_p_DDSurface_back->Unlock();
		}

		if(m_id < (*g_pCreatureTable).GetSize())
		{
			
			//char* str = (*g_pCreatureTable)[m_id].Name.GetString();

			char* str = m_name.GetString();
			
			if(str && strcmp(str, ""))
			{
				TextSystem::TextService& textService = TextSystem::TextService::Get();
				TextSystem::TextStyle nameStyle = textService.GetDefaultStyle();
				nameStyle.color = TextSystem::ColorFromRGB(0xFFFFFF);
				nameStyle.align = TextSystem::TextAlign::Center;

				TextSystem::SpriteSurfaceRenderTarget target(gpC_base->m_p_DDSurface_back);
				textService.DrawLine(
					target,
					str,
					_x,
					_y + m_p_NPCFace[0].GetHeight()+10,
					m_p_NPCFace[0].GetWidth(),
					nameStyle
				);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::C_VS_UI_DESC_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_DESC_DIALOG::C_VS_UI_DESC_DIALOG(id_t type, void* void_ptr, void* void_ptr2)
					:C_VS_UI_DESC()
{
	g_RegisterWindow(this);
	g_desc_dialog_window_id = this;

	m_bl_main_help = false;
	

	AttrTopmost(true);

	int close_x_offset, close_y_offset;

	PrintInfo pi, title_pi;
	COLORREF color, title_color;

	switch(g_eRaceInterface)
	{
	case RACE_SLAYER:
		close_x_offset = 422;
		close_y_offset = 345;
		break;

	case RACE_VAMPIRE:
		close_x_offset = 431;
		close_y_offset = 350;
		break;

	case RACE_OUSTERS:
		close_x_offset = 431;
		close_y_offset = 350;
		break;

	default:
		break;
	}
	
	w = 540; h = 405;
	Set(g_GameRect.right/2-w/2, g_GameRect.bottom/2-h/2, w, h);

	m_pC_scroll_bar = new C_VS_UI_SCROLL_BAR(0, Rect(510, 55, -1, 265));
	std::string filename;

	bool bl_title = false;
//	int sprID = -1;
	int corezapID = -1;
	switch(type)
	{
	case DID_HELP:
		{
			bl_title = true;
			title_pi = gpC_base->m_desc_menu_pi;
			title_color = RGB_YELLOW;
			pi = gpC_base->m_desc_msg_pi;
			color = RGB_WHITE;
			
			m_pack_file.SetRAR(RPK_HELP, RPK_PASSWORD);
			
			switch((int)(intptr_t)void_ptr)
			{
			case MAIN:
				{

//					CSpritePack *temp;
					switch(g_eRaceInterface)
					{
					case RACE_SLAYER:
						filename = "MainSlayer.txt";
						break;

					case RACE_VAMPIRE:
						filename = "MainVampire.txt";
						break;

					case RACE_OUSTERS:
						filename = "MainOusters.txt";
						break;

					default:
						break;
					}

					m_bl_main_help = true;
				}
				break;

			case CHATTING:
				{
					filename = "Chatting.txt";
				}
				break;

			case SKILL:
				{
					filename = "Skill.txt";
				}
				break;
			
			case BATTLE:
				{
					filename = "Battle.txt";
				}
				break;

			case INVENTORY:
				{
					filename = "Inventory.txt";
				}
				break;

			case GEAR:
				{
					switch(g_eRaceInterface)
					{
					case RACE_SLAYER:
						filename = "GearSlayer.txt"; 
						break;

					case RACE_VAMPIRE:
						filename = "GearVampire.txt";
						break;

					case RACE_OUSTERS:
						filename = "GearOusters.txt";
						break;

					default:
						break;
					}					
				}
				break;

			case INFO:
				{
					switch(g_eRaceInterface)
					{
					case RACE_SLAYER:
						filename = "InfoSlayer.txt";
						break;

					case RACE_VAMPIRE:
						filename = "InfoVampire.txt";
						break;

					case RACE_OUSTERS:
						filename = "InfoOusters.txt";
						break;

					default:
						break;
					}					
				}
				break;

			case SHOP:
				filename = "Shop.txt";
				break;

			case STORAGE:
				filename = "Storage.txt";
				break;

			case EXCHANGE:
				filename = "Exchange.txt";
				break;

			case PARTY_MANAGER:
				{
					filename = "PartyManager.txt";
				}
				break;

			case GUILD:
				switch(g_eRaceInterface)
				{
				case RACE_SLAYER:
					filename = "Team.txt";
					break;

				case RACE_VAMPIRE:
					filename = "Clan.txt";
					break;

				case RACE_OUSTERS:
					filename = "Guild.txt";
					break;

				default:
					break;
				}
				break;

			case OTHER_INFO :
				switch(g_eRaceInterface)
				{
				case RACE_SLAYER:
					filename = "OtherInfoSlayer.txt";
					break;

				case RACE_VAMPIRE:
					filename = "OtherInfoVampire.txt";
					break;

				case RACE_OUSTERS:
					filename = "OtherInfoOusters.txt";
					break;

				default:
					break;
				}
				break;
				
			case TRACE_WINDOW :
				filename = "";				
				break;
			case MINI_GAME_ARROW_TILE:
				filename = "arrowtiles.txt";
				break;
			case MINI_GAME_CRAZY_MINE :
				filename = "crazymine.txt";
				break;
			case CTF_WINDOW:
				filename = "CTF.txt";
				break;
				
			case NICK_NAME_WINDOW:
				filename = "nickname.txt";
				break;
			}
		}
		break;

	case DID_ITEM:
		{
			m_pack_file.SetRAR(RPK_ITEM, RPK_PASSWORD);

			MItem *p_item = (MItem *)void_ptr;

			if(p_item == NULL)
				break;

			if(!p_item->IsIdentified())
			{
				Run(CLOSE_ID);
				return;
			}
			
			title_pi = gpC_base->m_item_name_pi;

			if(p_item->IsUniqueItem() || p_item->IsQuestItem())			// Unique Item?
			{
				// The old [base + 13][31] access crossed a 30-entry row.
				// Name its actual row/gradation explicitly to preserve that color.
				constexpr int titleSetOffset = 13 + 31 / MAX_COLORGRADATION;
				constexpr int titleGradation = 31 % MAX_COLORGRADATION;
				const int colorSet = p_item->IsQuestItem()
					? g_pClientConfig->QuestItemColorSet : g_pClientConfig->UniqueItemColorSet;
				title_color = RGB_YELLOW;
				if (colorSet >= 0 && colorSet < MAX_COLORSET - titleSetOffset)
				{
					const WORD temp_color = CIndexSprite::ColorSet[colorSet + titleSetOffset][titleGradation];
					title_color = RGB(CSDLGraphics::Red(temp_color)<<3, CSDLGraphics::Green(temp_color)<<3, CSDLGraphics::Blue(temp_color)<<3);
				}
			} else
			if(p_item->GetItemOptionListCount() == 2 )			// Rare Item ? 
			{
				title_color = g_pClientConfig->COLOR_NAME_ITEM_RARE_OPTION;
			} 
			// add by Sonic 2006.10.28 增加显示三属性装备为红色
			else if(p_item->GetItemOptionListCount() > 2)
			{
				title_color = g_pClientConfig->COLOR_NAME_VAMPIRE; //Red
			}
			// end by Sonic 2006.10.28 增加显示三属性装备为红色
			else
				title_color = RGB_YELLOW;						// Normal Item
			pi = gpC_base->m_desc_msg_pi;
			color = RGB_WHITE;
			

			std::string temp_string;
			std::string h_name,e_name;
			char sz_temp[200]="";

//			filename  = TXT_ITEMROOT;
			filename += p_item->GetEName();
			filename += ".txt";		
			e_name="( ";

			if(!p_item->IsEmptyItemOptionList()&&!p_item->IsUniqueItem() && !p_item->IsQuestItem())				
			{
				for(int i=0;i<min(2,p_item->GetItemOptionListCount());i++)
				{
					if(p_item->GetItemOptionName(i)!=NULL)
					{
						h_name+=p_item->GetItemOptionName(i);						
						h_name+=" ";						
					}
					if(p_item->GetItemOptionEName(i)!=NULL)
					{						
						e_name+=p_item->GetItemOptionEName(i);
						e_name+=" ";
					}					
				}				
			}
			h_name+=p_item->GetName();
			e_name+=p_item->GetEName();
			e_name+=" )";

			SafeFormat::Format(sz_temp, "%s %s",h_name.c_str(),e_name.c_str());
			// 이름이 너무 길면 줄여주자.	ReduceString2 는 뒤쪽에 "..." 찍어주기~ 	by sonee
			ReduceString2(sz_temp,55);			
			SetDescTitle(sz_temp);

			// weight
//			wsprintf(sz_temp, "\t무게 : %d", p_item->GetWeight());
//			temp_string = sz_temp;
//			m_rep_string.push_back(temp_string);
			
			// Durability
			if(p_item->IsGunItem())
			{
				switch(p_item->GetItemClass())
				{
				case ITEM_CLASS_SG:
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SG));
					break;
					
				case ITEM_CLASS_SMG:
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SMG));
					break;
					
				case ITEM_CLASS_AR:
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_AR));
					break;
					
				case ITEM_CLASS_SR:
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_ITEM_CLASS_SR));
					break;

				default:
					break;
				}
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}

			if (p_item->GetMaxDurability() != -1&&!p_item->IsUniqueItem()&& p_item->GetItemClass() != ITEM_CLASS_VAMPIRE_AMULET
				&& p_item->GetItemClass() != ITEM_CLASS_COUPLE_RING && p_item->GetItemClass() != ITEM_CLASS_VAMPIRE_COUPLE_RING
				&& !p_item->IsQuestItem())
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_DURABILITY), p_item->GetMaxDurability());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Silver
			if (p_item->GetSilverMax() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_SILVERING), p_item->GetSilverMax());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Damage
			if (p_item->GetMaxDamage() != -1) // damage가 있는가?
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_DAMAGE), p_item->GetMinDamage(), p_item->GetMaxDamage());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// critical hit
			if (p_item->GetCriticalHit() != -1) // 크리티컬히트가 있는가?
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_CRITICALHIT), p_item->GetCriticalHit());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Defense
			if (p_item->GetDefenseValue() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_DEFENSE), p_item->GetDefenseValue());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Protection
			if (p_item->GetProtectionValue() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_PROTECTION), p_item->GetProtectionValue());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// TOHIT
			if (p_item->GetToHit() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_ACCURACY), p_item->GetToHit());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Heal point
			if (p_item->GetHealPoint() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_HP), p_item->GetHealPoint());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Mana point
			if (p_item->GetManaPoint() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_MP), p_item->GetManaPoint());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Attack range
			if (p_item->GetReach() != -1 && p_item->GetReach() != 1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_RANGE), p_item->GetReach());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Bullet
			if (p_item->GetMagazineSize() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_MAGAZINE_NUM), p_item->GetMagazineSize());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Pocket size
			if (p_item->GetPocketNumber() != -1)
			{
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_POCKET_NUM), p_item->GetPocketNumber());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			// Pile size
//			if (p_item->IsPileItem() == true)
//			{
//				wsprintf(sz_temp, "\t아이템 개수 : %d개", p_item->GetNumber());
//				temp_string = sz_temp;
//				m_rep_string.push_back(temp_string);
//			}
			
			// Add option

			BOOL fOptionCheck=FALSE;
			if(!p_item->IsEmptyItemOptionList())
			{				
				std::list<TYPE_ITEM_OPTION>::iterator itr;
				std::list<TYPE_ITEM_OPTION> optionList=p_item->GetItemOptionList();
				
				itr=optionList.begin();
				
				while(itr != optionList.end() && *itr < g_pItemOptionTable->GetSize())
				{
					const ITEMOPTION_INFO& optionInfo=(*g_pItemOptionTable)[*itr];
					const std::string pPartName = g_pItemOptionTable->GetPartName(optionInfo.Part,
						p_item->IsVampireItem() ? 'H' : 'M');

					BYTE PlusPoint	= optionInfo.PlusPoint; // 적용되는 수치
					
					if (!pPartName.empty())
					{
						if(!fOptionCheck)
						{
							if(optionInfo.Part == g_pItemOptionTable->PART_DURABILITY)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint-100);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
							if(
								optionInfo.Part >= ITEMOPTION_TABLE::PART_STR_TO_DEX &&
								optionInfo.Part <= ITEMOPTION_TABLE::PART_INT_TO_DEX)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint);
							fOptionCheck=TRUE;							
						} else
						{
							if(optionInfo.Part == g_pItemOptionTable->PART_DURABILITY)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint-100);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
							if(
								optionInfo.Part >= ITEMOPTION_TABLE::PART_STR_TO_DEX &&
								optionInfo.Part <= ITEMOPTION_TABLE::PART_INT_TO_DEX)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint);							
						}					
						temp_string=sz_temp;
						m_rep_string.push_back(temp_string);							
					}
					itr++;
				}
			}
			if(!(*g_pItemTable)[p_item->GetItemClass()][p_item->GetItemType()].DefaultOptionList.empty())
			{
				std::list<TYPE_ITEM_OPTION>::const_iterator itr;
				itr=(*g_pItemTable)[p_item->GetItemClass()][p_item->GetItemType()].DefaultOptionList.begin();
				while(itr != (*g_pItemTable)[p_item->GetItemClass()][p_item->GetItemType()].DefaultOptionList.end())
				{
					const ITEMOPTION_INFO& optionInfo=(*g_pItemOptionTable)[*itr];
					const std::string pPartName = g_pItemOptionTable->GetPartName(optionInfo.Part,
						p_item->IsVampireItem() ? 'H' : 'M');

					BYTE PlusPoint	= optionInfo.PlusPoint; // 적용되는 수치
					
					if (!pPartName.empty())
					{
						if(!fOptionCheck)
						{
							if(optionInfo.Part == g_pItemOptionTable->PART_DURABILITY)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint-100);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
							if(
								optionInfo.Part >= ITEMOPTION_TABLE::PART_STR_TO_DEX &&
								optionInfo.Part <= ITEMOPTION_TABLE::PART_INT_TO_DEX)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION), pPartName, PlusPoint);
							fOptionCheck=TRUE;							
						} else
						{
							if(optionInfo.Part == g_pItemOptionTable->PART_DURABILITY)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint-100);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
							if(
								optionInfo.Part >= ITEMOPTION_TABLE::PART_STR_TO_DEX &&
								optionInfo.Part <= ITEMOPTION_TABLE::PART_INT_TO_DEX)
							{
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint);
								SafeFormat::Format(sz_temp + std::strlen(sz_temp), sizeof(sz_temp) - std::strlen(sz_temp), "%%");
							}
							else
								SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY), pPartName, PlusPoint);							
						}					
						temp_string=sz_temp;
						m_rep_string.push_back(temp_string);							
					}
					itr++;
				}				
			}
			if(void_ptr2!=NULL)
			{
				char sz_buf1 [50];
				MItem* p_AddItem = (MItem*)void_ptr2;
			
				// Read a complete, decoded sprite tag from the core-zap description.
				std::string AddedFileName = p_AddItem->GetEName();
				std::string temp_string;
				AddedFileName += ".txt";
				if (m_pack_file.OpenText(AddedFileName.c_str())) {
					const std::string_view text(m_pack_file.GetFilePointer(), m_pack_file.GetRemainingSize());
					const auto first = TextSystem::NextUtf8Line(text, text.size(), {.skipSeamSpace = false});
					if (!first.text.empty() && first.text.front() == '%')
						DescriptorText::ReadIndex(std::string_view(first.text).substr(1), corezapID);
					m_pack_file.Release();
				}

				if(p_AddItem->GetItemClass() == ITEM_CLASS_CORE_ZAP && p_AddItem->GetItemType()<=3)
				{
					const std::list<TYPE_ITEM_OPTION> &optionList=p_item->GetItemOptionList();
					const std::list<TYPE_ITEM_OPTION> &DefaultOptionList = p_item->GetItemDefaultOptionList();
					if(optionList.size() ==0 && DefaultOptionList.size() == 0)
					{
						SafeFormat::Format(sz_temp, GetGameString(UI_STRING_CORE_ZAP_BLACK+p_AddItem->GetItemType()), p_AddItem->GetGrade());
						SafeFormat::Format(sz_buf1, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION2), sz_temp);
						temp_string=sz_buf1;
						m_rep_string.push_back(temp_string);
						
						//vx = g_PrintColorStr(px, py,(*g_pGameStringTable)[UI_STRING_MESSAGE_DESC_OPTION].GetString(), gpC_base->m_item_desc_pi, ITEM_DESC_RGB);

					}
					else
					{
						SafeFormat::Format(sz_temp, GetGameString(UI_STRING_CORE_ZAP_BLACK+p_AddItem->GetItemType()), p_AddItem->GetGrade());
						SafeFormat::Format(sz_buf1, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY2), sz_temp);
						temp_string=sz_buf1;
						m_rep_string.push_back(temp_string);
					}					
					SafeFormat::Format(sz_temp, GetGameString(UI_STRING_CORE_ZAP_REWARD_ALL_STAT), 3);
					SafeFormat::Format(sz_buf1, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY2), sz_temp);
					temp_string=sz_buf1;
					m_rep_string.push_back(temp_string);
					SafeFormat::Format(sz_temp, GetGameString(UI_STRING_CORE_ZAP_REWARD_ALL_REG), 9);
					SafeFormat::Format(sz_buf1, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_OPTION_EMPTY2), sz_temp);
					temp_string=sz_buf1;
					m_rep_string.push_back(temp_string);
					
				}
			}
			
			bool bl_required = false;
			if(p_item->IsSlayerItem())
			{
				if (p_item->GetRequireSTR() > 0)
				{
					if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
					bl_required = true;
					
					SafeFormat::Format(sz_temp + strlen(sz_temp), sizeof(sz_temp) - strlen(sz_temp), GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_STR), p_item->GetRequireSTR());
					if (p_item->GetRequireDEX() > 0 ||
						p_item->GetRequireINT() > 0 ||
						//					p_item->GetRequireLevel() > 0 ||
						(p_item->GetRequireSUM() > 0 && p_item->GetRequireSUM() > p_item->GetRequireSTR()))
						SafeFormat::Append(sz_temp, ",");
					temp_string = sz_temp;
					m_rep_string.push_back(temp_string);
				}
				
				if (p_item->GetRequireDEX() > 0)
				{
					if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
					bl_required = true;
					
					SafeFormat::Format(sz_temp + strlen(sz_temp), sizeof(sz_temp) - strlen(sz_temp), GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_DEX), p_item->GetRequireDEX());
					if (p_item->GetRequireINT() > 0 ||
						//					p_item->GetRequireLevel() > 0 ||
						(p_item->GetRequireSUM() > 0 && p_item->GetRequireSUM() > p_item->GetRequireDEX()))
						SafeFormat::Append(sz_temp, ",");
					temp_string = sz_temp;
					m_rep_string.push_back(temp_string);
				}
				
				if (p_item->GetRequireINT() > 0)
				{
					if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
					bl_required = true;
					
					SafeFormat::Format(sz_temp + strlen(sz_temp), sizeof(sz_temp) - strlen(sz_temp), GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_INT), p_item->GetRequireINT());
					
					if (//p_item->GetRequireLevel() > 0 || 
						(p_item->GetRequireSUM() > 0 && p_item->GetRequireSUM() > p_item->GetRequireINT()))
						SafeFormat::Append(sz_temp, ",");
					temp_string = sz_temp;
					m_rep_string.push_back(temp_string);
				}
				
				if (p_item->GetRequireSUM() > 0 &&
				p_item->GetRequireSUM() > p_item->GetRequireSTR() &&
				p_item->GetRequireSUM() > p_item->GetRequireDEX() &&
				p_item->GetRequireSUM() > p_item->GetRequireINT() )
				{
					if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
					bl_required = true;
					
					//				if (p_item->GetRequireLevel() > 0)
					//					strcat(sz_temp, " and");
	
					SafeFormat::Format(sz_temp + strlen(sz_temp), sizeof(sz_temp) - strlen(sz_temp), GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_ALL_STAT_SUM), p_item->GetRequireSUM());
					temp_string = sz_temp;
					m_rep_string.push_back(temp_string);
				}
			}
			else if(p_item->IsVampireItem())
			{
				if (p_item->GetRequireLevel() > 0)
				{
					if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
					bl_required = true;
					
					SafeFormat::Format(sz_temp + strlen(sz_temp), sizeof(sz_temp) - strlen(sz_temp), GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_LEVEL), p_item->GetRequireLevel());
					temp_string = sz_temp;
					m_rep_string.push_back(temp_string);
				}
			}

			if((!p_item->IsSlayerItem() && g_eRaceInterface == RACE_SLAYER) ||
				(!p_item->IsVampireItem() && g_eRaceInterface == RACE_VAMPIRE) ||
				(!p_item->IsOustersItem() && g_eRaceInterface == RACE_OUSTERS)
				)
			{
				if(bl_required)
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
				else
					snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
				bl_required = true;
				
				SafeFormat::Append(sz_temp, (*g_pGameStringTable)[UI_STRING_MESSAGE_DESC_OTHER_TRIBE].GetString());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			
			if (p_item->IsGenderForMale())
			{
				if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
				bl_required = true;

				SafeFormat::Append(sz_temp, (*g_pGameStringTable)[UI_STRING_MESSAGE_DESC_ONLY_MALE].GetString());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}
			else if (p_item->IsGenderForMale())
			{
				if(bl_required)
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE_EMPTY));
					else
						snprintf(sz_temp, sizeof(sz_temp), "%s", GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_REQUIRE));
				bl_required = true;

				SafeFormat::Append(sz_temp, (*g_pGameStringTable)[UI_STRING_MESSAGE_DESC_ONLY_FEMALE].GetString());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}

			// Price
//			if(//p_item->GetItemClass() != ITEM_CLASS_EVENT_GIFT_BOX
				// && p_item->GetItemClass() != ITEM_CLASS_EVENT_STAR)
//				p_item->GetItemClass() != ITEM_CLASS_EVENT_ETC
//				&& p_item->GetItemClass() != ITEM_CLASS_EVENT_TREE)
			{
				SafeFormat::Format(sz_temp, "%d", g_pPriceManager->GetItemPrice(p_item, MPriceManager::NPC_TO_PC));
				std::string sstr = sz_temp;
				for(int i = 3; i <= 13; i += 4)
					if(sstr.size() > static_cast<size_t>(i))sstr.insert(sstr.size()-i, ",");
					
				MString kkkstr = (*g_pGameStringTable)[UI_STRING_MESSAGE_DESC_DIALOG_PRICE];
				SafeFormat::Format(sz_temp, GetGameString(UI_STRING_MESSAGE_DESC_DIALOG_PRICE), sstr.c_str());
				temp_string = sz_temp;
				m_rep_string.push_back(temp_string);
			}

			// EVENT_STAR || EVENT_BOX
			if(p_item->GetItemClass() == ITEM_CLASS_EVENT_GIFT_BOX
				// || p_item->GetItemClass() == ITEM_CLASS_EVENT_STAR)
				|| p_item->GetItemClass() == ITEM_CLASS_EVENT_ETC
				|| p_item->GetItemClass() == ITEM_CLASS_EVENT_TREE)
				m_rep_string.push_back("\t");
			
		}
		break;
	
	case DID_SKILL:
		{
			m_pack_file.SetRAR(RPK_SKILL, RPK_PASSWORD);
			
			CSpritePack *temp;
			temp = new CSpritePack;
			temp->Init( 1 );		// 임시로 1개만 loading.. - -;;

			int id = (int)(intptr_t)void_ptr;
			int sprID = (*g_pSkillInfoTable)[id].GetSpriteID();

			bool re = temp->LoadFromFileData( 0, sprID, SPK_SKILL_ICON, SPKI_SKILL_ICON );
			assert(re);

			m_pC_inpicture.push_back(temp);

			char sz_temp[10] = "%";
			SafeFormat::Format(sz_temp + 1, sizeof sz_temp - 1, "%d", 0);
			m_rep_string.push_back(sz_temp);

			title_pi = gpC_base->m_desc_menu_pi;
			title_color = RGB_YELLOW;
			pi = gpC_base->m_desc_msg_pi;
			color = RGB_WHITE;
			
			filename += (*g_pSkillInfoTable)[id].GetName();
			filename += ".txt";

			std::string title;
			title = (*g_pSkillInfoTable)[id].GetHName();
			title += " (";
			title += (*g_pSkillInfoTable)[id].GetName();
			title += ")";
			
			SetDescTitle(title.c_str());
		}
		break;

	}

	if(LoadDesc(filename.c_str(), 60, 17, bl_title,corezapID)==false)Run(CLOSE_ID);

	m_rep_string.clear();


	switch(g_eRaceInterface)
	{
	case RACE_SLAYER:
		SetDesc(50, 35, color, pi);
		SetDescTitle(50, 5, title_color, title_pi);
		break;
		
	case RACE_VAMPIRE:
		SetDesc(50, 35, color, pi);
		SetDescTitle(50, 5, title_color, title_pi);
		break;
		
	case RACE_OUSTERS:
		SetDesc(50, 35, color, pi);
		SetDescTitle(53, 8, title_color, title_pi);
		break;

	default:
		break;
	}

	m_pC_scroll_bar->SetPosMax(GetDescSize() - GetDescCol() +1);
	m_pC_button_group = new ButtonGroup(this);
//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(scroll_x_offset, scroll_up_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_UP), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_UP), SCROLL_UP_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_UP));
//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(scroll_x_offset, scroll_down_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), SCROLL_DOWN_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_DOWN));

	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(close_x_offset, close_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_CLOSE), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_CLOSE), CLOSE_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_CLOSE));
//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(x+help_x_offset, y+help_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_HELP), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_HELP), HELP_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_HELP));

}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::~C_VS_UI_DESC_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_DESC_DIALOG::~C_VS_UI_DESC_DIALOG()
{
	g_UnregisterWindow(this);
	g_desc_dialog_window_id = NULL;
	DeleteNew(m_pC_button_group);
	DeleteNew(m_pC_scroll_bar);
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::Start
//
// 
//-----------------------------------------------------------------------------
void C_VS_UI_DESC_DIALOG::Start()
{
	PI_Processor::Start();
	m_pC_button_group->Init();

	AttrPin(true);
	gpC_window_manager->AppearWindow(this);
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::MouseControl
//
// 
//-----------------------------------------------------------------------------
bool C_VS_UI_DESC_DIALOG::MouseControl(UINT message, int _x, int _y)
{
	Window::MouseControl(message, _x, _y);
	_x-=x;_y-=y;//윈도우 이동을 위한
	bool re = m_pC_scroll_bar->MouseControl(message, _x, _y);
	re &= m_pC_button_group->MouseControl(message, _x, _y);

	g_descriptor_manager.Unset();

	switch(message)
	{
	case M_WHEEL_UP:
		m_pC_scroll_bar->ScrollUp();
		break;

	case M_WHEEL_DOWN:
		m_pC_scroll_bar->ScrollDown();
		break;

	case M_LEFTBUTTON_DOWN:
	case M_LB_DOUBLECLICK:
//		if(_x > m_scroll_x && _x < m_scroll_x+gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth((gbl_vampire_interface?C_GLOBAL_RESOURCE::AB_VAMPIRE_SCROLL_TAG:C_GLOBAL_RESOURCE::AB_SLAYER_SCROLL_TAG)) &&
//			_y > m_scroll_y && _y < m_scroll_y+m_scroll_h+gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight((gbl_vampire_interface?C_GLOBAL_RESOURCE::AB_VAMPIRE_SCROLL_TAG:C_GLOBAL_RESOURCE::AB_SLAYER_SCROLL_TAG)))
//		{
//			if(_y > m_scroll_y+m_scroll_h)_y = m_scroll_y+m_scroll_h;
//			LB_SCROLL_DOWN = true;
//			SetDescScrollPos((GetDescSize()-GetDescCol()) * (_y - m_scroll_y) / (m_scroll_h));//+gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight((gbl_vampire_interface?C_GLOBAL_RESOURCE::AB_VAMPIRE_SCROLL_TAG:C_GLOBAL_RESOURCE::AB_SLAYER_SCROLL_TAG))));
//		}
//		else
		{
			if (gpC_mouse_pointer->GetPickUpItem() == NULL && re)
			{
				MoveReady();
				SetOrigin(_x, _y);
				break;
			}
		}
		break;

	case M_LEFTBUTTON_UP:
		break;

	case M_MOVING:
		break;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::KeyboardControl
//
// 바로 숫자 입력할 수 있게 한다.
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{
	(void)extra;
	if (message == WM_KEYDOWN)
		switch (key)
		{
			case VK_RETURN: // ok로 간주. !cancel은 반드시 ok와 함께 있다.
			case VK_ESCAPE: // 아무것도 안함.
				Run(CLOSE_ID);
				break;
		}
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::KeyboardControl
//
// 
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::Show()
{
	if(gpC_base->m_p_DDSurface_back->Lock())
	{
		gpC_global_resource->DrawDialogLocked2(x, y, w, h, GetAttributes()->alpha);
		m_pC_button_group->Show();
		gpC_base->m_p_DDSurface_back->Unlock();
	}

	SetDescScrollPos(m_pC_scroll_bar->GetScrollPos());
	ShowDesc(x, y);

	m_pC_scroll_bar->Show(x, y);
	
	m_pC_button_group->ShowDescription();	
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::ShowButtonWidget
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::ShowButtonDescription(C_VS_UI_EVENT_BUTTON * p_button)
{
	static char *	m_help_string[1] = {
		(*g_pGameStringTable)[UI_STRING_MESSAGE_CLOSE_DESC_WINDOW].GetString(),
	};
	
	if (p_button->GetID() == CLOSE_ID)
		g_descriptor_manager.Set(DID_INFO, x+p_button->x, y+p_button->y, (void *)m_help_string[p_button->GetID()],0,0);
}


//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::ShowButtonWidget
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::ShowButtonWidget(C_VS_UI_EVENT_BUTTON * p_button)
{
	assert(p_button);


	if(p_button->GetFocusState())
	{
		if(p_button->GetPressState())
			gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index+C_GLOBAL_RESOURCE::AB_BUTTON_PUSHED_OFFSET);
		else
			gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index+C_GLOBAL_RESOURCE::AB_BUTTON_HILIGHTED_OFFSET);
	}
	else
		gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index);

}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::IsPixel
//
//
//-----------------------------------------------------------------------------
bool	C_VS_UI_DESC_DIALOG::IsPixel(int _x, int _y)
{
	return IsInRect(_x, _y);
//	return IsInRect(_x, _y);
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::Run
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::Run(id_t id)
{
	switch (id)
	{
	case CLOSE_ID:
//		if(m_bl_main_help)
//			gC_vs_ui.HotKey_Help();
//		else
			gpC_base->SendMessage(UI_CLOSE_DESC_DIALOG);
		break;
	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::Process
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::Process()
{
	m_pC_button_group->Process();
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::UnacquireMouseFocus
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::UnacquireMouseFocus()
{
	m_pC_button_group->UnacquireMouseFocus();
}

//-----------------------------------------------------------------------------
// C_VS_UI_DESC_DIALOG::Finish
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_DESC_DIALOG::Finish()
{
	PI_Processor::Finish();

	gpC_window_manager->DisappearWindow(this);
}




//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::C_VS_UI_FILE_DIALOG
//
// Default Mode Value = MODE_PRO_FILE_SELECT
//-----------------------------------------------------------------------------
C_VS_UI_FILE_DIALOG::C_VS_UI_FILE_DIALOG(MODE Mode)
{
	STAY_FOLDER=-1;
	g_RegisterWindow(this);
	AttrTopmost(true);

	int close_x_offset, close_y_offset;
	int help_x_offset, help_y_offset;
	
	m_string_x = 50; m_string_y = 70; m_string_gap = 20;
	m_scroll_max = 13;
	m_select = -1;
	m_tempselect = -1;
	m_bl_open_drive = false;
	m_flag_preview_image=false;

	m_show_long_name = 1000;


	m_p_image_spk=NULL;
	m_p_icon_spk=NULL;


	m_pC_scroll_bar = NULL;
	m_pC_scroll_bar = new C_VS_UI_SCROLL_BAR(1, Rect(370, 74, -1, 245));

	w = 400;
	h = 400;
	x = g_GameRect.right/2 - w/2;
	y = g_GameRect.bottom/2 - h/2;
	
	help_x_offset = w-150;
	help_y_offset = h-50;
	
	close_x_offset = w-80;
	close_y_offset = h-50;

	m_mode=Mode;

	m_temp_face.InitOffsurface(55,70);
	m_temp_face.FillSurface(0);
//	m_scroll_x = 510;
//	m_scroll_y = 95;
//	m_scroll_h = 212;
	
//	Set(g_GameRect.right/2-m_p_image_spk->GetWidth()/2, g_GameRect.bottom/2-m_p_image_spk->GetHeight()/2, m_p_image_spk->GetWidth(), m_p_image_spk->GetHeight());
	
	m_pC_button_group = new ButtonGroup(this);
//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(scroll_x_offset, scroll_up_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_UP), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_UP), SCROLL_UP_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_UP));
//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(scroll_x_offset, scroll_down_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), SCROLL_DOWN_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_DOWN));
	
	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(help_x_offset, help_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_CLOSE), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_CLOSE), OK_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_OK));
	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(close_x_offset, close_y_offset, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_HELP), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_HELP), CANCEL_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_CANCEL));

//	m_pC_button_group->Add(new C_VS_UI_EVENT_BUTTON(drive_button_x, drive_button_y, gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight(C_GLOBAL_RESOURCE::AB_BUTTON_DOWN), DRIVE_ID, this, C_GLOBAL_RESOURCE::AB_BUTTON_DOWN));

//	m_scroll = 0;

}



//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::~C_VS_UI_FILE_DIALOG
//
// 
//-----------------------------------------------------------------------------
C_VS_UI_FILE_DIALOG::~C_VS_UI_FILE_DIALOG()
{
	g_UnregisterWindow(this);
	if(m_p_image_spk)
	{
		DeleteNew(m_p_image_spk);
	}
	if(m_p_icon_spk)
	{
		DeleteNew(m_p_icon_spk);
	}
	DeleteNew(m_pC_scroll_bar);
	DeleteNew(m_pC_button_group);


}


//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::Start
//
// 여러개의 파일 검색 가능.	; 로 구분
// ex> C_VS_UI_FILE_DIALOG::Start(".bmp;.jpg;.pcx;.tga;.gif"); by sonee
//-----------------------------------------------------------------------------
void C_VS_UI_FILE_DIALOG::Start(const char *type)
{
	PI_Processor::Start();
	m_pC_button_group->Init();

	m_filter = Basic::SplitDialogFilters(type);

	m_filename = "";
	m_select_file_num.clear();


	// 파일 다이알로그에 들어가는 아이콘
	m_p_icon_spk=new C_SPRITE_PACK(SPK_FILE_DIALOG);


//	AttrPin(true);
	gpC_window_manager->AppearWindow(this);

	if (m_directories.paths.empty())
	{
		std::error_code error;
		const auto current = std::filesystem::current_path(error);
		m_directories = Basic::MakeDialogDirectories(GetLogicalDrives(), error ? "" : current.string());
	}
	RefreshFileList(m_directories.paths[m_directories.current]);
	m_pC_scroll_bar->SetItemCount(m_vs_file_list.size(), m_scroll_max);
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::MouseControl
//
// 
//-----------------------------------------------------------------------------

bool C_VS_UI_FILE_DIALOG::MouseControl(UINT message, int _x, int _y)
{
	if (m_directories.current >= m_directories.paths.size()) return true;
	
	Window::MouseControl(message, _x, _y);
	_x-=x;
	_y-=y;//윈도우 이동을 위한
	bool re = m_pC_button_group->MouseControl(message, _x, _y);
	g_descriptor_manager.Unset();
	re&=m_pC_scroll_bar->MouseControl(message, _x, _y);	
	int temp_m_select;
//	m_scroll=m_pC_scroll_bar->GetScrollPos();
	
	
	switch(message)
	{
	case M_WHEEL_UP:
		m_flag_preview_image=false;
		m_pC_scroll_bar->ScrollUp();
		break;

	case M_WHEEL_DOWN:
		m_flag_preview_image=false;
		m_pC_scroll_bar->ScrollDown();
		break;

	case M_LEFTBUTTON_DOWN:		
	case M_LB_DOUBLECLICK :		
		if(m_bl_open_drive)		// Select From Drive Box
		{
			if(_x>51&&_y>54&&_x<350&&static_cast<size_t>(_y)<54+m_string_gap*m_directories.paths.size())
			{
				
				const int nextDrive = (_y - (37+m_string_gap))/m_string_gap;
				if (nextDrive < 0 || static_cast<size_t>(nextDrive) >= m_directories.paths.size()) break;
				m_directories.current = nextDrive;
				
				RefreshFileList(m_directories.paths[m_directories.current]);
				m_pC_scroll_bar->SetItemCount(m_vs_file_list.size(), m_scroll_max);
				
				
				m_bl_open_drive = false;
			} else
				m_bl_open_drive = false;
		} else
		if(_y>35&&_x>30&&_y<49&&_x<340)			// Open Drive Box
		{
			m_bl_open_drive=true;		
		}
				/*		else if(_x > m_scroll_x && _x < m_scroll_x+gpC_global_resource->m_pC_assemble_box_button_spk->GetWidth((gbl_vampire_interface?C_GLOBAL_RESOURCE::AB_VAMPIRE_SCROLL_TAG:C_GLOBAL_RESOURCE::AB_SLAYER_SCROLL_TAG)) &&
				_y > m_scroll_y && _y < m_scroll_y+m_scroll_h)
				{
				LB_SCROLL_DOWN = true;
				m_scroll = (m_vs_file_list.size()-m_scroll_max) * (_y - m_scroll_y) / m_scroll_h;
				SetDescScrollPos(GetDescSize() * (_y - m_scroll_y) / (m_scroll_h+gpC_global_resource->m_pC_assemble_box_button_spk->GetHeight((gbl_vampire_interface?C_GLOBAL_RESOURCE::AB_VAMPIRE_SCROLL_TAG:C_GLOBAL_RESOURCE::AB_SLAYER_SCROLL_TAG))));
	}*/
		else 
			if(_x > m_string_x-25 && _x < m_string_x+280 && 
				_y > m_string_y && _y < m_string_y + m_scroll_max*m_string_gap)		// File List
			{
				int m_old_select = m_select;
				m_select = (_y - m_string_y) / m_string_gap+m_pC_scroll_bar->GetScrollPos();
				
				if(static_cast<size_t>(m_select) >= m_vs_file_list.size()) m_select = -1;
				
				//if(message == M_LB_DOUBLECLICK && m_select != -1)
				if( m_select == m_old_select && m_select != -1)			// Double Click Check
				{
					if(m_vs_file_list_attr[m_select] & FILE_ATTRIBUTE_DIRECTORY) // 디렉토리일 경우 해당디렉토리로 옮김
					{
						ChangeDir(m_vs_file_list[m_select].c_str(), m_directories.paths[m_directories.current]);
						m_filename = "";
						m_select_file_num.clear();
						m_select = -1;
						m_tempselect = -1;
					}
					else if(m_mode==MODE_PROFILE_SELECT)				// MODE_PROFILE_SELECT 는 파일더블클릭을 지원한다. 
					{
						Run(OK_ID);
					}
					
					m_pC_scroll_bar->SetItemCount(m_vs_file_list.size(), m_scroll_max);
				}
				if(m_select!=-1&&m_mode!=MODE_PROFILE_SELECT)	// 프로필 모드가 아니면
				{
					if(!(m_vs_file_list_attr[m_select] & FILE_ATTRIBUTE_DIRECTORY))	
					{
						if(g_GetCtrlPushState()) 
						{
							if(m_select_file_num.size()>0)
							{
								std::vector<int>::iterator i;
								i=std::find(m_select_file_num.begin(),m_select_file_num.end(),m_select);
								if(i!=m_select_file_num.end()) m_select_file_num.erase(i);
							} 							
						} 
						else
						{
							m_select_file_num.clear();
						}						
						m_select_file_num.insert(m_select_file_num.end(), m_select);
					} 
					else		// 그냥 클릭했을경우
					{
						if(!g_GetCtrlPushState())	
							m_select_file_num.clear();
					}
				}
				//m_stay_openfolder=GetTickCount();			// 긴이름등을 표시해줄때 쓰는 타이머
				Timer(true);
				STAY_FOLDER=0xfffffffe;
			}
			else
			{
				if (gpC_mouse_pointer->GetPickUpItem() == NULL && re)
				{
					MoveReady();
					SetOrigin(_x, _y);
					break;
				}
			}		
		break;

	case M_LEFTBUTTON_UP:
		break;

	case M_MOVING:		
		// 드라이브 박스가 열렸을 경우 마우스를 갖다 놓았을때 1초이상 해당 드라이브에 위치해있으면
		// 이름이 압축된 경우 풀 네임을 출력하여 준다.	
		m_flag_preview_image=true;
		if( _y > 35 && _x > 30 && _y < 49 && _x < 340 )
			STAY_FOLDER=0;

		if(m_bl_open_drive)
		{
			if( _x > 51 && _y > 54 && _x < 350 && static_cast<size_t>(_y) < 54 +m_string_gap*m_directories.paths.size())
			{
				int mi_open_drive = (_y - (37+m_string_gap))/m_string_gap;
				mi_open_drive++;				
				if(STAY_FOLDER!=mi_open_drive) 
					Timer(true);
		//			m_stay_openfolder=GetTickCount();
				STAY_FOLDER=mi_open_drive;
			} 
			else
				STAY_FOLDER=-1;
		}
		temp_m_select = m_tempselect;				
		m_tempselect= (_y - m_string_y) / m_string_gap+m_pC_scroll_bar->GetScrollPos();
		
		if(static_cast<size_t>(m_tempselect) >= m_vs_file_list.size() || m_tempselect < 0 || m_bl_open_drive)
			m_tempselect = -1;
		else
			STAY_FOLDER=-2;

//		m_stay_openfolder=GetTickCount();
		Timer(true);

		if(m_tempselect != -1)
		{
			if(temp_m_select != m_tempselect && !(m_vs_file_list_attr[m_tempselect] & FILE_ATTRIBUTE_DIRECTORY))
			{
					CSpriteSurface bmpSurface;
				POINT destPoint={0,0};
				std::string filename;
				
				filename = Basic::DialogDirectoryPath(m_directories.paths[m_directories.current]);
				filename+=m_vs_file_list[m_tempselect].c_str();							
				
				if(LoadImageToSurface(filename.c_str(), bmpSurface))
				{					
					RECT srcRect={0,0,bmpSurface.GetWidth(),bmpSurface.GetHeight()};					
					m_temp_face.FillSurface(0);
					m_temp_face.Blt(&destPoint,&bmpSurface,&srcRect);
				} else
				{
					m_flag_preview_image=false;
					m_temp_face.FillSurface(0);
				}
			} 
		}
		
		break;

	default :
		break;
	}	
	return true;
}

bool	C_VS_UI_FILE_DIALOG::Timer(bool reset)
{
	// The long-name delay: strictly more than m_show_long_name since the
	// last reset, as the "prev + delay < now" it replaces was.
	static MonotonicClock::IntervalTimer s_long_name_timer;

	if(reset)
	{
		s_long_name_timer.Restart();
	} else
	{
		if(s_long_name_timer.Elapsed() > MonotonicClock::Millis(m_show_long_name))
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::KeyboardControl
//
// 바로 숫자 입력할 수 있게 한다.
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::KeyboardControl(UINT message, UINT key, long extra)
{	
	(void)extra;
	if (message == WM_KEYDOWN)
	{
		switch (key)
		{		
		case VK_RETURN: // ok로 간주. !cancel은 반드시 ok와 함께 있다.
			Run(OK_ID);
			break;

		case VK_ESCAPE: // 아무것도 안함.
			Run(CANCEL_ID);
			break;		
		}
	}	
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::Show
//
// 문자열 처리함수랑 함수명들이 좀 길어서 소스내용역시 좀 깁니다.
// 속도에는 별다른 지장을 주지 않습니다. 
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::Show()
{
	if (m_directories.current >= m_directories.paths.size()) return;
	std::string title;
	RECT rect;
	// 600,133

	// 미리보기
	//SetRect(&rect, x+w,y,x+w+55,y+77);
	//DrawAlphaBox(&rect, 255, 255, 255, 20);
	

	gpC_global_resource->DrawDialog2(x, y, w, h, GetAttributes()->alpha);
	
	//----------------------------------------------------------------------------------------
	// 선택영역 표시
	switch(m_mode)
	{
	case MODE_PROFILE_SELECT :
		if(m_select != -1 && m_select >= m_pC_scroll_bar->GetScrollPos() && m_select < m_pC_scroll_bar->GetScrollPos()+m_scroll_max)	
		{
			SetRect(&rect, x+m_string_x,y + m_string_y + (m_select - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3,x+m_string_x+275,y + m_string_y + (m_select - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3+m_string_gap);
			DrawAlphaBox(&rect, 0, 0, 20, 20);
		}
		break;

	case MODE_SENDFILE_SELECT :
		{
			for(int i=0;static_cast<size_t>(i)<m_select_file_num.size();i++)	
			{
				if(m_select_file_num[i] >= m_pC_scroll_bar->GetScrollPos() && m_select_file_num[i] < m_pC_scroll_bar->GetScrollPos()+m_scroll_max)		{
					SetRect(&rect, x+m_string_x, y + m_string_y + (m_select_file_num[i] - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3,x+m_string_x + 275,rect.bottom = y + m_string_y + (m_select_file_num[i] - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3 + m_string_gap);
					DrawAlphaBox(&rect, 0, 0, 20, 20);	}
			}

			//if(m_select_file_num.size()<=0)
			if(m_select_file_num.empty())
			if(m_select != -1 && m_select >= m_pC_scroll_bar->GetScrollPos() && m_select < m_pC_scroll_bar->GetScrollPos()+m_scroll_max)	{
			SetRect(&rect, x+m_string_x,y + m_string_y + (m_select - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3,x+m_string_x+275,y + m_string_y + (m_select - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3+m_string_gap);
			DrawAlphaBox(&rect, 0, 0, 20, 20);}
		}
		break;
	}
	//----------------------------------------------------------------------------------------
	// 아이콘 부분 출력		
	if(gpC_base->m_p_DDSurface_back->Lock())
	{
		for(int i = 0; i < m_scroll_max && static_cast<size_t>(i+m_pC_scroll_bar->GetScrollPos()) < m_vs_file_list.size(); i++){
			const char *get=m_vs_file_list[i+m_pC_scroll_bar->GetScrollPos()].c_str();
			if(get[0]=='\\')
			{
				if(!strcmp(get,"\\.."))	m_p_icon_spk->BltLocked(x+m_string_x-19,y+m_string_y+i*m_string_gap,UPFOLDER_ID);
				else m_p_icon_spk->BltLocked(x+m_string_x-19,y+m_string_y+i*m_string_gap,FOLDER_ID);
			}
		}
		m_pC_button_group->Show();
		//----------------------------------------------------------------------------------------
		// 현재 오픈된 폴더명 출력
		m_p_icon_spk->BltLocked(x+m_string_x-19, y+37,OPENFOLDER_ID);
		gpC_base->m_p_DDSurface_back->Unlock();
	}

	g_FL2_GetDC();	
	// 열린 폴더명의 길이가 길경우 잘라준다 
	// 38자 이상 되지 않도록 한다. 
	title = Basic::BuildDialogPathLabel(m_directories.paths[m_directories.current], m_filter);
	const std::string folderName = Basic::ShortenDialogLabel(title);
	
	g_PrintColorStr(x+m_string_x, y+37, folderName.c_str(), gpC_base->m_desc_menu_pi, RGB_WHITE);

	//----------------------------------------------------------------------------------------
	// 타이틀 출력
	const char* dialogTitle = GetGameString(m_mode == MODE_PROFILE_SELECT
		? UI_STRING_MESSAGE_FILE_DIALOG_SELECT_PROFILE_PICTURE
		: UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE);
	
	int titleX = x+40, titleY = y+5;
	if(g_eRaceInterface == RACE_OUSTERS)
	{
		titleX += 3;
		titleY += 3;
	}
	g_PrintColorStr(titleX, titleY, dialogTitle,gpC_base->m_desc_menu_pi, RGB_WHITE);

	for(int i = 0; i < m_scroll_max && static_cast<size_t>(i+m_pC_scroll_bar->GetScrollPos()) < m_vs_file_list.size(); i++) {		
		// 이름들 출력
		// 파일이름이 너무 길면 적당히 자른다. 
		const std::string filename = Basic::ShortenDialogLabel(m_vs_file_list[i+m_pC_scroll_bar->GetScrollPos()]);
		const char* p_name = filename.c_str();
		if (p_name[0] == '\\') ++p_name;
		g_PrintColorStr(x+m_string_x, y+m_string_y+i*m_string_gap, p_name, gpC_base->m_desc_msg_pi, RGB_WHITE);
	}	
	m_pC_button_group->ShowDescription();
	
	g_FL2_ReleaseDC();	
	m_pC_scroll_bar->Show(x, y);
	
	//----------------------------------------------------------------------------------------
	// Drive List 출력
	//drive 선택 open 되어있으면
	if(m_bl_open_drive) {
		SetRect(&rect, x+m_string_x-5,y+37+m_string_gap-3, x+m_string_x-5 + 350, static_cast<int>(y+37+m_string_gap-3+m_string_gap*m_directories.paths.size()));
		
		DrawAlphaBox(&rect, 100, 100,100, 20);
		gpC_global_resource->DrawOutBox(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);		

		if(gpC_base->m_p_DDSurface_back->Lock())
		{
			for(int i = 0; static_cast<size_t>(i) < m_directories.paths.size(); i++)	{
				char ch=m_directories.paths[i][0];
				if(ch=='a'||ch=='A'||static_cast<size_t>(i)==m_directories.paths.size()-1) 	m_p_icon_spk->BltLocked(x+m_string_x,y+37+(i+1)*m_string_gap,CDDRIVE_ID);
				else m_p_icon_spk->BltLocked(x+m_string_x,y+37+(i+1)*m_string_gap,HDDRIVE_ID);
			}
			gpC_base->m_p_DDSurface_back->Unlock();
		}
		g_FL2_GetDC();
		for(int i = 0; static_cast<size_t>(i) < m_directories.paths.size(); i++)	{
			std::string filename = Basic::BuildDialogPathLabel(m_directories.paths[i], m_filter);
			filename = Basic::ShortenDialogLabel(filename);
			g_PrintColorStr(x+m_string_x+17, y+37+(i+1)*m_string_gap, filename.c_str(), gpC_base->m_desc_menu_pi, RGB_WHITE);
		}
		g_FL2_ReleaseDC();
	}
	
	//----------------------------------------------------------------------------------------
	// Write the long name out once the folder has been open a while.
	if(Timer(false))
	{
		std::string title;
		if(STAY_FOLDER==0)
		{				// 현재 열린 폴더
			title = Basic::BuildDialogPathLabel(m_directories.paths[m_directories.current], m_filter);
			if(title.size()>38)
			{	
				SetRect(&rect, x+m_string_x+10,y+45,static_cast<int>(x+m_string_x+9*title.size()),y+68);
				DrawAlphaBox(&rect, 0, 0, 0, 20);		
				gpC_global_resource->DrawOutBox(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);
				g_PrintColorStr(x+m_string_x+15, y+49, title.c_str(), gpC_base->m_desc_menu_pi, RGB_WHITE);
			}
		}
		else if(m_bl_open_drive && STAY_FOLDER > 0 && static_cast<size_t>(STAY_FOLDER) <= m_directories.paths.size())
		{		// 드라이브 리스트 중 			
			title = Basic::BuildDialogPathLabel(m_directories.paths[STAY_FOLDER-1], m_filter);
			if(title.size()>38)
			{	
				SetRect(&rect, x+m_string_x+17,y+35+(STAY_FOLDER)*m_string_gap,static_cast<int>(x+m_string_x+17+9*title.size()),y+39+(STAY_FOLDER)*m_string_gap+18);
				DrawAlphaBox(&rect, 0, 0, 0, 20);		
				gpC_global_resource->DrawOutBox(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);
				g_PrintColorStr(x+m_string_x+17, y+37+(STAY_FOLDER)*m_string_gap, title.c_str(), gpC_base->m_desc_menu_pi, RGB_WHITE);
			}			
		} 
		else
		{				// 파일에서 가만히 있을경우
			if(m_tempselect != -1 && m_tempselect >= m_pC_scroll_bar->GetScrollPos() && m_tempselect < m_pC_scroll_bar->GetScrollPos()+m_scroll_max)
			if(m_vs_file_list[m_tempselect].size()>38)	
			{
				SetRect(&rect,  x+m_string_x, y + m_string_y + (m_tempselect - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3, static_cast<int>(x+m_string_x + 8*m_vs_file_list[m_tempselect].size()),y + m_string_y + (m_tempselect - m_pC_scroll_bar->GetScrollPos()) * m_string_gap-3 + m_string_gap);
				DrawAlphaBox(&rect, 0, 0, 0, 20);
				gpC_global_resource->DrawOutBox(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);
				const char *temp=m_vs_file_list[m_tempselect].c_str();
				if(temp[0]=='\\')	
					g_PrintColorStr(rect.left+2,rect.top+2, (1+m_vs_file_list[m_tempselect].c_str()) ,gpC_base->m_desc_msg_pi, RGB_WHITE);
				else g_PrintColorStr(rect.left+2,rect.top+2, (m_vs_file_list[m_tempselect].c_str()) ,gpC_base->m_desc_msg_pi, RGB_WHITE);
			}
		}
	}
	//---------------------------------------------------------------------------------------------------
	// 미리보기
	// 
	if(m_tempselect != -1 && m_tempselect >= m_pC_scroll_bar->GetScrollPos() && m_tempselect < m_pC_scroll_bar->GetScrollPos()+m_scroll_max && m_flag_preview_image)	
	{
		if(!(m_vs_file_list_attr[m_tempselect] & FILE_ATTRIBUTE_DIRECTORY)&&m_mode==MODE_PROFILE_SELECT)
		{
			POINT point = { x+m_string_x-m_temp_face.GetWidth()-2,  
				y + m_string_y + (m_tempselect - m_pC_scroll_bar->GetScrollPos()) * m_string_gap};
			SetRect(&rect,0,0,m_temp_face.GetWidth(), m_temp_face.GetHeight());
			gpC_base->m_p_DDSurface_back->BltNoColorkey(&point,&m_temp_face,&rect);
		}
	}		
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::ShowButtonWidget
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::ShowButtonDescription(C_VS_UI_EVENT_BUTTON *p_button)
{
	static char*	m_help_string[2] = {
		(*g_pGameStringTable)[UI_STRING_MESSAGE_FILE_DIALOG_SELECT_FILE_OK].GetString(),			
		(*g_pGameStringTable)[UI_STRING_MESSAGE_FILE_DIALOG_CANCEL].GetString(),
	};	

	g_descriptor_manager.Set(DID_INFO, x+p_button->x, y+p_button->y, (void *)m_help_string[p_button->GetID()],0,0);	
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::ShowButtonWidget
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::ShowButtonWidget(C_VS_UI_EVENT_BUTTON * p_button)
{			
	assert(p_button);

	
	if(p_button->GetFocusState())
	{
		if(p_button->GetPressState())
			gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index+C_GLOBAL_RESOURCE::AB_BUTTON_PUSHED_OFFSET);
		else
			gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index+C_GLOBAL_RESOURCE::AB_BUTTON_HILIGHTED_OFFSET);
	}
	else
		gpC_global_resource->m_pC_assemble_box_button_spk->BltLocked(x+p_button->x, y+p_button->y, p_button->m_image_index);

}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::IsPixel
//
//
//-----------------------------------------------------------------------------
bool	C_VS_UI_FILE_DIALOG::IsPixel(int _x, int _y)
{
//	return m_p_image_spk->IsPixel(SCR2WIN_X(_x), SCR2WIN_Y(_y));
	return IsInRect(_x, _y);
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::Run
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::Run(id_t id)
{
	int i;
	switch (id)
	{
//	case SCROLL_UP_ID:
//		ScrollDescUp();
//		if(m_scroll > 0)
//			m_scroll--;
//		break;

//	case SCROLL_DOWN_ID:
//		if(m_scroll < m_vs_file_list.size()-m_scroll_max && m_vs_file_list.size() > m_scroll_max)
//			m_scroll++;
//		ScrollDescDown();
//		break;

	case CANCEL_ID:
//		if(m_bl_main_help)
//			gC_vs_ui.HotKey_Help();
//		else
		gpC_base->SendMessage(UI_CLOSE_FILE_DIALOG);
		break;
	//case DRIVE_ID:
	//	m_bl_open_drive = !m_bl_open_drive;
	//	break;
	case OK_ID :
		// SendMessage 로 파일이름을 보내도록 한다.
		//
		if( m_mode == MODE_PROFILE_SELECT )
		{
			if(m_directories.current < m_directories.paths.size() && m_select >= 0 &&
				static_cast<size_t>(m_select) < m_vs_file_list.size() &&
				static_cast<size_t>(m_select) < m_vs_file_list_attr.size() &&
				!(m_vs_file_list_attr[m_select] & FILE_ATTRIBUTE_DIRECTORY) &&
				m_select >= m_pC_scroll_bar->GetScrollPos() && m_select < m_pC_scroll_bar->GetScrollPos()+m_scroll_max)
			{
				m_filename = Basic::DialogDirectoryPath(m_directories.paths[m_directories.current]);
				m_filename += m_vs_file_list[m_select];
				gpC_base->SendTextMessage(UI_CLOSE_FILE_DIALOG, m_mode, 0, m_filename);
			} 
			else
				gpC_base->SendMessage(UI_CLOSE_FILE_DIALOG, m_mode, 0, NULL);
		} else
		{
			if( m_select_file_num.size() > 0 )
			{
				// 파일이 여러개 선택된 경우에는 세미콜론으로 파일을 구분한다.
				std::string multiple_filename;
				
				for( i = 0; static_cast<size_t>(i) < m_select_file_num.size(); i++ )
				{				
					multiple_filename += m_vs_file_list[m_select_file_num[i]];
					multiple_filename += ";";
				}
				multiple_filename.erase(multiple_filename.end()-1, multiple_filename.end());	
				gpC_base->SendTextMessage(UI_CLOSE_FILE_DIALOG, m_mode, 0, multiple_filename);
				
			} else
				gpC_base->SendMessage(UI_CLOSE_FILE_DIALOG, m_mode, 0, NULL);
		}
		break;

	}
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::Process
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::Process()
{
	m_pC_button_group->Process();	
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::UnacquireMouseFocus
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::UnacquireMouseFocus()
{
	m_pC_button_group->UnacquireMouseFocus();
}

//-----------------------------------------------------------------------------
// C_VS_UI_FILE_DIALOG::Finish
//
//
//-----------------------------------------------------------------------------
void	C_VS_UI_FILE_DIALOG::Finish()
{
	PI_Processor::Finish();

	gpC_window_manager->DisappearWindow(this);

	if(m_p_image_spk)
	{
		DeleteNew(m_p_image_spk);
	}
	if(m_p_icon_spk)
		DeleteNew(m_p_icon_spk);	
}

/*-----------------------------------------------------------------------------
- RefreshFileList
-

  Running this method over and over is a severe slowdown when the directory
  holds a great many files, so it is run only when the list has to be
  refreshed.

  Refreshes the selected owned directory path.
-----------------------------------------------------------------------------*/
void C_VS_UI_FILE_DIALOG::RefreshFileList(std::string& sz_dirname)
{
	std::string			sz_filename;

	Basic::NormalizeDialogSearchPath(sz_dirname);

	m_vs_file_list.clear();
	m_vs_file_list_attr.clear();
	
	// The owned string is a search pattern, "<dir>\*.*"; ListDirectory wants
	// the directory and the pattern apart. Three characters come off, not
	// four, so the '\' is kept and a drive root stays the absolute "C:\".
	// The pattern is "*", not "*.*": the '.' is a literal to the helper.
	const std::string sz_directory = Basic::DialogDirectoryPath(sz_dirname);
	std::vector<Basic::SDirectoryEntry>	v_entries;
	std::vector<Basic::SDirectoryEntry>	v_listed;

	// directory_iterator does not return "..", and ".." is the entry the
	// user clicks to go up, so it is synthesised - first, where
	// FindFirstFile delivered it, and whether or not the listing succeeds.
	// A volume root has no parent, which has_relative_path() tests for.
	if (std::filesystem::path(sz_directory).has_relative_path())
	{
		Basic::SDirectoryEntry	parent_entry;

		parent_entry.sName = "..";
		parent_entry.bIsDirectory = true;

		v_entries.push_back(parent_entry);
	}

	if (Basic::ListDirectory(sz_directory.c_str(), "*", v_listed,
			Basic::LIST_FILES_AND_DIRECTORIES))
	{
		v_entries.insert(v_entries.end(), v_listed.begin(), v_listed.end());
	}

	for (size_t i_entry = 0; i_entry < v_entries.size(); i_entry++)
	{
		const char	*sz_entry_name = v_entries[i_entry].sName.c_str();

		// m_vs_file_list_attr is only ever tested for FILE_ATTRIBUTE_DIRECTORY
		// (see MouseControl() and Show()), so the directory bit is all that
		// has to be reproduced here.
		DWORD		dw_attributes = v_entries[i_entry].bIsDirectory ?
							FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

		// The '\.' entry is skipped.
		//if ((dw_attributes & FILE_ATTRIBUTE_DIRECTORY) && sz_entry_name == ".")
		if (dw_attributes & FILE_ATTRIBUTE_DIRECTORY)
			if (sz_entry_name[0] == '.' &&
				 sz_entry_name[1] == '\0') continue;

		if (dw_attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			sz_filename = "\\";
			sz_filename += sz_entry_name;
		}
		else
		{
//			strcpy(str_buf, sz_entry_name);
			sz_filename = sz_entry_name;

			if (!Basic::MatchesAnySuffixCaseInsensitive(sz_filename, m_filter))
				continue;
		}

		Basic::InsertDialogEntry(m_vs_file_list, m_vs_file_list_attr,
				sz_filename, dw_attributes);

//		gC_ui.AddListUnit(dp, str_buf, dw_attributes, true);
	}

	m_pC_scroll_bar->SetScrollPos(0);
	m_select = -1;
	m_tempselect = -1;
	m_select_file_num.clear();
	m_flag_preview_image = false;

}

/*-----------------------------------------------------------------------------
- ChangeDir
-
-----------------------------------------------------------------------------*/
void C_VS_UI_FILE_DIALOG::ChangeDir(const char* sz_cur_dirname, std::string& sz_pathname)
{
	Basic::ChangeDialogSearchPath(sz_cur_dirname, sz_pathname);
	RefreshFileList(m_directories.paths[m_directories.current]);
	//if(m_vs_file_list.size()<=0)

	m_select = -1;
	m_tempselect = -1;

	EMPTY_MOVE;
}

int		C_VS_UI_FILE_DIALOG::GetMode()
{
	return m_mode;
}
