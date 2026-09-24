#!/usr/bin/env python3
"""Extracts species data from the built ELF (exact, macro-proof) into build/rh_species.json.
Struct layouts come from DWARF of a tiny helper compiled with -g."""
import json, re, subprocess, os, sys, struct
from elftools.elf.elffile import ELFFile
R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'
ELF = sys.argv[1] if len(sys.argv) > 1 else R + 'pokefirered.elf'
VER = sys.argv[2] if len(sys.argv) > 2 else 'FIRERED'
tmp = R + 'build/rh_tmp'; os.makedirs(tmp, exist_ok=True)
open(tmp + '/layout.c', 'w').write('#include "global.h"\n#include "pokemon.h"\nstruct SpeciesInfo gDwS; struct Evolution gDwE;\n')
subprocess.check_call(['arm-none-eabi-gcc', '-g', '-c', '-iquote', R + 'include', '-DMODERN=1', '-DTESTING=0', f'-D{VER}',
                       '-std=gnu17', '-mthumb', '-mthumb-interwork', '-O0', '-mabi=apcs-gnu', '-march=armv4t',
                       tmp + '/layout.c', '-o', tmp + '/layout.o'])

def struct_layout(objpath, name):
    with open(objpath, 'rb') as f:
        e = ELFFile(f); dw = e.get_dwarf_info()
        for cu in dw.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag == 'DW_TAG_structure_type' and die.attributes.get('DW_AT_name') and die.attributes['DW_AT_name'].value.decode() == name and 'DW_AT_byte_size' in die.attributes:
                    size = die.attributes['DW_AT_byte_size'].value; mem = {}
                    for ch in die.iter_children():
                        if ch.tag != 'DW_TAG_member' or 'DW_AT_name' not in ch.attributes: continue
                        n = ch.attributes['DW_AT_name'].value.decode()
                        a = ch.attributes
                        if 'DW_AT_data_bit_offset' in a:
                            bo = a['DW_AT_data_bit_offset'].value; bs = a['DW_AT_bit_size'].value
                            mem[n] = ('bits', bo, bs)
                        else:
                            off = a['DW_AT_data_member_location'].value
                            t = ch.get_DIE_from_attribute('DW_AT_type')
                            while 'DW_AT_byte_size' not in t.attributes and 'DW_AT_type' in t.attributes:
                                t = t.get_DIE_from_attribute('DW_AT_type')
                            ts = t.attributes['DW_AT_byte_size'].value if 'DW_AT_byte_size' in t.attributes else 4
                            if 'DW_AT_bit_size' in a:
                                mem[n] = ('bits', off * 8 + 0, a['DW_AT_bit_size'].value)
                            else:
                                mem[n] = ('bytes', off, ts)
                    return size, mem
    raise SystemExit('struct not found ' + name)

SI_SIZE, SI = struct_layout(tmp + '/layout.o', 'SpeciesInfo')
EV_SIZE, EV = struct_layout(tmp + '/layout.o', 'Evolution')

f = open(ELF, 'rb'); elf = ELFFile(f)
symtab = elf.get_section_by_name('.symtab')
def sym(n):
    s = symtab.get_symbol_by_name(n)[0]; return s['st_value'], s['st_size']
rom_sec = [s for s in elf.iter_sections() if s['sh_addr'] and s['sh_type'] == 'SHT_PROGBITS']
def read(addr, n):
    for s in rom_sec:
        if s['sh_addr'] <= addr < s['sh_addr'] + s['sh_size']:
            o = addr - s['sh_addr']; return s.data()[o:o + n]
    raise ValueError(hex(addr))

def field(buf, m):
    kind, a, b = m
    if kind == 'bytes':
        return int.from_bytes(buf[a:a + b], 'little')
    v = int.from_bytes(buf, 'little'); return (v >> a) & ((1 << b) - 1)

# enum names
def enum_names(path, prefix):
    names = {}
    for m in re.finditer(r'^\s*(' + prefix + r'\w+)\s*=\s*(\d+)\s*,', open(path).read(), re.M):
        names.setdefault(int(m.group(2)), m.group(1))
    return names
spn = enum_names(R + 'include/constants/species.h', 'SPECIES_')
# types enum (sequential)
def seq_enum(path, enumname, prefix):
    s = open(path).read(); m = re.search(r'enum\s+(?:__attribute__\(\(packed\)\)\s+)?' + enumname + r'\s*\{(.*?)\};', s, re.S)
    out, i = {}, 0
    for line in m.group(1).split('\n'):
        line = line.split('//')[0].strip().rstrip(',')
        if not line.startswith(prefix): continue
        if '=' in line:
            n, v = [x.strip() for x in line.split('=')]
            try: i = int(v, 0)
            except ValueError: i = {vv: kk for kk, vv in out.items()}.get(v, i)
        else: n = line
        out[i] = n; i += 1
    return out
TYPES = seq_enum(R + 'include/constants/pokemon.h', 'Type', 'TYPE_')
EGG = seq_enum(R + 'include/constants/pokemon.h', 'EggGroup', 'EGG_GROUP_')

base, size = sym('gSpeciesInfo')
n = size // SI_SIZE
out = {}
for i in range(n):
    buf = read(base + i * SI_SIZE, SI_SIZE)
    name = spn.get(i)
    if not name: continue
    d = {'id': i}
    for k in ['natDexNum', 'baseHP', 'baseAttack', 'baseDefense', 'baseSpeed', 'baseSpAttack', 'baseSpDefense', 'catchRate',
              'isLegendary', 'isRestrictedLegendary', 'isSubLegendary', 'isMythical', 'isUltraBeast', 'isParadox', 'isTotem',
              'isMegaEvolution', 'isPrimalReversion', 'isUltraBurst', 'isGigantamax', 'isTeraForm', 'isAlolanForm',
              'isGalarianForm', 'isHisuianForm', 'isPaldeanForm', 'cannotBeTraded', 'isFrontierBanned', 'genderRatio']:
        if k in SI: d[k] = field(buf, SI[k])
    t0 = SI['types']; ts = t0[2]
    d['types'] = [TYPES.get(int.from_bytes(buf[t0[1] + ts * j:t0[1] + ts * (j + 1)], 'little'), '?') for j in range(2)]
    eg = SI['eggGroups']; es = eg[2]
    d['eggGroups'] = [EGG.get(int.from_bytes(buf[eg[1] + es * j:eg[1] + es * (j + 1)], 'little'), '?') for j in range(2)]
    ep = field(buf, SI['evolutions']); evos = []
    if ep:
        j = 0
        while True:
            eb = read(ep + j * EV_SIZE, EV_SIZE)
            meth = field(eb, EV['method'])
            if meth == 0xFFFF: break
            evos.append({'method': meth, 'param': field(eb, EV['param']), 'target': spn.get(field(eb, EV['targetSpecies']), '?')})
            j += 1
    d['evolutions'] = evos
    fp = field(buf, SI['formSpeciesIdTable']); forms = []
    if fp:
        j = 0
        while True:
            v = int.from_bytes(read(fp + 2 * j, 2), 'little')
            if v == 0xFFFF: break
            forms.append(spn.get(v, '?')); j += 1
    d['forms'] = forms
    out[name] = d
os.makedirs(R + 'build', exist_ok=True)
json.dump(out, open(R + f'build/rh_species_{VER.lower()}.json', 'w'), indent=0)
print('species extracted:', len(out), 'struct size', SI_SIZE)
