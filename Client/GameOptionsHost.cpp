#include "Client_PCH.h"
#include "GameOptionsHost.h"

namespace GameOptions {

namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* old = s_Host;
	s_Host = host;
	return old;
}

bool BloodDrop()
{
	return s_Host && s_Host->BloodDrop && s_Host->BloodDrop();
}

bool ChatWhite()
{
	return s_Host && s_Host->ChatWhite && s_Host->ChatWhite();
}

std::uint32_t ChattingColor()
{
	return s_Host && s_Host->ChattingColor ? s_Host->ChattingColor() : 0x00FFFFFFu;
}

}
