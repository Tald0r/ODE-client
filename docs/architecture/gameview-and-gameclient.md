---
type: Design
title: "The gameview and gameclient libraries"
description: "gameview draws the model with SpriteLib over the packs MTopView hands it; gameclient puts the game together, from GameInitInfo to ClientMain, each piece behind a host the executable installs."
tags: [architecture, testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The gameview and gameclient libraries

**`gameview`** is the view's drawing that reads the model and draws with `SpriteLib`, which
`gamemodel` may not include: the divine-guidance silhouettes, the creature shadows and the
creatures drawn with an effect, over the packs and selection `MTopView` hands it as a
`CreatureDrawPacks`. Membership is `tests/arch/gameview_files.txt`
([restructuring plan](../reviews/RESTRUCTURING.md) tasks 4.248 to 4.250).

**`gameclient`** is the client that puts the game together, and may use `VS_UI`. It holds the
start-up's loading of the game information (`GameInitInfo`, behind the `GameInitInfoHost`
`ClientMain` installs), the packet handlers' helpers (`PacketFunction`: the packet item tables, a
creature's look and the player's info, items, effects and pets from their packets, item use,
portals and the skills' shows), the login and waiting screens' update loop (`CWaitUIUpdate`,
behind `CWaitUIHost`), the game's calls into the UI (`GameUI`, the `UI_` functions, behind
`GameUI::Host`), the UI's results turned into packets (`UIMessageManager`, behind
`UIMessages::Host`), the world view (`MTopView`: its packs, picking and every draw), the in-game
frame loop (`CGameUpdate`, behind `CGameUpdateHost`), the game's main functions (`GameMain`:
modes, sounds, music, zones, behind `GameMainHost`), the start-up and teardown (`GameInit`:
`InitGame`, `InitGameObject` and the hosts it installs, `ReleaseAllObjects`), and the client's
entry (`ClientMain`: the launch command, the DLL whitelist, the frame loop, behind `ClientHost`).
Membership is `tests/arch/gameclient_files.txt` (tasks 4.252, 4.253, 4.255 to 4.258 and 4.260
to 4.265).

`unit_tests` links `basic`, `SpriteLib`, `TextSystem`, `packetwire`, `gamemodel`, `gameview`,
`gameclient` (and through it the whole of `VS_UI`) and `dxlib`. The input pump reaches
application state and text editors through `DXInput::Host`, installed in `Client/GameInit.cpp`;
tests link the real adapter and backend without game-global stubs. `ui_tests` links `VS_UI` for
independently reachable widgets; `user_option_tests` also links it for key bindings
([testing overview](../testing/overview.md)).
