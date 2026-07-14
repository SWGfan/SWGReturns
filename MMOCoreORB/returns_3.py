#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

FILES = [

"bin/scripts/object/weapon/ranged/rifle/rifle_t21.lua",
"bin/scripts/object/custom_content/weapon/ranged/rifle_t21_legendary.lua",
"bin/scripts/object/custom_content/weapon/ranged/rifle_t21_generic.lua",

"bin/scripts/object/weapon/ranged/rifle/rifle_bounty_dc15.lua",
"bin/scripts/object/weapon/ranged/rifle/rifle_a280.lua",
"bin/scripts/object/weapon/ranged/rifle/rifle_ld1.lua",
"bin/scripts/object/weapon/ranged/rifle/rifle_proton.lua",
"bin/scripts/object/weapon/ranged/rifle/rifle_westar_m5.lua",
"bin/scripts/object/weapon/ranged/rifle/rifle_alliance_gauss_generic.lua",

]

CHANGES = [

# Base T21
(
r'minDamage\s*=\s*350',
'minDamage = 275'
),

(
r'maxDamage\s*=\s*400',
'maxDamage = 340'
),

# Legendary rolls
(
r'\{"mindamage",150,200,0\}',
'{"mindamage",110,150,0}'
),

(
r'\{"maxdamage",200,250,0\}',
'{"maxdamage",150,200,0}'
),

]

patched=0

for file in FILES:

    p=Path(file)

    if not p.exists():
        continue

    txt=p.read_text()

    orig=txt

    bak=Path(str(p)+".returns_patch3.bak")

    if not bak.exists():
        shutil.copy2(p,bak)

    for pat,new in CHANGES:

        txt,n=re.subn(
            pat,
            new,
            txt,
            count=1
        )

        patched+=n

    if txt!=orig:
        p.write_text(txt)

print()
print("Patch 3 complete.")
print("Changes:",patched)
