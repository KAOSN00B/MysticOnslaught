# Aim Default and Class Mechanic HUD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Press / Press the default aimed-ability input and replace the small top-screen class indicators with one readable bottom-HUD mechanic bar beside Armour.

**Architecture:** Preserve the existing ability-aim state machine and serialized setting key, changing only the new-settings default. Render the class mechanic from `Engine::DrawAbilityBar`, where Armour, EXP, and the Engine-owned Hunter shot counter are already available; remove the superseded Warrior, Rogue, and Paladin indicators from `HUDRenderer`.

**Tech Stack:** C++17, raylib 5.5, Visual Studio 2022/MSBuild, assertion-based C++ regression test.

## Global Constraints

- Preserve all unrelated user and Claude changes in the dirty worktree.
- Do not change which abilities aim, aim geometry, movement scaling, mana costs, cooldowns, or class balance.
- Keep `ability_aim_toggle` as the serialized settings key.
- Existing saved Hold / Release preferences remain respected.
- Mage and Warlock do not display a class-mechanic bar.
- Build `Debug|x64` after implementation.

---

### Task 1: Press / Press Default

**Files:**
- Modify: `TestGame/SettingsManager.h`
- Create: `TestGame/SettingsDefaultsTests.cpp`

**Interfaces:**
- Consumes: `GameSettings::abilityAimToggle` and the existing Engine aiming state machine.
- Produces: new `GameSettings` instances defaulting to Press / Press while loaded settings continue overriding the field.

- [ ] **Step 1: Write the failing default regression test**

Create `TestGame/SettingsDefaultsTests.cpp`:

```cpp
#include "SettingsManager.h"
#include <cassert>
#include <cstdio>

int main()
{
    GameSettings settings{};
    assert(settings.abilityAimToggle);
    std::puts("Settings default tests passed");
    return 0;
}
```

- [ ] **Step 2: Compile and verify the test fails**

Run:

```powershell
cl /nologo /std:c++17 /EHsc TestGame\SettingsDefaultsTests.cpp /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" /Fe:x64\Debug\SettingsDefaultsTests.exe
x64\Debug\SettingsDefaultsTests.exe
```

Expected: assertion failure because `abilityAimToggle` currently defaults to `false`.

- [ ] **Step 3: Change only the default value**

In `TestGame/SettingsManager.h`, change the field to:

```cpp
bool abilityAimToggle = true; // false: hold/release, true: press/press
```

Do not alter `SettingsManager::Load`, `SettingsManager::Save`, or Engine input handling; those already implement and persist both modes.

- [ ] **Step 4: Run the regression test**

Run the Step 2 commands again.

Expected: `Settings default tests passed` and exit code `0`.

---

### Task 2: Bottom Class-Mechanic Bar

**Files:**
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/HUDRenderer.cpp`

**Interfaces:**
- Consumes: `Character::GetRagePercent`, `Character::GetComboPoints`, `Character::GetMaxComboPoints`, `Character::GetFaithPercent`, `Character::GetHunterMarkEvery`, and `Engine::_hunterShotsSinceMark`.
- Produces: one class-specific bar drawn beside Armour above the EXP bar.

- [ ] **Step 1: Calculate collision-safe bar geometry**

Inside the EXP/Armour block in `Engine::DrawAbilityBar`, calculate the Armour row's maximum right edge from `maxArmour`, `iconW`, and `iconGap`. Place the mechanic bar after that edge with an `18.f` gap. Constrain its right edge to `expBarX + expBarW`, use the same vertical band above `expBarY`, and skip drawing if the available width is less than `180.f`.

Use these dimensions:

```cpp
const float mechanicH = std::max(28.f, armourIconH);
const float mechanicY = expBarY - mechanicH - 10.f;
const float mechanicX = armourRight + 18.f;
const float mechanicRight = expBarX + expBarW;
const float mechanicW = mechanicRight - mechanicX;
```

- [ ] **Step 2: Resolve class progress, label, and colour**

Immediately before drawing, use one class switch:

```cpp
bool showMechanic = true;
float progress = 0.f;
Color classColor = WHITE;
const char* label = "";
bool primed = false;

switch (_player.GetClass())
{
case PlayerClass::Warrior:
    progress = std::clamp(_player.GetRagePercent(), 0.f, 1.f);
    classColor = Color{255, 120, 30, 255};
    label = TextFormat("RAGE  %d%%", (int)std::round(progress * 100.f));
    primed = progress >= 1.f;
    break;
case PlayerClass::Rogue:
{
    const int current = _player.GetComboPoints();
    const int maximum = std::max(1, _player.GetMaxComboPoints());
    progress = (float)current / (float)maximum;
    classColor = Color{235, 65, 85, 255};
    label = TextFormat("COMBO  %d / %d", current, maximum);
    primed = current >= maximum;
    break;
}
case PlayerClass::Hunter:
{
    const int required = std::max(1, _player.GetHunterMarkEvery());
    const int current = std::clamp(_hunterShotsSinceMark, 0, required);
    progress = (float)current / (float)required;
    classColor = Color{95, 210, 105, 255};
    label = TextFormat("MARK  %d / %d", current, required);
    primed = current == required - 1;
    break;
}
case PlayerClass::Paladin:
    progress = std::clamp(_player.GetFaithPercent(), 0.f, 1.f);
    classColor = Color{240, 205, 85, 255};
    label = TextFormat("FAITH  %d%%", (int)std::round(progress * 100.f));
    primed = progress >= 1.f;
    break;
default:
    showMechanic = false;
    break;
}
```

For Hunter, the primed pulse means the next landed basic shot applies Mark; the displayed count remains the landed shots already banked.

- [ ] **Step 3: Draw a readable bar**

When `showMechanic && mechanicW >= 180.f`, draw:

```cpp
Color pulseColor = classColor;
if (primed)
{
    const float pulse = sinf((float)GetTime() * 8.f) * 0.5f + 0.5f;
    pulseColor = ColorLerp(classColor, WHITE, pulse * 0.35f);
}

DrawRectangleRounded({mechanicX, mechanicY, mechanicW, mechanicH}, 0.22f, 6,
                     Color{8, 10, 14, 235});
DrawRectangleRounded({mechanicX, mechanicY, mechanicW * progress, mechanicH}, 0.22f, 6,
                     Fade(pulseColor, 0.88f));
DrawRectangleRoundedLines({mechanicX, mechanicY, mechanicW, mechanicH}, 0.22f, 6,
                          pulseColor);
```

Measure the label at font size `18`, center it, draw four black outline copies at offsets `{-2,0}`, `{2,0}`, `{0,-2}`, `{0,2}`, then draw the white foreground text.

- [ ] **Step 4: Remove superseded top indicators**

Delete the Warrior Rage, Paladin Faith, and Rogue combo rendering blocks from `HUDRenderer::DrawHUD`. Do not change HP, mana, room labels, elite warnings, minimap, touch UI, or debug UI.

- [ ] **Step 5: Build and manually verify**

Build:

```powershell
MSBuild.exe TestGame.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

Verify Warrior, Rogue, Hunter, and Paladin in a dungeon. Confirm the bar does not overlap Armour or EXP, values update during combat, Hunter resets after applying Mark, and Mage/Warlock show no empty bar.

Expected: `Debug|x64` builds with zero errors.

---

### Task 3: Final Regression Pass

**Files:**
- Verify: `TestGame/SettingsManager.h`
- Verify: `TestGame/Engine.cpp`
- Verify: `TestGame/HUDRenderer.cpp`

**Interfaces:**
- Consumes: the completed settings and HUD changes.
- Produces: a verified desktop build with no aiming-profile or class-balance changes.

- [ ] **Step 1: Search for unintended aiming edits**

Run:

```powershell
git diff -- TestGame/SettingsManager.h TestGame/Engine.cpp TestGame/HUDRenderer.cpp
```

Confirm only the default flag, bottom mechanic-bar rendering, and old indicator removal changed.

- [ ] **Step 2: Run tests and final build**

Run `x64\Debug\SettingsDefaultsTests.exe`, then build `Debug|x64` again.

Expected: settings test passes and build exits `0`.
