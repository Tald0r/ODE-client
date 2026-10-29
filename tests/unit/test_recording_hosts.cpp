//----------------------------------------------------------------------
// test_recording_hosts.cpp
//----------------------------------------------------------------------
//
// The recording presentation hosts the handler fuzz targets install
// (tests/fuzz/recording_hosts.h). As in VS_UI, a window the UI keeps a
// model object in opens with none, takes the one a Set entry hands it
// only while open, and forgets it when a close the real UI makes closes
// it; IsRunning answers from that. The end of a run reads the objects of
// the windows still open, and only those.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "../fuzz/recording_hosts.h"

namespace {

// The hosts the test replaces, put back when it ends.
struct Hosts
{
	const DialogPresentation::Host*		dialog = DialogPresentation::SetHost(nullptr);
	const GameInfoPresentation::Host*	info = GameInfoPresentation::SetHost(nullptr);
	const VsUiPresentation::Host*		ui = VsUiPresentation::SetHost(nullptr);
	const ZoneLoading::Host*			loading = ZoneLoading::SetHost(nullptr);
	~Hosts()
	{
		RecordingHosts::Reset();
		ZoneLoading::SetHost(loading);
		VsUiPresentation::SetHost(ui);
		GameInfoPresentation::SetHost(info);
		DialogPresentation::SetHost(dialog);
	}
};

// Whether the end of a run read anything.
bool	ReadsAtTheEnd()
{
	const unsigned long before = RecordingHosts::g_Sink;
	RecordingHosts::ReadOpenWindows();
	return RecordingHosts::g_Sink != before;
}

} // namespace

TEST(RecordingHosts, AWindowKeepsWhatItIsHandedWhileOpenUntilTheUIClosesIt)
{
	Hosts hosts;
	RecordingHosts::Install();
	MShop shop;
	shop.Init(1);
	MStorage storage, pets, sale;
	storage.Init(1);
	pets.Init(1);
	sale.Init(1);

	// A Set on a closed window is dropped; Run opens one with no object.
	DialogPresentation::SetShop(&shop);
	CHECK(!DialogPresentation::IsRunningShop());
	DialogPresentation::RunShop();
	CHECK(DialogPresentation::IsRunningShop());
	CHECK(RecordingHosts::g_Shop.pObject == NULL);
	DialogPresentation::SetShop(&shop);
	CHECK(RecordingHosts::g_Shop.pObject == &shop);
	DialogPresentation::RunShop();		// already open: kept
	CHECK(RecordingHosts::g_Shop.pObject == &shop);

	DialogPresentation::RunStorage();
	DialogPresentation::SetStorage(&storage);
	DialogPresentation::RunPetStorage();
	DialogPresentation::SetPetStorage(&pets);
	VsUiPresentation::RunPersnalShop();
	VsUiPresentation::SetPersnalShop(&sale);
	CHECK(DialogPresentation::IsRunningStorage());
	CHECK(VsUiPresentation::IsRunningPersnalShop());
	CHECK(ReadsAtTheEnd());

	// A zone change closes the shop and the storage only.
	ZoneLoading::CloseWindows();
	CHECK(!DialogPresentation::IsRunningShop());
	CHECK(!DialogPresentation::IsRunningStorage());
	CHECK(RecordingHosts::g_Shop.pObject == NULL);
	CHECK(RecordingHosts::g_PetStorage.pObject == &pets);
	CHECK(VsUiPresentation::IsRunningPersnalShop());
	VsUiPresentation::ClosePersnalShop();
	CHECK(!VsUiPresentation::IsRunningPersnalShop());
	CHECK(ReadsAtTheEnd());		// the pet storage, still open

	// CloseAllDialog closes every window the hosts keep an object in.
	DialogPresentation::RunShop();
	VsUiPresentation::RunPersnalShop();
	DialogPresentation::CloseAllDialog();
	CHECK(!DialogPresentation::IsRunningShop());
	CHECK(!VsUiPresentation::IsRunningPersnalShop());
	CHECK(!RecordingHosts::g_PetStorage.bOpen);
	CHECK(!ReadsAtTheEnd());
}
