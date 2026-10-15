#include "Client_PCH.h"
#include "DialogWindowHost.h"

namespace DialogWindow {

namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* old = s_Host;
	s_Host = host;
	return old;
}

Window* NewTalk(int creatureType, const char* name)
{
	return s_Host && s_Host->NewTalk ? s_Host->NewTalk(creatureType, name) : nullptr;
}

Window* NewMessage(int x, int y, int extraHeight, std::uint16_t buttons)
{
	return s_Host && s_Host->NewMessage ? s_Host->NewMessage(x, y, extraHeight, buttons) : nullptr;
}

void SetMenu(Window* pWindow, const MenuItem* pItems, int count)
{
	if (s_Host && s_Host->SetMenu) s_Host->SetMenu(pWindow, pItems, count);
}

void SetMessage(Window* pWindow, const char* const* pLines, int count, bool noFit)
{
	if (s_Host && s_Host->SetMessage) s_Host->SetMessage(pWindow, pLines, count, noFit);
}

void Start(Window* pWindow)
{
	if (s_Host && s_Host->Start) s_Host->Start(pWindow);
}

void Finish(Window* pWindow)
{
	if (s_Host && s_Host->Finish) s_Host->Finish(pWindow);
}

void Run(Window* pWindow, std::uint32_t id)
{
	if (s_Host && s_Host->Run) s_Host->Run(pWindow, id);
}

void Delete(Window* pWindow)
{
	if (s_Host && s_Host->Delete) s_Host->Delete(pWindow);
}

}
