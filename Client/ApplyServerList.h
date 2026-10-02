#ifndef __APPLYSERVERLIST_H__
#define __APPLYSERVERLIST_H__

class CServerInformation;
class LCWorldList;
class LCServerList;

// Replace the worlds and their servers, consuming the packet's records.
// Select the requested nonzero world if it is listed, otherwise the first
// record's world. Duplicate IDs update the same record in packet order.
void ApplyWorldList(CServerInformation& selection, LCWorldList& packet);

// Update servers within the selected world, retaining unlisted servers.
// Select the requested nonzero server if listed, otherwise the first record's
// server. Return false only when the selected world is missing, in which case
// neither the model nor the packet is changed. True includes an empty list,
// matching the handler's existing UI/mode transition after an accepted list.
bool ApplyServerList(CServerInformation& selection, LCServerList& packet);

#endif
