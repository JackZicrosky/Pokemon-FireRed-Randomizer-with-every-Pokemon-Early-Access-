# Makes the public GitHub Release files for every version:
#   build/release/README vX.txt  - the release README with lines addressed to the owner removed
#   build/release/notes vX.md    - the release page text
# Reads the owner's release files from the Fire Red folder (the parent of this project folder).
# For a new version: add it to CHANGES (last entry = latest), then run publish_releases.ps1.
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
FIRE_RED = os.path.dirname(ROOT)                     # the project lives in ...\Fire Red\source
OUT = os.path.join(ROOT, "build", "release")
REPO_URL = "https://github.com/JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-"

CHANGES = {
    "0.1": "First release: the in-game randomizer at New Game (first FVX-style option set), plus the base game changes (all 1025 Pokémon, evolution-item sellers, Celadon Dept. Store, herb shop, Safari Zone generation selector, Mom gives the Mega Ring and Z-Power Ring).",
    "0.2": "Tabbed dark options menu, battle mechanics generation, type effectiveness options, HM Kit and Nuzlocke key items, custom player graphics, permanent wild Megas.",
    "0.3": "Every option re-checked against UPR FVX; A now toggles options; new options (base stat totals, Pokémon palettes, follow Mega Evolutions, adjust evolution levels, no premature evolutions, random intro Pokémon); many fixes.",
    "0.4": "Rival Keeps Same Team, settings codes and presets, special shops randomize the evolution sellers, Mom's walking animation, legendary and lower-case defaults off, fixes.",
    "0.5": "Randomizer Settings key item, Reset The Run, dark \"RANDOMIZE GAME\" title menu.",
    "0.5.1": "Clearer Randomizer Settings description; new Infinite Candy and HM Kit icons.",
    "0.5.2": "New Healing Kit icon: a white case with a shaded red cross.",
    "0.6": "The bag never fills up (one of every item fits; overflow goes to the PC); Mom gives the Running Shoes and you can run indoors; a level 100 Mewtwo blocks the road to Mt. Moon until you beat Brock; new Poke Ball seller in Celadon (every Ball, incl. Master/Cherish/Beast); new shop price curve; no sell-only items in random shops; no Rare Candy in Nuzlocke shops; the rival always uses the Pokemon he took from Oak's lab; Vivillon patterns count as one Pokemon; Amulet Coin + Luck Incense stack; Reset The Run returns to the options with a new seed (and the intro graphics bug is fixed); new player character: Lucoa. Start a new game: older saves and settings codes don't carry over.",
    "0.6.1": "Fixes: the rival always uses exactly the Pokemon he took from Oak's lab (whatever the trainer options); blank Custom starters keep Bulbasaur / Charmander / Squirtle; the Route 3 barricade no longer pops in late; losing to the Pewter Mewtwo no longer affects other catches; less memory used. v0.6 saves keep working.",
    "0.7": "NEW: UI Theme option (Misc. Tweaks): Default, Randomizer (dark menus in the randomizer screen's colors) or AMOLED (pitch black). It re-colors text boxes, menus, the bag, party, summary, shop, battle text boxes, Pokedex, Trainer Card, Options, PC boxes and the naming screen. v0.6 saves keep working; old settings codes and presets don't load.",
    "0.7.1": "UI Theme fixes: the white labels on the summary screen (name, level, Dex number, page title...) stay white and readable; the striped backgrounds of the bag and party screens become one plain color in the Randomizer and AMOLED themes. v0.6 / v0.7 saves keep working.",
    "0.7.2": "UI Theme fix: the START RELEARN prompt on the summary screen's move pages is white and readable again in the Randomizer and AMOLED themes. v0.6 / v0.7 saves keep working.",
    "0.7.3": "UI Theme restyle (the Default theme is unchanged): the Randomizer theme follows the randomizer screen's colors with purple highlights (party selector, chosen options, bag arrows and pocket dot, naming screen, summary page dot); AMOLED goes true black with red and white highlights (Poke Ball bag icon and Pokedex background, red battle cursors, white START / SELECT); lighter grey text and PP numbers, colored text kept; dark battle surroundings and menus; visible party genders; readable Pokedex labels; white BADGES label and numbers on the Trainer Card. v0.6 / v0.7 saves keep working.",
    "0.7.4": "Fix: \"Change Impossible Evos\" now turns the trade evolutions of Kadabra, Machoke, Graveler, Haunter, Boldore, Gurdurr, Phantump and Pumpkaboo into level-up evolutions (before, they could still only evolve with a Linking Cord). v0.6 / v0.7 saves keep working.",
}
SAVES = {"0.1": "Start a new game.", "0.2": "Start a new game (v0.1 saves don't carry over).",
         "0.6": "Start a new game (saves from older versions don't carry over).",
         "0.6.1": "v0.6 saves keep working; start a new game if you come from v0.5.2 or older.",
         "0.7": "v0.6 / v0.6.1 saves keep working; start a new game if you come from v0.5.2 or older.",
         "0.7.1": "v0.6 and v0.7 saves keep working; start a new game if you come from v0.5.2 or older.",
         "0.7.2": "v0.6 and v0.7 saves keep working; start a new game if you come from v0.5.2 or older.",
         "0.7.3": "v0.6 and v0.7 saves keep working; start a new game if you come from v0.5.2 or older.",
         "0.7.4": "v0.6 and v0.7 saves keep working; start a new game if you come from v0.5.2 or older."}

# Lines written to the owner that shouldn't appear in public READMEs.
TEXT_SUBS = [
    (b" - the one in this folder is correct)", b")"),
    (b"Healing Kit icon redrawn from your design: ", b"Healing Kit icon redrawn: "),
    (b"is why you got the normal sprite.", b"is why the normal sprite showed up."),
    (b"UI Theme restyle, following your notes and edited screenshots.", b"UI Theme restyle."),
]
LINE_SUBS = {b"Your list": b"Requested changes", b"Bugs you hit": b"Bug fixes"}


def readme_path(v):
    # v0.1's README has no version in its name.
    return os.path.join(FIRE_RED, "README.txt" if v == "0.1" else f"README v{v}.txt")


def public_readme(v):
    data = open(readme_path(v), "rb").read()
    for a, b in TEXT_SUBS:
        data = data.replace(a, b)
    lines = data.split(b"\n")
    for i, line in enumerate(lines):
        core = line.rstrip(b"\r")  # keep each file's own line endings
        if core in LINE_SUBS:
            lines[i] = LINE_SUBS[core] + line[len(core):]
    return b"\n".join(lines)


def notes(v, latest):
    if v == latest:
        top = "✅ **This is the latest version and the recommended download.**"
    else:
        top = (f"⚠️ **This is an older version.** The recommended download is always the "
               f"[latest release]({REPO_URL}/releases/latest). Older versions are kept here in case you want to play them.")
    # GitHub turns spaces in uploaded file names into dots.
    return f"""{top}

**FireRed Expansion Randomizer v{v}** (early access / test build)

Pokémon FireRed with all 1025 Pokémon, forms, Megas and Z-Moves, plus an in-game randomizer based on the Universal Pokémon Randomizer FVX. See the [project page]({REPO_URL}#readme) for the full feature list.

### What's new in v{v}
{CHANGES[v]}

The full details are in `README.v{v}.txt` below.

### How to play
1. Get a clean **Pokémon FireRed (USA) v1.0** ROM that you dumped yourself (SHA-1 `dd5945db9b930750cb39d00c84da8571feebf417`). No ROM is included.
2. Download **`FireRed.Expansion.Randomizer.v{v}.bps`** below and apply it with [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/): pick the ROM, pick the `.bps`, then click **Apply patch**. Floating IPS (Flips) also works.
3. Play the patched `.gba` in **mGBA** (recommended), with the save type on auto / Flash 128K. {SAVES.get(v, "")}
""".rstrip() + "\n"


def main():
    os.makedirs(OUT, exist_ok=True)
    latest = list(CHANGES)[-1]
    for v in CHANGES:
        open(os.path.join(OUT, f"README v{v}.txt"), "wb").write(public_readme(v))
        with open(os.path.join(OUT, f"notes v{v}.md"), "w", encoding="utf-8", newline="\n") as f:
            f.write(notes(v, latest))
    print(f"Wrote {len(CHANGES)} READMEs and release notes to {OUT} (latest: v{latest})")


if __name__ == "__main__":
    main()
