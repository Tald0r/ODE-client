#pragma once

#include "MEffectTarget.h"
#include <memory>

// Prepare all result ownership before handing the target to a generator, which
// may complete or discard it synchronously. A missing action needs no result.
std::unique_ptr<MEffectTarget> PrepareInventoryEffectTarget(BYTE phases,
	int x, int y, TYPE_OBJECTID itemID, DWORD delayFrame,
	std::unique_ptr<MActionResultNode> action);
