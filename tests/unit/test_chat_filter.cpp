//----------------------------------------------------------------------
// test_chat_filter.cpp
//----------------------------------------------------------------------
//
// The chat filter (gamemodel, docs/RESTRUCTURING.md task 4.13):
// MStringMap, the MString-to-MString map the curse lists and the ignored
// IDs are kept in, and MChatManager::RemoveCurse, which rewrites every
// server chat line and the new-character name in place.
//
// RemoveCurse runs two passes over the line. The English pass keeps the
// letters (lower-cased, everything else skipped, so "d.a.r.n" is still
// "darn"), finds each listed word in them and overwrites the matched
// letters in the line with the mask 'x'. The Korean pass keeps the
// two-byte (CP949/EUC-KR) characters - any byte with the high bit set
// takes the byte after it - looks every window of one to four of them up
// in the list of that length and overwrites the matched bytes with the
// string table's replacement for that length (built-in English: "<3",
// "love", "love you", "I love you"). The lists here are loaded through
// the real loaders: LoadFromFileCurse (a text file of words, sorted into
// the English and the four Korean lists by what they contain) and
// LoadFromFile (the binary file the game ships).
//
// Korean text in this file is written as CP949 byte escapes, because the
// source is UTF-8 and the filter pairs bytes:
//   \xB0\xA1 (ga)  \xB3\xAA (na)  \xB4\xD9 (da)  \xB6\xF3 (ra)
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MChatManager.h"
#include "MStringMap.h"
#include "MGameStringTable.h"
#include "MStringArray.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

const char* const	kWordFile = "chat_filter_test.txt";
const char* const	kBinFile = "chat_filter_test.bin";

#define GA	"\xB0\xA1"
#define NA	"\xB3\xAA"
#define DA	"\xB4\xD9"
#define RA	"\xB6\xF3"

// The word list most tests filter with: two English words and one Korean
// word of each length the filter knows (one to four syllables).
const char* const	kWords =
	"darn\n"
	"heck\n"
	GA "\n"
	NA DA "\n"
	DA RA GA "\n"
	GA NA DA RA "\n";

//----------------------------------------------------------------------
// The string table RemoveCurse reads its Korean replacements from, and
// no chat host: each test starts from the executable's defaults.
//----------------------------------------------------------------------
struct ChatWorld
{
	MStringArray*	saved;
	MStringArray	table;

	ChatWorld()
	{
		saved = g_pGameStringTable;
		g_pGameStringTable = &table;
		InitGameStringTable();
		MChatManager::SetHost(NULL);
	}

	~ChatWorld()
	{
		MChatManager::SetHost(NULL);
		g_pGameStringTable = saved;
		std::remove(kWordFile);
		std::remove(kBinFile);
	}
};

void	WriteFile(const char* path, const std::string& bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), (std::streamsize)bytes.size());
}

void	LoadWords(MChatManager& chat, const char* words)
{
	WriteFile(kWordFile, words);
	chat.LoadFromFileCurse(kWordFile);
}

// One RemoveCurse call on a copy of the line, in a buffer exactly the
// line's size plus its terminator (so ASan sees any write past it).
struct Filtered
{
	bool		found;
	std::string	text;
};

Filtered	Filter(const MChatManager& chat, const std::string& line, bool bForce = false)
{
	std::vector<char> buffer(line.begin(), line.end());
	buffer.push_back('\0');
	Filtered result;
	result.found = chat.RemoveCurse(buffer.data(), bForce);
	result.text.assign(buffer.data(), std::strlen(buffer.data()));
	return result;
}

// Whether the filter found a curse and left exactly `expected`.
bool	Filters(const MChatManager& chat, const std::string& line, const std::string& expected)
{
	const Filtered r = Filter(chat, line);
	return r.found && r.text == expected;
}

// Whether the filter found nothing and left the line alone.
bool	Passes(const MChatManager& chat, const std::string& line)
{
	const Filtered r = Filter(chat, line);
	return !r.found && r.text == line;
}

bool	StrEq(const MString* s, const char* expected)
{
	return s != NULL && s->GetString() != NULL && std::strcmp(s->GetString(), expected) == 0;
}

// MString's on-disk form: a 4-byte little-endian length, then the bytes.
std::string	DiskString(const std::string& s)
{
	std::string out;
	const unsigned int n = (unsigned int)s.size();
	out += (char)(n & 0xFF);
	out += (char)((n >> 8) & 0xFF);
	out += (char)((n >> 16) & 0xFF);
	out += (char)((n >> 24) & 0xFF);
	return out + s;
}

std::string	DiskInt(int v)
{
	return std::string((const char*)&v, 4);
}

bool	gHostFiltering = true;
int		gHostCalls = 0;
bool	HostFiltering()	{ gHostCalls++; return gHostFiltering; }

} // namespace

//----------------------------------------------------------------------
// MStringMap
//----------------------------------------------------------------------
TEST(StringMap, AddGetRemove)
{
	MStringMap map;

	CHECK(!map.Add(NULL));
	CHECK(map.Add("alpha"));
	CHECK(map.Add("key", "value"));
	CHECK_EQ(2, (int)map.size());

	CHECK(StrEq(map.Get("alpha"), "alpha"));
	CHECK(StrEq(map.Get("key"), "value"));
	CHECK(map.Get("missing") == NULL);
	CHECK(map.Get("ALPHA") == NULL);	// keys compare byte for byte

	// A word added without a value is its own value: one string, shared.
	MStringMap::const_iterator it = map.begin();
	CHECK(StrEq(it->first, "alpha"));
	CHECK(it->first == it->second);

	CHECK(map.Remove("alpha"));
	CHECK(!map.Remove("alpha"));
	CHECK(map.Get("alpha") == NULL);
	CHECK_EQ(1, (int)map.size());
}

TEST(StringMap, AddAgainReplacesTheValue)
{
	MStringMap map;

	CHECK(map.Add("key", "one"));
	CHECK(map.Add("key", "two"));
	CHECK_EQ(1, (int)map.size());
	CHECK(StrEq(map.Get("key"), "two"));

	// A value equal to the key goes back to sharing the key's string.
	CHECK(map.Add("key", "key"));
	CHECK(StrEq(map.Get("key"), "key"));
	CHECK(map.begin()->first == map.begin()->second);

	CHECK(map.Add("key", "three"));
	CHECK(StrEq(map.Get("key"), "three"));
	CHECK(map.begin()->first != map.begin()->second);

	CHECK(map.Add("key"));
	CHECK(StrEq(map.Get("key"), "key"));
	CHECK(map.begin()->first == map.begin()->second);

	map.Release();
	CHECK_EQ(0, (int)map.size());
	CHECK(map.Get("key") == NULL);
}

TEST(StringMap, SaveAndLoadRoundTripWordsThatAreTheirOwnValue)
{
	MStringMap map;
	map.Add("beta");
	map.Add("alpha");
	{
		std::ofstream out(kBinFile, std::ios::binary | std::ios::trunc);
		map.SaveToFile(out);
	}

	// The layout: a count, then per entry a same-string flag and the key.
	std::ifstream check(kBinFile, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
	CHECK(bytes == DiskInt(2) + std::string(1, '\1') + DiskString("alpha")
		+ std::string(1, '\1') + DiskString("beta"));

	MStringMap loaded;
	{
		std::ifstream in(kBinFile, std::ios::binary);
		loaded.LoadFromFile(in);
		CHECK(in.good());
		CHECK(in.peek() == EOF);
	}
	CHECK_EQ(2, (int)loaded.size());
	CHECK(StrEq(loaded.Get("alpha"), "alpha"));
	CHECK(StrEq(loaded.Get("beta"), "beta"));
	CHECK(loaded.begin()->first == loaded.begin()->second);
	std::remove(kBinFile);
}

TEST(StringMap, SaveAndLoadRoundTripAValueThatIsNotTheKey)
{
	MStringMap map;
	map.Add("key", "value");
	map.Add("self");
	{
		std::ofstream out(kBinFile, std::ios::binary | std::ios::trunc);
		map.SaveToFile(out);
	}

	// An entry whose value is not its key: flag 0, the key, the value.
	std::ifstream check(kBinFile, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
	CHECK(bytes == DiskInt(2) + std::string(1, '\0') + DiskString("key") + DiskString("value")
		+ std::string(1, '\1') + DiskString("self"));

	// The loader read the value into a second, inner pValueString and
	// inserted the outer one, which was never set.
	MStringMap loaded;
	{
		std::ifstream in(kBinFile, std::ios::binary);
		loaded.LoadFromFile(in);
		CHECK(in.good());
		CHECK(in.peek() == EOF);
	}
	CHECK_EQ(2, (int)loaded.size());
	CHECK(StrEq(loaded.Get("key"), "value"));
	CHECK(StrEq(loaded.Get("self"), "self"));
	MStringMap::const_iterator it = loaded.begin();
	CHECK(StrEq(it->first, "key"));
	CHECK(it->first != it->second);
	std::remove(kBinFile);
}

TEST(StringMap, LoadOfAnEmptyMapReadsOnlyTheCount)
{
	WriteFile(kBinFile, DiskInt(0) + "rest");
	MStringMap map;
	std::ifstream in(kBinFile, std::ios::binary);
	map.LoadFromFile(in);
	CHECK_EQ(0, (int)map.size());
	CHECK(in.peek() == 'r');
	in.close();
	std::remove(kBinFile);
}

TEST(StringMap, AnyFlagByteButZeroMeansTheValueIsTheKey)
{
	// The flag was read straight into a bool: a byte of 2 or 0x80 is
	// neither true nor false, and Clang tests the low bit.
	WriteFile(kBinFile, DiskInt(3)
		+ std::string(1, '\x02') + DiskString("alpha")
		+ std::string(1, '\x80') + DiskString("beta")
		+ std::string(1, '\xFF') + DiskString("gamma"));
	MStringMap map;
	std::ifstream in(kBinFile, std::ios::binary);
	map.LoadFromFile(in);
	CHECK(in.good());
	CHECK(in.peek() == EOF);
	CHECK_EQ(3, (int)map.size());
	CHECK(StrEq(map.Get("alpha"), "alpha"));
	CHECK(StrEq(map.Get("beta"), "beta"));
	CHECK(StrEq(map.Get("gamma"), "gamma"));
	in.close();
	std::remove(kBinFile);
}

TEST(StringMap, ACountLargerThanTheFileLoadsWhatIsThere)
{
	// Three entries announced, one there: the second's reads failed, its
	// key stayed an MString with no string, and inserting it compared a
	// NULL with strcmp.
	WriteFile(kBinFile, DiskInt(3) + std::string(1, '\1') + DiskString("alpha"));
	MStringMap map;
	std::ifstream in(kBinFile, std::ios::binary);
	map.LoadFromFile(in);
	CHECK(in.fail());
	CHECK_EQ(1, (int)map.size());
	CHECK(StrEq(map.Get("alpha"), "alpha"));
	in.close();
	std::remove(kBinFile);
}

TEST(StringMap, AnEntryCutShortIsDropped)
{
	// Cut inside the second entry's value, then inside its key.
	const std::string first = DiskInt(2) + std::string(1, '\0') + DiskString("key") + DiskString("value");
	const std::string second = std::string(1, '\0') + DiskString("next") + DiskString("longer");
	const std::string cuts[] = {
		first + second.substr(0, second.size() - 2),
		first + second.substr(0, 1 + 4 + 2),
		first + second.substr(0, 1),
	};
	for (const std::string& bytes : cuts)
	{
		WriteFile(kBinFile, bytes);
		MStringMap map;
		std::ifstream in(kBinFile, std::ios::binary);
		map.LoadFromFile(in);
		CHECK(in.fail());
		CHECK_EQ(1, (int)map.size());
		CHECK(StrEq(map.Get("key"), "value"));
		CHECK(map.Get("next") == NULL);
	}
	std::remove(kBinFile);
}

TEST(StringMap, AFileWithoutACountLoadsNothing)
{
	// The count was read into an uninitialised int, so a file shorter
	// than four bytes looped over whatever the stack held.
	const std::string files[] = { "", "\x05", "\x05\x00\x00" };
	for (const std::string& bytes : files)
	{
		WriteFile(kBinFile, bytes);
		MStringMap map;
		map.Add("before");
		std::ifstream in(kBinFile, std::ios::binary);
		map.LoadFromFile(in);
		CHECK(in.fail());
		CHECK_EQ(1, (int)map.size());	// what the map held is kept
		CHECK(StrEq(map.Get("before"), "before"));
	}
	std::remove(kBinFile);
}

TEST(StringMap, ANegativeCountLoadsNothing)
{
	WriteFile(kBinFile, DiskInt(-1) + std::string(1, '\1') + DiskString("alpha"));
	MStringMap map;
	std::ifstream in(kBinFile, std::ios::binary);
	map.LoadFromFile(in);
	CHECK(in.good());
	CHECK_EQ(0, (int)map.size());
	in.close();
	std::remove(kBinFile);
}

TEST(StringMap, ARepeatedKeyKeepsItsFirstValue)
{
	WriteFile(kBinFile, DiskInt(3)
		+ std::string(1, '\0') + DiskString("key") + DiskString("one")
		+ std::string(1, '\0') + DiskString("key") + DiskString("two")
		+ std::string(1, '\1') + DiskString("key"));
	MStringMap map;
	std::ifstream in(kBinFile, std::ios::binary);
	map.LoadFromFile(in);
	CHECK(in.good());
	CHECK(in.peek() == EOF);
	CHECK_EQ(1, (int)map.size());
	CHECK(StrEq(map.Get("key"), "one"));
	in.close();
	std::remove(kBinFile);
}

//----------------------------------------------------------------------
// MChatManager: the ignore list
//----------------------------------------------------------------------
TEST(ChatManager, AcceptAndIgnoreModesReadTheIdList)
{
	MChatManager chat;

	CHECK(chat.IsAcceptMode());
	CHECK(chat.AddID("pest"));
	CHECK(!chat.IsAcceptID("pest"));	// accept mode: a listed ID is ignored
	CHECK(chat.IsAcceptID("friend"));

	chat.SetIgnoreMode();
	CHECK(chat.IsIgnoreMode());
	CHECK(chat.IsAcceptID("pest"));		// ignore mode: only a listed ID is heard
	CHECK(!chat.IsAcceptID("friend"));

	CHECK(chat.RemoveID("pest"));
	CHECK(!chat.IsAcceptID("pest"));
	chat.ClearID();
	chat.SetAcceptMode();
	CHECK(chat.IsAcceptID("pest"));
}

//----------------------------------------------------------------------
// MChatManager: the host
//----------------------------------------------------------------------
TEST(ChatFilter, WithoutAHostTheFilterIsOn)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(Filters(chat, "darn", "xxxx"));

	// A host whose entry is missing answers the same default.
	MChatHost empty;
	CHECK(MChatManager::SetHost(&empty) == NULL);
	CHECK(Filters(chat, "darn", "xxxx"));
	CHECK(MChatManager::SetHost(NULL) == &empty);
}

TEST(ChatFilter, TheHostTurnsTheFilterOffUnlessForced)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	MChatHost host;
	host.FilteringCurse = HostFiltering;
	MChatManager::SetHost(&host);

	gHostFiltering = false;
	gHostCalls = 0;
	CHECK(Passes(chat, "darn " GA));
	CHECK_EQ(1, gHostCalls);

	// Whispers and the new-character name pass bForce.
	const Filtered forced = Filter(chat, "darn " GA, true);
	CHECK(forced.found);
	CHECK(forced.text == "xxxx <3");

	// The option is read on every call, never cached.
	gHostFiltering = true;
	CHECK(Filters(chat, "darn", "xxxx"));
	CHECK_EQ(3, gHostCalls);
}

TEST(ChatFilter, NullAndEmptyLinesFindNothing)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(!chat.RemoveCurse(NULL));
	CHECK(!chat.RemoveCurse(NULL, true));
	CHECK(Passes(chat, ""));
	CHECK(Passes(chat, " "));
	CHECK(Passes(chat, "hello there"));
	CHECK(Passes(chat, "12345 !?"));

	// No lists at all: nothing to find.
	MChatManager bare;
	CHECK(Passes(bare, "darn " GA));
}

//----------------------------------------------------------------------
// MChatManager: English words
//----------------------------------------------------------------------
TEST(ChatFilter, EnglishWordsAreMaskedWhateverTheirCase)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(Filters(chat, "darn", "xxxx"));
	CHECK(Filters(chat, "DARN", "xxxx"));
	CHECK(Filters(chat, "DaRn it", "xxxx it"));
	CHECK(Passes(chat, "dam"));
	CHECK(Passes(chat, "dar"));			// a word cut short is no word
}

TEST(ChatFilter, EnglishWordsAreFoundAcrossWhatIsNotALetter)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Only letters count, so spacing or punctuating a word out does not
	// hide it, and only its letters are masked.
	CHECK(Filters(chat, "d.a.r.n", "x.x.x.x"));
	CHECK(Filters(chat, "d a r n!", "x x x x!"));
	CHECK(Filters(chat, "d4a5r6n", "x4x5x6x"));
	CHECK(Filters(chat, "d" GA "arn", "x<3xxx"));	// a Korean word between the letters
	// The match ignores word boundaries: a listed word inside another is masked.
	CHECK(Filters(chat, "adarnb", "axxxxb"));
	CHECK(Filters(chat, "say darn", "say xxxx"));
}

TEST(ChatFilter, EnglishWordsAtTheStartAndTheEnd)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(Filters(chat, "darn you", "xxxx you"));
	CHECK(Filters(chat, "oh darn", "oh xxxx"));
	CHECK(Filters(chat, "  darn  ", "  xxxx  "));
}

TEST(ChatFilter, AdjacentAndOverlappingEnglishWordsAreAllMasked)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, "darn\nheck\narnie\n");

	CHECK(Filters(chat, "heckdarn", "xxxxxxxx"));
	CHECK(Filters(chat, "darn heck", "xxxx xxxx"));
	// "darnie" holds "darn" and "arnie", sharing "arn".
	CHECK(Filters(chat, "darnie", "xxxxxx"));
	CHECK(Filters(chat, "Darnie!", "xxxxxx!"));
}

TEST(ChatFilter, AnEnglishWordIsMaskedEveryTimeItOccurs)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(Filters(chat, "darn darn", "xxxx xxxx"));
	CHECK(Filters(chat, "darndarn", "xxxxxxxx"));
	// The second match must not be marked from where the first ended:
	// that masked the "a" and left the second word.
	CHECK(Filters(chat, "darn a darn", "xxxx a xxxx"));
	CHECK(Filters(chat, "darn, Darn and DARN!", "xxxx, xxxx and xxxx!"));
	CHECK(Filters(chat, "heck darn heck darn", "xxxx xxxx xxxx xxxx"));
}

TEST(ChatFilter, OnlyLowerCaseLettersEnterTheEnglishList)
{
	ChatWorld world;
	MChatManager chat;
	// A word with a digit, a capital or punctuation is neither English nor
	// Korean and is dropped; a word with a letter and a Korean character is
	// dropped too.
	LoadWords(chat, "d4rn\nDarn\nda-rn\ndarn" GA "\n");

	CHECK(Passes(chat, "d4rn"));
	CHECK(Passes(chat, "darn"));
	CHECK(Passes(chat, "da-rn"));
	CHECK(Passes(chat, "darn" GA));
}

//----------------------------------------------------------------------
// MChatManager: Korean words
//----------------------------------------------------------------------
TEST(ChatFilter, KoreanWordsOfEachLengthGetTheirReplacement)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	CHECK(Filters(chat, GA, "<3"));
	CHECK(Filters(chat, NA DA, "love"));
	CHECK(Filters(chat, "a " GA " b", "a <3 b"));
	CHECK(Filters(chat, "x " NA DA " y", "x love y"));
	CHECK(Filters(chat, GA " " GA, "<3 <3"));
	CHECK(Filters(chat, "darn " GA, "xxxx <3"));
	CHECK(Passes(chat, NA));
	CHECK(Passes(chat, DA NA));
	CHECK(Passes(chat, RA RA));
}

TEST(ChatFilter, KoreanWordsAreFoundAcrossSpacesAndLetters)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Only the two-byte characters count, so a spaced-out word is found;
	// the replacement goes over the word's own bytes, around the space.
	CHECK(Filters(chat, NA " " DA, "lo ve"));
	CHECK(Filters(chat, NA "zz" DA, "lozzve"));
}

TEST(ChatFilter, LongerKoreanReplacementsAreCutAtTheEndOfTheLine)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Three and four syllables are six and eight bytes; their replacements
	// ("love you", "I love you") are eight and ten. A word that ends the
	// line gets as much of the replacement as its bytes hold.
	CHECK(Filters(chat, DA RA GA, "love y"));
	CHECK(Filters(chat, GA NA DA RA, "I love y"));
}

TEST(ChatFilter, AKoreanWordEndingALineLeavesTheTextBeforeIt)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// The replacement's bytes past the word's last Korean byte have no
	// Korean byte to go to; they were written wherever the unset rest of
	// the index array pointed inside the line - here its first bytes.
	CHECK(Filters(chat, "hi " DA RA GA, "hi love y"));
	CHECK(Filters(chat, "hi " GA NA DA RA, "hi I love y"));
	CHECK(Filters(chat, "abc " DA RA GA "!", "abc love y!"));
	CHECK(Filters(chat, "darn " DA RA GA, "xxxx love y"));
}

TEST(ChatFilter, LongerKoreanReplacementsRunOnOverTheNextCharacter)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Pinned as it is, not as it should be: a replacement longer than the
	// word goes on over the Korean character after it. The shipped Korean
	// string table's replacements are as long as the words (2, 4, 6 and 8
	// bytes); the built-in English ones are not.
	CHECK(Filters(chat, DA RA GA NA, "love you"));
	CHECK(Filters(chat, DA RA GA " " NA, "love y ou"));
}

TEST(ChatFilter, OverlappingKoreanWordsTakeTheLongerReplacement)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, GA "\n" GA NA "\n" NA DA "\n");

	// The lists are searched shortest first and each match overwrites the
	// marks under it, so the two-syllable word wins over the one inside it.
	CHECK(Filters(chat, GA NA, "love"));
	// Two two-syllable words sharing NA: the first replacement, then the
	// second from where the first ended, cut at the line's end.
	CHECK(Filters(chat, GA NA DA, "lovelo"));
	// Adjacent words, each its own replacement.
	CHECK(Filters(chat, NA DA GA, "love<3"));
	CHECK(Filters(chat, GA GA, "<3<3"));
}

TEST(ChatFilter, KoreanCharactersPairFromTheFirstHighByte)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// A lead byte takes the byte after it, whatever it is: a stray lead
	// byte shifts the pairing, and the word after it is not found.
	CHECK(Passes(chat, "\xB0" GA));
	CHECK(Passes(chat, "x\xB0" GA));
	CHECK(Passes(chat, "\xB0" "a"));
	// A lead byte that ends the line is left alone.
	CHECK(Passes(chat, "\xB0"));
	CHECK(Passes(chat, "a\xB0"));
	CHECK(Filters(chat, GA "\xB0", "<3\xB0"));
	// UTF-8 text is not CP949: the same syllable in UTF-8 is not found.
	CHECK(Passes(chat, "\xEA\xB0\x80"));
	CHECK(Passes(chat, "a \xEA\xB0\x80 b"));
}

TEST(ChatFilter, KoreanWordsFromTheBinaryListAreConvertedAndMatchNothing)
{
	ChatWorld world;

	// The game loads its lists from a binary file (FILE_INFO_CHAT): the
	// English list, the four Korean lists and the IDs, each a count and
	// its entries. MString::LoadFromFile converts every entry from the
	// resource encoding (CP949) to UTF-8, so the one-syllable list holds
	// the three UTF-8 bytes of GA - while the filter looks it up with
	// two-byte windows. Pinned as a known defect (task 4.13): the Korean
	// lists the game ships match no text, CP949 or UTF-8.
	WriteFile(kBinFile,
		DiskInt(1) + std::string(1, '\1') + DiskString("darn")
		+ DiskInt(1) + std::string(1, '\1') + DiskString(GA)
		+ DiskInt(0) + DiskInt(0) + DiskInt(0)
		+ DiskInt(1) + std::string(1, '\1') + DiskString("pest"));
	MChatManager chat;
	chat.LoadFromFile(kBinFile);

	CHECK(Filters(chat, "oh darn", "oh xxxx"));
	CHECK(!chat.IsAcceptID("pest"));
	CHECK(Passes(chat, GA));
	CHECK(Passes(chat, "\xEA\xB0\x80"));
	CHECK(Passes(chat, "a\xEA\xB0\x80"));
}

TEST(ChatFilter, AnEmptyWordInTheBinaryListIsIgnored)
{
	ChatWorld world;

	// An empty English entry (a zero length) in the binary file: strstr
	// finds "" at once and the search never moved on, so the first line
	// with a letter in it hung the client.
	WriteFile(kBinFile,
		DiskInt(2) + std::string(1, '\1') + DiskString("")
			+ std::string(1, '\1') + DiskString("darn")
		+ DiskInt(1) + std::string(1, '\1') + DiskString("")
		+ DiskInt(0) + DiskInt(0) + DiskInt(0) + DiskInt(0));
	MChatManager chat;
	chat.LoadFromFile(kBinFile);

	CHECK(Filters(chat, "oh darn", "oh xxxx"));
	CHECK(Passes(chat, "hello"));
	CHECK(Passes(chat, GA));
}

TEST(ChatFilter, EnglishListsRoundTripThroughTheBinaryFile)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, "darn\nheck\n");
	chat.AddID("pest");
	chat.SaveToFile(kBinFile);

	MChatManager loaded;
	loaded.LoadFromFile(kBinFile);
	CHECK(Filters(loaded, "heck darn", "xxxx xxxx"));
	CHECK(!loaded.IsAcceptID("pest"));
	CHECK(loaded.IsAcceptID("friend"));
}

//----------------------------------------------------------------------
// MChatManager: adversarial lines
//----------------------------------------------------------------------
TEST(ChatFilter, EveryByteValueIsSafe)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Bytes 1 to 255 in order and reversed: no listed word, no change.
	std::string up, down;
	for (int c = 1; c <= 255; c++)	up += (char)c;
	for (int c = 255; c >= 1; c--)	down += (char)c;
	CHECK(Passes(chat, up));
	CHECK(Passes(chat, down));

	// Every byte value before and after a word: the word is still found,
	// and the byte either side is kept unless it pairs with a Korean byte.
	for (int c = 1; c <= 255; c++)
	{
		const std::string b(1, (char)c);
		const Filtered r = Filter(chat, b + " darn " + b);
		CHECK(r.found);
		CHECK(r.text == b + " xxxx " + b);
	}
}

TEST(ChatFilter, ALineAsLongAsTheCallersBuffersHoldsItsWords)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// The chat handlers copy a line into char[256]. Words early in a
	// line of 255 bytes, English and Korean.
	std::string line = "darn " GA " ";
	std::string expected = "xxxx <3 ";
	line += std::string(255 - line.size(), '.');
	expected += std::string(255 - expected.size(), '.');
	CHECK(Filters(chat, line, expected));

	// A line of 127 Korean characters with a word at the end.
	std::string korean, koreanExpected;
	for (int k = 0; k < 126; k++)	{ korean += RA; koreanExpected += RA; }
	korean += GA;
	koreanExpected += "<3";
	CHECK(Filters(chat, korean, koreanExpected));
}

TEST(ChatFilter, AnEnglishWordLateInALongLineIsMaskedNotCut)
{
	ChatWorld world;
	MChatManager chat;
	LoadWords(chat, kWords);

	// Each masked letter takes the mask text's character at the letter's
	// place among the line's letters. The text is 165 x's in a 256-byte
	// array: letters 165 to 255 took a NUL, which cut the line there, and
	// letters from 256 on read past the array.
	CHECK(Filters(chat, std::string(200, 'a') + "darn", std::string(200, 'a') + "xxxx"));
	// The callers' longest line: 255 bytes, the word in its last letters.
	CHECK(Filters(chat, std::string(251, 'b') + "darn", std::string(251, 'b') + "xxxx"));
	CHECK(Filters(chat, std::string(160, 'b') + "darn" + std::string(91, 'b'),
		std::string(160, 'b') + "xxxx" + std::string(91, 'b')));
	// Past the array: the word's letters are 256 to 259, then 400 to 403.
	CHECK(Filters(chat, std::string(256, 'c') + " darn", std::string(256, 'c') + " xxxx"));
	CHECK(Filters(chat, std::string(400, 'c') + " darn", std::string(400, 'c') + " xxxx"));
}

//----------------------------------------------------------------------
// MChatManager: AddMask (the hallucination and distance garbling)
//----------------------------------------------------------------------
TEST(ChatMask, AHundredPercentLeavesTheLine)
{
	MChatManager chat;
	char line[] = "hello " GA " there";
	chat.AddMask(line, 100);
	CHECK(std::strcmp(line, "hello " GA " there") == 0);
	chat.AddMask(line, 250);
	CHECK(std::strcmp(line, "hello " GA " there") == 0);
}

TEST(ChatMask, ZeroPercentMasksEverythingButSpaces)
{
	MChatManager chat;
	const char original[] = "ab " GA " c";
	char line[sizeof(original)];
	std::memcpy(line, original, sizeof(original));
	chat.AddMask(line, 0);

	CHECK_EQ((int)std::strlen(original), (int)std::strlen(line));
	for (size_t i = 0; i < sizeof(original) - 1; i++)
	{
		if (original[i] == ' ')
			CHECK(line[i] == ' ');
		else
			CHECK(line[i] != original[i] && std::strchr("#&*%!$@", line[i]) != NULL);
	}
}

TEST(ChatMask, ALongLineIsMaskedToItsEnd)
{
	MChatManager chat;

	// Each masked byte takes the next character of a 165-character mask
	// text in a 256-byte array, from a random start below 16: past the
	// text a NUL cut the line, and past the array the read ran on.
	for (int round = 0; round < 8; round++)
	{
		std::vector<char> line(300, 'a');
		line.push_back('\0');
		chat.AddMask(line.data(), 0);
		CHECK_EQ(300, (int)std::strlen(line.data()));
		bool allMasked = true;
		for (int i = 0; i < 300; i++)
			if (line[i] == 'a' || std::strchr("#&*%!$@", line[i]) == NULL)
				allMasked = false;
		CHECK(allMasked);
	}
}
