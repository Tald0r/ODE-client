#include "Client_PCH.h"
#include "InventoryEffectTarget.h"

std::unique_ptr<MEffectTarget> PrepareInventoryEffectTarget(BYTE phases,
	int x, int y, TYPE_OBJECTID itemID, DWORD delayFrame,
	std::unique_ptr<MActionResultNode> action)
{
	auto target = std::make_unique<MEffectTarget>(phases);
	target->Set(x, y, 0, itemID);
	target->SetDelayFrame(delayFrame);
	if (action)
	{
		auto result = std::make_unique<MActionResult>();
		// Add consumes its node even if allocating the queue entry throws.
		result->Add(action.release());
		target->SetResult(result.release());
	}
	return target;
}
