# Diagnosing the held-button skill repeat (`[SKILLREPEAT]` logging)

**Date:** 2026-08-31
**Branch:** `diag/vampire-skill-repeat`
**Status:** RESOLVED — root cause found the same evening, fixed server-side
(opendarkeden-server PR #23). The instrumentation is kept as a permanent
regression signal; see "Keeping this" at the bottom.

---

## Resolution (2026-08-31 evening)

The stutter was real, client-visible, and server-caused. The chain, proven by
this instrumentation plus the server's `VampireSkillSave` rows:

1. **Learning a skill seeds its server-side cooldown slot with the
   `SkillBalance` table's MaxDelay** — 20 (= 2.0 s) for both Bloody Nail and
   Violent Phantom — instead of the per-cast formula delay (0.6 s / 0 s). The
   first *successful* cast overwrites ("heals") the interval with the formula
   value and persists it.
2. **The client re-reads every skill's delay from that slot interval on every
   skill-info refresh** — login *and* every zone change (`GCUpdateInfo` →
   `SkillInfoTable Init`, then per-skill `SetDelayTime(skillTurn * 100)` in
   `GCSkillInfoHandler`). `SetDelayTime` floors anything under 1800 ms to zero,
   so the healed values vanish — but the learn seed (2000 ms) survives the
   floor and arms a per-cast cooldown.
3. **The client then holds that value until the next refresh.** The visible
   signature is `abort=SKILLSET avail=1 enable=1 time=0` with `left=` counting
   down from ≈ (2000 − animation length) ms after every cast — a ~2.2 s cast
   cadence against a 682–930 ms animation floor.
4. A later zone change (or relog) after the heal delivers the small interval,
   the floor zeroes it, and the stutter ends mid-session. This is why "relog
   fixes it" was reported — any refresh after the first landed cast fixes it.

The fix is server-side (opendarkeden-server PR #23): a freshly learned skill's
slot now starts with a zero interval and no run-time lock; the first successful
cast installs the real formula delay exactly as before. Attack-speed animation
tiers are unaffected — measured post-fix at 930/806/682 ms per Bloody Nail cast
on SLOW/NORMAL/FAST characters, so low-dex players still swing slower.

The one wrong turn recorded below ("Learning a skill mid-session ... cannot
sustain a stutter") is left in place with a correction, so the reasoning error
is visible: the learn path itself *is* one-shot, but the seed it plants is
re-delivered by every zone change, which is what sustained it.

---

## What this is for

Holding the right mouse button on a target is supposed to chain a skill
continuously. Some vampire melee skills were reported as stuttering — Bloody
Nail and Violent Phantom cast once, pause, cast again — while left-click melee
and Hands of Wisdom chained smoothly.

A first measurement pass off an ordinary debug log localised the problem to
`MPlayer::TraceCreatureToSpecialAction()`, but could not say *which* of its
guarded early-returns was firing, because none of them logged. This branch adds
that logging. It changes no behaviour: every added statement is a
`DEBUG_ADD_FORMAT` or a brace around one.

A follow-up session on this build measured clean. If the stutter recurs, one
run now names the cause outright — that is what this document is for.

## Building and running

Ordinary Debug tree, nothing special:

```bash
cmake --build build/vs2022 --config Debug -- -m
```

`DEBUG_ADD_FORMAT` is live in Debug builds (see the `_DEBUG` trap in
`CLAUDE.md`), so no extra define is needed. Run the client from the Visual
Studio debugger as usual.

**The log is truncated on every launch.** `log_set_file_output` opens
`DarkEden-debug.log` with `fopen(..., "w")` (`Client/DebugLog.cpp:189`), so copy
the file aside before relaunching if a run is worth keeping.

```
build/vs2022/bin/Debug/DarkEden-debug.log
```

### Getting a useful sample

The failure only shows up under a *sustained hold*. Casting once and releasing
tells you nothing — the log will be full of `rb=0` cancels, which are just the
button coming up.

- Hold the button down and keep it down.
- Pick a target tanky enough that it does not die mid-chain. A dead target ends
  the repeat legitimately (`abort=TARGET dead=1`), and with high DEX that
  happens constantly, which masks everything else.
- Do a run per skill, and include a known-good skill (Hands of Wisdom) as a
  control.

## The log lines

All diagnostic lines are prefixed `[SKILLREPEAT]`.

### `enter` — one per re-issue attempt

Emitted at the top of `TraceCreatureToSpecialAction`, after the skill id is
resolved and before any of the guards.

```
[SKILLREPEAT] enter ai=328 rep=1 stop=1 act=9 cnt=8/8 mv=8/8 mvAct=1 used=65535 trace=02 buf=02
```

| field | meaning |
|---|---|
| `ai` | resolved skill id (`originalSkill`) |
| `rep` | `m_bRepeatAction` — is the repeat armed? |
| `stop` | `IsStop()` — the gate that decides commit vs. buffer |
| `act` | `m_Action` (current animation) |
| `cnt` | `m_ActionCount` / `m_ActionCountMax` |
| `mv` | `m_MoveCount` / `m_MoveCountMax` |
| `mvAct` | `m_MoveAction` (1 = `ACTION_MOVE`, the on-foot value) |
| `used` | `m_nUsedActionInfo` (65535 = `ACTIONINFO_NULL`, i.e. idle) |
| `trace` / `buf` | `m_fTrace` / `m_fTraceBuffer` bitmasks, hex. `02` = `FLAG_TRACE_CREATURE_SPECIAL` |

### `abort=` — the attempt did not commit

One of these fires when a guard returns early. They map one-to-one onto the
early-returns between the `enter` line and the commit.

| tag | meaning |
|---|---|
| `abort=SKILLSET` | skill not usable. Fields `avail= enable= time= passive= left=` say which of the four conditions failed and how much cooldown is left |
| `abort=TARGET` | target rejected. `null= dead= dark=` say why |
| `abort=SAFESECTOR` | blocked by safe-zone rules (player targets only) |
| `abort=NPC` | target is an NPC, redirected to basic action |
| `abort=NOTSTOP` | `IsStop()` was false. Fields repeat the counters |

`abort=SKILLSET`, `TARGET`, `SAFESECTOR` and `NPC` all cancel the repeat.
`abort=NOTSTOP` **only** cancels it when `rep=1`; with `rep=0` it takes the
branch that buffers the action for the next tick and returns success. See
*Reading a session* below — this distinction matters a lot.

### Commit

Not one of ours, but it is the line that says the attempt succeeded:

```
Trace Creature To SpecialAction: ID=10293, (92, 33)
```

### `unset` — the repeat was cancelled

Emitted from `UnSetRepeatAction()` itself, and only on the transition (the
function is called unconditionally from dozens of sites, so logging every call
would bury the signal).

```
[SKILLREPEAT] unset used=328 special=328 act=9 cnt=2/8 rb=0 lb=0 lock=0
```

`rb` / `lb` are `g_bRButtonDown` / `g_bLButtonDown`, and `lock` is
`m_bLockMode` (Caps Lock auto-attack).

**`rb=0` means the user had already released the button — that cancel is
correct and uninteresting.** Only cancels with `rb=1` are candidates for a bug.

### `why=` — call-site tags on the cancels that can fire mid-hold

`UnSetRepeatAction()` has too many callers to identify from the `unset` line
alone. The ones that can fire while the button is held are tagged:

| tag | site |
|---|---|
| `why=KEEPTRACE` | `CheckRepeatAction()` guard — `m_bKeepTraceCreature`, request mode, or darkness |
| `why=BLITZBLAZE` | the Blitz Sliding / Blaze Walk special case in `MPlayer::Action()` |
| `why=REPEATTIMER` | the `REPEAT_TIME` ceiling expired (10 min in `ClientConfig.inf`) |
| `why=NOENEMY` | lock mode found no enemy in range |
| `why=RBDOWN_NOOBJ` | **a missed pick on the press edge.** This is the path the 60 fps picking regression came in through; it also logs the camera gap and cursor position |

The four button-release handlers in `CGameUpdate.cpp` are deliberately not
tagged — they are already identifiable by `rb=0` on the `unset` line.

Note there is also a pre-existing `unsetrepeat: actionCount=N / M` message with
**two** emitting sites: `CheckRepeatAction()`'s repeat-limit branch and
`UnSetRepeatAction()`. They are distinguished only by the `MPlayer.cpp:<line>`
prefix the logger prints. Both mean "the shortened repeat animation has been
restored to full length".

## Reading a session

### 1. The headline metric: entries per commit

```bash
cd build/vs2022/bin/Debug
echo "enters:  $(grep -c '\[SKILLREPEAT\] enter' DarkEden-debug.log)"
echo "commits: $(grep -c 'Trace Creature To SpecialAction' DarkEden-debug.log)"
```

Divide. **A healthy chain sits at roughly 1.3.**

That baseline is not 1.0 for a structural reason worth knowing:
`CheckRepeatAction()` fires at `m_ActionCount >= m_ActionCountMax - 1`
(`MPlayer.cpp:8736`) — one frame *early* — while `IsStop()` needs
`>= m_ActionCountMax` (`MCreature.cpp:2699`). So a healthy cast normally spends
one attempt on a harmless `abort=NOTSTOP` before the one that commits. Counting
raw abort percentages and calling them failures is the mistake that sent the
first investigation off course.

### 2. Break it down by cause

```bash
grep -oE "\[SKILLREPEAT\] (abort|why)=[A-Z_]+" DarkEden-debug.log | sort | uniq -c | sort -rn
```

Then split `NOTSTOP` by whether the repeat was armed, because only `rep=1`
counts as a lost cycle:

```bash
grep -oE "abort=NOTSTOP rep=[01]" DarkEden-debug.log | sort | uniq -c
```

### 3. Look only at cancels with the button held

This is the highest-signal query in the whole document:

```bash
grep -B2 "unset .*rb=1" DarkEden-debug.log
```

`-B2` picks up the `enter` and `abort=` lines immediately above, which name the
branch. Every hit here is either a genuine bug or a legitimate target loss.

### 4. Measure the cast cadence

```bash
grep "Trace Creature To SpecialAction" DarkEden-debug.log \
| perl -ne 'if(/\[\d{4}-\d\d-\d\d (\d\d):(\d\d):(\d\d)\.(\d{3})\]/){
    $t=(($1*60+$2)*60+$3)*1000+$4;
    if(defined $p && $t-$p<6000){push @d,$t-$p} $p=$t }
  END{ @d=sort{$a<=>$b}@d;
    printf("n=%d min=%d p25=%d median=%d p75=%d\n",
      scalar @d,$d[0],$d[int(@d/4)],$d[int(@d/2)],$d[int(3*@d/4)]) }'
```

### 5. Classify

- Long gaps preceded by an `unset` with `rb=1` → **cancel family**. The adjacent
  `abort=` line names the branch. Fix that branch's input, not its consequence.
- Long gaps preceded by runs of `abort=NOTSTOP rep=1` → **stall family**. The
  repeat never dropped; the action/move counters are wedging `IsStop()`. Look at
  `cnt=` and `mv=` in those lines.
- A quiet window with no `enter` lines at all before a cast means the delay gate
  at the top of `TraceCreatureToSpecialAction` was swallowing retries — it
  returns *before* the `enter` log. That is evidence the previous cast went out
  with the repeat flag already down, because `ActionToSendPacket` charges no
  delay at all while `m_bRepeatAction` is set (`MPlayer.cpp:6959`).

## Reference numbers

Measured on a vampire with attack speed 85 / DEX 300.

| | stuttering session (14:32–15:33) | clean session (17:55–17:58) |
|---|---|---|
| entries per commit, Hands of Wisdom | 1.30 | — |
| entries per commit, Bloody Nail | 2.65 | — |
| entries per commit, Violent Phantom | 3.25 | — |
| entries per commit, overall | — | **1.29** |
| Bloody Nail cadence (median) | 2293 ms | **682 ms** |
| Violent Phantom cadence | ~2500 ms gaps | median **807 ms**, p25 497 ms |
| cancels with `rb=1` | not measurable (no instrumentation) | 3, all `abort=TARGET` |
| `why=` tags fired | — | **none** |

682 ms for Bloody Nail is exactly 11 frames × 62 ms — its hard floor.

## Background needed to interpret the numbers

### The repeat chain

1. `ProcessInputRButtonDown` (`CGameUpdate.cpp:2834`) runs **once per physical
   press** — `m_rb_down` is a one-tick edge pulse, matching the DirectInput
   original (`CDirectInput.cpp:452` clears all six flags per poll). It calls
   `SetRepeatAction()` on success.
2. `CheckRepeatAction()` (`MPlayer.cpp:8722`) re-arms `m_fNextTrace` from
   `m_fTraceBuffer` when the animation finishes.
3. `CheckBufferAction()` (`MPlayer.cpp:8897`) dispatches it back into
   `TraceCreatureToSpecialAction`.

Because step 1 only fires on the press edge, **a single `UnSetRepeatAction()` is
terminal** until the user releases and presses again. Re-aiming for a
creature-target skill goes through the stored `m_TraceIDBuffer`, not through
picking, so the cursor is not involved once a chain is running.

### Attack speed does not scale timing — it swaps animations

`SetWeaponSpeed` (`MCreature.cpp:2854`) buckets into three tiers:

| attack speed | tier |
|---|---|
| ≤ 33 | `WEAPON_SPEED_SLOW` |
| ≤ 66 | `WEAPON_SPEED_NORMAL` |
| > 66 | `WEAPON_SPEED_FAST` |

`s_VampireActionSpeed[action][m_WeaponSpeed]` (`MCreature.cpp:8491`) then maps
to a *different action id* with its own frame count, and the tier also selects
the column of the `[3]`-element arrays in `Action.inf`
(`StartFrame`, `RepeatStartFrame`, `RepeatEndFrame`).

Consequences when reading logs:

- Hands of Wisdom runs at 15 / 13 / 11 frames on SLOW / NORMAL / FAST. A frame
  count changing mid-session means the tier changed, not that anything broke.
- **There is no tier above 67**, so attack speed beyond that does nothing for
  animation length.
- `GetCreatureActionCountMax` (`MCreature.cpp:391`) takes no speed input at all
  — it is a fixed table by creature type and advancement action.

### The three skills, from `Data/Info/Action.inf`

| | Bloody Nail (93) | Hands of Wisdom (199) | Violent Phantom (328) |
|---|---|---|---|
| `UseRepeatFrame` | false | false | **true** |
| repeat window (FAST) | — | — | frames 3–8 |
| `RepeatLimit` | 65535 | 65535 | **2** |
| action delay | 750 ms | 750 ms | 750 ms |
| range | 1 | 1 | 2 |

Violent Phantom's `RepeatLimit = 2` means: two casts on the short 3–8 window,
then `CheckRepeatAction` restores `m_ActionCountMax` to the full 11 frames for
one cast, then repeat. **The resulting fast-fast-long rhythm is by design in the
data file, not a code defect** — it is visible in a log as `cnt=8/8`, `cnt=8/8`,
`cnt=11/11`. Skills with `UseRepeatFrame = false` have no short window at all,
so Bloody Nail can never chain faster than its full 11 frames.

A decoder for `Action.inf` is not committed; the format is
`MActionInfo::LoadFromFile` (`Client/MActionInfoTable.cpp:359`) preceded by two
`int32` header fields and a `CTypeTable` count.

Note that the `// 82`-style comments in `Client/SkillDef.h` are stale by 11
through the `MAGIC_*` block. `MAGIC_BLOODY_NAIL` is really **93**; the compiled
enum matches `Action.inf` and only the comments are wrong.

## Already ruled out

Recorded so the next person does not re-walk these:

- **The 60 fps interpolation work is not re-timing the repeat.** `UpdateInput()`
  and `ProcessInput()` are both inside the 62 ms tick loop
  (`CGameUpdate.cpp:6176`), `g_CurrentFrame` is incremented in that same loop so
  it is still a tick counter, and commit `fbfc274` touches only draw-time pixel
  positions — no animation counter.
- **The picking regression fix is holding.** `why=RBDOWN_NOOBJ` did not fire once
  across a clean session.
- **Animation counter overrun.** The engine's own `"So Terrible!"` message
  (logged when `m_ActionCount > m_ActionCountMax`) has zero occurrences in either
  session.
- **`m_RepeatTimer` being uninitialised.** It is only read under
  `if (IsRepeatAction())`, which implies `SetRepeatAction()` already wrote it.
- **Learning a skill mid-session.** `GCLearnSkillOKHandler.cpp:53` does set a
  client-side `maxDelay` and start a cooldown, where login instead takes the
  server's delay (`GCSkillInfoHandler.cpp:130`) — but `SetNextAvailableTime()` is
  only re-armed on the inventory-item skill paths, never for creature-target
  ones, so it is a one-shot and cannot sustain a stutter.
  **CORRECTION — this was the root cause after all**, through a delivery path
  this ruling-out missed: the learn-time *server-side* seed (SkillBalance
  MaxDelay, 2.0 s) is re-sent by every `GCUpdateInfo` zone change, survives
  `SetDelayTime`'s 1800 ms floor, and *is* re-armed per cast — by
  `MPlayer.cpp`'s `ActionToSendPacket`, which calls `SetNextAvailableTime()`
  for every committed skill cast, not just item paths. See "Resolution" at the
  top. Two reasoning errors stacked: the wrong `SetNextAvailableTime` call-site
  inventory, and treating "learn path" as client-only.
- **`AttachSelf`** (set on Bloody Nail and Violent Phantom, clear on the two
  working skills) only steers which creature the effect sprite binds to. Nothing
  in the repeat path reads it.

## Keeping this

Decision (2026-08-31, after resolution): the instrumentation stays. These
routine diagnostics now use `LOG_LEVEL_DEBUG`; the client defaults to INFO
in every build. Set `DARKEDEN_TRACE=1` before starting the client to collect
them when investigating another repeat regression (see the README's
[logging instructions](../README.md#troubleshooting)). Filtered debug macros
skip both formatting and argument evaluation:

- entries-per-commit ≈ 1.3 is healthy; sustained > 2 is a regression;
- `abort=SKILLSET` with a large `left=` is the cooldown family (check what the
  server sent as `skillTurn` — this whole investigation);
- `unset ... rb=1` is the cancel family (input/picking).

Every added line is still tagged `[SKILLREPEAT DIAG]` in a comment and
greppable if removal is ever wanted:

```bash
grep -rn "SKILLREPEAT" Client/
```
