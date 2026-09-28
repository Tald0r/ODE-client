#include "test_framework.h"
#include "CrtCompat.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cstring>
#include <new>
#include <system_error>
#include "UserOption.h"
#include "KeyAccelerator.h"

namespace {
struct SettingsFixture {
    std::filesystem::path path = "user-option-test.tmp";
    KeyAccelerator keys;
    SettingsFixture() {
        keys.Init(4);
        keys.SetAcceleratorKey(1, DIK_I);
        g_pKeyAccelerator = &keys;
    }
    ~SettingsFixture() {
        g_pKeyAccelerator = nullptr;
        std::filesystem::remove(path);
    }
    std::string Read() {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    void Write(const std::string& contents) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(contents.data(), contents.size());
    }
};
}

TEST(UserOption, EntireBackupIdIsInitialized)
{
    alignas(UserOption) unsigned char storage[sizeof(UserOption)];
    std::memset(storage, 0x1A, sizeof storage);
    auto* options = new (storage) UserOption;
    for (char byte : options->BackupID) CHECK_EQ(0, byte);
    options->~UserOption();
}

TEST(UserOption, LegacyIdFieldDoesNotSerializeAdjacentMembers)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = 0x1A;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    const auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    CHECK(std::string("abcdefghij") + std::string(5, '\0') == bytes.substr(id, 15));
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(options.UseEnterChat, loaded.UseEnterChat);
    CHECK_EQ(FALSE, loaded.UseXbrz);
}

TEST(UserOption, LegacyPaddingAndCrLfRemainReadable)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = TRUE;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    // Ignore the old writer's trailing padding/adjacent-member bytes.
    bytes.replace(id + 11, 4, std::string(4, '\x7F'));
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\n' && (i == 0 || bytes[i - 1] != '\r')) {
            bytes.insert(i, 1, '\r');
            ++i;
        }
    }
    fixture.Write(bytes);
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(TRUE, loaded.UseEnterChat);
    CHECK_EQ(FALSE, loaded.UseXbrz);
}

TEST(UserOption, DiskIdIsBoundedAndTruncatedFieldIsRejected)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = TRUE;
    options.SaveToFile(fixture.path.string().c_str());
    auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    bytes.replace(id, 15, "abcdefghijklmno");
    fixture.Write(bytes);
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(TRUE, loaded.UseEnterChat);
    bytes.resize(id + 14);
    fixture.Write(bytes);
    CHECK(!loaded.LoadFromFile(fixture.path.string().c_str()));
}

TEST(UserOption, XbrzDefaultsOnAndBothChoicesSurviveReload)
{
    SettingsFixture fixture;
    UserOption options;
    CHECK_EQ(TRUE, options.UseXbrz);
    options.DrawFPS = TRUE;
    options.VolumeMusic = 7;
    for (int enabled : {0, 1}) {
        options.UseXbrz = enabled;
        options.SaveToFile(fixture.path.string().c_str());
        UserOption loaded;
        CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
        CHECK_EQ(enabled, loaded.UseXbrz);
        CHECK_EQ(TRUE, loaded.DrawFPS);
        CHECK_EQ(7, loaded.VolumeMusic);
        CHECK_EQ(DIK_I, fixture.keys.GetKey(1));
    }
}

TEST(UserOption, OldMissingAndInvalidSettingsDefaultXbrzOn)
{
    SettingsFixture fixture;
    UserOption options;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    auto legacy = fixture.Read();
    const auto tail = legacy.rfind("0\tUseXbrz");
    CHECK(tail != std::string::npos);
    if (tail == std::string::npos) return;
    legacy.resize(tail);
    for (const char* extension : {"", "0\tUnknownOption\n", "2\tUseXbrz\n", "0"}) {
        fixture.Write(legacy + extension);
        options.UseXbrz = FALSE;
        CHECK(options.LoadFromFile(fixture.path.string().c_str()));
        CHECK_EQ(TRUE, options.UseXbrz);
    }
    std::filesystem::remove(fixture.path);
    options.UseXbrz = FALSE;
    CHECK(!options.LoadFromFile(fixture.path.string().c_str()));
    CHECK_EQ(TRUE, options.UseXbrz);
}

// FileDef.inf names this file the Windows way, UserSet\UserOption.set, and a
// data tree's letter case need not match the names it is given. Off Windows
// the reader and the writer resolve the name (basic/DataPath.h), so the file
// lands in the directory instead of beside it under a name with backslashes
// in it, and is found again under another case; on Windows the name is used
// as given. NormalizeDataPath caches a directory's listing the first time it
// looks in it, so each case gets a directory nothing has resolved through.
namespace {
struct TemporaryRoot {
    std::filesystem::path path;
    explicit TemporaryRoot(const char* tag) {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error) / (std::string("user_option_") + tag + "_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path / "UserSet", error);
    }
    ~TemporaryRoot() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
}

TEST(UserOption, BackslashedNameIsWrittenIntoItsDirectory)
{
    SettingsFixture fixture;
    TemporaryRoot root("write");
    const std::string name = root.path.string() + "\\UserSet\\UserOption.set";
    UserOption options;
    options.VolumeMusic = 5;
    options.SaveToFile(name.c_str());
    std::error_code error;
    CHECK(std::filesystem::is_regular_file(root.path / "UserSet" / "UserOption.set", error));
    UserOption loaded;
    CHECK(loaded.LoadFromFile(name.c_str()));
    CHECK_EQ(5, loaded.VolumeMusic);
}

TEST(UserOption, NameIsFoundUnderAnotherLetterCase)
{
    SettingsFixture fixture;
    TemporaryRoot root("case");
    UserOption options;
    options.VolumeMusic = 6;
    options.SaveToFile(fixture.path.string().c_str());
    std::error_code error;
    std::filesystem::copy_file(fixture.path, root.path / "UserSet" / "UserOption.set", error);
    CHECK(!error);
    UserOption loaded;
    CHECK(loaded.LoadFromFile((root.path.string() + "\\USERSET\\useroption.SET").c_str()));
    CHECK_EQ(6, loaded.VolumeMusic);
}

// KeyAccelerator::Init clears the accelerators and resizes them, and a
// vector keeps its capacity through clear(), so after a larger Init the
// capacity exceeds the size. Loading a settings file with fewer entries
// than the defaults did leaves exactly that. The count saved and the keys
// looked up are the size's; entries past it are unbound.
namespace {
struct TemporaryFile {
    std::filesystem::path path;
    explicit TemporaryFile(const char* tag) {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error) / (std::string("key_accelerator_") + tag + "_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    }
    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
}

TEST(KeyAccelerator, SaveWritesSizeNotCapacity)
{
    KeyAccelerator keys;
    keys.Init(8);
    keys.Init(3);
    keys.SetAcceleratorKey(1, DIK_I);
    TemporaryFile temp("save");
    FILE* file = Basic::OpenFile(temp.path.string().c_str(), "w+b");
    CHECK(file != nullptr);
    if (file == nullptr) return;
    keys.SaveToFile(file);
    std::fflush(file);
    const long length = std::ftell(file);
    std::rewind(file);
    int count = 0;
    CHECK_EQ(4, static_cast<long long>(std::fread(&count, 1, 4, file)));
    WORD key = 0;
    CHECK_EQ(2, static_cast<long long>(std::fread(&key, 1, 2, file)));
    std::fclose(file);
    CHECK_EQ(3, count);
    CHECK_EQ(DIK_I, key);
    CHECK_EQ(4 + 2 * 2, length);
}

TEST(KeyAccelerator, GetKeyBeyondSizeIsUnbound)
{
    KeyAccelerator keys;
    keys.Init(8);
    keys.SetAcceleratorKey(5, DIK_K);
    keys.Init(3);
    CHECK_EQ(0, keys.GetKey(5));
}
