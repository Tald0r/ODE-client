#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "Lpackets/LCWorldList.h"
#include "Lpackets/LCServerList.h"
#include "DebugLog.h"

void ApplyWorldList(CServerInformation& selection, LCWorldList& packet)
{
	selection.Release();

	const int currentID = packet.getCurrentWorldID();
	const int count = packet.getListNum();
	int firstID = 0;
	bool hasDefault = false;

	for (int i = 0; i < count; ++i)
	{
		WorldInfo* info = packet.popFrontListElement();
		if (info != nullptr)
		{
			if (i == 0)
				firstID = info->getID();
			if (info->getID() == currentID)
				hasDefault = true;

			ServerGroup* world = selection.GetData(info->getID());
			if (world == nullptr)
			{
				world = new ServerGroup;
				selection.AddData(info->getID(), world);
			}
			world->SetGroupName(info->getName().c_str());
			world->SetGroupStatus(static_cast<int>(info->getStat()));
			delete info;
		}
		else
		{
			DEBUG_ADD_ERR("[Error] ServerGroupInfo is NULL");
		}
	}

	selection.SetServerGroupID(currentID == 0 || !hasDefault ? firstID : currentID);
}

bool ApplyServerList(CServerInformation& selection, LCServerList& packet)
{
	const int groupID = selection.GetServerGroupID();
	ServerGroup* world = selection.GetData(groupID);
	if (world == nullptr)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] ServerGroup(%d) is NULL", groupID);
		return false;
	}

	// Login replies contain a complete snapshot for the selected world.
	// Keep its object and metadata while removing servers no longer advertised.
	world->Release();
	selection.ClearServerSelection();

	const int currentID = packet.getCurrentServerGroupID();
	const int count = packet.getListNum();
	int firstID = 0;
	bool hasDefault = false;

	for (int i = 0; i < count; ++i)
	{
		ServerGroupInfo* info = packet.popFrontListElement();
		if (info != nullptr)
		{
			if (i == 0)
				firstID = info->getGroupID();
			if (info->getGroupID() == currentID)
				hasDefault = true;

			SERVER_INFO* server = world->GetData(info->getGroupID());
			if (server == nullptr)
			{
				server = new SERVER_INFO;
				world->AddData(info->getGroupID(), server);
			}
			server->ServerName = info->getGroupName().c_str();
			server->ServerStatus = static_cast<int>(info->getStat());
			delete info;
		}
		else
		{
			DEBUG_ADD_ERR("[Error] ServerGroupInfo is NULL");
		}
	}

	selection.SetServerID(currentID == 0 || !hasDefault ? firstID : currentID);
	return true;
}
