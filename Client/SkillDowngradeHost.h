#pragma once

// Skill downgrade replies mutate model state before reporting their outcome.
// A missing presentation service skips the popup while model updates still run.
namespace SkillDowngrade {

struct Host
{
	void (*PopupMessage)(int gameStringID) = nullptr;
};

// Borrowed until replaced; presentation reads the current service each time.
const Host* SetHost(const Host* host);
void PopupMessage(int gameStringID);

}
