---
type: Design
title: "The gamemodel library"
description: "The game model extracted from the executable, every packet handler under Client/PacketHandler among it, each piece reaching the running game through a named host; membership is tests/arch/gamemodel_files.txt."
tags: [architecture, testing]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# The gamemodel library

# Purpose

`gamemodel` holds the game logic that was cut out of the executable so that `unit_tests` can link
it: data tables, items, skills, status, chat, the event queue, login metadata, effects, the packet
handlers and the other model state below. Everything it needs from the running game comes through
a named host ([host interfaces](host-interfaces.md)) that `Client/GameInit.cpp` installs; a test
binary runs with no host. Membership is `tests/arch/gamemodel_files.txt`, whose comments say what
each group is and which task moved it; the extractions are the
[restructuring plan](../reviews/RESTRUCTURING.md)'s phase 4 tasks (4.1 to 4.43 and the follow-up
extractions through 4.87, with the later slices to 4.265 for the view and client libraries).

# What it holds

- **Tables**: the action and effect tables (`MActionInfoTable`, `MEffectSpriteTypeTable`,
  `MEffectStatusTable`, `MCreatureSpriteTable`), the item table, creature name tables and
  selection (`CreatureNameSelection`, which `MCreature` uses for titles and hallucination names,
  including the operator-prefix exception), music metadata and track selection (`MMusicTable`,
  `SelectZoneMusic`; playback stays in `GameMain`), zone and sector sound metadata (`MZoneSound`,
  `SectorSoundInfo`), help strings with display tracking (`MHelpStringTable`), NPC dialogue lookup
  and substitution with its English overlay (`MNPCScriptTable`, `ApplyEnglishNPCScriptTable`) and
  the English NPC name overlay (`MNPCTableEnglish`), interaction-object metadata
  (`INTERACTIONOBJECTTABLE_INFO`), per-world character-selection settings (`PCConfigTable`), and
  the fixed-point trigonometry and steering in `MathTable` that orbit, parabola and homing effects
  use.
- **Items and money**: `MItem`, with what an item requires and whether the player may use it
  (`MItem::IsUsableBy`, which `MCreature::CheckAffectStatus` asks), the gear families, the item
  managers, the containers (inventory, storage, shop shelves), the trade manager over them, the
  gear the three races wear and the shop behind `MItemHost`, the money manager, the price manager
  behind `MPriceHost`, and NPC fixed and mysterious shop stock (`BuildNPCShopShelf`,
  `MShopTemplateTable`), with item creation, gender and portal actions behind `NPCShopHost`.
- **Skills, status and combat**: the skill core (the info table, with a vampire's skill cost,
  Will of Life's cost and reuse time and a skill's range at its level; the skill set; the domains
  and their tree), `MStatusManager`'s combat-stat preview (the character-select to-hit, defense,
  protection and damage, and the attack speed `MPlayer::CalculateStatus` takes from it), the
  creature status array (`MStatus`, with `AffectModifyInfo`, which applies a ModifyInfo packet to
  it), the request/answer mode register (`TempInformation`), the player/view request mode
  (`MRequestMode`), and the war-state manager (`MWarManager`, with live zone, UI and skill actions
  behind `MWarHost`).
- **Chat**: `MChatManager`'s curse lists and `RemoveCurse` over `MStringMap`, behind `MChatHost`.
- **Frame-driven state**: the event queue (`MEventQueue`, the game's `g_pEventManager`, with
  gamma, effect and fade actions behind the `MEventHost` `GameInitInfo` installs; the never-drawn
  background images were retired in task 4.224), screen-fade progression (`MScreenFade`, which
  `MTopView` draws and advances with the frame number), weather particles and progression
  (`MWeather`, with the player origin and viewport size behind `MWeatherHost`), delayed sound
  scheduling (`SOUND_NODE`, `MakeThunderSound` and `DelayedSoundQueue`; the caller supplies
  frame timestamps and playback), ambient sound decisions (`AmbientSoundState`, with propeller
  transitions and randomized requests), show-time hour windows and scheduling (`ShowTimeChecker`,
  with explicit frame, game-hour and random-source inputs), deferred action-result ownership
  (`MActionResultQueue`) and effect-target state (`MEffectTarget`, with player-roster removal
  behind `MEffectTargetHost`).
- **Effects and motion**: orbit paths and progression (`EffectOrbit`; drawing and effect lifetime
  stay with the effect), linear trajectory state shared by linear, guided and chase effects
  (`LinearEffectMotion`), homing angle progression and horizontal displacement
  (`HomingEffectSteering`, consumed by `MHomingEffect`), parabolic trajectory and arrival
  (`ParabolaEffectMotion`; `MParabolaEffect` keeps the smoke and impact actions), effect deadlines
  (`EffectTiming`, given the current frame by `MEffect` and the attached-effect constructor), the
  complete wave and ripple generators (`MBloodyWaveEffectGenerator`, `MRippleZoneEffectGenerator`)
  with frame, bounds and queue services behind designated hosts, the ground ring, selectable and
  rectangle generators with the ground queues that consume them, and the duration conversions
  `ConvertDurationToFrame` and `ConvertDurationToMillisecond`.
- **Login and peers**: the login world and server metadata and selection (`CServerInformation`,
  with `ApplyWorldList` and `ApplyServerList` consuming the login packets; their handlers keep the
  UI and mode changes), login endpoint selection before DNS and connection
  (`SelectLoginEndpoint`), dimension-specific login and reconnect configuration
  (`ServerInfoFileParser`), peer endpoint records (`RequestUserManager`), the self-defense target
  roster (`MJusticeAttackManager`), portal records and zone-info parsing consumed by map changes
  (`MPortal`, `ZoneInfoData`), callback dispatch inherited by status reactions and keyboard
  accelerators (`MFunctionManager`), crash-report records and named profiling state, and the user,
  config and timed-item loaders with their string support.
- **The world**: the creature, the creature that wears gear, the NPC, the fake creature, the
  player and the zone, moved on 2026-10-15 (tasks 4.242 to 4.247), the day after the last of the
  handlers that drive them.

# The packet handlers

Every handler under `Client/PacketHandler` compiles into `gamemodel` (280 files, counted
2026-10-23), and `Client/PacketHandlerRegistry.cpp`, which binds each to its id, is a member too
(task 4.218). The first seventeen moved because their bodies reach only model state and named host
services: the phone slots (`GCPhoneConnected`, `GCPhoneDisconnected`, `GCPhoneSay`, `GCRing`), the
trade box (`GCTradeMoney`, `GCTradeRemoveItem`), `GCSystemAvailabilities`,
`GCMonsterKillQuestInfo` (task 4.15), the self-defense roster (`GCAddInjuriousCreature`,
`GCRemoveInjuriousCreature`, task 4.25), rank-bonus state (`GCRankBonusInfo`,
`GCSelectRankBonusOK`, behind `RankBonusHandlers::Host`), bonus eligibility and skill snapshots
(`GCHolyLandBonusInfo`, `GCSweeperBonusInfo`, `GCSkillInfo`, whose `ApplySkillInfo` rebuild clears
model-owned sweeper bonuses and asks `BonusSkillHost` for the live refresh), and the login lists
(`LCWorldList`, `LCServerList`). The rest followed in later slices, each over the hosts its body
reaches; the last, the player's update on entering a zone, moved in task 4.198 (2026-10-14). The
executable's own source has been `Client/SDLMain.cpp` alone since task 4.265 (2026-10-19;
[library boundaries](library-boundaries.md)). `test_model_handlers.cpp` runs every
out-of-range slot byte past the phone handlers ([packet input safety](../protocol/packet-input-safety.md)).
