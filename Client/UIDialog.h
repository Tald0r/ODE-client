//-----------------------------------------------------------------------------
// UIDialog.h
//-----------------------------------------------------------------------------
// The client's dialogs: the NPC talk box and the message box (task 4.238:
// gamemodel's since then; the windows are DialogWindow's).
//-----------------------------------------------------------------------------

#ifndef __UIDIALOG_H__
#define __UIDIALOG_H__

#include "../basic/Platform.h"
#include "DialogWindowHost.h"

#define UI_DIALOG_OK							0x0001

#define	FLAG_UIDIALOG_LOCK_PCTALK		0x01
#define	FLAG_UIDIALOG_LOCK_MESSAGE		0x02

class UIDialog {
	public :
		enum UIDIALOG_MESSAGE
		{
			MESSAGE_PCTALK,
			MESSAGE_FREE,

			MAX_MESSAGE
		};

	public :
		UIDialog();
		~UIDialog();

		//---------------------------------------------------------
		// Release
		//---------------------------------------------------------
		void			Init();
		void			Release();

		//---------------------------------------------------------
		// Lock Input
		//---------------------------------------------------------
		static void		SetLockInputPCTalk()		{ s_LockGameInput |= FLAG_UIDIALOG_LOCK_PCTALK; }
		static void		SetLockInputMessage()		{ s_LockGameInput |= FLAG_UIDIALOG_LOCK_MESSAGE; }
		static void		UnSetLockInputPCTalk()		{ s_LockGameInput &= ~FLAG_UIDIALOG_LOCK_PCTALK; }
		static void		UnSetLockInputMessage()		{ s_LockGameInput &= ~FLAG_UIDIALOG_LOCK_MESSAGE; }
		static DWORD	IsLockInput()				{ return s_LockGameInput; }

		//---------------------------------------------------------
		// PC Talk Dlg
		//---------------------------------------------------------
		void			PopupPCTalkDlg(int x=-1, int y=-1);		// -1 centres it (the default)
		void			HidePCTalkDlg();
		void			ShowPCTalkDlg();
		static void		ProcessPCTalkDlg(DialogWindow::Window* pDlg, id_t id);
		void			ClosePCTalkDlg();

		//---------------------------------------------------------
		// Message Dlg
		//---------------------------------------------------------
		void			PopupFreeMessageDlg(const char* msg, int x=-1, int y=-1, WORD fButton = UI_DIALOG_OK, bool IsAfterExit = false);		// -1 centres it (the default)
		static void		ProcessMessageDlg(DialogWindow::Window* pDlg, id_t id);
		void			CloseMessageDlg();

	protected :
		static DWORD	s_LockGameInput;	// blocks the game's input

		// Picking one of the PC talk box's lines
		DialogWindow::Window*		m_pPCTalkDlg;
		DialogWindow::Window*		m_pMessageDlg;


		int 			m_ppDlgMessageSize[MAX_MESSAGE];
		char **			m_ppDlgMessage[MAX_MESSAGE];
};

extern UIDialog*			g_pUIDialog;

#endif
