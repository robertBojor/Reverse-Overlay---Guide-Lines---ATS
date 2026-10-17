"""Builds the guide-line mod package (out/ats_reverse_overlay.scs).

The line models are derived from the MIT-licensed "Reverse Posture Assistant"
sweep_edge model (assets/base): a 0.36 m bar that the runtime lays down every
0.25 m so consecutive pieces overlap into a continuous line. Variants differ
only in width and in the material path stored in the .pmd.

    python3 -I tools/make_assets.py      # stage files in out/mod_stage
    tools/pack_scs.ps1                    # zip them into out/ats_reverse_overlay.scs
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'assets' / 'base'
STAGE = ROOT / 'out' / 'mod_stage'

MODEL_DIR = 'model/rto_guides'
HALF_WIDTH = 0.06  # base model is 0.0375 (7.5 cm wide); 12 cm reads better at distance

# name -> sRGB colour. The material diffuse is the linear equivalent, as in the
# original assets (texture and diffuse multiply).
COLOURS = {
    'red': (235, 45, 35),
    'yellow': (255, 196, 0),
    'green': (40, 215, 80),
}

# Material paths must keep the original length (32 chars) because the .pmd
# stores string offsets. Names follow the engine's automat convention.
MATERIAL_PATHS = {
    'red': '/automat/e1/e10ad5e0000000r1.mat',
    'yellow': '/automat/e1/e10ad5e0000000y1.mat',
    'green': '/automat/e1/e10ad5e0000000g1.mat',
}
BASE_MATERIAL = b'/automat/f5/f57370b76733c5aa.mat'


def srgb_to_linear(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def make_dds(rgb):
    base = (BASE / 'guide_color.dds').read_bytes()
    header = base[:128]  # 4x4 A8R8G8B8, no mips
    r, g, b = rgb
    pixel = bytes([b, g, r, 255])
    return header + pixel * 16


def make_tobj(dds_path):
    base = (BASE / 'guide_color.tobj').read_bytes()
    header = base[:0x28]
    path = dds_path.encode()
    return header + struct.pack('<Q', len(path)) + path


def make_material(rgb, tobj_path):
    lin = [srgb_to_linear(c) for c in rgb]
    return (
        'effect : "eut2.dif.lum.rfx" {\n'
        '\tadditional_ambient : 1.000000\n'
        f'\tdiffuse : {{ {lin[0]:.6f} , {lin[1]:.6f} , {lin[2]:.6f} }}\n'
        '\tluminance_night : 4.000000\n'
        '\tluminance_output : 600.000000\n'
        '\treflection : 0.000000\n'
        '\tshininess : 8.000000\n'
        '\tspecular : { 0.000000 , 0.000000 , 0.000000 }\n'
        '\ttexture : "texture_base" {\n'
        f'\t\tsource : "{tobj_path}"\n'
        '\t}\n'
        '}'
    ).encode()


def make_pmd(material_path):
    data = (BASE / 'sweep_edge.pmd').read_bytes()
    new = material_path.encode()
    assert len(new) == len(BASE_MATERIAL), material_path
    assert data.count(BASE_MATERIAL) == 1
    return data.replace(BASE_MATERIAL, new)


def make_pmg():
    data = bytearray((BASE / 'sweep_edge.pmg').read_bytes())
    swaps = {
        struct.pack('<f', 0.0375): struct.pack('<f', HALF_WIDTH),
        struct.pack('<f', -0.0375): struct.pack('<f', -HALF_WIDTH),
    }
    for old, new in swaps.items():
        count = 0
        i = data.find(old)
        while i != -1:
            if i % 4 == 0:
                data[i:i + 4] = new
                count += 1
            i = data.find(old, i + 4)
        assert count == 14, (old, count)  # 12 vertices + 2 bounding boxes
    # Bounding sphere radius (two copies) grows with the wider bar.
    radius_old = data[0x2C:0x30]
    assert data[0xA8:0xAC] == radius_old
    data[0x2C:0x30] = data[0xA8:0xAC] = struct.pack('<f', 0.26)
    return bytes(data)


MANIFEST = '''SiiNunit
{
mod_package : .package_name {
 package_version: "0.1.0"
 display_name: "Reverse Overlay - Guide Lines - ATS"
 author: "ats-reverse-overlay"
 category[]: "interior"
 description_file: "description.txt"
 compatible_versions[]: "1.61.*"
}
}
'''

DESCRIPTION = '''Reverse Overlay - Guide Lines - ATS

Guide-line models for the Reverse Overlay telemetry plugin.

This package only contains the coloured line models. The lines are placed by
ats_reverse_overlay.dll (bin/win_x64/plugins) while reversing.

Line models derived from "Reverse Posture Assistant For ATS" by yyysheng (MIT).
'''


def main():
    files = {
        'manifest.sii': MANIFEST.encode(),
        'description.txt': DESCRIPTION.encode(),
    }
    pmg = make_pmg()
    for name, rgb in COLOURS.items():
        dds = f'/{MODEL_DIR}/line_{name}.dds'
        tobj = f'/{MODEL_DIR}/line_{name}.tobj'
        files[dds[1:]] = make_dds(rgb)
        files[tobj[1:]] = make_tobj(dds)
        files[MATERIAL_PATHS[name][1:]] = make_material(rgb, tobj)
        files[f'{MODEL_DIR}/line_{name}.pmd'] = make_pmd(MATERIAL_PATHS[name])
        files[f'{MODEL_DIR}/line_{name}.pmg'] = pmg

    # Stage the tree; tools/pack_scs.ps1 zips it with .NET's ZipFile, the same
    # writer the original mod's working .scs packages used. (A Python-written
    # stored zip mounted but the game did not see its manifest.)
    import shutil
    if STAGE.exists():
        shutil.rmtree(STAGE)
    for path, data in files.items():
        target = STAGE / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    print(f'staged {len(files)} files in {STAGE}')
    for path in sorted(files):
        print('  ', path)


if __name__ == '__main__':
    main()
