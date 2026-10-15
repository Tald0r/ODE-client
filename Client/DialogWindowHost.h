#pragma once

#include <cstdint>

// The two kinds of VS_UI dialog window UIDialog opens (task 4.238): the
// NPC talk window, with the NPC's picture and a menu of answers, and the
// message box. A Window is the host's dialog, opaque here: UIDialog keeps
// it and hands it back. When the player picks an answer or a button, the
// host calls UIDialog::ProcessPCTalkDlg or UIDialog::ProcessMessageDlg
// with the window and the ID picked. Without a host no window opens (the
// New entries answer NULL) and the rest is skipped, but UIDialog still
// locks the game's input for the window it asked for, and its Close
// calls, which free only a window they have, leave the lock set; the
// executable installs the host before any window opens.
namespace DialogWindow {

struct Window;

// A menu line: its text, borrowed until SetMenu returns, and the ID that
// picking it reports.
struct MenuItem
{
	const char*		text;	// never NULL ("" for an empty line)
	std::uint32_t	id;
};

// The ID a window reports when it is left without a choice (VS_UI's
// DIALOG_EXECID_EXIT; GameInit.cpp checks the two agree).
constexpr std::uint32_t EXECID_EXIT = 0xFFFF0000u;

struct Host
{
	// The talk window, with the creature type's picture and the name.
	Window* (*NewTalk)(int creatureType, const char* name) = nullptr;
	// A message box three units wide at (x, y) (-1 centres it), 0 or 1
	// units taller than one line, with the buttons asked for.
	Window* (*NewMessage)(int x, int y, int extraHeight, std::uint16_t buttons) = nullptr;
	// The talk window's answers, shown below its message.
	void (*SetMenu)(Window* pWindow, const MenuItem* pItems, int count) = nullptr;
	// The window's text lines, borrowed for the window's life. noFit keeps
	// the box its own size rather than fitting it to the text.
	void (*SetMessage)(Window* pWindow, const char* const* pLines, int count, bool noFit) = nullptr;
	// Shows the window, hides it, picks the ID as the player would, and
	// frees it.
	void (*Start)(Window* pWindow) = nullptr;
	void (*Finish)(Window* pWindow) = nullptr;
	void (*Run)(Window* pWindow, std::uint32_t id) = nullptr;
	void (*Delete)(Window* pWindow) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
Window* NewTalk(int creatureType, const char* name);
Window* NewMessage(int x, int y, int extraHeight, std::uint16_t buttons);
void SetMenu(Window* pWindow, const MenuItem* pItems, int count);
void SetMessage(Window* pWindow, const char* const* pLines, int count, bool noFit);
void Start(Window* pWindow);
void Finish(Window* pWindow);
void Run(Window* pWindow, std::uint32_t id);
void Delete(Window* pWindow);

}
