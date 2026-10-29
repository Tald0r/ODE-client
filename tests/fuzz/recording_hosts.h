//----------------------------------------------------------------------
// recording_hosts.h
//----------------------------------------------------------------------
//
// The presentation hosts the handler targets install in place of the
// executable's (task 4 of docs/reviews/NEXT.md). The real adapters call
// into VS_UI, which the world never starts (gC_vs_ui is not initialised,
// so g_pUIDialog and gpC_mouse_pointer are NULL), so these do what the
// real ones do to the model objects a handler hands them, which is what
// a sanitizer can see:
//
//   - an entry whose real adapter reads its object reads it here too,
//     so ASan reports an object a handler freed before it handed it on;
//     PickUpItem's (UI_PickUpItem) reads it without testing it, so a
//     NULL aborts here, the harness's crash path, as it would crash
//     there;
//   - the shop, the storage, the pet storage and the personal shop
//     windows draw from the object they were given on every frame
//     (their Show walks its slots). As in VS_UI, a Run entry opens a
//     window with no object if none is open, a Set entry hands an open
//     window its object and is dropped while it is closed, and a close
//     forgets it; the
//     closes are those a handler reaches (ZoneLoading::CloseWindows
//     closes the shop and the storage, ClosePersnalShop the personal
//     shop) and CloseAllDialog, taken to close all four, which can only
//     miss a finding. IsRunning answers from that. At the end of a run
//     every open window's object is walked as its Show walks it, so ASan
//     reports one a later packet freed while the window still drew it.
//     A free that a later packet in the same run hides, by closing the
//     window or handing it another object, is not seen.
//
// What the real adapters do with a handler's integers is not modelled;
// the walks of packet input safety followed them by reading.
//
//----------------------------------------------------------------------

#ifndef __RECORDING_HOSTS_H__
#define __RECORDING_HOSTS_H__

#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "ZoneLoadingHost.h"
#include "MCreature.h"
#include "MItem.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MStorage.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace RecordingHosts {

// What a read leaves, so the compiler keeps the read.
inline volatile unsigned long	g_Sink = 0;

inline void	Read(const MItem* pItem)
{
	if (pItem != NULL)
		g_Sink += pItem->GetID() + pItem->GetItemClass();
}

inline void	Read(const MCreature* pCreature)
{
	if (pCreature != NULL)
		g_Sink += pCreature->GetID();
}

// As C_VS_UI_STORAGE::Show and the personal shop's walk the slots of the
// current storage.
inline void	Read(const MStorage* pStorage)
{
	if (pStorage == NULL)
		return;
	g_Sink += pStorage->GetSize() + pStorage->GetCurrent();
	for (unsigned int slot = 0; slot < STORAGE_SLOT; ++slot)
		Read(pStorage->GetItem(slot));
}

// C_VS_UI_SHOP::Show walks the current shelf's items; every shelf's is a
// superset of that.
inline void	Read(const MShop* pShop)
{
	if (pShop == NULL)
		return;
	for (unsigned int i = 0; i < pShop->GetSize(); ++i)
		if (const MShopShelf* pShelf = pShop->GetShelf(i))
			for (unsigned int slot = 0; slot < SHOP_SHELF_SLOT; ++slot)
				Read(pShelf->GetItem(slot));
}

// A window the UI draws an object in while it is open.
template <class T>
struct Window
{
	T*		pObject = NULL;
	bool	bOpen = false;

	// A window that is open stays as it is, as VS_UI makes one only once.
	void	Run()			{ if (!bOpen) { bOpen = true; pObject = NULL; } }
	void	Set(T* p)		{ if (bOpen) { pObject = p; Read(p); } }
	void	Close()			{ bOpen = false; pObject = NULL; }
	void	ReadIfOpen() const	{ if (bOpen) Read(pObject); }
};

inline Window<MShop>	g_Shop;
inline Window<MStorage>	g_Storage;
inline Window<MStorage>	g_PetStorage;
inline Window<MStorage>	g_PersnalShop;

inline void	Reset()
{
	g_Shop.Close();
	g_Storage.Close();
	g_PetStorage.Close();
	g_PersnalShop.Close();
}

// The end of a run, before the world frees what it made.
inline void	ReadOpenWindows()
{
	g_Shop.ReadIfOpen();
	g_Storage.ReadIfOpen();
	g_PetStorage.ReadIfOpen();
	g_PersnalShop.ReadIfOpen();
}

// Installed over the empty hosts, after the model's; every other entry
// stays empty, and a missing entry is skipped as before.
inline void	Install()
{
	Reset();

	static DialogPresentation::Host		dialog;
	static GameInfoPresentation::Host	info;
	static VsUiPresentation::Host		ui;
	static ZoneLoading::Host			loading;

	dialog = {};
	dialog.PickUpItem = [](MItem* pItem) {
		if (pItem == NULL)
		{
			std::fprintf(stderr, "recording_hosts: PickUpItem(NULL), which UI_PickUpItem reads\n");
			std::abort();
		}
		Read(pItem);
	};
	dialog.OkMixingForge = [](std::uint32_t, MItem* pItem, MItem* pItem2) { Read(pItem); Read(pItem2); };
	dialog.OkRemoveOption = [](std::uint32_t, MItem* pItem) { Read(pItem); };
	dialog.RunShop = []() { g_Shop.Run(); };
	dialog.SetShop = [](MShop* pShop) { g_Shop.Set(pShop); };
	dialog.IsRunningShop = []() { return g_Shop.bOpen; };
	dialog.RunStorage = []() { g_Storage.Run(); };
	dialog.SetStorage = [](MStorage* pStorage) { g_Storage.Set(pStorage); };
	dialog.IsRunningStorage = []() { return g_Storage.bOpen; };
	dialog.RunPetStorage = []() { g_PetStorage.Run(); };
	dialog.SetPetStorage = [](MStorage* pStorage) { g_PetStorage.Set(pStorage); };
	dialog.CloseAllDialog = []() { Reset(); };
	DialogPresentation::SetHost(&dialog);

	info = {};
	info.RunOtherInfo = [](MCreature* pCreature) { Read(pCreature); };
	GameInfoPresentation::SetHost(&info);

	ui = {};
	ui.RunQuestInventory = [](const MItem* pItem) { Read(pItem); };
	ui.RunXmasCardWindow = [](const MItem* pItem) { Read(pItem); };
	ui.RunBulletinBoardWindow = [](const MItem* pItem) { Read(pItem); };
	ui.RunPersnalShop = []() { g_PersnalShop.Run(); };
	ui.SetPersnalShop = [](MStorage* pStorage) { g_PersnalShop.Set(pStorage); };
	ui.ClosePersnalShop = []() { g_PersnalShop.Close(); };
	ui.IsRunningPersnalShop = []() { return g_PersnalShop.bOpen; };
	VsUiPresentation::SetHost(&ui);

	loading = {};
	loading.CloseWindows = []() { g_Shop.Close(); g_Storage.Close(); };
	ZoneLoading::SetHost(&loading);
}

} // namespace RecordingHosts

#endif
