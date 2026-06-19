from pathlib import Path
import re

ROOT = Path("bin/scripts/object")

SKIP = {"objects.lua", "serverobjects.lua"}

REPLACEMENTS = {
    # One-handed
    "cert_onehandlightsaber": "force_title_jedi_rank_02",
    "cert_onehandlightsaber_gen1": "jedi_padawan_novice",
    "cert_onehandlightsaber_gen2": "jedi_padawan_saber_04",
    "cert_onehandlightsaber_gen3": "jedi_padawan_master",

    # BOTH Light and Dark can use Gen 4
    "cert_onehandlightsaber_gen4": "force_title_jedi_rank_02",

    # Two-handed
    "cert_twohandlightsaber": "jedi_light_side_journeyman_novice",
    "cert_twohandlightsaber_gen1": "jedi_light_side_journeyman_novice",
    "cert_twohandlightsaber_gen2": "jedi_light_side_journeyman_saber_02",
    "cert_twohandlightsaber_gen3": "jedi_light_side_journeyman_saber_04",
    "cert_twohandlightsaber_gen4": "jedi_light_side_journeyman_master",

    # Polearm
    "cert_polearmlightsaber": "jedi_light_side_master_novice",
    "cert_polearmlightsaber_gen1": "jedi_light_side_master_novice",
    "cert_polearmlightsaber_gen2": "jedi_light_side_master_saber_02",
    "cert_polearmlightsaber_gen3": "jedi_light_side_master_saber_04",
    "cert_polearmlightsaber_gen4": "jedi_light_side_master_master",
}

pattern = re.compile(
    r'(certificationsRequired\s*=\s*\{\s*")([^"]+)("\s*\})'
)

patched = 0

for lua in ROOT.rglob("*.lua"):
    if lua.name in SKIP:
        continue

    try:
        text = lua.read_text(encoding="utf-8", errors="ignore")
    except:
        continue

    # Only touch lightsaber files
    if "lightsaber" not in lua.name.lower() and "saber" not in lua.name.lower():
        continue

    original = text

    def repl(m):
        cert = m.group(2)
        if cert in REPLACEMENTS:
            print(f"{lua}: {cert} -> {REPLACEMENTS[cert]}")
            return f'{m.group(1)}{REPLACEMENTS[cert]}{m.group(3)}'
        return m.group(0)

    text = pattern.sub(repl, text)

    if text != original:
        lua.write_text(text, encoding="utf-8", newline="\n")
        patched += 1

print(f"\nPatched {patched} files.")
