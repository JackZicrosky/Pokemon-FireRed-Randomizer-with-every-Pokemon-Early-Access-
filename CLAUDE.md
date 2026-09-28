# CLAUDE.md — Handoff notes for the "FireRed Expansion Randomizer" romhack

> **Read all of this before doing anything.** These notes came from a previous Claude session.
> It built this whole project with the owner over several days (v0.1 → v0.5.2).
> The owner asked for "an extremely detailed notes file to make the new chat understand EVERYTHING
> we've done … and make absolutely sure it knows my personal preferences and changes."
> Keep this file at the repository root as `CLAUDE.md` so every future session loads it automatically.
> When you finish a version, update the **Version history** and **Current state** sections.

---

## 0. TL;DR for the next session

- **What it is:** a Pokémon **FireRed** romhack built on **pokeemerald-expansion (RHH)** compiled as FireRed.
  It adds all 1025 Pokémon, forms, Megas and Z-Moves, plus a full **in-game randomizer** modelled on the
  **Universal Pokémon Randomizer FVX** (UPR FVX). Randomizing happens at runtime, seeded from settings
  chosen on a dark options screen at New Game.
- **Emerald version comes later.** The owner keeps an "Emerald" folder for it. Do not start it unless asked.
- **The owner (GitHub: JackZicrosky) has little git experience.**
  They said *"I'd rather you handle all the building."* You do everything: code, build, test, patch, commit, push.
- **Every deliverable** is a **BPS patch + README + source bundle**, copied to the owner's PC folder
  `D:\AI shit\Claude\Pokemon Romhack Stuff\Fire Red\`, with versioned file names (see §6).
- **Scope discipline is critical.** When the owner says *"Change nothing else"* or
  *"Do not add or remove anything else"*, do exactly what was asked and nothing more.
- **Test everything** you change: the in-ROM self-test on 3 pools, plus emulator screenshots of the real
  flows (see §5). The owner once asked to "test every single setting to make sure they work", and that
  standard still applies.
- **Latest released version: v0.5.2** (tag `v0.5.2`).

---

## 1. The owner's preferences (MUST follow)

1. **Handle all building yourself.** Never ask the owner to run git, make or emulators. Explain things in
   plain language. When they ask for an explanation "in great detail", give exact step-by-step
   instructions with button names and URLs.
2. **Deliver to the PC folder** `D:\AI shit\Claude\Pokemon Romhack Stuff\Fire Red\`. Their other folder is
   `...\Emerald\`. Use the device bridge tools (`mcp__remote-devices__device_commit_files`, loaded through
   ToolSearch) when the session is linked to their computer. The files are:
   - `FireRed Expansion Randomizer vX.Y.Z.bps`
   - `README vX.Y.Z.txt`
   - `rh_firered_changes vX.Y.Z.bundle`

   Never overwrite older versions; add new files with new version numbers.
3. **GitHub:** the owner's repository is
   `https://github.com/JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-`.
   The owner said: *"We can push/pull or change anything for this project to this spot."*
   Push `main` and version tags there.
   **Never push to `rh-hideout/pokeemerald-expansion`** (the upstream). It's public and not theirs.
   The old session could not push because the repo wasn't attached to that session. A session started with
   the repo selected can push.
4. **Exact scope.** If told "change nothing else", don't slip in extra fixes. If you notice a bug, *tell*
   the owner and ask, or list it as a known issue. The owner cares a lot about this.
5. **Follow UPR FVX behaviour and descriptions.** The owner asked to *"re-check the Universal Pokemon
   Randomizer FVX descriptions for settings to code them properly."* Options should behave like FVX's, and
   menu descriptions should explain what the option really does.
6. **Visual style / colour palette:** the randomizer's dark UI palette is the project's "brand". Reuse it
   for new UI and icons when asked, as with the title menu and the HM Kit icon. The values are in
   `sPal[]` in `src/rh_randomizer_menu.c`:
   - background RGB5(2,3,4)
   - panel (3,4,5)
   - text (28,28,29)
   - shadow (7,8,10)
   - value cyan (12,26,31)
   - dim (13,14,16)
   - header amber (31,22,10)
   - selected row (7,9,13)
   - accent purple (20,13,31)
   - header bar (5,6,9)
   - OK green (10,27,12)
   - warn (31,11,9)
   - **blood red (31,2,3)**, used for "Reset The Run"
7. **Sprites:**
   - Prefer keeping existing sprites. For Mom's walk the owner said *"if you NEED to I give you permission
     to change her sprite but preferably keep it the same."*
   - When the owner supplies pixel art, **follow it pixel-for-pixel**, apply only the recolour they ask
     for, and use **proper shading**.
   - The owner dislikes stray or out-of-place pixels.
   - Item icons must be **24×24, 16 colours** (index 0 transparent).
8. **Descriptions describe function, not appearance.** Example: the Randomizer Settings item says what it
   does, not "a GBA SP…".
9. **Defaults the owner chose** (keep them unless they ask otherwise):
   - Foe "Don't Use Legendaries" = **Off**
   - Wild "Don't Use Legendaries" = **Off**
   - "Lower Case Pokémon Names" = **Off**
   - For the rest see `RH_SetDefaultSettings` in `src/rh_core.c`.
10. **Safety and UX expectations:** dangerous actions get confirmations. "Reset The Run" asks **twice**,
    with the cursor defaulting to **NO** both times, like ORAS/Sun. Menus need an easy way to back out
    without changes.
11. **Commits:**
    - Clear, descriptive messages, one commit per version (e.g. "Randomizer v0.5.2: …").
    - Tag each release (`v0.5.2`, …).
    - Follow the attribution/trailer instructions your own session gives you.
    - The previous session committed as `Claude <noreply@anthropic.com>`.
12. **Communication:** the owner is friendly and direct. They report bugs from real play (they play in
    mGBA on PC). Keep replies clear. Say what changed, what was tested and where the files are.

---

## 2. Repository layout and what is ours

- Base: **rh-hideout/pokeemerald-expansion @ `0a9c697c`** ("Fix multi level move learning (#10816)").
  Built as FireRed with `make firered`.
- On GitHub the history is a **single "Base" snapshot commit** of upstream `0a9c697c`, followed by our
  commits in order. The original clone was shallow, so the history was rebuilt this way.
- All romhack code uses the prefix **`rh_` / `RH_`**:

| File | Purpose |
|---|---|
| `include/rh_settings.h` | `struct RhSettings`, all options (stored in **SaveBlock3** as `gSaveBlock3Ptr->rhSettings`). `RH_SETTINGS_VERSION 2`. Add new fields by **carving bytes out of `reserved[]`** (6 bytes left) so old saves keep loading. |
| `include/rh.h` | Public hooks called from vanilla code (wild levels, catch rate, trainer mons, statics, trades, prices, HM Kit, move data, base stats, TM text, new game/continue, open settings…). |
| `include/rh_internal.h` | Internal helpers for the rh_*.c files. `#define S (&gSaveBlock3Ptr->rhSettings)`; the menu redefines `S` as the pending settings. |
| `src/rh_core.c` | Settings defaults, `RH_ApplyPendingSettings`, seeded hashing (`RH_Hash`, permutations), species pool, species picking with filters, BST ranking, legal-evolution-at-level. |
| `src/rh_traits.c` | Base stats / BST modes, EXP curves, types, abilities, evolutions (incl. FVX **No Convergence** global table, estimated evo levels, adjust levels), catch rates, "follow Mega/forms". |
| `src/rh_typechart.c` | Type-effectiveness options (port of FVX TypeEffectivenessRandomizer) and Battle Mechanics generation. |
| `src/rh_moves.c` | Move data (power/acc/PP/type/category, FVX-style random move **names** with context word lists), movesets, TMs/HMs, tutors, good-damaging %, field-move TMs kept, TM description text fixups. |
| `src/rh_sources.c` | Starters (custom/random, type rules, BST limits), statics & gifts, in-game trades, wild encounters (zones, 1-to-1, themes, catch-em-all, megas…). |
| `src/rh_trainers.c` | Foe Pokémon: teams, themes, league-unique, rival keeps starter, **Rival Keeps Same Team** roster, evolve by level, extra Pokémon, held items, battle style, names. |
| `src/rh_items.c` | Field items, shops, **special shops** (evolution seller in every Mart, herb shop, Celadon counters), pickup, prices. |
| `src/rh_shops.c` | Progressive (badge-gated) Poké Mart stock. `data/scripts/rh_shops.inc` holds the shop scripts. |
| `src/rh_safari.c` | Safari Zone generation selector (Kanto / Gen 2–9 / All). |
| `src/rh_palettes.c` | Random Pokémon palettes (hue shift, follow types/evos, shiny-from-normal). |
| `src/rh_player_graphics.c` | Custom Player Graphics packs converted from UPR FVX (Ethan, Kris, Red, Leaf, Brendan, May, Wally, Birch, Cynthia…). |
| `src/rh_misc.c` | Misc tweaks, `RH_OnNewGame` (gives key items), `RH_OnContinue`, HM Kit logic, intro species, test setups (`RH_TestSetup`, `RH_TestSetupLight`, `RH_TestMenuPreset`, debug-only). |
| `src/rh_randomizer_menu.c` | The **options screen** (≈2200 lines): rows table, drawing, input, text entry via the naming screen, **settings codes**, **presets**, **in-game mode**, **Reset The Run**. |
| `src/rh_selftest.c` | Debug-only in-ROM **self-test** (≈80+ test cases). Never in RELEASE. |
| `data/scripts/rh_misc.inc` | Randomizer Settings item script + warning text, Mom gives rings scene, Safari scientist, etc. |
| `tools/rh/*.py` | Generators for tables: randomizer tables, gen data, names/word lists, evo levels, prices, shops, safari, player graphics. |
| `src/data/rh_*.h` | Generated data (tables, names, evo levels…). |

Vanilla files we hook (search for `RH_` / `rh` in them): `pokemon.c` (most hooks), `battle_*`, `new_game.c`,
`trade.c`, `item_use.c`, `scrcmd.c`, `oak_speech.c`, `wild_encounter.c`, `trainer_util.c`, `daycare.c`,
`item.c`, `roamer.c`, `naming_screen.c` (`NAMING_SCREEN_RH_TEXT`), `main_menu.c`, `overworld.c`
(`RH_OnContinue`), `field_message_box.c`, `pokedex_area_screen.c`, `trainer_card.c`, `battle_setup.c`,
`item_menu.c`.

Romhack vars (in `include/constants/vars.h`):
- `VAR_RH_MOM_GAVE_RINGS` 0x40F7
- `VAR_RH_SAFARI_GEN` 0x40F8

Romhack items (in `include/constants/items.h`, after the Mega Stones):
- `ITEM_INFINITE_CANDY` 874
- `ITEM_HEALING_KIT` 875
- `ITEM_HM_KIT` 876
- `ITEM_RANDOMIZER_SETTINGS` 877

---

## 3. Features (everything the romhack does), by version

### Base game changes (v0.1)
- All 1025 Pokémon + regional/alt forms, Mega Evolution, Z-Moves, modern moves/abilities/items,
  Physical/Special split, Fairy.
- **Mom gives the Mega Ring and Z-Power Ring** the first time you come downstairs. The scene: "!", she
  walks over, faces you, talks. It's a map_script_2 on `VAR_RH_MOM_GAVE_RINGS`.
- **Shops:**
  - an **evolution-item seller in every Poké Mart** (plus Indigo Plateau, Trainer Tower, Celadon 4F);
  - badge-gated clerk stock;
  - town-themed held items;
  - **Lavender herb shop** (herbs, remedies, all 21 Mints);
  - **Celadon Dept. Store**: 2F battle gear, 3F form items, 4F all Mega Stones + Z-Crystals, 5F Ability
    Capsule/Patch/Bottle Caps/Rare Candy, roof berry + snack stands.
- **Safari Zone** generation scientist (left counter of the entrance): Classic Kanto, Gen 2…Gen 9, or All.
  Areas are type-themed; water types go on water; about 1/16 legendaries.

### Randomizer (v0.1–v0.2; FVX option set)
- **New Game → dark options screen before Prof. Oak.** The FRLG controls/story pages before Oak are
  skipped; Oak's talk is kept.
- **Controls:**
  - SELECT = section menu (hamburger)
  - L/R = previous/next section
  - UP/DOWN = option
  - LEFT/RIGHT = change
  - A = toggle / next choice / type text
  - START = Begin Run (asks YES/NO)
- **Sections:** General, Pokémon Traits, Type Effectiveness, Starters/Statics/Trades, Moves & Movesets,
  Foe Pokémon, Wild Pokémon, TM/HMs & Tutors, Items, Custom Player Graphics, Misc. Tweaks,
  **Settings Codes & Presets** (v0.4), Begin Run. The full row list is in the Appendix.
- The Seed is typed like a nickname (any text, ≤10 chars). The same seed + same options gives the same
  game. The default seed is 8 random chars from `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`.
- **Key items:**
  - **HM Kit**: everyone gets it. It counts as every HM your party could use, once you own that HM and
    have its badge. Use it from the bag for Fly/Flash, or walk up to trees, rocks or water.
  - **Nuzlocke Mode** gives **Infinite Candy** (reusable Rare Candy) and **Healing Kit** (full party heal
    anywhere).
- **Permanent wild Megas:** a wild Mega stays Mega after you catch it.
- **Custom Player Graphics packs** from FVX (credits in the README).

### v0.3 — "test every single setting" + FVX re-check
- The owner reported that **Cynthia didn't show when picking Boy**. Fix: "Character to Replace" is now
  "Boy & Girl" (default), "Boy Only" or "Girl Only".
- They also reported that **Instantaneous Text didn't work**. Cause: A didn't toggle choice rows. Now A
  toggles On/Off and advances choices.
- Every option was re-checked against FVX source and descriptions.
- New FVX options:
  - Base Stat Totals (Buff/Nerf %, Shuffle, Random, Follow Evos, Separate Legendaries)
  - Follow Mega Evolutions ×3
  - Adjust Evo Levels
  - No Premature Evolutions
  - Random Intro Pokémon
  - Pokémon Palettes
- Global No-Convergence table, wild 1-to-1 zone fixes, and the in-ROM self-test suite.

### v0.4 — the owner's 10-item list (verbatim intent)
1. Foe "Don't use legendaries" → **off by default**.
2. New option **"Rival Keeps Same Team"** (Foe Pokémon):
   - one roster of basic Pokémon for the whole game; his starter keeps its own slot;
   - every member evolves as far as his level allows, into the same branch every battle;
   - verified by a self-test on every rival battle.
3. Wild "Don't use legendaries" → **off by default**.
4. "Lower Case Pokémon Names" → **off by default**.
5. **Pallet Town start-menu tutorial lady disappears after you pick your starter.** Her object uses flag
   `FLAG_PALLET_LADY_NOT_BLOCKING_SIGN`.
6. **No Prof. Oak battle tutorial in the first rival battle.** It's a normal battle; losing still doesn't
   black out.
7. **Bag glitch fixed:** a half-drawn text line flashed when using the Healing Kit/HM Kit. The fix clears
   the description window before item messages (`item_menu.c`).
8. **Randomize Special Shops** also randomizes the **evolution seller in every Mart**. With "Guarantee
   Evolution Items", every evolution item is still sold exactly once somewhere in the special shops.
9. **Mom walks with real walking frames.** Her FRLG sprite (`mom_frlg.png`) had only 3 static frames; the
   old session added 6 walking frames with a leg change and a 1px bob. Same sprite.
10. **Settings export/import.** A ROM can't write files to the PC, so it's done two ways:
    - **Settings Code**: show the code, or type one in on a character grid. The format:
      - alphabet `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`, 5 bits per char, shown in groups of 5;
      - 3-bit version (`CODE_VERSION 1`);
      - seed as text (if made of code chars) or a raw 32-bit seed;
      - rows either **full** (every row in `sRows[]` order) or **sparse** (only rows that differ from the
        defaults), whichever is shorter;
      - custom starters / type / BST only if they aren't default;
      - a 10-bit checksum;
      - a typical code is 15–30 chars.
    - **3 Presets** stored in **flash sector 30** (the Trainer Hill sector, unused in FireRed), with magic
      `"RHPS"`. They are independent of the game save and survive new games.
    - Also **Reset to Defaults**, which keeps the seed.

### v0.5 — Randomizer Settings key item, Reset The Run, title menu
- **Randomizer Settings key item** (ITEM 877, GBA SP icon):
  - Everyone gets it at new game; older saves get it on Continue (`RH_OnContinue`).
  - Using it runs `RH_EventScript_RandomizerSettings`: a warning ("changing settings mid-run can BREAK your
    current playthrough…"), then "Open the RANDOMIZER SETTINGS?" YES/NO, then
    `special RH_OpenRandomizerSettings` + `waitstate`.
  - The menu opens in **in-game mode** (`sInGame`):
    - pending settings are copied from the save;
    - the Begin section becomes "Apply or Exit", with "▶ APPLY & RETURN" and "Exit Without Changes";
    - **B** asks "Leave without changing anything?" (default YES);
    - Apply calls `RH_ApplyPendingSettings`; the player must save the game to keep the changes;
    - the menu returns via `CB2_ReturnToFieldContinueScriptPlayMapMusic`.
  - It can be registered to SELECT.
- **Reset The Run**: a blood-red first row in General.
  - Visible only when a save exists (`gSaveFileStatus` OK/CORRUPT) **or** in in-game mode.
  - The cursor starts on the row **below** it.
  - Two confirmations, both defaulting to NO:
    - "Reset the run? Your save gets ERASED."
    - "Are you REALLY sure? No undo!"
  - Then it erases flash sectors **0–29** (both save slots + Hall of Fame; **not** sector 30 presets), sets
    `gSaveFileStatus = EMPTY`, and starts Oak's intro with the on-screen settings.
  - In-game, it resets the heap/globals first (`Task_ResetRunInGame`).
  - Purpose (the owner's words): reset the run while keeping randomizer options, instead of deleting the
    `.sav`, which would also delete presets.
- **Title menu restyle** (`main_menu.c`):
  - randomizer dark colours;
  - **"NEW GAME" → "RANDOMIZE GAME"** in cyan; "CONTINUE" unchanged;
  - labels centred; the stack is vertically centred via a BG0 Y-offset (48px with no save, 16px with a
    save, 0 for the mystery gift/event menus) plus the matching WIN0V highlight;
  - fades in from black.

### v0.5.1
- Randomizer Settings description now says what it does: "Opens randomizer / settings to change / or reset
  your run."
- **Infinite Candy icon** = rainbow Rare Candy, a diagonal pink→orange→yellow→green→sky-blue gradient
  (`rh_rainbow_candy.png/.pal`).
- **HM Kit icon** = the TM Case shape recoloured to the randomizer palette (purple body, cyan disc,
  `rh_hm_kit.pal`), so it differs from the TM Case.

### v0.5.2
- **Healing Kit icon** = the owner's own drawing (32×32, cropped to 24×24 by removing the one-pixel purple
  handle line).
  - The owner asked: **plus symbol red, outside white, with proper shading.**
  - White case with a bright top-left, grey bottom/right shading, red cross with a light-red highlight and
    dark-red shading, dark outline, grey handle.
  - Files: `rh_medkit.png/.pal`.

---

## 4. Building

- Toolchain: `arm-none-eabi-gcc` (the previous environment had 13.2.1 from Ubuntu `gcc-arm-none-eabi`),
  `make`, `python3` + Pillow, `libpng-dev`.
- **Debug/test build:** `make firered -j8` → `pokefirered.gba` and `pokefirered.elf`.
- **Release build:** `make firered RELEASE=1 -j8` → `pokefirered-release.gba`. Asserts and test hooks are
  off; this is what gets patched.
- **EWRAM is tight: 97.97% in release.** The debug build is higher.
  - Avoid new large `EWRAM_DATA`.
  - Use `Alloc`/`Free` for temporary buffers (the heap is reset by `InitHeap` on load, so nothing
    persistent can live there).
- Changing `include/rh_settings.h` rebuilds almost everything, which is slow.
- `include/config/rh_test.h` holds the test hooks.
  - The committed version contains self-test defines. That's harmless: they're ignored in RELEASE.
  - Edit it freely for tests, then **`git checkout include/config/rh_test.h` before committing or doing
    release builds.**
  - Hooks:
    - `RH_TEST_MAP/X/Y/BADGES` = quickstart new game warps straight to a map
      (e.g. `MAP_VIRIDIAN_CITY`, 20, 20).
    - `RH_TEST_EXTRA` = code run at new game, e.g.:
      - `RH_TestSetupLight()` gives a Bulbasaur + Pidgey, key items and **instant text**;
      - `RH_SelfTest()` runs the self-test.
    - `RH_SELFTEST_POOL` 0 = All 1025, 1 = Kanto 151, 2 = All + Forms.
    - `RH_SELFTEST_NEW_ONLY` runs only the newest tests.
    - `RH_TEST_MENU_PRESET` presets the menu.

## 5. Testing (the owner expects real testing)

### 5a. In-ROM self-test
- Set `rh_test.h` to `RH_TEST_EXTRA RH_SelfTest()` with pool 0, then 1, then 2 (a full run, not
  NEW_ONLY), and build the debug ROM.
- Run it in the headless emulator. It writes ASCII to `char gRhSelfTestLog[5120]` (EWRAM) and sets
  `gRhSelfTestDone` to `0x00c0ffee` when done. Read both with the harness (see `run_st.sh` in the tools).
- Expect `TOTAL FAILS 0` on all three pools. It takes about 15–25k frames.
- The log is 5KB, so early lines may scroll off; `TOTAL FAILS` is authoritative.
- Add a self-test for every new rule (see existing tests such as `TestRivalSameTeam`,
  `TestSettingsCodes`, `TestSpecialShops`, `TestNewGameItems`).

### 5b. Emulator screenshots (headless mGBA harness)
- The tools are in `rh_test_tools.zip`. Put them in `tools/rh/test/` if the owner agrees.
  - `harness.c`: build with `gcc -O2 harness.c -o harness -lmgba -lpng`, after
    `apt install libmgba-dev libpng-dev`.
  - Usage: `./harness ROM script.txt [savefile.sav]`.
  - Script commands:
    - `wait N`
    - `press KEYS N` (hold N frames, then release plus 6 frames)
    - `shot file.png`
    - `savestate f` / `loadstate f`
    - `peek32 ADDR`, `peek16 ADDR`, `dumpstr ADDR LEN`, `dump ADDR|*PTR LEN FILE`
    - `repeat KEYS count`
  - **Keys:**
    - `A` `B`
    - `S` = START, `s` = SELECT
    - `U D L R` = D-pad
    - **`l` `r` = shoulder buttons** (easy to mix up with the D-pad `L`/`R`)
  - `grid.py out.png a.png b.png …` makes a 3-column contact sheet at 2× to view many shots at once.
  - The savefile argument gives a persistent 128K flash `.sav`. Use it for Continue / Reset The Run /
    preset tests.
- **Gotchas:**
  - Savestates are invalid after every rebuild.
  - Text speed makes timing fragile. Use `RH_TestSetupLight` (instant text), or add long waits and take
    shots every step.
  - Loading a savestate taken mid-transition can land a few frames early; prefer one continuous script
    from a known point.
  - `head -7 boot_rt1.txt` boots to the field in quickstart builds. Then `wait 300` before input.
  - For the title → main menu: `wait 900`, `press S 4`, `wait 300`, `press A 4`, `wait 300`.
- **Navigation facts:**
  - The quickstart start menu has BAG first.
  - From the ITEMS pocket, **D-pad Left** goes to KEY ITEMS.
  - Key items order in a fresh game: HM Kit, Randomizer Settings, Infinite Candy, Healing Kit.
- **Look at the screenshots yourself** (Read the PNG) before claiming something works.

### 5c. Release checklist (every version)
1. `git checkout include/config/rh_test.h`, then `make firered RELEASE=1 -j8`.
2. Create the BPS against the clean ROM:
   - ROM: **"Pokemon FireRed (USA) v1.0.gba", SHA-1 `dd5945db9b930750cb39d00c84da8571feebf417`**. The
     owner has it; ask them to attach it if you don't have it.
   - Tool: Flips (`https://github.com/Alcaro/Flips`, build with `make` or `./make-linux.sh`):
     `flips --create --bps clean.gba pokefirered-release.gba "FireRed Expansion Randomizer vX.bps"`.
   - Verify: `flips --apply` onto the clean ROM, then `cmp` with `pokefirered-release.gba`. It must be
     identical.
3. Write the README: copy the previous one, bump the version, and add a "WHAT CHANGED IN vX" section at
   the top. Keep "HOW TO PLAY", the feature list, "KNOWN LIMITATIONS" and credits.
4. Commit with a descriptive message, then tag `vX`.
5. Source bundle: `git bundle create rh_firered_changes.bundle <base>..vX`. Base = the "Base" snapshot
   commit on GitHub (formerly `aa3ac162~1`).
6. Copy the three files to `D:\AI shit\Claude\Pokemon Romhack Stuff\Fire Red\` with versioned names.
7. Push `main` + tags to the owner's GitHub.
8. Tell the owner, in plain language, what changed, what was tested and where the files are.

---

## 6. Important design rules / pitfalls learned

- **Settings codes depend on the order and ranges of `sRows[]`.**
  - Adding, removing or reordering option rows, or changing a row's range, **breaks old codes and
    presets**.
  - When you change rows, bump `CODE_VERSION` in `rh_randomizer_menu.c`, and tell the owner that old
    codes/presets won't load. The README already warns about this.
  - The self-test `TestSettingsCodes` round-trips random settings.
- **RhSettings layout:** only take new bytes from `reserved[]` (6 left). Keep `RH_SETTINGS_VERSION 2` so
  older saves load.
- **Mid-run settings changes** (key item) don't re-run new-game-only effects: Nuzlocke items, National Dex
  at start, random PC potion. This is documented as a known limitation.
- `min()`/`max()` are macros that **evaluate their arguments twice**. Never write
  `min(GetBits(...), x)`. This caused a real bug.
- `RH_Hash(salt, a, b)` + `RH_Permute` give deterministic, seed-dependent picks. Many results are cached
  keyed on `RH_SettingsHash()`; call `RH_InvalidateSettingsHash()` after changing settings in tests.
- Flash sectors:
  - 0–27 = two save slots;
  - 28–29 = Hall of Fame;
  - **30 = our presets** (`TryRead/WriteSpecialSaveSector(SECTOR_ID_TRAINER_HILL)`);
  - 31 = recorded battle.
- The in-game menu relies on `CleanupOverworldWindowsAndTilemaps`, and on the script's
  `fadescreen` + `waitstate`. Its return path is `CB2_ReturnToFieldContinueScriptPlayMapMusic`.
- Item icons: `graphics/items/icons/*.png` + `graphics/items/icon_palettes/*.pal` (JASC, CRLF), declared
  in `src/data/graphics/items.h` + `include/graphics.h`, referenced in `src/data/items.h`.
- The trainer card now ignores invalid icon species values (a v0.4 fix).
- The UPR FVX source was used as the reference for option behaviour. It's a public GitHub project
  (Universal Pokemon Randomizer FVX); re-clone it if you need it.

## 7. Known limitations / open items (as of v0.5.2)
- Some NPC dialogue still names the original Pokémon.
- New-game-only settings don't apply when changed mid-run with the key item.
- Codes/presets may not load after option rows change (see §6).
- The Emerald version is not started.
- The owner may continue reporting bugs from play; fix exactly what they report and ask before extra
  changes.

## 8. Version history (tags)
| Tag | Summary |
|---|---|
| (pre) | Shops (evo seller, Celadon, herbs), Mom rings; Safari generation selector |
| v0.1 | In-game randomizer at New Game (first FVX-style option set) |
| v0.2 | Dark hamburger menu (FVX tabs), mechanics gen, type chart, HM Kit + Nuzlocke items, player graphics, permanent wild Megas |
| v0.3 | Every option re-checked vs FVX; A toggles; new FVX options; self-test suite |
| v0.4 | Owner's 10 items: legend defaults off, Rival Keeps Same Team, lower-case off, Pallet lady, no Oak tutorial, bag glitch, evo seller randomized, Mom walking, settings codes + presets |
| v0.5 | Randomizer Settings key item, Reset The Run, dark centred "RANDOMIZE GAME" title menu |
| v0.5.1 | Item description; rainbow Infinite Candy; randomizer-palette HM Kit icon |
| v0.5.2 | Healing Kit icon from the owner's drawing (white case, red cross) |

---

## Appendix — every option row in the settings screen (label → field)

```
[SEC_GENERAL]
  - Reset The Run  <ACTION, blood red, only with a save / in-game>
  - Battle Mechanics  (mechanicsGen)
  - Randomizer  (enabled)
  - Pokémon Pool  (speciesPool)
  - Seed  <TEXT>
  - Nuzlocke Mode  (nuzlocke)
  - No Premature Evolutions  (noPrematureEvos)
  - Random Intro Pokémon  (randomIntroMon)
[SEC_TRAITS]
  ## Pokémon Base Statistics
  - Base Stats  (baseStats)
  - Follow Evolutions  (baseStatsFollowEvos)
  - Rand. Added Stats on Evo  (baseStatsRandomAdded)
  - Follow Mega Evolutions  (statsFollowMegas)
  - Base Stat Totals  (bstMode)
  - Maximum Change  (bstChangePct)
  - Follow Evolutions  (bstFollowEvos)
  - Separate Legendaries  (bstSeparateLegends)
  - Update Base Stats to Gen  (updateBaseStatsGen)
  - Standardize EXP Curves  (expCurve)
  - Applies To  (expCurveWho)
  ## Pokémon Types
  - Types  (types)
  - Force Dual Types  (forceDualTypes)
  - Follow Mega Evolutions  (typesFollowMegas)
  ## Pokémon Abilities
  - Abilities  (abilities)
  - Allow Wonder Guard  (allowWonderGuard)
  - Combine Duplicate Abil.  (combineDuplicateAbilities)
  - Ensure Two Abilities  (ensureTwoAbilities)
  - Follow Evolutions  (abilitiesFollowEvos)
  - Follow Mega Evolutions  (abilitiesFollowMegas)
  - Ban Trapping Abilities  (banTrapAbilities)
  - Ban Negative Abilities  (banNegativeAbilities)
  - Ban Bad Abilities  (banBadAbilities)
  ## Pokémon Evolutions
  - Evolutions  (evolutions)
  - Similar Strength  (evoSimilarStrength)
  - Same Typing  (evoSameTyping)
  - Limit to Three Stages  (evoLimitThreeStages)
  - No Convergence  (evoNoConvergence)
  - Force Change  (evoForceChange)
  - Force Growth  (evoForceGrowth)
  - Change Impossible Evos  (evoChangeImpossible)
  - Make Evolutions Easier  (evoMakeEasier)
  - Use Estimated Evo Levels  (evoEstimatedLevels)
  - Remove Time-Based Evos  (evoRemoveTimeBased)
  - Adjust Evolution Levels  (evoAdjustLevels)

[SEC_TYPES]
  - Type Effectiveness  (typeChart)
  - Add Random Immunities  (inverseRandomImmunities)
  - Update Type Effectiveness  (updateTypeChart)

[SEC_STARTERS]
  ## Starter Pokémon
  - Starters  (starters)
  - Starter 1  <TEXT>
  - Starter 2  <TEXT>
  - Starter 3  <TEXT>
  - Allow Alternate Formes  (starterAllowAltFormes)
  - Don't Use Legendaries  (starterNoLegends)
  - Random Starter Held Items  (starterHeldItems)
  - Ban Bad Items  (starterBanBadItems)
  - Limit BST: Minimum  (starterBstMinOn)
  - Minimum BST  <TEXT>
  - Limit BST: Maximum  (starterBstMaxOn)
  - Maximum BST  <TEXT>
  - Type Restrictions  (starterTypes)
  - Single Type  <TEXT>
  - No Dual Types  (starterNoDualTypes)
  ## Static Pokémon
  - Static Pokémon  (statics)
  - Randomize 600+ BST  (staticRandomize600)
  - Allow Alternate Formes  (staticAllowAltFormes)
  - Fix Music  (staticFixMusic)
  - Static Level Modifier  (staticLevelModOn)
  - Level Change  (staticLevelMod)
  ## In-Game Trades
  - In-Game Trades  (trades)
  - Randomize Nicknames  (tradeNicknames)
  - Randomize OTs  (tradeOTs)
  - Randomize IVs  (tradeIVs)
  - Randomize Items  (tradeItems)

[SEC_MOVES]
  ## Move Data
  - Randomize Move Power  (movePower)
  - Randomize Move Accuracy  (moveAccuracy)
  - Randomize Move PP  (movePP)
  - Randomize Move Types  (moveType)
  - Randomize Move Category  (moveCategory)
  - Randomize Move Names  (moveNames)
  - Update Moves to Gen  (updateMovesGen)
  ## Pokémon Movesets
  - Movesets  (movesets)
  - Guaranteed Level 1 Moves  (guaranteedLevel1On)
  - Moves at Level 1  (guaranteedLevel1Moves)
  - Reorder Damaging Moves  (reorderDamagingMoves)
  - Evolution Moves for All  (evolutionMovesForAll)
  - No Game-Breaking Moves  (movesetNoGameBreaking)
  - Force % Good Damaging  (movesetGoodDamagingOn)
  - Good Damaging Moves  (movesetGoodDamaging)

[SEC_FOES]
  - Trainer Pokémon  (trainers)
  ## Better Movesets for...
  - Boss Trainers  (betterMovesets[0])
  - Important Trainers  (betterMovesets[1])
  - Regular Trainers  (betterMovesets[2])
  ## Additional Pokémon for...
  - Boss Trainers  (additionalMons[0])
  - Important Trainers  (additionalMons[1])
  - Regular Trainers  (additionalMons[2])
  ## Add Held Items to...
  - Boss Trainers  (heldItemsFor[0])
  - Important Trainers  (heldItemsFor[1])
  - Regular Trainers  (heldItemsFor[2])
  - Consumable Only  (heldConsumableOnly)
  - Sensible Items  (heldSensible)
  - Highest Level Only  (heldHighestOnly)
  ## Force Diverse Types for...
  - Boss Trainers  (diverseTypes[0])
  - Important Trainers  (diverseTypes[1])
  - Regular Trainers  (diverseTypes[2])
  ## Battle Style
  - Battle Style  (battleStyle)
  - Style  (battleStyleDoubles)
  ## Other
  - Rival Carries Starter  (rivalCarriesTeam)
  - Rival Keeps Same Team  (rivalSameTeam)
  - Similar Strength  (trainerSimilarStrength)
  - Try to Avoid Duplicates  (trainerAvoidDuplicates)
  - Weight Types by Count  (trainerWeightTypes)
  - Use Local Pokémon  (trainerLocalPokemon)
  - Allow Alternate Formes  (trainerAllowAltFormes)
  - Random Shiny Trainer Mons  (trainerRandomShiny)
  - Don't Use Legendaries  (trainerNoLegends)
  - No Early Wonder Guard  (noEarlyWonderGuard)
  - League Unique Pokémon  (leagueUnique)
  - Randomize Trainer Names  (randomTrainerNames)
  - Random Trainer Classes  (randomTrainerClassNames)
  - Trainers Evolve Pokémon  (trainersEvolveOn)
  - Fully Evolved By  (trainersEvolveLevel)
  - Percentage Level Modifier  (trainerLevelModOn)
  - Level Change  (trainerLevelMod)

[SEC_WILD]
  - Randomize Wild Pokémon  (wild)
  ## Replacements Per Species
  - Replacements  (wildZone)
  - Split by Encounter Types  (wildSplitEncounterTypes)
  ## Type Restrictions
  - Types  (wildTypeRestriction)
  - Keep Set/Zone Themes  (wildKeepThemes)
  ## Evolution Restrictions
  - Evolutions  (wildEvoRestriction)
  - Keep Relations  (wildKeepRelations)
  ## Other
  - Allow Alternate Formes  (wildAllowAltFormes)
  - Don't Use Legendaries  (wildNoLegends)
  - Set Minimum Catch Rate  (wildCatchRateOn)
  - Catch Rate Level  (wildCatchRate)
  - Randomize Held Items  (wildHeldItems)
  - Ban Bad Items  (wildBanBadItems)
  - Catch Em' All Mode  (wildCatchEmAll)
  - Similar Strength  (wildSimilarStrength)
  - Balance Low Level  (wildBalanceLowLevel)
  - Permanent Mega Pokémon  (wildMegas)
  - Percentage Level Modifier  (wildLevelModOn)
  - Level Change  (wildLevelMod)

[SEC_TMS]
  ## TMs & HMs
  - TM Moves  (tmMoves)
  - No Game-Breaking Moves  (tmNoGameBreaking)
  - Keep Field Move TMs  (tmKeepFieldMoves)
  - Force % Good Damaging  (tmGoodDamagingOn)
  - Good Damaging Moves  (tmGoodDamaging)
  - TM/HM Compat.  (tmCompat)
  - TM/Levelup Move Sanity  (tmLevelupSanity)
  - Follow Evolutions  (tmCompatFollowEvos)
  - Full HM Compatibility  (fullHMCompat)
  ## Move Tutors
  - Move Tutor Moves  (tutorMoves)
  - No Game-Breaking Moves  (tutorNoGameBreaking)
  - Force % Good Damaging  (tutorGoodDamagingOn)
  - Good Damaging Moves  (tutorGoodDamaging)
  - Tutor Compat.  (tutorCompat)
  - Tutor/Levelup Move Sanity  (tutorLevelupSanity)
  - Follow Evolutions  (tutorCompatFollowEvos)

[SEC_ITEMS]
  - Field Items  (fieldItems)
  - Ban Bad Items  (fieldBanBad)
  - Shop Items  (shopItems)
  - Ban Bad Items  (shopBanBad)
  - Ban Regular Shop Items  (shopBanRegular)
  - Ban Overpowered Items  (shopBanOverpowered)
  - Guarantee Evolution Items  (shopGuaranteeEvo)
  - Guarantee X Items  (shopGuaranteeX)
  - Balance Shop Prices  (shopBalancePrices)
  - Add Cheap Rare Candies  (shopAddCheapRareCandy)
  - Randomize Special Shops  (shopSpecial)
  - Pickup Items  (pickupItems)
  - Ban Bad Items  (pickupBanBad)
  - Player Character  <custom row>

[SEC_GRAPHICS]
  - Character to Replace  (playerGraphicsReplace)
  ## Pokémon Palettes
  - Pokémon Palettes  (paletteMode)
  - Follow Types  (paletteFollowTypes)
  - Follow Evolutions  (paletteFollowEvos)
  - Shiny From Normal  (paletteShinyFromNormal)

[SEC_MISC]
  - Instantaneous Text  (instantText)
  - Running Shoes Indoors  (runIndoors)
  - Randomize PC Potion  (randomPcPotion)
  - National Dex at Start  (nationalDexAtStart)
  - Fast Egg Hatching  (fastEggs)
  - Lower Case Pokémon Names  (lowerCaseNames)
  - Random Catching Tutorial  (randomCatchTutorial)
  - Ban Lucky Egg  (banLuckyEgg)
  - Balance Static Levels  (balanceStaticLevels)
  - Run Without Running Shoes  (runWithoutShoes)
  - Infinitely Reusable TMs  (reusableTMs)
  - Forgettable HMs  (forgettableTMs)
  - No EVs From Pokémon  (noEVs)

[SEC_CODES]
  ## Settings Code
  - Show Settings Code  <ACTION>
  - Enter Settings Code  <ACTION>
  ## Presets (kept on this cartridge)
  - Save as Preset 1  <ACTION>
  - Save as Preset 2  <ACTION>
  - Save as Preset 3  <ACTION>
  - Load Preset 1  <ACTION>
  - Load Preset 2  <ACTION>
  - Load Preset 3  <ACTION>
  ## Other
  - Reset to Defaults  <ACTION>

[SEC_BEGIN]
  - ▶ BEGIN RUN  (in-game mode: "▶ APPLY & RETURN")  <ACTION>
  - Exit Without Changes  <ACTION, in-game mode only>
```
