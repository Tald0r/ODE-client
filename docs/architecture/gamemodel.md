---
type: Design
title: "The gamemodel library"
description: "The pure game model extracted from the executable, with the seventeen packet handlers whose bodies reach only model state and named hosts; membership is tests/arch/gamemodel_files.txt."
tags: [architecture, testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The gamemodel library

# Purpose

`gamemodel` holds the game logic that was cut out of the executable so that `unit_tests` can link
it: data tables, items, skills, status, chat, the event queue, login metadata, effects and the
other model state below. Everything it needs from the running game comes through a named host
([host interfaces](host-interfaces.md)) that `Client/GameInit.cpp` installs; a test binary runs
with no host. Membership is `tests/arch/gamemodel_files.txt`; the extractions are the
[restructuring plan](../reviews/RESTRUCTURING.md)'s phase 4 tasks (4.1 to 4.43 and the follow-up
extractions through 4.87, with the later slices to 4.265 for the view and client libraries).

# What it holds

- **Tables**: the action and effect tables (`MActionInfoTable`, `MEffectSpriteTypeTable`,
  `MEffectStatusTable`, `MCreatureSpriteTable`), the item table, the skill info table, creature
  name tables and selection (`CreatureNameSelection`), the music table (`MMusicTable`,
  `SelectZoneMusic`), zone and sector sound metadata (`MZoneSound`, `SectorSoundInfo`), the help
  strings (`MHelpStringTable`), NPC dialogue and its English overlay (`MNPCScriptTable`,
  `ApplyEnglishNPCScriptTable`, `MNPCTableEnglish`), interaction-object metadata, per-world
  character-selection settings (`PCConfigTable`), and the fixed-point trigonometry in `MathTable`.
- **Items and money**: `MItem` with what an item requires and `MItem::IsUsableBy`, the gear
  families, the item managers, the containers (inventory, storage, shop shelves), the trade manager
  over them, the gear the three races wear and the shop behind `MItemHost`, the money manager, the
  price manager behind `MPriceHost`, and NPC shop stock (`BuildNPCShopShelf`,
  `MShopTemplateTable`) behind `NPCShopHost`.
- **Skills, status and combat**: the skill core (info table, skill set, domains and their tree;
  what the player can use right now stays executable-side), `MStatusManager`'s combat-stat
  preview, the creature status array (`MStatus`, `AffectModifyInfo`), the request/answer mode
  register (`TempInformation`), `MRequestMode`, and the war-state manager (`MWarManager`) behind
  `MWarHost`.
- **Chat**: `MChatManager`'s curse lists and `RemoveCurse` over `MStringMap`, behind `MChatHost`.
- **Frame-driven state**: the event queue (`MEventQueue`, behind `MEventHost`), screen fades
  (`MScreenFade`), weather (`MWeather`, behind `MWeatherHost`), delayed and ambient sound
  (`DelayedSoundQueue`, `AmbientSoundState`), show-time windows (`ShowTimeChecker`), deferred action
  results (`MActionResultQueue`) and effect targets (`MEffectTarget`, behind `MEffectTargetHost`).
- **Effects and motion**: orbit, linear, homing and parabolic motion (`EffectOrbit`,
  `LinearEffectMotion`, `HomingEffectSteering`, `ParabolaEffectMotion`), effect deadlines
  (`EffectTiming`), the wave, ripple, ground ring, selectable and rectangle generators with their
  hosts, and the duration conversions `ConvertDurationToFrame` and `ConvertDurationToMillisecond`.
- **Login and peers**: `CServerInformation` with `ApplyWorldList` and `ApplyServerList`,
  `SelectLoginEndpoint`, `ServerInfoFileParser`, `RequestUserManager`, the self-defense roster
  (`MJusticeAttackManager`), portals and zone info (`MPortal`, `ZoneInfoData`), callback dispatch
  (`MFunctionManager`), crash-report records and named profiling state, and the user, config and
  timed-item loaders with their string support.

# The seventeen handlers

Seventeen packet handlers stay in `Client/PacketHandler` but compile into `gamemodel`, because
their bodies reach only model state and named host services: the phone slots
(`GCPhoneConnected`, `GCPhoneDisconnected`, `GCPhoneSay`, `GCRing`), the trade box
(`GCTradeMoney`, `GCTradeRemoveItem`), `GCSystemAvailabilities`, `GCMonsterKillQuestInfo`, the
self-defense roster (`GCAddInjuriousCreature`, `GCRemoveInjuriousCreature`), rank-bonus state
(`GCRankBonusInfo`, `GCSelectRankBonusOK`, behind `RankBonusHandlers::Host`), bonus eligibility
and skill snapshots (`GCHolyLandBonusInfo`, `GCSweeperBonusInfo`, `GCSkillInfo`, whose
`ApplySkillInfo` rebuild clears model-owned sweeper bonuses and asks `BonusSkillHost` for the
live refresh), and the login lists (`LCWorldList`, `LCServerList`). `test_model_handlers.cpp`
runs every out-of-range slot byte past the phone handlers
([packet input safety](../protocol/packet-input-safety.md)). Every handler, model-only or not, is
bound to its id in `Client/PacketHandlerRegistry.cpp`.
