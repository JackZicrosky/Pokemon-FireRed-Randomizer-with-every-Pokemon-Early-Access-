#!/usr/bin/env python3
"""Builds the Safari Zone generation pools from the extracted species data.
Output: src/data/rh_safari.h"""
import json, os, sys
R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'
VER = sys.argv[1] if len(sys.argv) > 1 else 'firered'
S = json.load(open(R + f'build/rh_species_{VER}.json'))

GEN_RANGES = [(1, 151), (152, 251), (252, 386), (387, 493), (494, 649), (650, 721), (722, 809), (810, 905), (906, 1025)]
def gen_of(dex):
    for g, (a, b) in enumerate(GEN_RANGES, 1):
        if a <= dex <= b: return g
    return 0

# Species "native" to the base game's region are excluded (they're already in the game).
if VER == 'firered':
    NATIVE = lambda sp, v: gen_of(v['natDexNum']) == 1 and not is_regional(v)
else:
    HOENN = set()  # filled in when we do Emerald
    NATIVE = lambda sp, v: v['natDexNum'] in HOENN and not is_regional(v)

def is_regional(v): return v['isAlolanForm'] or v['isGalarianForm'] or v['isHisuianForm'] or v['isPaldeanForm']
def regional_gen(v):
    if v['isAlolanForm']: return 7
    if v['isGalarianForm'] or v['isHisuianForm']: return 8
    if v['isPaldeanForm']: return 9
BAD_FLAGS = ['isMegaEvolution', 'isGigantamax', 'isTotem', 'isPrimalReversion', 'isUltraBurst', 'isTeraForm']
BAD_NAME_PARTS = ['_ZEN', '_STARTER', '_SHADOW', '_GMAX', '_MEGA', '_TOTEM']

# Cosmetic / wild variant forms: the pool holds the base species, and an encounter picks one of these at random.
FORM_GROUPS = {
    'UNOWN': 'all',
    'BURMY': ['BURMY', 'BURMY_SANDY', 'BURMY_TRASH'],
    'WORMADAM': ['WORMADAM', 'WORMADAM_SANDY', 'WORMADAM_TRASH'],
    'MOTHIM': ['MOTHIM', 'MOTHIM_SANDY', 'MOTHIM_TRASH'],
    'SHELLOS': ['SHELLOS', 'SHELLOS_EAST'], 'GASTRODON': ['GASTRODON', 'GASTRODON_EAST'],
    'BASCULIN': ['BASCULIN', 'BASCULIN_BLUE_STRIPED', 'BASCULIN_WHITE_STRIPED'],
    'DEERLING': 'all', 'SAWSBUCK': 'all',
    'SCATTERBUG': 'all', 'SPEWPA': 'all', 'VIVILLON': 'all',
    'FLABEBE': 'all', 'FLOETTE': ['FLOETTE', 'FLOETTE_YELLOW', 'FLOETTE_ORANGE', 'FLOETTE_BLUE', 'FLOETTE_WHITE'],
    'FLORGES': 'all',
    'MEOWSTIC': ['MEOWSTIC', 'MEOWSTIC_F'], 'INDEEDEE': ['INDEEDEE', 'INDEEDEE_F'],
    'BASCULEGION': ['BASCULEGION', 'BASCULEGION_F'], 'OINKOLOGNE': ['OINKOLOGNE', 'OINKOLOGNE_F'],
    'PUMPKABOO': 'all', 'GOURGEIST': 'all',
    'ORICORIO': 'all',
    'LYCANROC': ['LYCANROC', 'LYCANROC_MIDNIGHT', 'LYCANROC_DUSK'],
    'MINIOR': ['MINIOR', 'MINIOR_METEOR_ORANGE', 'MINIOR_METEOR_YELLOW', 'MINIOR_METEOR_GREEN',
               'MINIOR_METEOR_BLUE', 'MINIOR_METEOR_INDIGO', 'MINIOR_METEOR_VIOLET'],
    'TOXTRICITY': ['TOXTRICITY', 'TOXTRICITY_LOW_KEY'],
    'URSHIFU': ['URSHIFU', 'URSHIFU_RAPID_STRIKE'],
    'SQUAWKABILLY': 'all', 'TATSUGIRI': 'all',
    'DUDUNSPARCE': 'all', 'MAUSHOLD': 'all',
    'GIMMIGHOUL': ['GIMMIGHOUL', 'GIMMIGHOUL_ROAMING'],
    'TAUROS_PALDEA': None,  # the three Paldean breeds are separate pool entries (different types)
}

import re as _re
ALIAS = dict(_re.findall(r'^\s*(SPECIES_\w+)\s*=\s*(SPECIES_\w+)\s*,', open(R + 'include/constants/species.h').read(), _re.M))
def canon(n):
    for _ in range(4):
        if n in S: return n
        n = ALIAS.get(n, n)
    return n
GROUPS_CANON = {}
for _k, _fg in FORM_GROUPS.items():
    if not _fg: continue
    _b = canon('SPECIES_' + _k)
    if _b not in S: raise SystemExit('FORM_GROUPS base not found: ' + _k)
    GROUPS_CANON[_b] = _fg

def base_name(sp, v):
    return v['forms'][0] if v['forms'] else sp

# stages via prevolutions
prevo = {}
for sp, v in S.items():
    for e in v['evolutions']:
        t = e['target']
        if t in S and t != sp: prevo.setdefault(t, sp)
def stage(sp, depth=0):
    if sp not in prevo or depth > 4: return 1
    return 1 + stage(prevo[sp], depth + 1)

def legendary(v): return v['isRestrictedLegendary'] or v['isSubLegendary'] or v['isMythical'] or v['isUltraBeast'] or v['isParadox']

AREAS = {
    'CENTER': {'TYPE_NORMAL', 'TYPE_GRASS', 'TYPE_BUG', 'TYPE_FAIRY'},
    'EAST': {'TYPE_FIRE', 'TYPE_ELECTRIC', 'TYPE_DRAGON', 'TYPE_FLYING'},
    'NORTH': {'TYPE_ROCK', 'TYPE_GROUND', 'TYPE_STEEL', 'TYPE_FIGHTING', 'TYPE_ICE'},
    'WEST': {'TYPE_POISON', 'TYPE_GHOST', 'TYPE_PSYCHIC', 'TYPE_DARK'},
}
def area_of(v):
    t1, t2 = v['types']
    if 'TYPE_WATER' in (t1, t2):
        return 'FISH' if ({'EGG_GROUP_WATER_2', 'EGG_GROUP_WATER_3'} & set(v['eggGroups'])) else 'SURF'
    if t1 == 'TYPE_NORMAL' and t2 == 'TYPE_FLYING': t1 = 'TYPE_FLYING'
    for a, ts in AREAS.items():
        if t1 in ts: return a
    for a, ts in AREAS.items():
        if t2 in ts: return a
    raise SystemExit('no area for ' + str(v['types']))

pools = {a: [] for a in ['CENTER', 'EAST', 'NORTH', 'WEST', 'SURF', 'FISH']}
groups = {}
seen_forms = set()
for sp, v in sorted(S.items(), key=lambda kv: (kv[1]['natDexNum'], kv[1]['id'])):
    dex = v['natDexNum']
    if not (1 <= dex <= 1025) or sp in ('SPECIES_NONE', 'SPECIES_EGG'): continue
    if any(v.get(f) for f in BAD_FLAGS) or any(p in sp for p in BAD_NAME_PARTS): continue
    short = sp[8:]
    reg = is_regional(v)
    if not reg and base_name(sp, v) != sp: continue          # alternate (non-regional) forms: only via FORM_GROUPS
    if NATIVE(sp, v): continue
    g = regional_gen(v) if reg else gen_of(dex)
    st = stage(sp)
    w = 0 if legendary(v) else {1: 10, 2: 9}.get(st, 8)
    pools[area_of(v)].append([sp, g, w, 0])
    if sp in GROUPS_CANON:
        fg = GROUPS_CANON[sp]
        members = [f for f in v['forms'] if f in S and not any(S[f].get(x) for x in BAD_FLAGS) and not is_regional(S[f])] if fg == 'all' else [canon('SPECIES_' + f) for f in fg]
        for m in members:
            if m not in S: raise SystemExit('bad form ' + m)
        groups[sp] = members

# Legendary weights: tuned so ~LEG_SHARE of encounters (per generation, and in All mode) are legendary.
LEG_SHARE = 0.06
def leg_weight(entries):
    W = sum(e[2] for e in entries if e[2]); L = sum(1 for e in entries if not e[2])
    return max(1, min(10, round(LEG_SHARE * W / ((1 - LEG_SHARE) * L)))) if L else 1
allents = [e for l in pools.values() for e in l]
lw_all = leg_weight(allents)
for g in range(1, 10):
    lw = leg_weight([e for e in allents if e[1] == g])
    for e in allents:
        if e[1] == g:
            e[3] = e[2] if e[2] else lw_all
            if not e[2]: e[2] = lw
print({a: len(l) for a, l in pools.items()}, 'legend weight (all mode):', lw_all)
for g in range(10):
    ents = [e for e in allents if g == 0 or e[1] == g]; wi = 3 if g == 0 else 2
    tw = sum(e[wi] for e in ents); lwsum = sum(e[wi] for e in ents if e[0] and (e[2] < 8 and e[3] < 8))
    print(f'gen {g or "ALL"}: {len(ents)} species, legendary share {100*lwsum/max(1,tw):.1f}%')

c = '// AUTO-GENERATED by tools/rh/gen_safari.py - do not edit by hand\n'
c += 'struct RhSafariMon { u16 species; u8 gen; u8 weight; u8 weightAll; };\n\n'
for a, l in pools.items():
    c += f'static const struct RhSafariMon sRhSafari_{a}[] = {{\n'
    for sp, g, w, wa in l: c += f'    {{{sp}, {g}, {w}, {wa}}},\n'
    c += '};\n\n'
for sp, m in groups.items():
    c += f'static const u16 sRhSafariForms_{sp[8:]}[] = {{ {", ".join(m)} }};\n'
c += '\nstruct RhSafariFormGroup { u16 base; u16 count; const u16 *forms; };\n'
c += 'static const struct RhSafariFormGroup sRhSafariFormGroups[] = {\n'
for sp, m in groups.items(): c += f'    {{{sp}, {len(m)}, sRhSafariForms_{sp[8:]}}},\n'
c += '};\n'
open(R + 'src/data/rh_safari.h', 'w').write(c)
