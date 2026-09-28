# FireRed Expansion Randomizer

A Pokémon **FireRed** romhack with **all 1025 Pokémon**, their forms, Mega Evolution and Z-Moves, plus a
full **in-game randomizer** based on the
[Universal Pokémon Randomizer FVX](https://github.com/upr-fvx/universal-pokemon-randomizer-fvx).
You don't need a PC tool to randomize: pick your options on a dark settings screen when you start a
new game, and the game is randomized as you play.

**Current version: v0.5.2 (early access / test build).**

It is built on [RHH's pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion),
compiled as FireRed.

---

## Download

**[⬇ Download the latest version](https://github.com/JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-/releases/latest)**
(recommended). Every older version is also available on the
[Releases page](https://github.com/JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-/releases)
if you'd like to play one of those instead.

## How to play

1. Get a clean **Pokémon FireRed (USA) v1.0** ROM that you dumped yourself
   (SHA-1 `dd5945db9b930750cb39d00c84da8571feebf417`). No ROMs are provided here.
2. Apply the `FireRed Expansion Randomizer vX.Y.Z.bps` patch from the release to it. The easiest way is
   [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/): pick the ROM, pick the `.bps`, then
   click **Apply patch**. Floating IPS (Flips) also works.
3. Play the patched `.gba` in **mGBA** (recommended). Leave the save type on auto / Flash 128K.
4. Choose **RANDOMIZE GAME** on the title screen, set your options, then press **START** to begin.

Saves from v0.3 onward keep loading in newer versions.

---

## Features

### Base game
- All 1025 Pokémon plus regional and alternate forms, Mega Evolution, Z-Moves, and modern moves,
  abilities and items. Uses the Physical/Special split and includes the Fairy type.
- **Mom gives you the Mega Ring and Z-Power Ring** the first time you come downstairs.
- **Shops:**
  - an evolution-item seller in every Poké Mart;
  - stock that grows with your badges;
  - town-themed held items;
  - a herb shop in Lavender Town (herbs, remedies and all 21 Mints);
  - an expanded Celadon Dept. Store: battle gear, form items, every Mega Stone and Z-Crystal,
    Ability Capsules/Patches, Bottle Caps and more.
- **Safari Zone generation selector:** talk to the scientist at the entrance to pick Classic Kanto,
  Gen 2 to Gen 9, or All. Each area is type-themed.

### The randomizer
Choosing **RANDOMIZE GAME** opens the options screen before Prof. Oak's speech. Every option shows a
description at the bottom of the screen. The same seed and options always give the same game.

**Controls:**

| Button | Action |
|---|---|
| SELECT | Section menu |
| L / R | Previous / next section |
| UP / DOWN | Choose an option |
| LEFT / RIGHT | Change it |
| A | Toggle, next choice, or type text |
| START | Begin the run |

**Sections:**

| Section | What it covers |
|---|---|
| General | Battle mechanics generation, Pokémon pool, seed, Nuzlocke Mode, no premature evolutions, random intro Pokémon, **Reset The Run** |
| Pokémon Traits | Base stats and stat totals, EXP curves, types, abilities, evolutions |
| Type Effectiveness | Random, balanced, keep type identities, inverse, random immunities, Gen 9 chart |
| Starters, Statics & Trades | Custom or random starters (BST and type rules), static and gift Pokémon, in-game trades |
| Moves & Movesets | Move power/accuracy/PP/type/category, random move names, movesets |
| Foe Pokémon | Trainer teams and themes, league-unique Pokémon, rival keeps his starter or the same team, extra Pokémon, held items, double battles, trainer names and classes, level changes |
| Wild Pokémon | Random, area 1-to-1 or global 1-to-1, type themes, catch-'em-all, similar strength, catch rate, held items, **permanent wild Megas** |
| TM/HMs & Tutors | TM and tutor moves and compatibility |
| Items | Field items, shops, special shops, prices, pickup |
| Custom Player Graphics | Play as Ethan, Kris, Red, Leaf, Brendan, May, Wally, Prof. Birch, Cynthia and more |
| Misc. Tweaks | Instant text, running indoors, fast eggs, reusable TMs, forgettable HMs, no EVs, and more |
| Settings Codes & Presets | Share your whole setup as a short code, save 3 presets on the cartridge, reset to defaults |

### Key items
- **HM Kit** (everyone gets it): acts as every HM your party could learn, once you own that HM and have
  its badge. Use it from the Bag for Fly and Flash, or just walk up to the tree, rock or water.
- **Randomizer Settings** (everyone gets it): reopens the randomizer screen during your run. You can
  change options, back out without changes, or reset the run. Changing settings mid-run can break a
  playthrough, so it warns you first.
- **Infinite Candy** and **Healing Kit** (Nuzlocke Mode): a reusable Rare Candy, and a full party heal
  you can use anywhere.

### Reset The Run
Erases your saved game and starts a new run with the settings on screen, without deleting your `.sav`
file. It asks twice, with the cursor on NO both times. Your presets are kept.

---

## Version history

| Version | Highlights |
|---|---|
| v0.1 | In-game randomizer at New Game |
| v0.2 | Tabbed dark options menu, battle mechanics generation, type chart options, HM Kit and Nuzlocke items, player graphics, permanent wild Megas |
| v0.3 | Every option re-checked against UPR FVX; new options (base stat totals, palettes, follow Megas, adjust evolution levels…); self-test suite |
| v0.4 | Rival Keeps Same Team, settings codes and presets, special shops randomize the evolution sellers, Mom's walking animation, fixes |
| v0.5 | Randomizer Settings key item, Reset The Run, dark "RANDOMIZE GAME" title menu |
| v0.5.1 | Clearer Randomizer Settings description; new Infinite Candy and HM Kit icons |
| v0.5.2 | New Healing Kit icon |

Each version is tagged in this repository and has its own download on the
[Releases page](https://github.com/JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-/releases).

## Known limitations
- Settings that only act when a game starts (Nuzlocke key items, National Dex at start, random PC
  potion) don't take effect if you change them mid-run with the Randomizer Settings item.
- Some NPC dialogue still names the original Pokémon.
- Settings codes and presets from one version may not load in a later version if options are added.
- An Emerald version is planned for later.

This is an early-access test build. Please report anything odd, with where you were and which options
you used.

---

## Building from source

The romhack's own code uses the `rh_` / `RH_` prefix (`src/rh_*.c`, `include/rh*.h`,
`data/scripts/rh_*.inc`, `tools/rh/`). To build it, follow pokeemerald-expansion's
[INSTALL.md](INSTALL.md), then run:

```
make firered RELEASE=1
```

That produces `pokefirered-release.gba`. Test tools for the in-ROM self-test and a headless mGBA
harness are in [`tools/rh/test/`](tools/rh/test/).

## Credits
- **[pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) by RHH (Rom Hacking
  Hideout)** and its [contributors](CREDITS.md). This romhack is based on RHH's pokeemerald-expansion
  1.17.0. The original pokeemerald-expansion README is kept as
  [README_pokeemerald-expansion.md](README_pokeemerald-expansion.md).
- The [pret](https://github.com/pret) decompilation projects.
- **[Universal Pokémon Randomizer FVX](https://github.com/upr-fvx/universal-pokemon-randomizer-fvx)**:
  option set, word lists and player graphics packs.
- **Player graphics** (from UPR FVX graphics packs; all art belongs to its creators):

  | Character | Source | Creator / adapter |
  |---|---|---|
  | Ethan | Generation II | [FourLeafSunny](https://www.spriters-resource.com/custom_edited/pokemongeneration2customs/asset/487434/) |
  | Kris | Generation II | [FourLeafSunny](https://www.spriters-resource.com/custom_edited/pokemongeneration2customs/asset/488043/) |
  | Red (FR/LG), Leaf | Generation III | Game Freak |
  | Brendan (E), May (E) | Generation III | Game Freak; voliol |
  | Brendan (R/S), May (R/S) | Generation III | Game Freak |
  | Wally | Generation III | Game Freak; adapted by [NachoPeñalva](https://www.deviantart.com/nachope126/art/Comission-To-Pokemon-Quetzal-wally-963498868) |
  | Prof. Birch | Generation III | Game Freak; adapted by [NachoPeñalva](https://www.deviantart.com/nachope126/art/Comission-To-Pokemon-Quetzal-Prof-Birch-964603747) |
  | Cynthia | Generation IV | Game Freak; adapted by [NachoPeñalva](https://www.deviantart.com/nachope126/art/Comission-To-Pokemon-Quetzal-Cynthia-930605682) |
  | Ghost, Wraith | Pokémon Snakewood | Cutlerine; adapted by Voliol |

Pokémon is © Nintendo, Creatures Inc. and GAME FREAK inc. This is a non-commercial fan project and
does not include any ROM.
