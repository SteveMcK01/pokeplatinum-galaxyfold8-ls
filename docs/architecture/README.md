# Architecture & Fork Technical Reference

Developer reference for this **vanilla Pokémon Platinum + Following Pokémon** Android fork (`platinum-thor`). Use these guides to quickly locate hook sites, data structures, and thread-safety rules before modifying any subsystem.

## Custom Fork Features (`docs/architecture/features/`)

| Guide | Covers | Primary Source Files |
| --- | --- | --- |
| [Gen 5 Battles & Summary](features/gen5-battles-and-summary.md) | Embedded BW2 battle engine (`pokebw2`), Pokémon Summary, move-forget screen, `Gen5Battle_Wants` status mask, type offset, and `amend_patch.py` workflow | `gen5/`, `src/port/gen5_battle.c`, `src/port/gen5_summary.c`, `src/port/gen5_data.c`, `src/port/gen5_rom.cpp` |
| [Full-Screen Touch HUD & Gear Companion](features/touch-hud-and-gear-companion.md) | Foldable/phone full-screen layout, top-left DS PiP overlay, 2x2 action/move/target/Yes-No cards, Poké Ball quick-throw carousel, and Gear companion bridge | `src/port/touch_controls.cpp`, `src/port/gear_data.c`, `src/battle/battle_controller_player.c`, `src/battle/battle_subscreen.c` |
| [Following Pokémon & Overworld Wilds](features/following-pokemon-and-overworld-wilds.md) | Following Pokémon C rewrite (`weather_sys.narc` member 65), script object `NULL` guards, and visible roaming/submerged overworld wild Pokémon | `src/port/following_hack.c`, `src/port/follower.c`, `src/port/overworld_wild.c`, `src/scrcmd.c` |
| [Pokédex Radar & Encounter Tools](features/pokedex-radar-and-encounter-tools.md) | *Gotta Catch 'Em All!* encounter table injection, unified Route Encounter & Dex Radar, GBA dual-slot, *No Wild Encounters*, and *Always Bite When Fishing* | `src/port/catch_em_all.c`, `src/port/gear_dex.c`, `src/port/gba_slot.c`, `src/overlay006/wild_encounters.c` |
| [QoL, Portable PC, HMs, Evolution & Events](features/qol-pc-hms-evolution-and-events.md) | Start Menu PC Box, direct field HMs, Universal/Infinite TMs, trade evolutions without trading, Modern EXP Share, Hard Level Cap, timers, clock offset, and Mystery Gifts | `src/port/hm_free.c`, `src/port/gear_pc.c`, `src/port/save_editor.c`, `src/start_menu.c`, `src/pokemon.c`, `src/overlay005/honey_tree.c`, `src/berry_patches.c` |

## Core Game Engine References (`docs/architecture/core/`)

| Guide | Covers | Primary Source Files |
| --- | --- | --- |
| [Encounters & Battle Flow](core/encounters-and-battle-flow.md) | Wild/script/trainer encounter entry, `FieldBattleDTO`, `FieldSystem_StartBattleProcess`, `BattleControllerPlayer`, `BattleSubscreen`, and DTO writeback | `src/encounter.c`, `src/field_battle_data_transfer.c`, `src/unk_0203D1B8.c`, `src/battle/` |
| [Scripts & Map Objects](core/scripts-and-map-objects.md) | Script VM (`ScriptContext`), `src/scrcmd.c` command handlers, `MapObjectManager` 64-object pool, map load/connection hooks, and Android ARM64 `NULL` safety | `src/scrcmd.c`, `src/map_object.c`, `src/field_map_change.c`, `src/overlay005/fieldmap.c` |
| [Save Data, Party, Bag & Threading](core/save-party-bag-and-threading.md) | `FieldSystem`, `SaveData`, `Party`/`Pokemon`, `Bag`, `SIM_Config_prj_type` persistence, and `SDLThread` vs. ImGui UI thread boundaries | `include/port/sim_config_prj.h`, `src/port/sim_config_prj.c`, `src/pokemon.c`, `src/party.c`, `src/bag.c` |

## Related Documentation
- **Player Manual & Historical Notes:** [`docs/thor/README.md`](../thor/README.md) and `docs/thor/dev/`
- **Pret Decompilation Docs:** [`docs/index.md`](../index.md)
