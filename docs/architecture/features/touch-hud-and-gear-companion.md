# Full-Screen Touch HUD & Gear Companion (`src/port/touch_controls.cpp`, `src/port/gear_data.c`)

Provides the Android foldable/phone full-screen display layout, the floating Picture-in-Picture (PiP) DS bottom screen, the transparent full-screen battle HUD, the Poké Ball quick-throw carousel, and the Gear companion panel.

## 1. Display Layout & PiP Bottom Screen (`src/port/touch_controls.cpp`, `src/port/sim_gui_prj.cpp`)

- **Config Flags (`include/port/sim_config_prj.h`):**
  - `topRightOverlay`: Renders the top DS screen full-screen and places the bottom DS touch screen in a rounded top-left PiP bezel.
  - `topRightOverlayScale`: Scales the PiP overlay.
  - `topRightOverlayMinimized`: Toggles PiP visibility via the bottom-row eye pill (`VBTN_PIP`).
  - `topRightOverlayAutoFade`: Auto-hides/fades the PiP during non-interactive scenes when `Gear_BattleNeedsDsScreen()` is `FALSE`.
  - `fullScreenCompanion`: Toggles the full-screen Gear companion panel (`VBTN_SWITCH`).
- **PiP Battle Suppression:** During Gen 5 battles (`Gen5Battle_IsShown()`), `Gear_BattleNeedsDsScreen()` (`src/port/gear_data.c:937`) delegates to `Gen5_BattleNeedsDsScreen()`, keeping the PiP hidden until a sub-screen (Bag, Party, Move-Forget) opens.

## 2. Full-Screen Battle Overlay (`DrawFullScreenBattleOverlay` in `src/port/touch_controls.cpp:724`)

When `battleOverlayActive` is `TRUE`, ImGui draws the battle HUD directly over the main screen:

1. **Gen V Weather Pill (`DrawWeatherBadge`):** Reads `Gear_BattleWeather()` (`1=Sun`, `2=Rain`, `3=Sandstorm`, `4=Hail`, `5=Fog`).
2. **Poké Ball Quick-Throw Carousel:**
   - Queries `Gear_GetBattlePokeBalls(balls, max, &canThrow)` (`src/port/gear_data.c:1387`), displaying `< [Ball Sprite + Qty] >` beside the player's HP bar whenever `CanThrowBattlePokeBall()` is `TRUE`.
   - Tapping the center pill calls `Gear_UseBattlePokeBall(selectedItem)` (`src/port/gear_data.c:1427`):
     - **Gen 5 active:** Calls `Gen5_UseBattlePokeBall(item)` -> `Gen5_PortQueuePokeBall(item)`.
     - **Platinum fallback active:** Calls `BattleController_PortUseBall(sBattleSys, item)` (`src/battle/battle_controller_player.c:344`), which queues `sPendingBallItem` for `BattleControllerPlayer_CommandSelectionInput` to consume safely on `SDLThread`.
3. **Bottom-Right Command Panel (`Gear_GetBattleMenu` -> `Gear_PressBattleButton`):**
   - **`GEAR_MENU_ACTION`:** 2x2 grid (`FIGHT`=`0`, `BAG`=`2`, `POKEMON`=`1`, `RUN`/`BACK`=`3`, plus Safari variants). When `Gen5Battle_IsShown()` is `FALSE`, `Gear_PressBattleButton` (`src/port/gear_data.c:1158`) swaps `1` and `2` to match Platinum's `sActionMenuTouchRects` (`1=BAG`, `2=POKEMON`).
   - **`GEAR_MENU_MOVES`:** Left `BACK` bar (`0`) + 2x2 move cards (`1..4`) colored by `mv.type` with PP counters and `CalcOverlayMoveEffectiveness` badges (`SUPER EFFECTIVE`, `NOT VERY EFFECTIVE`, `NO EFFECT`, accounting for `ABILITY_LEVITATE`, `ABILITY_WONDER_GUARD`, and `ABILITY_MOLD_BREAKER`).
   - **`GEAR_MENU_TARGETS`:** Left `BACK` bar (`0`) + 2x2 target cards (`1..4`), color-coded red for foes (`row == 0`) and green for allies/self (`row == 1`).
   - **`GEAR_MENU_YES_NO`:** Two-card prompt (`menu.yes` -> `0`, `menu.no` -> `1`).
   - **`GEAR_MENU_FIRST` / `GEAR_MENU_PAL_PARK`:** Single `FIGHT` card (`0`) or `PARK BALL` (`0`) / `RETIRE` (`1`) cards.
