#!/usr/bin/env bash
#----------------------------------------------------------------------
# ratchets.sh - shrink-only ratchets (docs/RESTRUCTURING.md, task 0.2)
#----------------------------------------------------------------------
#
# Each ratchet measures one axis of legacy debt. The baselines below are
# the committed truth: a measurement ABOVE its baseline fails (debt may
# not grow), and a measurement BELOW it also fails (progress must be
# recorded - tighten the baseline here AND in docs/RESTRUCTURING.md's
# ratchet table in the same commit as the change that earned it).
#
# Run from anywhere; the script cd's to the repo root. Optional first
# argument: a CMake build directory whose generated DarkEden.vcxproj is
# current (ctest passes its own binary dir) - used by R1.
#
# Never generates into the source tree; measurements are grep-only.
#
#----------------------------------------------------------------------
set -u
# Source files include legacy code-page bytes. These are byte counts;
# every stage (including BSD sed) must use the same single-byte locale.
# A UTF-8 locale can stop sed early yet leave a downstream count at zero.
export LC_ALL=C
cd "$(dirname "$0")/../.." || exit 2

BUILD_DIR="${1:-}"
FAIL=0

check () {
	local name="$1" measured="$2" baseline="$3"
	# An unmeasurable metric is a failure, never a pass: with an empty
	# $measured both comparisons below error out as "false" and would
	# fall through to OK.
	if ! [ "$measured" -ge 0 ] 2>/dev/null; then
		echo "FAIL $name: could not measure (got '$measured')"
		FAIL=1
		return
	fi
	if [ "$measured" -gt "$baseline" ]; then
		echo "FAIL $name: measured $measured > baseline $baseline - this change grows debt the ratchet exists to shrink"
		FAIL=1
	elif [ "$measured" -lt "$baseline" ]; then
		echo "FAIL $name: measured $measured < baseline $baseline - progress! Record it: tighten the baseline in tests/ratchet/ratchets.sh AND docs/RESTRUCTURING.md in this same commit"
		FAIL=1
	else
		echo "OK   $name = $measured"
	fi
}

#----------------------------------------------------------------------
# R1 - translation units compiled directly into the DarkEden target.
#
# Counted from the generated DarkEden.vcxproj, which reflects the CMake
# source lists after configure, or on the Ninja generator from
# build.ninja (the branch below). Preference order: the build dir ctest
# passed in, then the day-to-day tree. On a generator that produces
# neither the ratchet is skipped with a message - skipped, not passed.
#----------------------------------------------------------------------
# 515: 516 - 1. Task 5.2 deleted MitemTableInit.cpp, the server's in-code
# item data (dead here). History: 516 = 517 - 1 (task 4.4's first slice
# moved MItemTable.cpp into gamemodel).
# History: 517 = 518 - 1 (task 4.2 moved MMoneyManager.cpp into gamemodel,
# another of the double-compiled VS_UI_CLIENT_SOURCES entries).
# 518 = 528 - 10. Task 4.1's gamemodel library took its ten members out
# of the executable: ExpInfo.cpp, MString.cpp, MStringArray.cpp and
# DebugLog.cpp, and the six tables that were nominally VS_UI's - the
# relative VS_UI_CLIENT_SOURCES list never matched the exe glob's
# absolute paths in REMOVE_ITEM, so they compiled into both (the
# LNK4217 trap); the membership removal is absolute and asserted.
# History: 493 = 492 + 1 (task 5.3: TextServiceScreen.cpp is a new exe TU.
# A recorded GROWTH, the same trade as 4.4's MItemUse.cpp and
# MSkillAvailable.cpp: TextService::RenderText draws through g_pLast, the
# executable's back buffer, and was the only thing in the TextSystem
# library reaching a client global - so a test binary had to define that
# global to link the library at all. The definition moved to the
# executable side and tests/stubs/ is deleted. One more TU here buys a
# library that links on its own.)
# History: 492 = 493 - 1 (task 5.1's second slice: ClientCommunicationManager.cpp
# moved into packetwire once the three tuning values it read from the
# executable's config went behind WireHost).
# History: 493 = 495 - 2 (task 5.1: Player.cpp and DatagramSocket.cpp
# moved into packetwire once the debug facilities stopped gating the
# wire layer's include rules - the logging header is in basic/ now).
# History: 495 = 497 - 3 + 1 (task 4.4's fourth slice: MSkillManager.cpp,
# MSkillInfoTable.cpp and SkillDef.cpp moved into gamemodel; the
# player-facing half split out of the first, MSkillAvailable.cpp, is a
# new exe TU).
# History: 497 = 502 - 5 (task 4.4's third slice: MPlayerGear.cpp,
# MSlayerGear.cpp, MVampireGear.cpp, MOustersGear.cpp and MShop.cpp
# moved into gamemodel).
# History: 502 = 503 - 1 (task 4.2's third slice: MPriceManager.cpp moved
# into gamemodel).
# History: 503 = 505 - 2 (task 4.2's second slice: MTradeManager.cpp and
# MSortedItemManager.cpp moved into gamemodel).
# History: 505 = 508 - 3 (task 4.3's second slice: MInventory.cpp,
# MStorage.cpp and MShopShelf.cpp moved into gamemodel).
# History: 508 = 512 - 4 (task 4.3's first slice: MItemManager.cpp,
# MGridItemManager.cpp, MSlotItemManager.cpp and MQuickSlot.cpp moved
# into gamemodel).
# History: 512 = 515 + 2 - 5 (task 4.4's second slice: MItem.cpp,
# MObject.cpp, UserInformation.cpp, ClientConfig.cpp and
# MTimeItemManager.cpp moved into gamemodel; the executable halves split
# out of the first two, MItemUse.cpp and MObjectScreen.cpp, are new exe
# TUs). 515 = 516 - 1 (task 5.2 deleted the dead MitemTableInit.cpp).
# 516 = 517 - 1 (4.4's first slice moved MItemTable.cpp). 517 = 518 - 1
# (4.2 moved MMoneyManager.cpp). 518 = 528 - 10 (4.1's gamemodel).
# History: 528 = 529 - 1 (task 2.5 deleted CRRequest2, a dead duplicate
# of CRRequest claiming the same packet id, with its handler
# Client/PacketHandler/CRRequest2Handler.cpp). 529 = 992 - 463. Task 2.4 moved every packet class, the
# factory/validator tables and the last held-back info classes (465
# .cpp files, tests/arch/packetwire_files.txt) into packetwire; the exe
# gained the split-out GCExchangeBuyHandler.cpp (+1) and the five root
# info classes that moved under Client/Packet were already counted
# before (0 net). 992 = 993 - 1 (task 2.2's PacketHandlerRegistry.cpp,
# a recorded +1, offset by the finished migration deleting
# CGHandlersStub.cpp).
#
# 492: 493 - 1. Task 5.1's third slice took ClientPlayer.cpp into
# packetwire. Its last two game-code includes - MZone.h and
# UserInformation.h - were wanted by setEncryptCode() alone, and both
# are behind WireHost now.
#
# That slice said the body those includes served is never compiled,
# because nothing defines __USE_ENCRYPTER__. THAT WAS WRONG.
# Client/Packet/Encrypter.h defines it, ClientPlayer.cpp includes
# SocketEncryptInputStream.h two lines before its first #ifdef on it,
# and GCUpdateInfoHandler calls setEncryptCode() on every login. The
# move is therefore load-bearing rather than cosmetic, and the seed
# collapse it performed is a live behaviour change - one a test now
# pins across every server number a byte can hold.
#
# 489: 492 - 3. Task 5.1's fourth slice took the request-service family
# bar one: RequestClientPlayer.cpp, RequestServerPlayer.cpp and
# RequestServerPlayerManager.cpp. The clock, the in-game test and six
# calls of the peer file-transfer manager are behind WireHost; the
# manager itself stays in the executable, because it draws, writes the
# profile directory and reads the UI.
#
# One of the three needed nothing at all. RequestServerPlayerManager.cpp
# reached no executable symbol on any live line - both ClientDef.h and
# ServerInfo.h were unused includes - and had sat in the holdouts file
# for two slices because the list there was compiled by grepping for
# symbol names without asking whether the line was live. Every
# g_pGameMessage use in all four files is inside
# `#if defined(_DEBUG) && defined(OUTPUT_DEBUG)`, and nothing defines
# OUTPUT_DEBUG.
#
# 488: 489 - 1. Task 5.1's fifth slice took the last holdout,
# RequestClientPlayerManager.cpp, and Client/Packet is wire-only
# (tests/arch/packetwire_holdouts.txt lists nothing). Its seams - the
# logged-in character's name, world and race, the four whisper-queue
# calls and the two failure notifications - are nine WireHost entries;
# the Ninja baseline moves with it (486 -> 485).
#
# 487: 488 - 1. Task 5.2's seventh slice deleted Client/WhisperManager.cpp
# with the rest of the peer-to-peer whisper path, which upstream had
# compiled out (`0 &&` around every writer of its queue): whispers go
# to the game server as CGWhisper from UIMessageManager, and the seven
# WireHost entries that served the path went too. Ninja 485 -> 484.
#
# 484: 487 - 3. The eighth slice deleted the rest of the outbound peer
# side - this client dialling other clients - which upstream had also
# compiled out: RequestClientPlayer and its manager (packetwire, so not
# in this count), the three RC handlers that took that player (the
# three TUs here), the receive half of RequestFileManager, the dialling
# half of ProfileManager and the requesting half of RequestUserManager.
# The inbound side (RequestServerPlayer, its manager, the file sender)
# and the UDP party datagrams stay. Ninja 484 -> 481 by the same
# arithmetic (no Ninja tree here; the Linux CI reads it).
# 482 on 2026-09-17: task 4.5 moves RankBonusTable.cpp into gamemodel
# (483 -> 482). The preceding bounded-formatting slice deleted md5.cpp
# without recording its 484 -> 483 decrease here. Ninja 481 -> 479 for
# the same two files; its three platform exclusions are unchanged.
# 478 on 2026-09-17: delete the four unused Direct3D texture/shadow cache
# translation units. Their last object declarations were commented out;
# the active sprite path never constructed them. Ninja 479 -> 475.
# 477: remove the unused WinINet downloader (Windows-only source).
# The Ninja baseline stays 475; the platform difference is now two files.
# 473: map record classes/header move to gamemodel; two executable files
# retain screen geometry and live interaction actions (six out, two in).
# 472: CMessageArray moves from the executable to basic.
# 471: the three string-reduction functions move into basic.
# 470: UserOption.cpp moves byte-for-byte into VS_UI, so its existing test
# links the same library object as the game rather than recompiling the source.
# 469: the corrected SXml implementation moves unchanged into VS_UI; its
# older duplicate is removed and ui_tests links the production parser.
# 468: the help-message loader moves unchanged into VS_UI for parser tests.
# 467: CToken moves byte-identically into gamemodel for reset/lifetime tests.
# 466: remove the empty Client_PCH.cpp; CMake now generates the PCH producer.
# Count application sources, excluding only that generated target-specific file.
# 465: remove the inactive Client/DebugInfo.cpp implementation.
# 460: move five unchanged world metadata implementations into gamemodel.
# 456: promote two quest models plus the profile and shrine adapters.
# 455: party membership joins gamemodel behind MPartyHost.
# 456 = 455 + 1: MNPCTableEnglish.cpp, the generated English NPC name
# table (tools/i18n/npc_gen.pl). A recorded GROWTH, the same shape as
# MNPCScriptTableEnglish.cpp: it writes g_pNPCTable, an executable global,
# so it is an exe TU by construction.
# 454: the NPC talk boxes (TalkBox.cpp, MStringList.cpp) join gamemodel so
# the menu-answer mapping can be tested.
# 453: MStatusManager.cpp joins gamemodel so the character-select stat
# preview can be tested (task 4.12, slice 3).
# 449: the action and effect tables (MActionInfoTable, MEffectSpriteTypeTable,
# MEffectStatusTable, MCreatureSpriteTable) join gamemodel so their loaders
# can be tested (task 4.1).
# 447: the chat filter (MStringMap.cpp, MChatManager.cpp) joins gamemodel,
# its option read behind MChatHost, so the curse filter can be tested
# (task 4.13).
# 445: the status array (MStatus.cpp) and the mode register
# (TempInformation.cpp) join gamemodel unchanged so they can be tested
# (task 4.14); measured on the tree with both moves.
# 437: the eight packet handlers that reach only model state (the phone
# slots, the trade money and offer, the system switches and the
# monster-kill quest goals) join gamemodel from Client/PacketHandler so a
# test can run them on real packets (task 4.15).
# 436: CServerInformation joins gamemodel unchanged so login selection
# and its owning world/server maps can be tested (task 4.19).
# 434: MFunctionManager and MRequestMode join gamemodel unchanged (task 4.20).
# 433: MathTable joins gamemodel unchanged (task 4.21).
# 432: ServerInfoFileParser joins gamemodel after dropping MinTr (task 4.22).
# 431: MWeather joins gamemodel behind its origin/viewport host (task 4.23).
# 428: self-defense membership and its two packet handlers join gamemodel (task 4.25).
# 427: per-world character-selection settings join gamemodel unchanged (task 4.26).
# 425: monster and level-name tables join gamemodel unchanged (task 4.27).
# 424: war-state manager joins gamemodel behind live UI/zone services (4.28).
# 423: NPC shop templates join gamemodel with shelf-building rules (4.29).
# 421: NPC dialogue and its English overlay join gamemodel (4.30).
# 420: effect-target state joins gamemodel with deferred result ownership (4.31).
# 419: portal records join gamemodel (task 4.32).
# 418: background-music metadata joins gamemodel with track selection (4.33).
# 417: delayed sound records join gamemodel with queue scheduling (4.34).
# 416: ShowTimeChecker scheduling joins gamemodel with explicit time inputs (task 4.36).
# 415: the help-string table joins gamemodel (task 4.37).
# 414: interaction-object metadata joins gamemodel (task 4.38).
# 413: base effect state joins gamemodel behind frame/light services (4.44).
# 411: moving and screen effects join gamemodel (4.45).
# 410: randomized draw-skipping effects join gamemodel (4.46).
# 409: complete linear effects join gamemodel (4.48).
# 407: creature guidance and chase effects join gamemodel (4.49).
# 406: complete homing effects join gamemodel (4.50).
# 405: complete parabolic effects join gamemodel (4.51).
# 404: attached effects join gamemodel (4.52).
# 403: complete orbiting attachments join gamemodel (4.53).
# 401: effect ownership and screen lifecycle managers join gamemodel (4.54).
# 400: inventory effect generation joins gamemodel (4.55).
# 399: falling effect generation joins gamemodel (4.56).
# 398: rising effect generation joins gamemodel (4.57).
# 397: multi-projectile falling generation joins gamemodel (4.58).
# 396: zone-attack effect generation joins gamemodel (4.59).
# 394: parabolic zone and bomb generators join gamemodel (4.60).
# 393: stationary zone generation joins gamemodel (4.61).
# 392: cross-shaped zone generation joins gamemodel (4.62).
# 390: fixed X and rhombus generation joins gamemodel (4.63).
# 389: empty-cross generation joins gamemodel (4.64).
# 387: horizontal and vertical empty-wall generation joins gamemodel (4.65).
# 386: empty-rectangle generation joins gamemodel (4.66).
# 385: full-rectangle generation joins gamemodel (4.67).
# 384: complete wall generation joins gamemodel (4.68).
# 383: multiple stationary generation joins gamemodel (4.69).
# 382: random-zone generation joins gamemodel (4.70).
# 381: selectable stationary generation joins gamemodel (4.71).
# 380: spread-out generation joins gamemodel (4.72).
# 379: around-zone generation joins gamemodel (4.73).
# 378: follow-path generation joins gamemodel (4.74).
# 377: meteor-drop generation joins gamemodel (4.75).
# 376: creature parabola generation joins gamemodel (4.76).
# 375: wide ripple generation joins gamemodel (4.77).
# 374: Bloody Breaker generation joins gamemodel (4.78).
# 373: Bloody Wall generation joins gamemodel (4.79).
# 367: metadata and rank handlers join gamemodel (4.80, 4.82).
R1_BASELINE=367

R1_VCXPROJ=""
for candidate in "$BUILD_DIR/DarkEden.vcxproj" "build/vs2022/DarkEden.vcxproj"; do
	if [ -n "$candidate" ] && [ -f "$candidate" ]; then
		R1_VCXPROJ="$candidate"
		break
	fi
done

if [ -n "$R1_VCXPROJ" ]; then
	# A tree configured before the source lists were last edited
	# measures a different tree (parallel sessions share this checkout);
	# that is a failure to measure, not a pass. The configure time is
	# CMakeFiles/generate.stamp - the Visual Studio generator touches it
	# on every generate, while it leaves an unchanged .vcxproj (and an
	# unchanged CMakeCache.txt) alone, so those mtimes would report a
	# fresh reconfigure as stale.
	R1_CACHE="$(dirname "$R1_VCXPROJ")/CMakeFiles/generate.stamp"
	if [ ! -f "$R1_CACHE" ] || [ CMakeLists.txt -nt "$R1_CACHE" ] || [ tests/arch/packetwire_files.txt -nt "$R1_CACHE" ] || [ tests/arch/gamemodel_files.txt -nt "$R1_CACHE" ]; then
		echo "FAIL R1: $(dirname "$R1_VCXPROJ") was configured before CMakeLists.txt or a library membership file last changed - reconfigure that tree first"
		FAIL=1
	else
		R1=$(grep "<ClCompile Include" "$R1_VCXPROJ" | grep -vcE '[/\\]CMakeFiles[/\\]DarkEden\.dir[/\\]cmake_pch\.cxx"')
		check "R1 (TUs in DarkEden.vcxproj: $R1_VCXPROJ)" "$R1" "$R1_BASELINE"
	fi
elif [ -n "$BUILD_DIR" ] && [ -f "$BUILD_DIR/build.ninja" ]; then
	# The Ninja generator (the Linux and macOS presets): the same count,
	# read from build.ninja's object rules for the DarkEden target. Its
	# own baseline, because the non-Windows source list is two files
	# shorter: of CMakeLists.txt's NOT WIN32 filters only GetWinVer,
	# and WavePackFileManager match anything the glob
	# yields (the Immersion, D3D, EXECryptor and VolumeOut patterns match
	# no file at all). Before
	# this branch existed the ratchet SKIPPED on every non-MSVC tree,
	# which the port assessment listed as fail-open (area A). build.ninja
	# is rewritten on every configure, so its mtime is the configure time.
	# 454 = 453 + 1: MNPCTableEnglish.cpp, the same growth as the MSVC
	# baseline above records.
	# 452: the NPC talk boxes join gamemodel, as the MSVC baseline above records.
	# 451: MStatusManager.cpp joins gamemodel, as the MSVC baseline records.
	# 447: the four action and effect tables join gamemodel, as the MSVC
	# baseline records.
	# 445: the chat filter joins gamemodel, as the MSVC baseline records.
	# 443: MStatus.cpp and TempInformation.cpp join gamemodel, as the MSVC
	# baseline records.
	# 435: the eight model-reach packet handlers join gamemodel, as the
	# MSVC baseline records.
	# 434: CServerInformation joins gamemodel, as the MSVC baseline records.
	# 432: callback dispatch and request modes join gamemodel (task 4.20).
	# 431: effect math joins gamemodel (task 4.21).
	# 430: dimension-specific server configuration joins gamemodel (task 4.22).
	# 429: weather simulation joins gamemodel (task 4.23).
	# 426: self-defense membership and its two packet handlers join gamemodel (task 4.25).
	# 425: per-world character-selection settings join gamemodel (task 4.26).
	# 423: monster and level-name tables join gamemodel (task 4.27).
	# 422: war-state manager joins gamemodel (task 4.28).
	# 421: NPC shop templates join gamemodel (task 4.29).
	# 419: NPC dialogue and its English overlay join gamemodel (task 4.30).
	# 418: effect-target state joins gamemodel (task 4.31).
	# 417: portal records join gamemodel (task 4.32).
	# 416: background-music metadata joins gamemodel (task 4.33).
	# 415: delayed sound records join gamemodel (task 4.34).
	# 414: ShowTimeChecker scheduling joins gamemodel (task 4.36).
	# 413: the help-string table joins gamemodel (task 4.37).
	# 412: interaction-object metadata joins gamemodel (task 4.38).
	# 411: base effect state joins gamemodel behind frame/light services (4.44).
	# 409: moving and screen effects join gamemodel (4.45).
	# 408: randomized draw-skipping effects join gamemodel (4.46).
	# 407: complete linear effects join gamemodel (4.48).
	# 405: creature guidance and chase effects join gamemodel (4.49).
	# 404: complete homing effects join gamemodel (4.50).
	# 403: complete parabolic effects join gamemodel (4.51).
	# 402: attached effects join gamemodel (4.52).
	# 401: complete orbiting attachments join gamemodel (4.53).
	# 399: effect ownership and screen lifecycle managers join gamemodel (4.54).
	# 398: inventory effect generation joins gamemodel (4.55).
	# 397: falling effect generation joins gamemodel (4.56).
	# 396: rising effect generation joins gamemodel (4.57).
	# 395: multi-projectile falling generation joins gamemodel (4.58).
	# 394: zone-attack effect generation joins gamemodel (4.59).
	# 392: parabolic zone and bomb generators join gamemodel (4.60).
	# 391: stationary zone generation joins gamemodel (4.61).
	# 390: cross-shaped zone generation joins gamemodel (4.62).
	# 388: fixed X and rhombus generation joins gamemodel (4.63).
	# 387: empty-cross generation joins gamemodel (4.64).
	# 385: horizontal and vertical empty-wall generation joins gamemodel (4.65).
	# 384: empty-rectangle generation joins gamemodel (4.66).
	# 383: full-rectangle generation joins gamemodel (4.67).
	# 382: complete wall generation joins gamemodel (4.68).
	# 381: multiple stationary generation joins gamemodel (4.69).
	# 380: random-zone generation joins gamemodel (4.70).
	# 379: selectable stationary generation joins gamemodel (4.71).
	# 378: spread-out generation joins gamemodel (4.72).
	# 377: around-zone generation joins gamemodel (4.73).
	# 376: follow-path generation joins gamemodel (4.74).
	# 375: meteor-drop generation joins gamemodel (4.75).
	# 374: creature parabola generation joins gamemodel (4.76).
	# 373: wide ripple generation joins gamemodel (4.77).
	# 372: Bloody Breaker generation joins gamemodel (4.78).
	# 371: Bloody Wall generation joins gamemodel (4.79).
	# 365: the same cross-platform members leave the executable.
	R1_NINJA_BASELINE=365
	R1_NINJA="$BUILD_DIR/build.ninja"
	if [ CMakeLists.txt -nt "$R1_NINJA" ] || [ tests/arch/packetwire_files.txt -nt "$R1_NINJA" ] || [ tests/arch/gamemodel_files.txt -nt "$R1_NINJA" ]; then
		echo "FAIL R1: $BUILD_DIR was configured before CMakeLists.txt or a library membership file last changed - reconfigure that tree first"
		FAIL=1
	else
		R1=$(grep -E '^build CMakeFiles/DarkEden\.dir/.*\.(cpp|c|cxx|cc)\.o:' "$R1_NINJA" | grep -vcE '^build CMakeFiles/DarkEden\.dir/cmake_pch\.cxx\.o:')
		check "R1 (TUs in the DarkEden target of $R1_NINJA)" "$R1" "$R1_NINJA_BASELINE"
	fi
else
	echo "SKIP R1: no generated DarkEden.vcxproj or build.ninja found - pass the build directory as the first argument"
fi

#----------------------------------------------------------------------
# R2 - packet .cpp files still defining a packet-style ::execute
# (first parameter Player*). The client twin of the server's R4; task
# 2.2 drove it to 0 and this ratchet holds it there. The regex was
# refined when it reached 0: the original '::execute\s*\(' also
# matched comments and handler bodies defined inside packet files
# (GCExchangeBuy), which are not the debt being measured. Anchored
# 'void X::execute(Player' matches exactly the legacy virtual.
#----------------------------------------------------------------------
R2_BASELINE=0

# A zero baseline is fail-open if the measured tree silently is not
# there (a directory rename would make grep count 0 of nothing and
# PASS), so the directories are asserted first.
for d in Client/Packet/Gpackets Client/Packet/Cpackets Client/Packet/Lpackets \
	Client/Packet/Rpackets Client/Packet/Upackets; do
	if [ ! -d "$d" ]; then
		echo "FAIL R2: directory $d is missing - fix the path list in this script"
		FAIL=1
	fi
done

R2=$(grep -rlE '^void\s+\w+::execute\s*\(\s*Player' \
	Client/Packet/Gpackets Client/Packet/Cpackets Client/Packet/Lpackets \
	Client/Packet/Rpackets Client/Packet/Upackets \
	--include='*.cpp' 2>/dev/null | grep -v Handler | wc -l)
check "R2 (packet cpps defining execute)" "$R2" "$R2_BASELINE"

#----------------------------------------------------------------------
# R3 - live sprintf/strcpy/strcat lines in the packet tree: the wire
# classes under Client/Packet AND the handlers under Client/PacketHandler
# (task 2.4 moved the handlers out; they are where most of the strcpy
# targets fed by server strings live, so the metric follows them).
# (snprintf does not match: \b rejects the preceding 'n'. The // comment
# tail of every line is stripped BEFORE matching, so commented-out code
# does not count - a quarter of the first baseline was - while a live
# call with a trailing comment still does; the earlier whole-line
# exclusion let `sprintf(...); // was strcpy` through.)
#
# 18: 19 - 1. The review round of task 5.4's first slice found a
# pre-existing overflow one line below a site that slice had converted:
# GCBloodBibleListHandler sprintf'd "%3d %s" into char[192] from a
# char[192], four bytes short. Bounding it converts a 28th sprintf line,
# even though its format is a literal and it was never a C19 site - which
# is the difference between R3 and R7 in one example.
# History: 19 = 46 - 27. Task 5.4's first slice converted all 31
# sprintf-family calls in Client/PacketHandler whose format came from the
# game string table to SafeFormat::Format. Only 27 of them moved this
# number: the other four are wsprintf, which \b rejects because of the
# preceding 'w'. R7 below counts all 31, which is why both exist - each
# is blind to something the other sees.
#----------------------------------------------------------------------
# 0: 18 - 18. The packet-tree copy pass bounded all seventeen live sites
# in Client/PacketHandler - sprintf(dst, ...) became
# snprintf(dst, sizeof(dst), ...) and strcpy(dst, src) became
# snprintf(dst, sizeof(dst), "%s", src), every destination checked by
# hand to be a real array so sizeof() is the right bound - and deleted
# the eighteenth, which was inside a /* */ block in ClientPlayer.cpp
# that also named three executable symbols from a library source.
#
# One of the seventeen was a live overflow, not a regression guard:
# GCNPCSayDynamic::read accepts a message of up to 2048 bytes and
# GCNPCSayDynamicHandler strcpy'd it into char[256] - 1792 bytes of
# stack, at a server's discretion. The wire-side limits the other
# handlers rest on are pinned by tests now
# (tests/unit/test_packetwire_parsers.cpp), because the packets are
# library code and the handlers are not.
#
# From here this holds a line rather than tracking a retreat: a new
# unbounded copy in either directory fails the suite. Read the zero the
# way R7's comment says to read its own - it is a statement about this
# grep, which is line-based, strips only // comments, and cannot see a
# copy whose destination or call is spelled some other way.
R3_BASELINE=0

for d in Client/Packet Client/PacketHandler; do
	if [ ! -d "$d" ]; then
		echo "FAIL R3: directory $d is missing - fix the path list in this script"
		FAIL=1
	fi
done

R3=$(grep -rhE '\b(sprintf|strcpy|strcat)\s*\(' Client/Packet Client/PacketHandler --include='*.cpp' \
	| sed -e 's://.*::' | grep -cE '\b(sprintf|strcpy|strcat)\s*\(')
check "R3 (unsafe format/copy lines in Client/Packet + Client/PacketHandler)" "$R3" "$R3_BASELINE"

#----------------------------------------------------------------------
# R4 - library-compiled .cpp files referencing g_p* client globals.
#
# Membership: every .cpp under the whole-directory library trees, plus
# the packetwire and gamemodel membership files (tests/arch/
# packetwire_files.txt, tests/arch/gamemodel_files.txt - the CMake
# targets and the include checker read the same files). Extraction work
# (Phase 4) shrinks this by cutting the global seams. The pattern is the
# g_p prefix: the two clocks the item host carries (g_CurrentFrame,
# g_CurrentTime) were never in this count, so cutting them did not move
# it.
#
# 25: 27 - 2, a reclassification (task 4.4's fourth slice moved the skill
# core into gamemodel): VS_UI_SKILL_VIEW.cpp and VS_UI_skill_tree.cpp
# reached past the libraries only for g_pSkillInfoTable and
# g_pSkillManager, which MSkillManager.cpp defines. Note what this
# ratchet cannot see: it matches g_p* only, so a library file calling an
# executable-side function - VS_UI_GameCommon.cpp calls
# g_pSkillAvailable->SetAvailableSkills(), which lives in the
# executable's MSkillAvailable.cpp - is a seam the number does not
# count, before or after.
# 27: 28 - 1, a reclassification (task 4.4's third slice moved the gear
# into gamemodel): VS_UI_Game.cpp's only reaches past the libraries were
# g_pSlayerGear, g_pVampireGear and g_pOustersGear, which the gear
# sources define.
# 28: 35 + 1 - 8, again a reclassification, by the library-wide
# definition rule below (task 4.4's second slice). MItem.cpp joined the
# library reading gamemodel's own tables (+1 under the old per-file
# rule); the union rule then excludes it and seven earlier members:
# Datagram.cpp (g_pPacketFactoryManager, packetwire's own) and six
# VS_UI sources (AcceleratorDef, VS_UI_ELEVATOR, VS_UI_Item,
# VS_UI_Message, VS_UI_Shop, VS_UI_progress) whose only reaches are
# gamemodel's tables (g_pItemTable, g_pItemOptionTable,
# g_pGameStringTable, g_pUserInformation), packetwire's g_pFileDef, or
# their own library's g_pKeyAccelerator and g_pSystemAvailableManager.
# 35: 59 - 24, a RECLASSIFICATION, not progress on the seams: the 36
# Client/*.cpp files VS_UI_CLIENT_SOURCES listed were compiled into
# both VS_UI.lib and the executable, and the executable's objects were
# the ones that linked; the list is gone and they compile once, into
# the executable, so the 24 of them that reach g_p globals are
# executable debt now, outside this ratchet (R1 already counts them).
# History: 21 = 25 - 4 (task 5.3). Partly seam-cutting and partly a
# refined measurement, and it is worth being clear which. The real cut
# is TextSystem: TextService.cpp lost its live g_pLast reach when
# RenderText moved to the executable. The other three never had one -
# they only NAMED a global in a comment, and R4 counted that, unlike R3
# and R5 which have always stripped comment lines. Dropped:
# Client/TextSystem/TextService.cpp (which after the move would have
# counted for the comment 5.3 wrote ABOUT the seam it had just cut -
# that is how the flaw surfaced), Client/Packet/SocketOutputStream.cpp
# (a commented-out g_pLogManager call), Client/SpriteLib/
# SpriteLibBackendSDL.cpp and VS_UI/src/VS_UI_WebBrowser.cpp. The 21
# that remain are real reaches, and every one is a VS_UI file.
# History: 59 = 61 - 2. Task 4.1 cut the two g_pFileDef seams in MGameStringTable
# (UseEnglishText takes the Properties table) and SystemAvailabilities
# (LoadFromStream; the executable reads the archive); the four support
# sources gamemodel added (ExpInfo, MString, MStringArray, DebugLog)
# reference no game global - DebugLog.cpp's dead #if 0 block that named
# g_pDebugMessage was deleted rather than counted.
# History: 61 = 81 - 20. Task 2.4 grew the membership from 52 to 517 files, and
# the count still went DOWN because the measurement was refined with it
# (see the R4 loop): a file referencing only globals it defines itself
# no longer counts. The two dead server-only blocks that referenced
# game globals (GCSelectQuestID's PlayerCreature constructor,
# GCStashList::setStashItem from a live Item*) were deleted rather than
# grandfathered.
#----------------------------------------------------------------------
# 20: editor rendering uses the common TextService-backed printer. Its direct
# g_pLast/g_pBack declarations disappear; the printer still reaches game state.
# 10: UserOption.cpp and g_pUserOption move into VS_UI. Ten existing UI files
# now resolve their only previously executable-owned global in the library;
# this is ownership reclassification, not extraction of those ten files.
# 11: force text mode when scanning source, including GameCommon's embedded
# NUL bytes. GNU grep had omitted that file; BSD grep already counted it.
# This corrects the measurement, not a new library-to-executable dependency.
# 10: remove the duplicate, inactive VS_UI/DebugInfo.cpp implementation.
# 5: world metadata globals are now owned by the tested gamemodel objects.
# 4: remove CImm's unused executable sound-manager declaration.
# 0: UiRuntime carries live UI services; party ownership is in gamemodel.
# An inline comment naming the player global is translated; scanner unchanged.
R4_BASELINE=0

lib_members () {
	# The directory trees minus the files CMake excludes from the
	# library builds (the hangul Ci/FL2 pair under USE_SDL_BACKEND,
	# which is forced ON) - the original raw find counted two
	# never-compiled g_p-referencing files (WinMain, Ci) as library
	# debt; FL2 is excluded for consistency though it references none.
	# VS_UI/WinMain.cpp was excluded here by name until task 5.2's sixth
	# slice deleted it (2026-09-09); the number did not move.
	find basic Client/SpriteLib Client/DXLib Client/framelib Client/TextSystem VS_UI \
		-name '*.cpp' 2>/dev/null \
		| grep -vE 'VS_UI/src/hangul/(Ci|FL2)\.cpp$'
	sed -e 's/#.*//' tests/arch/packetwire_files.txt \
		| grep -oE 'Client/Packet/[A-Za-z0-9_/]+\.cpp'
	sed -e 's/#.*//' tests/arch/gamemodel_files.txt \
		| grep -oE 'Client/[A-Za-z0-9_/]+\.cpp'
}

# A global DEFINED by a library file is not a seam into the executable:
# the reference resolves inside the libraries. The packet tables own
# g_pPacketFactoryManager / g_pPacketValidator (task 2.4); since task
# 4.4's item core, MItem.cpp reads g_pItemTable, g_pItemOptionTable,
# g_pGameStringTable, g_pUserInformation, g_pClientConfig and
# g_pTimeItemManager, every one defined by another gamemodel member -
# so the subtraction is against the union of every library file's
# definitions, not the file's own. (Until 4.4 it was per file, which
# would have counted the item core as debt for reading a table that
# sits beside it in the same library.)
lib_defs () {
	lib_members | sort -u | while read -r f; do
		[ -f "$f" ] || continue
		grep -aoE '^[A-Za-z_][A-Za-z0-9_:<>]*[[:space:]]*\*?[[:space:]]*g_p[A-Z]\w*[[:space:]]*(=|;)' "$f" \
			| grep -aoE '\bg_p[A-Z]\w*'
	done | sort -u
}
R4=$(defs=$(lib_defs); lib_members | sort -u | while read -r f; do
	[ -f "$f" ] || continue
	# Comment lines are not references. R3 and R5 have always filtered
	# them; R4 did not, so a file that only NAMED a global counted as
	# reaching it - which is worse than noise, because it can hide a
	# real tightening behind a sentence somebody wrote about the seam
	# they had just cut. Task 5.1's second slice worked around it by
	# rewording a comment; task 5.3 hit it again and fixed the
	# measurement instead. Line-based, so the same blindness R5
	# documents applies: a reference inside a /* */ block still counts.
	refs=$(grep -avE '^[[:space:]]*(//|\*|/\*)' "$f" \
		| grep -aoE '\bg_p[A-Z]\w*' | sort -u)
	[ -n "$refs" ] || continue
	if [ -n "$(comm -23 <(echo "$refs") <(echo "$defs"))" ]; then
		echo "$f"
	fi
done | wc -l)
check "R4 (library cpps referencing g_p globals)" "$R4" "$R4_BASELINE"

#----------------------------------------------------------------------
# R5 - direct packet execute() call sites outside Client/Packet.
#
# The receive loops and PacketDispatcher are the only sanctioned
# callers; the adversarial review of task 2.2 found the client also
# fabricates packets locally and executed them directly (skill echoes,
# GM system messages, the login-path CGConnectSetKey), which the R2
# migration would have silently broken. Those sites now go through
# PacketDispatcher::dispatch. The baseline of 1 is the commented-out
# PacketAttackMelee block in CGameUpdate.cpp (~line 5867), which lives
# inside a /* */ block this line-based grep cannot see; it leaves with
# that dead block's deletion. Client/PacketHandler (where task 2.4 moved
# the handlers) is IN scope: two handlers turned out to fabricate a
# CGConnectSetKey and execute() it directly - invisible while they lived
# under the excluded Client/Packet, caught by the compiler when
# Packet::execute was deleted, and now routed through the dispatcher.
# With the virtual gone a new direct caller is a compile error first.
#----------------------------------------------------------------------
# The unused PacketAttackMelee block is deleted (2026-09-24).
R5_BASELINE=0

R5=$(grep -rnE '(\.|->)execute\s*\(\s*(g_pSocket|NULL|this|0)\s*\)' \
	Client VS_UI --include='*.cpp' 2>/dev/null \
	| grep -v 'Client/Packet/' | grep -vE ':\s*//' | wc -l)
check "R5 (direct packet execute callers outside Client/Packet)" "$R5" "$R5_BASELINE"

#----------------------------------------------------------------------
# R7 - call sites that hand a game string table entry to printf as its
# format argument.
#
# Finding C19 (docs/code-health-review-2026-08-29.md) as a number. Every
# UI string in this client is read from Data/Info/String.inf, and at
# each of these sites it is the *format* rather than an argument: an
# entry carrying one %s more than the call site passes makes the CRT
# read a stack word as a char* and copy it, unbounded, into a
# destination of fifty or a hundred bytes. What converts a site is
# SafeFormat::Format (basic/SafeFormat.h), which checks the entry's
# conversions against the arguments it was really handed, so this counts
# what is left to do (task 5.4).
#
# The AddFormat family is in scope even though CMessageArray bounds its
# own buffer (finding C20, fixed in 0e9d247): what was left there was the
# arity half of the same defect, and task 5.4's third slice closed it
# through CMessageArray::AddSafeFormat. The alternative stays in the
# pattern, because what it guards now is a NEW AddFormat written against
# a table entry - which is the door a ratchet exists to hold.
#
# Two measurement decisions, each of them earned while this was written:
#
#   -a   Defensive, and NOT the reason it is here. VS_UI's sources are
#        CP949 encoded, and grep calls such a file binary and stops at
#        the first byte it dislikes - which is how the exploratory
#        per-file `grep -r` used to survey this population reported 113
#        of VS_UI's real 198. It does not bite the pipeline below,
#        where everything arrives on one stdin stream that grep does not
#        classify per file: measured, this pattern gives the same count with the
#        flag and without it. The review round of task 5.4's first
#        slice caught the comment claiming otherwise. The flag stays,
#        because a future per-file variant would need it and nothing
#        signals when binary detection truncates a scan.
#   tr   The files are joined into one stream before matching, because
#        four of Client/PacketHandler's sites put the destination and
#        the format on different lines. A line-based count cannot see
#        those, so converting them would have measured as a no-op -
#        which is the failure mode task 5.3 found in R4 and fixed there.
#        The gap this pattern allows between the destination and the
#        format admits no quote, paren or semicolon, which is what stops
#        a joined stream from matching across two unrelated statements;
#        counted per file and joined, the two agree exactly.
#
# It stays a count of sites rather than of files, because a file here is
# converted a call at a time and half a file is real progress.
#----------------------------------------------------------------------
# 0: 37 - 37. Task 5.4's fourth slice converted the last site THIS METRIC
# CAN SEE, so from here it holds a line rather than tracking a retreat:
# any new one fails the suite.
#
# Read that sentence literally, because the first draft of it did not and
# claimed the client no longer hands a String.inf entry to a printf as
# its format at all. It still does, at 24 sites. What this pattern
# matches is a lookup spelled AT THE FORMAT ARGUMENT. An entry copied
# into a static array or a local first, and formatted with from there, is
# invisible to it - and VS_UI_ExtraDialog.cpp does exactly that 21 times,
# 12 of them passing no varargs, into buffers sized strlen(format)+1.
# Both reviewers of that slice found them; the ratchet did not, and
# cannot. Finding C19 is open, and a zero here is a statement about this
# grep rather than about the code.
#
# The last two sites it could see never had a GetString() at all -
# GameUI.cpp passed the MString itself and leaned on its implicit
# conversion to const char*, which is why the mechanical sweeps could not
# match them and why they outlived every other site.
#
# History: 37 = 64 - 27. Task 5.4's third slice took the AddFormat family through
# CMessageArray::AddSafeFormat, which packs its arguments with their
# types instead of as varargs. What is left is 37 ordinary sprintf sites,
# all executable-side: UIMessageManager.cpp 14, MTopView.cpp 10,
# GameUI.cpp 7, ModifyStatusManager.cpp 3, CGameUpdate.cpp 2,
# PacketFunction.cpp 1. (The first draft of this line named
# ModifyStatusManager.cpp among the top three, copied from the text the
# slice had just made false by taking 16 of its 19 away.)
# History: 64 = 262 - 198 (the second slice: VS_UI's 193 sites and the 5
# offset-append sites its review round exposed). 262 = 293 - 31 (the
# first slice, Client/PacketHandler).
#
# Those history numbers have been restated twice, both times because the
# pattern could not see a whole shape rather than because the tree
# changed. First recorded as 287 and 256, when the pattern demanded the
# format at argument two and so matched no counted call at all; then as
# 288 and 257, before the offset-append alternative below. The lesson is
# in the numbers: this metric has been wrong twice in the same direction,
# and each time the missing shape was live code in a file the slice had
# just edited.
R7_BASELINE=0

for d in Client VS_UI; do
	if [ ! -d "$d" ]; then
		echo "FAIL R7: directory $d is missing - fix the path list in this script"
		FAIL=1
	fi
done

# The format is at a different argument position in each of the three
# families, which is why this is three alternatives and not one. The
# first draft had only the first and the third, and so could not match a
# counted call in ANY form - it listed _snprintf and swprintf in the
# alternation while requiring the format at argument two, where those
# take a size. That was not academic: it missed a live site
# (vs_ui_gamecommon2.cpp, the guild quest mission line), and it left the
# door open, since a newly written snprintf(dst, sizeof dst,
# GetGameString(...), ...) could not have raised the count.
GAMESTRING='(\(\*g_pGameStringTable\)\[|GetGameString[[:space:]]*\()'

# sprintf family: format at argument 2. fprintf is in the list for the
# same reason R8 has it - none of its 550 calls takes a table entry
# today, and nothing was stopping the next one.
R7_PATTERN="\\b(sprintf|wsprintf|swprintf|vsprintf|fprintf)[[:space:]]*\\([[:space:]]*[^,;()\"]+,[[:space:]]*$GAMESTRING"

# printf itself, where the format is the FIRST argument. It was in
# neither ratchet until the fifth slice's review round, and the leading
# alternation rather than \b is what lets it match: \b[A-Za-z_]... in
# R8's first draft required a character before printf and so could never
# see the bare call. 86 of them in the tree, none with a table entry.
R7_PATTERN="$R7_PATTERN|(^|[^A-Za-z0-9_])printf[[:space:]]*\\([[:space:]]*$GAMESTRING"

# counted family: format at argument 3, argument 2 being a size, which is
# usually sizeof(dst) and so may contain parentheses - bounded instead by
# refusing a semicolon or a quote and by a length cap.
R7_PATTERN="$R7_PATTERN|\\b(_?snprintf|_?vsnprintf|_?swprintf)[[:space:]]*\\([[:space:]]*[^,;()\"]+,[^;\"]{0,60},[[:space:]]*$GAMESTRING"

# message array family: format at argument 1.
R7_PATTERN="$R7_PATTERN|\\bAddFormat(VL)?[[:space:]]*\\([[:space:]]*$GAMESTRING"

# MString::Format: a printf spelled as a method, and so invisible to
# every pattern that matches on a printf's name. Three live sites in
# GameUI.cpp handed it a String.inf entry - the bound is fine, since it
# forwards to vsnprintf, but the read is not - and task 5.4's fifth
# slice moved them to MString::FormatChecked. This alternative is what
# keeps a fourth from being written.
R7_PATTERN="$R7_PATTERN|\\.Format[[:space:]]*\\([[:space:]]*$GAMESTRING"

# appending at an offset: sprintf(buf + strlen(buf), <entry>, ...). The
# destination class above forbids parentheses, so it cannot match this -
# and forbidding them is what keeps a joined stream from running across
# two statements, so the shape gets its own alternative instead. Five
# live sites in VS_UI_ExtraDialog.cpp hid behind that for a whole slice,
# in a file the slice edited, and both reviewers found them. They were
# the least safe form left: an unbounded append into char[200].
R7_PATTERN="$R7_PATTERN|\\b(w?sprintf)[[:space:]]*\\([[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\\+[[:space:]]*strlen[[:space:]]*\\([^)]*\\)[[:space:]]*,[[:space:]]*$GAMESTRING"

R7=$(find Client VS_UI -name '*.cpp' -print0 2>/dev/null | xargs -0 cat 2>/dev/null \
	| sed -e 's://.*::' | tr '\n' ' ' \
	| grep -aoE "$R7_PATTERN" \
	| wc -l)
check "R7 (data-file format strings passed to printf)" "$R7" "$R7_BASELINE"

#----------------------------------------------------------------------
# R8 - printf-family calls whose format argument is not a literal.
#
# R7 above is the same finding measured by what it can see: a table
# lookup SPELLED AT THE FORMAT ARGUMENT. It reached 0 at task 5.4's
# fourth slice and was read, for about an hour, as C19 being closed. It
# was not. VS_UI_ExtraDialog.cpp copies twenty-one entries into
# m_sz_question_msg in InitString() and formats with them from there, and
# VS_UI_GameCommon.cpp does the same through three const char* arrays.
# Both reviewers of that slice found them by hand; neither ratchet could,
# because neither was looking at the format ARGUMENT - they were looking
# for a particular spelling of it.
#
# So this one asks the question the other cannot: at a printf-family
# call, is the format a string literal? If it is not, the format is a
# variable, and a variable in this client is a String.inf entry until
# shown otherwise. That is a weaker claim than R7's - it cannot tell a
# table entry from a legitimately forwarded format - which is exactly why
# it is the right shape for a floor. It sees the population; R7 sees the
# part of it that is easy to name.
#
# The 43 that remain are all one of four harmless shapes, and were read
# one by one:
#
#   - 28 vararg forwarders, where the format IS the function's own
#     parameter and passing it on is the whole point. Client/MinTr.h
#     (10), the two DebugInfo.cpp (6) and DebugInfo.h (1),
#     CMessageArray.cpp (2) plus its 5 AddFormatVL(first, vl) call
#     sites, MString.cpp, WireHost.cpp, PacketDiagnostics.cpp,
#     Client.cpp, basic/DebugLog.cpp and basic/Platform.h's wsprintf
#     shim. Their CALLERS are where a table entry can enter, and those
#     are what R7 and the format_arity audit cover.
#   - 6 inside basic/SafeFormat.cpp's own Emit, where pSpec is a
#     specification the checked formatter built and validated itself -
#     the one place in the tree where a computed format is the point.
#   - 3 literals behind a macro: two sprintf(szTemp, TEXT("...")) in
#     vs_ui_gamecommon2.cpp and one wsprintf(buf, _T("...")) in
#     Client/MinTr.h. This pattern cannot see through a macro.
#   - 6 declarations rather than calls: the AddFormat/AddFormatVL
#     prototypes and definitions, and Platform.h's wsprintf signature.
#
# A new call formatting with anything else raises this, which is the
# property the fourth slice needed and did not have.
#
# THIS COMMENT SAID 13, AND 13 WAS NOT THE POPULATION. The review round
# of the fifth slice found three holes in the pattern that produced it,
# and every one of them is the same mistake this whole task keeps
# making - a measurement mistaken for the thing measured:
#
#   - AddFormat/AddFormatVL, the ONE sink family this task built a
#     checked front end for, was in R7 and not here. Proven by
#     injection, not by reading: an AddFormat with a hoisted table entry
#     raised neither ratchet.
#   - bare printf() was in neither, and the enumeration command below -
#     offered as the authority for the family list - provably cannot
#     produce it, because \b[A-Za-z_][A-Za-z0-9_]*printf demands an
#     identifier character BEFORE printf. 86 bare printf( calls in the
#     tree, 0 with a variable format today. The command is fixed below;
#     the lesson is that an enumeration is only as good as its own
#     regex, and this one was quoted as evidence in three documents.
#   - the scope said "Client, VS_UI and basic" and the find said
#     "Client VS_UI", .cpp only. basic/SafeFormat.cpp - the formatter
#     this task exists to install - was never scanned, nor was any
#     header, and Client/MinTr.h alone holds 11 matches.
#
# What it still cannot see, written down rather than discovered later:
# a destination expression containing parentheses, because the
# destination class forbids them so that a joined stream cannot run
# across two statements. A cast - vswprintf((wchar_t*)dst, n, fmt, ap) -
# is enough to hide a call. The probe that established the families
# above was written that way by accident and counted three of its four
# shapes, which is how this paragraph came to exist. R7 has the same
# hole and answers it with a fourth alternative for the one form that
# actually occurred; no site here needs one yet. Also unseen: a printf
# reached through a macro, and one reached as a method under any name
# but Format.
#----------------------------------------------------------------------
# 43. First recorded as 13, which was this pattern's answer before its
# review round widened it to the families and files the paragraph above
# describes; nothing in the tree changed between the two numbers. Of the
# 30 the widening added, none is a data-file format - they are
# forwarders, SafeFormat's own Emit, macro literals and declarations -
# so the conclusion held, but it had not been measured over the
# population it claimed.
#
# Task 5.4's fifth slice converted the twenty-one sites in
# VS_UI_ExtraDialog.cpp (through AllocAskMessage, which allocates and
# formats in one place so the bound cannot drift from the destination)
# and the three in VS_UI_GameCommon.cpp: 37 -> 13 on the narrow pattern,
# which is the shrink this baseline records. 37 was the first
# measurement, taken during the fourth slice's review round to find out
# how much R7 was missing - the number that showed the closure claim was
# wrong.
# 40: removing the wsprintf shim removes its declaration and vararg forward;
# the wide MinTrace command now uses an explicit wide literal instead of _T().
# 34: SafeFormat emits validated arguments through literal CRT conversions.
# 0: retire inactive debug formatters and carry tagged arguments to every live sink.
R8_BASELINE=0

for d in Client VS_UI; do
	if [ ! -d "$d" ]; then
		echo "FAIL R8: directory $d is missing - fix the path list in this script"
		FAIL=1
	fi
done

# The character after the format's comma decides it: a quote opens a
# literal, anything else is an expression. L is excluded with the quote
# so that L"..." counts as a literal too; it costs the handful of
# identifiers beginning with a capital L, and no call site here has one.
# As in R7, the format sits at a different argument in each family, the
# tree is joined before matching, and // matches are stripped first.
#
# The family list is the whole ratchet, so it was taken from the tree
# rather than from memory: every identifier ending in printf in Client,
# VS_UI and basic, counted on a joined stream. That is how fprintf got
# in - 550 calls, none of them with a variable format today, and not one
# of them visible to the first draft of this pattern - and vswprintf
# with it. Re-run that enumeration when adding a family:
#
#   find Client VS_UI basic \( -name '*.cpp' -o -name '*.h' \) -print0 \
#     | xargs -0 cat \
#     | grep -aoE '(^|[^A-Za-z0-9_])[A-Za-z_]*printf[A-Za-z0-9_]*[[:space:]]*\('
#
# Three things about that command are load-bearing, and the first draft
# of this comment got two of them wrong. The leading alternation, not
# \b[A-Za-z_][A-Za-z0-9_]*printf, because that form REQUIRES a character
# before printf and so can never match bare printf( - the review round
# proved it with `printf 'printf("x");' | grep`. The parenthesised -name
# group, because `-name '*.cpp' -o -name '*.h'` without it applies the
# implicit -print to the second branch only. And the -a with a single
# stream: run per file, the same enumeration reports 453 sprintf rather
# than 510, because grep calls this tree's UTF-8 sources binary and
# stops inside them.
R8_PATTERN="(^|[^A-Za-z0-9_])(sprintf|wsprintf|vsprintf|fprintf)[[:space:]]*\\([[:space:]]*[^,;()\"]+,[[:space:]]*[^\"L[:space:]]"
R8_PATTERN="$R8_PATTERN|(^|[^A-Za-z0-9_])(_?snprintf|_?vsnprintf|_?swprintf|vswprintf)[[:space:]]*\\([[:space:]]*[^,;()\"]+,[^;\"]{0,60},[[:space:]]*[^\"L[:space:]]"

# Bare printf takes its format FIRST, so it needs its own alternative
# rather than a place in the list above - which is where it was put at
# the first attempt, and the injection probe then caught only three of
# its four shapes: printf(GetGameString(id), n) does not match a pattern
# that wants a destination before the format, because the destination
# class forbids the parentheses in the lookup.
R8_PATTERN="$R8_PATTERN|(^|[^A-Za-z0-9_])printf[[:space:]]*\\([[:space:]]*[^\"L[:space:])]"

# The message-array family, which R7 has carried since it was written
# and this did not until its review round. It is the one sink family
# this task built a checked front end for (CMessageArray::AddSafeFormat,
# task 5.4's third slice), so a new AddFormat with a hoisted table entry
# is exactly what both ratchets exist to stop - and it raised neither.
R8_PATTERN="$R8_PATTERN|\\bAddFormat(VL)?[[:space:]]*\\([[:space:]]*[^\"L[:space:])]"

# A printf does not have to be named like one. MString::Format is an
# ordinary varargs printf reached as a method, which is why no pattern
# here saw the three GameUI.cpp sites that passed it a String.inf entry
# until they were looked for by hand. Its checked sibling is
# FormatChecked, which this deliberately does not match.
R8_PATTERN="$R8_PATTERN|\\.Format[[:space:]]*\\([[:space:]]*[^\"L[:space:])]"

# basic and the headers are in scope, unlike R7's. The row for this in
# docs/RESTRUCTURING.md said "across Client, VS_UI and basic" while this
# line said `find Client VS_UI -name '*.cpp'`, so the checked formatter
# this task installs was itself never scanned, and neither was any
# header - Client/MinTr.h alone holds 11 matches. A formatter defined
# inline in a header is exactly the shape that would hide here.
R8=$(find Client VS_UI basic \( -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null \
	| xargs -0 cat 2>/dev/null \
	| sed -e 's://.*::' | tr '\n' ' ' \
	| grep -aoE "$R8_PATTERN" \
	| wc -l)
check "R8 (printf-family calls whose format is not a literal)" "$R8" "$R8_BASELINE"

#----------------------------------------------------------------------
# R9 / R10 - dynamic exception specifications.
#
# `throw()` and `throw(X, Y)` written on a declaration or a definition.
# The non-empty form was REMOVED in C++17 and the empty form deprecated;
# MSVC accepts both and IGNORES the type list, which is why a permissive
# MSVC C++20 build succeeds here where clang stops at the first one
# (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, finding 3).
# A ratchet is the only instrument there is, because the BUILD says
# nothing: the Debug log of this tree at /std:c++20 contains C4290 - the
# warning CLAUDE.md names for the packet tree - exactly zero times, so
# none of the 11,463 sites below is visible as a diagnostic. What the
# build does emit is 539 distinct C4297, "function assumed not to throw
# an exception but does", across 257 Client/Packet files: those are
# `throw()` functions whose bodies throw, undefined under MSVC's own
# reading of the specification, and they are why `throw()` -> `noexcept`
# is a judgement per function rather than a substitution.
#
# R9 holds the libraries that carry none; R10 counts what is left, so a
# conformance slice has to record its progress here.
#
# THE PATTERN, and what separates a specification from a statement.
# Both start with the keyword, so the discriminator is what the
# parentheses hold: a specification holds a comma-separated list of TYPE
# NAMES or nothing at all, a `throw` statement holds an expression. The
# class between the parens is therefore identifiers, `::` and commas and
# nothing else - no quote, no digit-leading token, no operator, no call.
#
#   throw ()             matches - the empty specification
#   throw ( X )          matches
#   throw ( X , Y )      matches
#   throw ("...")        does NOT match, and is a statement: six live
#                        ones in GCShopList.cpp, GCShopListMysterious.cpp
#                        and GCShopVersion.h, plus two commented out in
#                        the two SXml.cpp
#   throw Error ( ... )  does NOT match - the keyword is not followed by
#                        a paren, which is how every other throw
#                        statement in this tree is written
#
# The false positive it would accept is `throw (e);`, a statement
# rethrowing a named object. There is none: every single-identifier match
# in the set was enumerated, 97 of them, and all 97 name an exception
# class on a declarator - Error 58, NoSuchElementException 15,
# InvalidProtocolException 13, DuplicatedException 5, IOException 3,
# AssertionError 3.
#
# The false negatives, written down here rather than discovered later:
#
#   - a type list this class cannot spell - a template argument
#     (`throw(A<B>)`), a pointer or reference, or a comment between the
#     parens. None exists today, and a new one would be invisible.
#   - a specification whose /* */ block or // tail is unbalanced or
#     nested in a way the two strips misread. Block comments ARE
#     stripped (36 of the first measurement were dead comment text in
#     CGBloodDrain.h, GCAddBat.h, GCAddWolf.h and the skill OK files),
#     so deleting commented-out code does not read as progress.
#   - a specification reached through a macro.
#
# The files are JOINED before matching, as R7 and R8 are, because five
# specifications put the keyword and the rest of the list on different
# lines - SocketAPI.cpp's bind_ex, connect_ex, accept_ex, send_ex and
# recv_ex. Line-based this set measures 11,458 rather than 11,463, so
# removing those five would have looked like a partial no-op, which is
# the failure mode task 5.3 found in R4. The type-name-only class is what
# makes joining safe: a match cannot run across a statement boundary,
# because a `;`, a quote or an operator ends it.
#
# THE LIBRARY SET (R10) is every library a test binary can link, spelled
# the way R4 spells its own membership, plus the headers: basic,
# Client/SpriteLib, Client/TextSystem, Client/DXLib, Client/framelib and
# VS_UI as whole trees; the gamemodel membership file's .cpp and .h
# lines; the packetwire membership file's .cpp lines AND every .h under
# Client/Packet.
#
# Those headers are in the set for a reason worth stating. 9,252 of the
# 11,463 are in them, and an exception specification is part of the
# function type, so a declaration and its definition have to change
# together. A .cpp-only metric - which is all
# tests/arch/packetwire_files.txt can give, since it lists .cpp by design
# - would have counted exactly half of every edit and called it progress.
#
# Every .cpp under Client/Packet is in the set since 2026-09-09, when
# task 5.1's fifth slice took the last holdout
# (RequestClientPlayerManager.cpp) into packetwire; until then that one
# file compiled into the executable and sat outside this count. It
# carried 0, so the move did not change the number.
#
# NOT counted, and they are the rest of the workload: Client/PacketHandler
# (284), the remaining executable sources (49 - the two request-side
# packet factory managers, PacketFunction.cpp, RequestFileManager and
# Updater/UpdateManager.h) and tests/ (9, in test doubles that implement
# wire interfaces and must match the specifications on their bases).
# 11,463 + 284 + 34 + 9 = 11,790, which is every .h and .cpp in the
# repository outside comments - 8,513 empty and 3,277 non-empty.
# The nine in tests/ went with the packet-root slice, 2026-09-07.
#----------------------------------------------------------------------
# R9 = 0, and it is not a removal. The first conformance slice
# (2026-09-06) went looking for basic/'s specifications and found none:
# basic/ has never carried one, and neither has Client/SpriteLib,
# Client/TextSystem, Client/DXLib, Client/framelib, VS_UI or any
# gamemodel member. The whole conformance workload is the packet tree, so
# the slice shipped the instrument instead of an edit. Read this zero the
# way R3's comment says to read its own: it is a statement about the
# pattern above, not about the language mode.
R9_BASELINE=0

# R10 = 11,463, all of it under Client/Packet: 2,211 in the membership
# file's .cpp and 9,252 in the headers, over the set's 1,473 files.
# 8,492 are the empty `throw()` and 2,971 are type lists. Later
# slices ratchet this down a subsystem at a time; the mapping each of
# them applies is in the assessment's finding 3, and the one place where
# the choice is NOT mechanical is already written down at
# Client/Packet/WireHost.h:155 - the request-file path throws through
# functions declared throw(ProtocolException, Error) by design, and
# giving them noexcept would turn a designed peer teardown into
# std::terminate.
#
# R10 = 9,664 (2026-09-07): the 162 files directly under Client/Packet
# are at 0 - 143 of them carried a specification: the wire core, the
# packet framework and players, and the info classes, 1,799 sites - and
# so is tests/. What is left is the packet directories:
# Cpackets, Gpackets, Lpackets, Rpackets, Upackets and Types. Ten
# destructors that carried a type list are spelled noexcept(false),
# which this pattern does not count; everything else is deleted or
# noexcept.
#
# R10 = 9,055 (2026-09-07): Lpackets, Upackets and Rpackets are at 0 as
# well, 609 sites, by script this time - the packet classes are regular
# enough for one, and it promotes only an inline one-liner returning a
# constant or a scalar member. Cpackets (3,172) and Gpackets (5,883) are
# what is left; Types carries none.
#
# R10 = 5,883 (2026-09-07): Cpackets is at 0, 3,172 sites by the same
# script. One packet derives from another there
# (CGUseMessageItemFromInventory from CGUseItemFromInventory), and the
# build refused the base's promoted size functions under the derived
# class's unpromoted overrides, so the base is unspecified too. Gpackets
# is all that is left.
#
# R10 = 0 (2026-09-07): Gpackets is at 0, 5,883 sites by the same
# script, with GCChangeInventoryItemNum::getPacketSize left unspecified
# under the two packets that derive from it. The whole library set is
# clean, and this ratchet now holds it there the way R9 holds basic/.
# What it never covered - Client/PacketHandler (284) and the remaining
# executable sources (34) - is the next slice, with a ratchet of its own.
R10_BASELINE=0

# Identifiers, `::` and commas between the parens, and nothing else. The
# leading alternation rather than \b for the reason R8's comment gives:
# it has to be able to match at the very start of the joined stream.
EXCSPEC='(^|[^A-Za-z0-9_])throw[[:space:]]*\([[:space:]]*([A-Za-z_][A-Za-z0-9_:]*[[:space:]]*(,[[:space:]]*[A-Za-z_][A-Za-z0-9_:]*[[:space:]]*)*)?\)'

# Reads repository-relative paths on stdin and counts the specifications
# in them. -a and one joined stream for the reason R7 and R8 give: grep
# calls this tree's CP949 and UTF-8 sources binary and stops inside them
# when it classifies per file.
count_exception_specs () {
	sort -u | while read -r f; do [ -f "$f" ] && echo "$f"; done \
		| tr '\n' '\0' | xargs -0 cat 2>/dev/null \
		| perl -0pe 's{/\*.*?\*/}{ }gs' \
		| sed -e 's://.*::' | tr '\n' ' ' \
		| grep -aoE "$EXCSPEC" | wc -l
}

basic_members () {
	find basic \( -name '*.h' -o -name '*.cpp' \) 2>/dev/null
}

libset_members () {
	basic_members
	find Client/SpriteLib Client/TextSystem Client/DXLib Client/framelib VS_UI \
		\( -name '*.h' -o -name '*.cpp' \) 2>/dev/null
	sed -e 's/#.*//' tests/arch/gamemodel_files.txt \
		| grep -oE 'Client/[A-Za-z0-9_/]+\.(cpp|h)'
	sed -e 's/#.*//' tests/arch/packetwire_files.txt \
		| grep -oE 'Client/Packet/[A-Za-z0-9_/]+\.cpp'
	find Client/Packet -name '*.h' 2>/dev/null
}

# Both baselines are fail-open if their inputs silently are not there: a
# renamed directory makes find print nothing, and R9 would then count 0
# of nothing and PASS. So the inputs are asserted, and R9's file list is
# asserted non-empty on top of that.
for d in basic Client/SpriteLib Client/TextSystem Client/DXLib Client/framelib \
	VS_UI Client/Packet; do
	if [ ! -d "$d" ]; then
		echo "FAIL R9/R10: directory $d is missing - fix the path list in this script"
		FAIL=1
	fi
done
for f in tests/arch/gamemodel_files.txt tests/arch/packetwire_files.txt; do
	if [ ! -f "$f" ]; then
		echo "FAIL R9/R10: membership file $f is missing - fix the path list in this script"
		FAIL=1
	fi
done

if [ "$(basic_members | wc -l)" -eq 0 ]; then
	echo "FAIL R9: basic/ enumerates no .h or .cpp - a zero here would measure nothing"
	FAIL=1
else
	R9=$(basic_members | count_exception_specs)
	check "R9 (dynamic exception specifications in basic/)" "$R9" "$R9_BASELINE"
fi

R10=$(libset_members | count_exception_specs)
check "R10 (dynamic exception specifications in the library set)" "$R10" "$R10_BASELINE"

#----------------------------------------------------------------------
# R12 - dynamic exception specifications anywhere in the tree.
#
# R9 and R10 cover the libraries; this one covers every .h, .cpp and
# .inl under Client, VS_UI, basic, tools, third_party and tests, so the
# executable side is held too. Same pattern, same strips, same blind
# spots as R10's comment lists. The last 319 outside the library set -
# 284 in Client/PacketHandler, one per handler's execute definition,
# 32 in the two request-side packet factory managers, and three in
# RequestFileManager and Updater/UpdateManager.h - went on 2026-09-07,
# all of them type lists deleted or throw() deleted, none promoted.
# R12 = 0 as of that day: finding 3 of the assessment is closed on the
# source side, and a new specification anywhere fails here.
#----------------------------------------------------------------------
R12_BASELINE=0

tree_members () {
	find Client VS_UI basic tools third_party tests \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' \) 2>/dev/null
}

if [ "$(tree_members | wc -l)" -eq 0 ]; then
	echo "FAIL R12: no C++ sources enumerated - a zero here would measure nothing"
	FAIL=1
else
	R12=$(tree_members | count_exception_specs)
	check "R12 (dynamic exception specifications in the whole tree)" "$R12" "$R12_BASELINE"
fi

#----------------------------------------------------------------------
# R11 - the `register` storage class in C++ sources.
#
# Removed in C++17; MSVC accepts it as an extension, clang 19 rejects it
# (assessment finding 4). Counted by tests/tools/count_register.pl,
# which strips comments and strings with a tokenizer, per file, instead
# of the joined-stream pipeline R9/R10 use - that pipeline reads the
# blitters' `//*pDest = ...` line comments as block-comment openers and
# saw 538 of the 627 declarations over the 23 files on master (a figure
# that moves with file order), and it does not strip string literals,
# so the NPC script lines that say "register as a couple" would count
# (the script's header has the detail).
#
# The set is every C++ source the tree compiles or may compile: the
# libraries, the executable, VS_UI, the tools (the viewers are
# unconditional targets, the engine sprite tool is behind BUILD_ENGINE),
# the vendored third_party sources and tests/, so nothing built here can
# reintroduce it. .c files are outside: they are compiled as C, where
# the keyword is still valid, and the 21 in deflate.c, inftrees.c and
# trees.c stay (crc32.c has the word only in its comments).
#
# R11 = 0 as of 2026-09-07, from 627 in 23 files (SpriteLib's blitters,
# MTopView.cpp, and a few UI and table loops).
R11_BASELINE=0

register_members () {
	find Client VS_UI basic tools third_party tests \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' \) 2>/dev/null
}

if [ ! -f tests/tools/count_register.pl ]; then
	echo "FAIL R11: tests/tools/count_register.pl is missing"
	FAIL=1
elif [ "$(register_members | wc -l)" -eq 0 ]; then
	echo "FAIL R11: no C++ sources enumerated - a zero here would measure nothing"
	FAIL=1
else
	R11=$(register_members | sort -u | perl tests/tools/count_register.pl)
	check "R11 (register storage class in C++ sources)" "$R11" "$R11_BASELINE"
fi

#----------------------------------------------------------------------
# R13 - platform macros spelled anywhere but basic/Platform.h.
#
# basic/Platform.h reads the platform off the compiler builtins and
# defines PLATFORM_WINDOWS, PLATFORM_LINUX, PLATFORM_MACOS and
# PLATFORM_POSIX. Before the port's build-contract slice (the assessment
# of 2026-09-07, area A) four spellings of "Linux" coexisted and the
# build defined none of them - __LINUX__ (63 sites, mostly the socket
# and file APIs, which therefore compiled neither their Windows nor
# their POSIX branch off Windows), _LINUX (1), the builtin __linux__
# and PLATFORM_LINUX - while every non-Windows target was handed
# PLATFORM_MACOS by CMake, in six places, so a Linux build was told it
# was macOS. The sum of three counts, each of which must be 0:
#
#   __LINUX__ or _LINUX as a token in any .h/.cpp/.inl under Client,
#   VS_UI, basic, tools, third_party and tests (the POSIX branches
#   test PLATFORM_POSIX now; a Linux-only one tests PLATFORM_LINUX);
#
#   PLATFORM_MACOS as a token in the same set minus basic/, where the
#   shim's genuinely Darwin-only code (mach-o/dyld, _NSGetExecutablePath)
#   lives - everywhere else the macro used to mean "not Windows", and
#   that is PLATFORM_POSIX. A real Darwin branch outside basic/ is
#   allowed and counted: there is one, the macOS font list in
#   Client/TextSystem/TextBackendSDL.cpp, so this count is 1 and a
#   second one is a deliberate baseline change;
#
#   PLATFORM_MACOS in any CMakeLists.txt or .cmake outside build
#   trees, comment tails stripped - the build defines no platform
#   macro off Windows, and one here would disagree with the compiler.
#   Matched without word boundaries since 2026-09-13: the R15 review
#   found that \b misses add_definitions(-DPLATFORM_MACOS), the spelling
#   basic/ and Client/DXLib use, because -D puts a word character in
#   front of the name. Over-counting is the safe direction at 0.
#
# Tokens are counted by tests/tools/count_identifier.pl, which strips
# comments and strings per file for the reason count_register.pl gives.
# Blind to a spelling assembled by the preprocessor and to the CMake
# variable that carries the Windows wire macros (DARKEDEN_PLATFORM_-
# DEFINITIONS is the intended single point), and to the compiler
# builtins themselves (_WIN32, __APPLE__, __linux__), which a file that
# does not include Platform.h still has to use - basic/DataPath.cpp
# includes it for that reason. R13 = 1 as of 2026-09-08: the font list.
#----------------------------------------------------------------------
# Font fallback policy now lives in basic/PlatformSDL.cpp.
R13_BASELINE=0

r13_cxx_members () {
	find Client VS_UI basic tools third_party tests \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' \) 2>/dev/null
}
r13_cxx_members_outside_basic () {
	find Client VS_UI tools third_party tests \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' \) 2>/dev/null
}
r13_cmake_members () {
	find . -path ./build -prune -o \( -name 'CMakeLists.txt' -o -name '*.cmake' \) -print 2>/dev/null | grep -v '^\./build/'
}

if [ ! -f tests/tools/count_identifier.pl ]; then
	echo "FAIL R13: tests/tools/count_identifier.pl is missing"
	FAIL=1
elif [ "$(r13_cxx_members | wc -l)" -eq 0 ] || [ "$(r13_cmake_members | wc -l)" -eq 0 ]; then
	echo "FAIL R13: no sources enumerated - a zero here would measure nothing"
	FAIL=1
else
	R13_LINUX=$(r13_cxx_members | sort -u | perl tests/tools/count_identifier.pl '__LINUX__|_LINUX')
	R13_MACOS=$(r13_cxx_members_outside_basic | sort -u | perl tests/tools/count_identifier.pl 'PLATFORM_MACOS')
	R13_CMAKE=$(r13_cmake_members | sort -u | tr '\n' '\0' | xargs -0 cat 2>/dev/null \
		| sed -e 's/#.*//' | grep -aoE 'PLATFORM_MACOS' | wc -l)
	R13=$((R13_LINUX + R13_MACOS + R13_CMAKE))
	check "R13 (platform macros spelled outside basic/Platform.h: __LINUX__/_LINUX $R13_LINUX, PLATFORM_MACOS outside basic/ $R13_MACOS, PLATFORM_MACOS in CMake $R13_CMAKE)" "$R13" "$R13_BASELINE"
fi

#----------------------------------------------------------------------
# R14 - live GetTickCount()/timeGetTime() calls under Client and VS_UI.
#
# The 32-bit millisecond tick the client is written against wraps every
# 49.7 days, and every "previous + delay <= now" over it fires early or
# stalls across the wrap (basic/MonotonicClock.h says how). The clocks
# work of the assessment (priority 5) moves the readers onto
# MonotonicClock one subsystem at a time, and this holds what is left:
# calls of the two functions in code, counted by
# tests/tools/count_tick_reads.pl with comments and string literals
# removed by a character-level scanner, because a regex strip of block
# comments reads "//*pDest" as an opener and swallowed live code - the
# clocks slices' first counts were low by that. basic/ is outside the
# count: its three hits are Platform.h's definitions. Definitions and
# #if-disabled code count like calls: the 186 includes the non-Windows
# GetTickCount shim in VS_UI_widget.h.
#
# R14 = 186 as of 2026-09-10, from 214 before the widget timers outside
# GameCommon moved; 100 of the 186 were the two VS_UI_GameCommon sources.
# 138 later that day: the interval and window gates in those two sources
# moved (45 calls), the last GetTickCount in VS_UI with them - two srand
# seeds reseeded and the VS_UI_widget.h stub definition deleted make 48;
# what is left there is 56 timeGetTime() sites in three files (the
# header's SetTimer is the third), four of them srand seeds, the rest of
# the deadline and elapsed-time shapes; the tree's other 82 are Client's.
# 113 later still: the deadlines set and read within VS_UI moved to
# TimePoint - the quest status countdowns, the blood bible timer, the
# image notice, the resurrect delays, the flag-war end (with its
# handler in Client), the timed missions, the gamble spin, and four
# srand seeds (25 calls); VS_UI holds 33, the minigames' clocks and
# the three deadlines the executable sets, and Client 80.
# 93 on 2026-09-16: the three minigames' clocks moved (20 calls) - the
# minesweeper's start point and the elapsed time it keeps once the game
# ends, the arrow tile's per-character start, end and move points, a
# trap-delay point nothing sets, and its monster-move gate, the crazy
# mine's start point; the elapsed millisecond counts still reach the
# score message as before.
# VS_UI holds 13, the three deadlines the executable sets through
# shared structs; Client 80.
# 72 on 2026-09-16: those three deadlines moved with their setters (21
# calls) - the quest status's quest_time and the war list's left_time to
# SecondPoint, the whole-second point the "timeGetTime() / 1000" they
# were counted in floors to, the effect status's delayFrame to TimePoint;
# VS_UI holds 0 live calls (a dozen mentions inside comments), Client 72.
# 53 on 2026-09-16: the event register's start stamp (MEvent, set by
# AddEvent and reset by the two ending cinematics in MTopView as each
# starts, read by the show-time blink, the expiry, the countdown
# captions, the scrolls, the fades and two dead script gates through
# one ElapsedMillis()) and the HP-modify list's stamp moved to
# TimePoint (19 calls) - width and clock only: every read was a
# wrap-safe unsigned subtraction. MTopView holds 6, the quest event's
# parameter fields read as ticks; MPlayer 9 (two in its header, the
# failing "previous + delay < now" shape), MFakeCreature 7.
# 35 on 2026-09-16: the player's four stamps (the repeat and lock
# gates and the pet's dissection delay - all three the failing
# "stamp + delay < now" shape, so this one does remove a wrap failure -
# and the trace limit, a plain subtraction), the fake creature's next
# move deadline (a sum-shaped deadline, the same failing shape) and the
# two executable setters in PacketFunction that fed them moved to
# TimePoint (18 calls). MTopView holds 6, the rest ones to fours.
# 10 on 2026-09-17: the scatter (25 calls) - the wait screens' double-click
# gate and the game screen's write-only click stamp, the title fade's
# start, the CGVerifyTime deadline (a sum-shaped deadline read against
# the frame clock, the failing shape), the update handler's loading
# stopwatch, the profiler's start stamp, the name window's send stamp
# (a tick parked in TempInformation's generic int slot, now its own
# TimePoint slot), the reconnect handler's two OUTPUT_DEBUG stopwatches
# and the socket stream's byte-rate window (an IntervalTimer) moved to
# the monotonic clock; MTopView's six were a quest caption no live
# event reaches, deleted with the block. What is left is the frame
# clock - g_CurrentTime's three live writers, g_StartTime and
# CWinUpdate's m_CurrentTime - and four that are not clocks: the two
# log-file names and the hack check that compares timeGetTime() against
# GetTickCount() on purpose.
# 5 on 2026-09-17: the frame clock's twelfth slice put a TimePoint,
# g_FrameNow, beside g_CurrentTime and stamps both in one
# StampFrameClock() in Client.cpp, so the three writers are one call
# (the two in CWaitPacketUpdate and GCUpdateInfoHandler call it), and
# CWinUpdate's clock members - a static tick, a last-update stamp and a
# delay that nothing outside the class read - are deleted with their
# two calls; g_StartTime, the FPS window, is a TimePoint. What is left:
# the one writer, the two log-file names and the hack check. R16 counts
# the readers of the DWORD from here.
# 4 on 2026-09-17: the frame clock's DWORD is gone with its writer, so
# what is left is the floor - the two log-file names in Client.cpp,
# which want a number that differs per run and are not clocks, and
# GameMain's hack check, which compares timeGetTime() against
# GetTickCount() on purpose and would measure nothing on one clock.
#----------------------------------------------------------------------
# 2 on 2026-09-17: CheckTime returned before the purported hack check.
# Remove that unreachable body and its two calls; only log-file naming remains.
# Log filenames use the existing 64-bit monotonic clock.
R14_BASELINE=0

if [ ! -f tests/tools/count_tick_reads.pl ]; then
	echo "FAIL R14: tests/tools/count_tick_reads.pl is missing"
	FAIL=1
elif [ "$(find Client VS_UI \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null | wc -l)" -eq 0 ]; then
	echo "FAIL R14: no C++ sources enumerated - a zero here would measure nothing"
	FAIL=1
else
	R14=$(perl tests/tools/count_tick_reads.pl Client VS_UI | tail -1 | awk '{print $1}')
	check "R14 (live GetTickCount/timeGetTime calls under Client and VS_UI)" "$R14" "$R14_BASELINE"
fi

#----------------------------------------------------------------------
# R15 - the build-wide side macros spelled in the sources.
#
# The once-shared client/server sources switched on five macros, and
# four more spellings no translation unit ever had defined guarded code
# the same way (the eleventh slice, below). The five:
# __GAME_CLIENT__, which every translation unit that read it had
# defined (CMake passed =1 to the executable, packetwire, gamemodel and
# the listed test sources; every other reader got it from
# Client_PCH.h), and __GAME_SERVER__, __LOGIN_SERVER__,
# __SHARED_SERVER__ and __UPDATE_SERVER__, which nothing ever defined.
# A conditional on any of the five therefore had one value in every
# translation unit. Task 5.2's ninth slice (2026-09-13) evaluated the
# 190 under Client/Packet out - server halves that included headers
# this repo does not have, client halves whose guard was always true -
# and the tenth slice, the same day, the 363 in the 256 files of the
# rest of the tree (always-true #ifdef __GAME_CLIENT__ wrappers in 237
# of the 280 handler files and 19 files directly under Client/, ten
# always-false server guards in five handlers; two of the 363 reduced
# to #ifdef OUTPUT_DEBUG rather than deleted) and then the definition
# itself, from CMakeLists.txt, tests/CMakeLists.txt and Client_PCH.h in
# the same commit. Three counts, all 0, checked separately because
# each is where a return would land:
#
#   the nine macros as tokens in any .h/.cpp/.inl under Client/Packet -
#   a server half behind one of the five cannot come back through a
#   copy from the server repo without this noticing (one behind another
#   spelling, outside the nine this counts, is the include
#   checker's both-branch walk to catch, not this count's);
#
#   the same tokens in the rest of Client, VS_UI, basic, tools,
#   third_party and tests - a #ifdef __GAME_CLIENT__ written from habit
#   is dead code now, since nothing defines the macro, and this is what
#   says so;
#
#   the nine names anywhere in a CMakeLists.txt or .cmake file outside
#   build trees, comment tails stripped - a definition put back there
#   would make every such #ifdef live again. Matched without word
#   boundaries on purpose: -D__GAME_CLIENT__ has a word character on
#   each side of the name, and basic/ and Client/DXLib already spell
#   their definitions add_definitions(-D...). Over-counting is the
#   safe direction for a baseline of 0. Not covered: CMakePresets.json,
#   the workflows, the Makefile and the tracked SpriteLib .vcxproj,
#   none of which passes a macro today.
#
# Counted by tests/tools/count_identifier.pl, so a mention in a comment
# or a string does not count (the //#ifdef __GAME_CLIENT__ inside the
# commented-out function at Client/MItemUse.cpp:1749 is one): "no live
# token", not "no mention". Blind to a spelling assembled by the
# preprocessor. All three counts pin their denominators (a baseline of
# 0 cannot notice a shrunken scan, the way R13's font-list 1 does): the
# two token counts by a file floor, the CMake count by a smaller one.
# The include checker (tests/arch/check_includes.pl) still evaluates
# the same nine macros, all as undefined, while walking library
# includes.
#
# Eleventh slice (2026-09-16): __EXPO_CLIENT__ (ten sites: the expo
# build's addon enum; its fullscreen sector geometry through the one
# macro it defined, __FULLSCREEN_MODE__, defined only inside that dead
# branch; an invisible-walk clause at four sites, in the two path
# finders and MPlayer's move check; a skill-to-tile send in MPlayer;
# the dark-light, weather and lightning handlers, which returned
# early), __UPDATE_CLIENT__ (one site, two includes in
# UCRequestLoginModeHandler.cpp) and __GUILD_MANAGER_TOOL__ (already at
# 0: its three sites went with the tenth slice) joined the pattern, so
# both source-side counts and the CMake count read the same nine names.
# Nothing defines any of the four, in CMake or in a source, and the
# CMake count holds that too.
#----------------------------------------------------------------------
R15_PACKET_BASELINE=0
R15_REST_BASELINE=0
R15_CMAKE_BASELINE=0
R15_PACKET_FILES_FLOOR=1000
R15_REST_FILES_FLOOR=1000
R15_CMAKE_FILES_FLOOR=5
R15_PATTERN='__GAME_CLIENT__|__GAME_SERVER__|__LOGIN_SERVER__|__SHARED_SERVER__|__UPDATE_SERVER__|__UPDATE_CLIENT__|__EXPO_CLIENT__|__FULLSCREEN_MODE__|__GUILD_MANAGER_TOOL__'

r15_packet_members () {
	find Client/Packet \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' \) 2>/dev/null
}
r15_rest_members () {
	find Client VS_UI basic tools third_party tests \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' \) 2>/dev/null | grep -v '^Client/Packet/'
}

if [ ! -f tests/tools/count_identifier.pl ]; then
	echo "FAIL R15: tests/tools/count_identifier.pl is missing"
	FAIL=1
elif [ "$(r15_packet_members | wc -l)" -lt "$R15_PACKET_FILES_FLOOR" ]; then
	echo "FAIL R15: only $(r15_packet_members | wc -l) files enumerated under Client/Packet (floor $R15_PACKET_FILES_FLOOR) - a zero over a shrunken scan would measure nothing"
	FAIL=1
elif [ "$(r15_rest_members | wc -l)" -lt "$R15_REST_FILES_FLOOR" ]; then
	echo "FAIL R15: only $(r15_rest_members | wc -l) files enumerated outside Client/Packet (floor $R15_REST_FILES_FLOOR) - a zero over a shrunken scan would measure nothing"
	FAIL=1
elif [ "$(r13_cmake_members | wc -l)" -lt "$R15_CMAKE_FILES_FLOOR" ]; then
	echo "FAIL R15: only $(r13_cmake_members | wc -l) CMake files enumerated (floor $R15_CMAKE_FILES_FLOOR) - a zero over a shrunken scan would measure nothing"
	FAIL=1
else
	R15_PACKET=$(r15_packet_members | sort -u | perl tests/tools/count_identifier.pl "$R15_PATTERN")
	R15_REST=$(r15_rest_members | sort -u | perl tests/tools/count_identifier.pl "$R15_PATTERN")
	R15_CMAKE=$(r13_cmake_members | sort -u | tr '\n' '\0' | xargs -0 cat 2>/dev/null \
		| sed -e 's/#.*//' | grep -aoE "($R15_PATTERN)" | wc -l)
	check "R15 (side macros spelled under Client/Packet)" "$R15_PACKET" "$R15_PACKET_BASELINE"
	check "R15 (side macros spelled outside Client/Packet)" "$R15_REST" "$R15_REST_BASELINE"
	check "R15 (side macros defined in a CMake file)" "$R15_CMAKE" "$R15_CMAKE_BASELINE"
fi

#----------------------------------------------------------------------
# R16 - live spellings of g_CurrentTime, the frame clock's DWORD, under
# Client and VS_UI.
#
# The frame clock is stamped once per frame and read at every gate the
# game runs on. It was one DWORD, g_CurrentTime, wrapping every 49.7
# days like the tick it copies, so the "deadline = g_CurrentTime +
# delay" shape at some forty stamps in a dozen classes had the sum-wrap
# failure R14 describes. The twelfth clocks slice (2026-09-17) put a
# TimePoint beside it, g_FrameNow, stamped by the same StampFrameClock()
# call, and moves the readers over one group at a time; the DWORD goes
# when nothing reads it, and its one remaining tick call with it. This
# counts what still reads the DWORD - the definition, its extern
# declarations and the one writer included - by
# tests/tools/count_identifier.pl, so a mention in a comment or a
# string does not count.
#
# 57 on 2026-09-17, from 144 before the slice, which moved the static-local gates
# in CGameUpdate (the fixed-step accumulator with its catch-up loop and
# draw interpolation, the sound-per-second window, the anti-cheat
# elapsed clamp, the help scroll, the resurrect-dialog frame step), in
# MTopView (the four message scrolls, the click frame), in GameMain
# (the request-manager tick, the keep-alive, the zone random sound
# deadline shared with MZone), in CWaitUIUpdate, in Client.cpp (the log
# flush and the FPS window) and in MPlayer (the party status send and the
# conversion countdown's blink gate); the
# wait-for-packet deadline; and the small stamp classes - SoundNode,
# ShowTimeChecker, MHelpDisplayer, MParty, MJusticeAttackManager,
# MZoneSoundManager. What is left is the creatures' and the player's
# member stamps (chat fade, recovery, regeneration, bleeding, action and
# death delays, the conversion countdown), two of UserInformation's three
# deadlines (a gamemodel member; GlobalSayTime's reads are commented
# out), MGameTime's start and current time,
# the item host's clock pointer and the wire host's clock function (two
# library seams), the debug prints, the one write in StampFrameClock()
# and the externs.
# 18 on 2026-09-17: the creatures' and the player's member stamps moved
# (MCreature's chat fade, HP and MP recovery, regeneration and its
# bonus, bleeding; MPlayer's action delay, death delay and conversion
# countdown; MFakeCreature's inherited read), with the four extern
# declarations those files carried. What is left: UserInformation's
# two live deadlines and their sites in CGameUpdate, UIMessageManager
# and GameUI, MGameTime's start and current time with
# GCUpdateInfoHandler's and CGameUpdate's calls, the two library seams
# in GameInit, the definition, the one write and the log flush's debug
# print in Client.cpp, and the externs in Client.h, MGameTime.cpp and
# GCUpdateInfoHandler.cpp.
# 6 on 2026-09-17: UserInformation's two live deadlines moved (the
# scheduled logout with its countdown, the item-drop enable point;
# GlobalSayTime, whose three uses are commented out, deleted) behind five
# small members the executable's sites call, tested in
# tests/unit/test_user_information_deadlines.cpp, and MGameTime's start
# and current time take the point. What is left: the two library seams
# in GameInit (MItemHost::pCurrentTime, WireHost::CurrentTime), the
# definition, the one write and the log flush's debug print in
# Client.cpp, and the extern in Client.h.
# 0 on 2026-09-17: the two library seams carry a TimePoint - the item
# host's clock pointer (the skill use delays and the trade accept
# delay behind it) and the wire host's clock function (the request
# player's timeout behind it), with their tests - and g_CurrentTime is
# deleted: its definition, its extern, its write and its tick call.
# The count stays at zero as the guard that nothing brings the name
# back; StampFrameClock() stamps g_FrameNow alone.
#----------------------------------------------------------------------
R16_BASELINE=0
R16_FILES_FLOOR=1000

r16_members () {
	find Client VS_UI \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null
}

if [ ! -f tests/tools/count_identifier.pl ]; then
	echo "FAIL R16: tests/tools/count_identifier.pl is missing"
	FAIL=1
elif [ "$(r16_members | wc -l)" -lt "$R16_FILES_FLOOR" ]; then
	echo "FAIL R16: only $(r16_members | wc -l) files enumerated under Client and VS_UI (floor $R16_FILES_FLOOR) - a count over a shrunken scan would measure nothing"
	FAIL=1
else
	R16=$(r16_members | sort -u | perl tests/tools/count_identifier.pl 'g_CurrentTime')
	check "R16 (live reads of the frame clock's DWORD, g_CurrentTime)" "$R16" "$R16_BASELINE"
fi

#----------------------------------------------------------------------
# R17 - unbounded format and copy lines outside the packet tree.
#
# R3 holds Client/Packet and Client/PacketHandler at zero. This is R3's
# grep with wsprintf added and headers scanned beside sources -
# \b(sprintf|wsprintf|strcpy|strcat)\s*\( with the // comment tail of
# every line stripped first - over the rest of the tree a test binary can
# reach or the executable is built from: basic, VS_UI and Client
# outside the two packet directories. It is the measure the bounded
# formatting work of the assessment (priority 7) retreats along: a
# site leaves this count when its destination is a real array and the
# call is snprintf(dst, sizeof(dst), ...) or an exact-length memcpy,
# checked by hand, never by sizeof() on a pointer. Read with -a,
# because VS_UI_GameCommon.cpp carries NUL bytes and a plain grep
# stops at the first one (CLAUDE.md, Traps).
#
# What it cannot see: vsprintf, lstrcpy/lstrcat, strncpy, or a copy
# spelled any other way; snprintf never matched. The old MinTr/DebugInfo
# vararg forwards were retired or made typed and bounded when R8 reached0.
#
# 1,086 on 2026-09-17, from 1,102 before the first slice, which took
# basic to 0 (C_DIRECTORY, an unused class whose constructor called a
# POSIX-only function and could not have linked on Windows, deleted
# with its six lines; PlatformSDL's three sites, four lines, bounded by
# the size each already checked; SafeFormat's two spec writes bounded by the
# spec buffer's size, now a parameter) and SpriteLib to 0 (two captions
# bounded; CTypePackVector.h, a header nothing includes, deleted). What is left is
# VS_UI (783) and the rest of Client (303), most of them wsprintf and
# sprintf into local char arrays.
# 1,016 on 2026-09-17: the second slice took ten Client utility files
# to zero (70 lines). GetWinVersion takes the caller's buffer size and
# writes with it (its one caller, the crash report, passes sizeof);
# CMd5, which nothing constructs, is deleted with its two files (its
# 50-byte error text overflowed for a path over 21 bytes, on a path
# nothing reached); the crash report's four stamps, the profile
# manager's five file-name joins, the utility functions' four copies,
# the guild-mark manager's four index names and the wait screen's one
# live caption are snprintf(sizeof); MMusic's error text becomes the char array it was
# always used as (it was declared as eighty LPSTRs and cast); and the
# sixteen lines inside block-commented debug code in CPartManager.h,
# MCreatureWear.cpp and CWaitUIUpdate.cpp - counted, since this grep
# strips only // tails, as R3's does - are deleted with their blocks.
# 970 on 2026-09-17: the third slice took Client.cpp, GameMain.cpp,
# MZone.cpp and MCreature.cpp to zero (46 lines). Thirty-one are
# converted: snprintf(sizeof) into the local array read at each site
# (the Futec command-line parser's 32-byte argument slots, the
# 128-byte log-file name built from a _MAX_PATH g_CWD and the 80-byte
# server-group name were live overflows), the three sprintf calls that
# read their own destination (the restart path, the two image-object
# traces) are appends behind strlen, the creature name is a memcpy of
# the length it was just allocated with, and the chat rows are bounded
# by their configured width. Fifteen were dead: get_rand_str, which
# only commented-out lines called, and thirteen lines of block-commented
# debug dumps in MZone.cpp and MCreature.cpp, deleted with their blocks.
#----------------------------------------------------------------------
# 969 on 2026-09-17: delete the unused WinINet downloader and its one strcpy.
# 966 on 2026-09-18: the message ring uses owned copies and bounded memmove.
# 963 later that day: three dead formatting lines left with GL drawing blocks.
# 961: three file-dialog copies consolidate into one label helper.
# 958: the label helper and both file-dialog title copies use owned text.
# 952: owned file-dialog paths replace six raw directory copies/appends.
# 950: MString assignment copies into exact-size replacement allocations.
# 944: owned queued chat text removes six raw string-copy lines.
# 587: bound 355 narrow wsprintf calls, bound the wide trace command, and
# remove the unused CMP3 string formatter. R18 prevents raw wsprintf returning.
# 586: CToken copies an owned replacement by its measured length.
# 584: team introduction and billing dialog wrap owned rows without strcpy.
# 570: descriptors own their rows and remove the unused replacement buffers.
# 569: rich-help layout removes the unused substitution loop's dead strcpy.
# 568: the mail-template notice date uses checked formatting.
# 518: option-name formatting/copies are bounded; two obsolete option blocks removed.
# 512: retire MinTrace formatters and bound two macro-wrapped UI literals.
# 0: remaining arrays and pointer capacities are bounded, semantic names/paths
# are owned, and obsolete comment blocks plus unused APIs are removed.
R17_BASELINE=0
R17_FILES_FLOOR=500

r17_members () {
	find basic VS_UI Client \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null | grep -v '^Client/Packet/\|^Client/PacketHandler/'
}

if [ "$(r17_members | wc -l)" -lt "$R17_FILES_FLOOR" ]; then
	echo "FAIL R17: only $(r17_members | wc -l) files enumerated (floor $R17_FILES_FLOOR) - a count over a shrunken scan would measure nothing"
	FAIL=1
else
	R17=$(r17_members | sort -u | tr '\n' '\0' | xargs -0 grep -haE '\b(sprintf|wsprintf|strcpy|strcat)\s*\(' \
		| sed -e 's://.*::' | grep -aE '\b(sprintf|wsprintf|strcpy|strcat)\s*\(' | wc -l)
	check "R17 (unbounded format/copy lines outside the packet tree)" "$R17" "$R17_BASELINE"
fi

#----------------------------------------------------------------------
# R18 - raw wsprintf identifiers. The POSIX shim is gone; every former
# caller supplies a real destination capacity through SafeFormat or swprintf.
# Unlike R17's line count, this masks comments/literals and catches aliases
# such as &wsprintfA too. The counter pins its source inventory and self-tests
# its lexer before reporting a number. Token-pasted names remain outside this
# source check; the full POSIX build has no compatibility declaration either.
#----------------------------------------------------------------------
if R18=$(perl tests/tools/count_wsprintf.pl); then
	check "R18 (raw wsprintf identifiers)" "$R18" 0
else
	echo "FAIL R18: wsprintf source counter failed"
	FAIL=1
fi

#----------------------------------------------------------------------
# R6 was here for exactly one slice, and retired by doing its job.
#
# Task 5.1 stubbed SendBugReport in tests/stubs/client_globals.cpp so
# the test binary would link, which disarmed the only thing that can
# see a library file calling an executable-side function - W1/W2 read
# includes, R4 greps g_p* globals, and neither sees a call. R6 counted
# packetwire members calling it, and the next slice tripped it at once:
# promoting ClientCommunicationManager.cpp took the count to 2. The
# answer was to move SendBugReport itself into the wire layer
# (Client/Packet/WireHost.cpp), so packetwire defines it, the stub is
# gone. The failed-link detector is back, narrower than this check
# was, since it catches a call only in a library unit_tests links and
# only in an object some test pulls in, which is what the link-proof
# tests are for. A ratchet over a symbol nothing is on the wrong side
# of measures nothing.
#----------------------------------------------------------------------

#----------------------------------------------------------------------

if [ "$FAIL" -ne 0 ]; then
	echo "ratchets: FAILED"
	exit 1
fi
echo "ratchets: all green"
exit 0
