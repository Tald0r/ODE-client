#include "Client_PCH.h"
#include "MActionResult.h"

#include <algorithm>
#include <memory>

// Track the entire recursive call chain without allocating. The raw identity
// stays registered while unique_ptr destroys the node, so destructor callbacks
// cannot transfer that same node back into the queue.
struct MActionResult::ActiveNode
{
	MActionResult& queue;
	ActiveNode* previous;
	MActionResultNode* const node;
	std::unique_ptr<MActionResultNode> owned;

	ActiveNode(MActionResult& owner, MActionResultNode* value)
		: queue(owner), previous(owner.m_pActive), node(value), owned(value)
	{
		queue.m_pActive = this;
	}
	~ActiveNode()
	{
		owned.reset();
		queue.m_pActive = previous;
	}
};

MActionResult::MActionResult()
{
}

MActionResult::~MActionResult()
{
	Release();
}

void MActionResult::Release()
{
	while (!m_List.empty())
	{
		ActiveNode current(*this, m_List.front());
		m_List.pop_front();
	}
}

void MActionResult::Add(MActionResultNode* node)
{
	if (!node || std::find(m_List.begin(), m_List.end(), node) != m_List.end()) return;
	for (const auto* active = m_pActive; active; active = active->previous)
		if (active->node == node) return;

	ActiveNode incoming(*this, node);
	m_List.push_back(node);
	incoming.owned.release();
}

void MActionResult::Execute()
{
	while (!m_List.empty())
	{
		ActiveNode current(*this, m_List.front());
		m_List.pop_front();
		if (current.node) current.node->Execute();
	}
}
