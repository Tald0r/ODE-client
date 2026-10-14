#include "Client_PCH.h"
#include "LoginPresentationHost.h"

namespace LoginPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void NewCharacterCreateOk()
{
	if (s_Host && s_Host->NewCharacterCreateOk) s_Host->NewCharacterCreateOk();
}

void DeleteCharacterOK()
{
	if (s_Host && s_Host->DeleteCharacterOK) s_Host->DeleteCharacterOK();
}

void AlreadyExistIDMessage()
{
	if (s_Host && s_Host->AlreadyExistIDMessage) s_Host->AlreadyExistIDMessage();
}

void NoAlreadyExistIDMessage()
{
	if (s_Host && s_Host->NoAlreadyExistIDMessage) s_Host->NoAlreadyExistIDMessage();
}

void StartCharacterManager()
{
	if (s_Host && s_Host->StartCharacterManager) s_Host->StartCharacterManager();
}

void SetSlayerCharacter(int slot, PCSlayerInfo* pInfo)
{
	if (s_Host && s_Host->SetSlayerCharacter) s_Host->SetSlayerCharacter(slot, pInfo);
}

void SetVampireCharacter(int slot, PCVampireInfo* pInfo)
{
	if (s_Host && s_Host->SetVampireCharacter) s_Host->SetVampireCharacter(slot, pInfo);
}

void SetOustersCharacter(int slot, PCOustersInfo* pInfo)
{
	if (s_Host && s_Host->SetOustersCharacter) s_Host->SetOustersCharacter(slot, pInfo);
}

void RunAskGoBilingPage(int n)
{
	if (s_Host && s_Host->RunAskGoBilingPage) s_Host->RunAskGoBilingPage(n);
}

void PopupExitMessageDlg(const char* message)
{
	if (s_Host && s_Host->PopupExitMessageDlg) s_Host->PopupExitMessageDlg(message);
}

void Sleep(std::uint32_t milliseconds)
{
	if (s_Host && s_Host->Sleep) s_Host->Sleep(milliseconds);
}

void EnterSelectPCMode()
{
	if (s_Host && s_Host->EnterSelectPCMode) s_Host->EnterSelectPCMode();
}

void QueueWaitPCListMode()
{
	if (s_Host && s_Host->QueueWaitPCListMode) s_Host->QueueWaitPCListMode();
}

void QueueLoginWrongMode()
{
	if (s_Host && s_Host->QueueLoginWrongMode) s_Host->QueueLoginWrongMode();
}

bool IsSelectPCMode()
{
	return s_Host && s_Host->IsSelectPCMode && s_Host->IsSelectPCMode();
}

void PopupErrorMessage(int errorID)
{
	if (s_Host && s_Host->PopupErrorMessage) s_Host->PopupErrorMessage(errorID);
}

void QueueWaitSelectPCMode()
{
	if (s_Host && s_Host->QueueWaitSelectPCMode) s_Host->QueueWaitSelectPCMode();
}

void ShowMessage(const char* message)
{
	if (s_Host && s_Host->ShowMessage) s_Host->ShowMessage(message);
}

void InitPlayer(int x, int y, int direction)
{
	if (s_Host && s_Host->InitPlayer) s_Host->InitPlayer(x, y, direction);
}

void EnterGameMode()
{
	if (s_Host && s_Host->EnterGameMode) s_Host->EnterGameMode();
}

void BackupLoginID(const char* id)
{
	if (s_Host && s_Host->BackupLoginID) s_Host->BackupLoginID(id);
}

void EnterWaitWorldListMode()
{
	if (s_Host && s_Host->EnterWaitWorldListMode) s_Host->EnterWaitWorldListMode();
}

void SetServerGroupName(const char* name)
{
	if (s_Host && s_Host->SetServerGroupName) s_Host->SetServerGroupName(name);
}

void SetGoreLevel(bool gore)
{
	if (s_Host && s_Host->SetGoreLevel) s_Host->SetGoreLevel(gore);
}

bool UseTeenVersion()
{
	return s_Host && s_Host->UseTeenVersion && s_Host->UseTeenVersion();
}

}
