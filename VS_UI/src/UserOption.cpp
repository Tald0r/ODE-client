//----------------------------------------------------------------------
// UserOption.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "Client_PCH.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "DataPath.h"
#ifdef PLATFORM_WINDOWS
#include <DInput.h>
#endif
#include <cstdio>
#include <cstring>


//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
UserOption*		g_pUserOption = NULL;

//----------------------------------------------------------------------
// 
// constructor
//
//----------------------------------------------------------------------
UserOption::UserOption()
{	
	Use3DHAL			= FALSE;
	UseSmoothCursor		= FALSE;//TRUE;
	DrawMinimap			= TRUE;		// minimap을 그릴까?
	DrawZoneName		= TRUE;		// Zone이름 출력
	DrawGameTime		= TRUE;		// 게임 시간 출력
	DrawInterface		= FALSE;		// interface
	DrawFPS				= FALSE;		// FPS
	BlendingShadow		= FALSE;//TRUE;			// 그림자 반투명
	FilteringCurse		= TRUE;		// 나쁜 말 제거
	PlayMusic			= TRUE;		// 음악 출력
	PlaySound			= TRUE;		// 효과음 출력
	VolumeMusic			= 15;		// 음악 소리크기
	VolumeSound			= 15;		// 효과음 소리 크기
	UseHelpEvent		= TRUE;		// 도움말 사용
	PlayWaveMusic		= TRUE;		// Wav로 음악 출력하기(아니면 MID로)
	BloodDrop			= TRUE;		// HP 낮을 때 피 흘리기
	OpenQuickSlot		= FALSE;
	UseHalfFrame		= FALSE;
	DrawTransHPBar		= TRUE;
	UseForceFeel		= FALSE;
	UseGammaControl		= TRUE;
	GammaValue			= 100;
	DrawChatBoxOutline	= TRUE;

	//new interface
	std::memset(BackupID, 0, sizeof BackupID);
	UseEnterChat		= FALSE;
	UseMouseSpeed		= FALSE;
	MouseSpeedValue		= 50;
	PlayYellSound		= TRUE;
	ShowChoboHelp		= TRUE;
	TribeChange			= FALSE;
	DenyPartyInvite		= FALSE;
	DenyPartyRequest	= FALSE;
	AutoHideSmoothScroll = TRUE;
	ChattingColor		= RGB(198, 195, 198);
	ALPHA_DEPTH			= 23;
	DefaultAlpha		= FALSE;
	IsPreLoadMonster	= TRUE;
	ChatWhite			= FALSE;
	UseTeenVersion		= FALSE;				// 틴버전으로 게임하기
	PopupChatByWhisper	= TRUE;			// 귓속말 왔을때 채팅창 잠깐 보이기
	NotSendMyInfo = FALSE;
	DoNotShowWarMsg = FALSE;
	DoNotShowLairMsg = FALSE;
	DoNotShowHolyLandMsg = FALSE;
	Chinese = FALSE;
	Korean = TRUE;
	Japanese = FALSE;
	English = FALSE;
	persnalShopupdatetime = 0;
	ShowGameMoneyWithHANGUL = FALSE;
	DoNotShowPersnalShopMsg = FALSE;
	UseXbrz = TRUE;
}

UserOption::~UserOption()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void	
UserOption::SaveToFile(const char* filename)
{
	// std::ofstream file(filename, ios::binary);	
	if (filename == NULL) return;
	// FileDef.inf names this file the Windows way, UserSet\UserOption.set;
	// off Windows the name is resolved against the disk (basic/DataPath.h).
	FILE* file = Basic::OpenFile(Basic::NormalizeDataPath(filename).c_str(), "w");
	if (file == NULL) return;

	DWORD flag = 0;
	fwrite((void*)&flag, 1, 4, file);
	g_pKeyAccelerator->SaveToFile(file);

	fprintf(file, "\n========\n");

	fprintf(file, "%d	UseSmoothCursor\n", UseSmoothCursor);
	fprintf(file, "%d	DrawMinimap\n", DrawMinimap);
	fprintf(file, "%d	DrawGameTime\n", DrawGameTime);
	fprintf(file, "%d	DrawInterface\n", DrawInterface);
	fprintf(file, "%d	DrawFPS\n", DrawFPS);
	fprintf(file, "%d	BlendingShadow\n", BlendingShadow);
	fprintf(file, "%d	FilteringCurse\n", FilteringCurse);
	fprintf(file, "%d	PlayMusic\n", PlayMusic);
	fprintf(file, "%d	PlaySound\n", PlaySound);
	fprintf(file, "%d	VolumeMusic\n", VolumeMusic);
	fprintf(file, "%d	VolumeSound\n", VolumeSound);
	fprintf(file, "%d	UseHelpEvent\n", UseHelpEvent);
	fprintf(file, "%d	PlayWaveMusic\n", PlayWaveMusic);
	fprintf(file, "%d	BloodDrop\n", BloodDrop);
	fprintf(file, "%d	OpenQuickSlot\n", OpenQuickSlot);
	fprintf(file, "%d	UseHalfFrame\n", UseHalfFrame);
	fprintf(file, "%d	Use3DHAL\n", Use3DHAL);
	fprintf(file, "%d	DrawTransHPBar\n", DrawTransHPBar);
	fprintf(file, "%d	UseForceFeel\n", UseForceFeel);
	fprintf(file, "%d	GammaValue\n", GammaValue);
	fprintf(file, "%d	DrawChatBoxOutline\n", DrawChatBoxOutline);

	// new interface
	// The legacy disk field is 15 bytes; BackupID is only 11. Pad the
	// field explicitly rather than reading padding and UseEnterChat.
	char backupField[15] = {};
	for (size_t i = 0; i < sizeof BackupID - 1 && BackupID[i] != '\0'; ++i)
		backupField[i] = BackupID[i];
	fwrite(backupField, sizeof backupField, 1, file);
	fprintf(file, "%d	UseEnterChat\n", UseEnterChat);
	fprintf(file, "%d	MouseSpeedValue\n", MouseSpeedValue);
	fprintf(file, "%d	PlayYellSound\n", PlayYellSound);
	fprintf(file, "%d	ShowChoboHelp\n", ShowChoboHelp);
	fprintf(file, "%d	TribeChange\n", TribeChange);
	fprintf(file, "%d	DenyPartyInvite\n", DenyPartyInvite);
	fprintf(file, "%d	DenyPartyRequest\n", DenyPartyRequest);
	fprintf(file, "%d	AutoHideSmoothScroll\n", AutoHideSmoothScroll);
	fprintf(file, "%d	ChattingColor\n", ChattingColor);
	fprintf(file, "%d	ALPHA_DEPTH\n", ALPHA_DEPTH);
	fprintf(file, "%d	DefaultAlpha\n", DefaultAlpha);
	fprintf(file, "%d	IsPreLoadMonster\n", IsPreLoadMonster);
	fprintf(file, "%d	ChatWhite\n", ChatWhite);
	fprintf(file, "%d	UseTeenVersion\n", UseTeenVersion);
	fprintf(file, "%d	PopupChatByWhisper\n", PopupChatByWhisper);
	fprintf(file, "%d	NotSendMyInfo\n", NotSendMyInfo);
	fprintf(file, "%d	DoNotShowWarMsg\n", DoNotShowWarMsg);
	fprintf(file, "%d	DoNotShowLairMsg\n", DoNotShowLairMsg);
	fprintf(file, "%d	DoNotShowHolyLandMsg\n", DoNotShowHolyLandMsg);
	fprintf(file, "%d	ShowGameMoneyWithHANGUL\n", ShowGameMoneyWithHANGUL);
	fprintf(file, "%d	DoNotShowPersnalShopMsg\n", DoNotShowPersnalShopMsg);
	// Append new options so existing settings retain their field order.
	fprintf(file, "%d\tUseXbrz\n", UseXbrz);

	fclose(file);
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
bool	
UserOption::LoadFromFile(const char* filename)
{
	UseXbrz = TRUE; // Older or missing settings files enable smoothing.
	if (filename == NULL) return false;
	FILE *file = Basic::OpenFile(Basic::NormalizeDataPath(filename).c_str(), "r");
	if (file == NULL) {
		return false;
	}
	
	DWORD flag = 0;
	fread((void*)&flag, 1, 4, file);
	g_pKeyAccelerator->LoadFromFile(file);
	
//	if(flag == 0)
//	{
//		g_pKeyAccelerator->SetAcceleratorKey(ACCEL_GRADE1INFO, DIK_R);
//		file.seekg(-2, ios::cur);
//	}

	char ignore[256];
	Basic::ScanFile(file, "\n%s\n", CRT_BUFFER(ignore)); // ignore =======

	Basic::ScanFile(file, "%d	%s\n", &UseSmoothCursor, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawMinimap, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawGameTime, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawInterface, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawFPS, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &BlendingShadow, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &FilteringCurse, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &PlayMusic, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &PlaySound, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &VolumeMusic, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &VolumeSound, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &UseHelpEvent, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &PlayWaveMusic, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &BloodDrop, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &OpenQuickSlot, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &UseHalfFrame, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &Use3DHAL, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawTransHPBar, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &UseForceFeel, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &GammaValue, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d	%s\n", &DrawChatBoxOutline, CRT_BUFFER(ignore));

	// new interface
	char backupField[15] = {};
	if (fread(backupField, 1, sizeof backupField, file) != sizeof backupField) {
		fclose(file);
		return false;
	}
	std::memcpy(BackupID, backupField, sizeof BackupID - 1);
	BackupID[sizeof BackupID - 1] = '\0';
	Basic::ScanFile(file, "%d %s\n", &UseEnterChat, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &MouseSpeedValue, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &PlayYellSound, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &ShowChoboHelp, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &TribeChange, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DenyPartyInvite, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DenyPartyRequest, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &AutoHideSmoothScroll, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &ChattingColor, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%hhu %s\n", &ALPHA_DEPTH, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DefaultAlpha, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &IsPreLoadMonster, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &ChatWhite, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &UseTeenVersion, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &PopupChatByWhisper, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &NotSendMyInfo, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DoNotShowWarMsg, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DoNotShowLairMsg, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DoNotShowHolyLandMsg, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &ShowGameMoneyWithHANGUL, CRT_BUFFER(ignore));
	Basic::ScanFile(file, "%d %s\n", &DoNotShowPersnalShopMsg, CRT_BUFFER(ignore));

	int xbrz = 1;
	if (Basic::ScanFile(file, "%d %255s", &xbrz, CRT_BUFFER(ignore)) == 2 && strcmp(ignore, "UseXbrz") == 0
		&& (xbrz == 0 || xbrz == 1))
		UseXbrz = xbrz;

	fclose(file);
	return true;
}
