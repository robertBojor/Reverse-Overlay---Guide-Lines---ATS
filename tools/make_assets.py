"""Builds the guide-line mod package (out/ats_reverse_overlay.scs).

The line models are derived from the MIT-licensed "Reverse Posture Assistant"
sweep_edge model (assets/base): a 0.36 m bar that the runtime lays down every
0.25 m so consecutive pieces overlap into a continuous line. Variants differ
only in width and in the material path stored in the .pmd. The forward dots
use the same bar shortened to a square tile (DOT_TILE); the runtime tiles it
for bigger dots, since model instances cannot be scaled.

    python3 -I tools/make_assets.py [scs|workshop|all]
      scs       stage files in out/mod_stage (zip with tools/pack_scs.ps1)
      workshop  write the uploader layout to workshop/ats/upload
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'assets' / 'base'
STAGE = ROOT / 'out' / 'mod_stage'

MODEL_DIR = 'model/rto_guides'
HALF_WIDTH = 0.06  # base model is 0.0375 (7.5 cm wide); 12 cm reads better at distance
BASE_HALF_WIDTH = 0.0375
BASE_HALF_LENGTH = 0.18
DOT_TILE = 0.12  # forward dot tile edge; must match DotOptions::tile in src/core/dots.hpp
DOT_COLOURS = ('red', 'green')  # left, right

# name -> sRGB colour. The material diffuse is the linear equivalent, as in the
# original assets (texture and diffuse multiply).
COLOURS = {
    'red': (235, 45, 35),
    'yellow': (255, 196, 0),
    'green': (40, 215, 80),
}

# Materials sit next to the models. /automat/ is reserved for files named by
# their content hash; the Workshop uploader rejects anything else there.
MATERIAL_PATHS = {name: f'/{MODEL_DIR}/line_{name}.mat' for name in COLOURS}
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
    # The material path is the last thing in the file, a NUL-terminated string
    # whose size (with the NUL) is stored at 0x20, so it can change length.
    data = bytearray((BASE / 'sweep_edge.pmd').read_bytes())
    start = len(data) - len(BASE_MATERIAL) - 1
    assert data[start:] == BASE_MATERIAL + b'\0'
    assert struct.unpack_from('<I', data, 0x20)[0] == len(BASE_MATERIAL) + 1
    new = material_path.encode() + b'\0'
    struct.pack_into('<I', data, 0x20, len(new))
    return bytes(data[:start]) + new


def make_pmg(half_width, half_length, radius):
    # Vertices and both bounding boxes hold the bar's half-width (x) and
    # half-length (z); the height (y, 0 to 0.055) stays.
    data = bytearray((BASE / 'sweep_edge.pmg').read_bytes())
    swaps = {}
    for old, new in ((BASE_HALF_WIDTH, half_width), (BASE_HALF_LENGTH, half_length)):
        if old != new:
            swaps[struct.pack('<f', old)] = struct.pack('<f', new)
            swaps[struct.pack('<f', -old)] = struct.pack('<f', -new)
    for old, new in swaps.items():
        count = 0
        i = data.find(old)
        while i != -1:
            if i % 4 == 0:
                data[i:i + 4] = new
                count += 1
            i = data.find(old, i + 4)
        assert count == 14, (old, count)  # 12 vertices + 2 bounding boxes
    # Bounding sphere radius (two copies). Its centre stays where the base
    # model put it, about (0.017, 0.009, -0.023), so the radius has margin.
    radius_old = data[0x2C:0x30]
    assert data[0xA8:0xAC] == radius_old
    data[0x2C:0x30] = data[0xA8:0xAC] = struct.pack('<f', radius)
    return bytes(data)


VERSION = (ROOT / 'VERSION').read_text().strip()
DISPLAY_NAME = 'Reverse Overlay - Guide Lines - ATS'
WORKSHOP_DIR = ROOT / 'workshop' / 'ats'
ICON = WORKSHOP_DIR / 'mod_icon.jpg'  # 276x162 JPEG, supplied by hand


def manifest(workshop, with_icon):
    # The Workshop uploader manages display_name and compatible_versions
    # (from the item title and versions.sii); they must not be in the manifest.
    lines = ['SiiNunit', '{', 'mod_package : .package_name {',
             f' package_version: "{VERSION}"']
    if not workshop:
        lines.append(f' display_name: "{DISPLAY_NAME}"')
    lines += [' author: "Telemichus"', ' category[]: "interior"']
    if with_icon:
        lines.append(' icon: "mod_icon.jpg"')
    lines.append(' description_file: "description.txt"')
    if not workshop:
        lines.append(' compatible_versions[]: "1.61.*"')
    lines += ['}', '}', '']
    return '\n'.join(lines).encode()


DESCRIPTION = f'''{DISPLAY_NAME} (v{VERSION})

Backup-camera style guide lines shown on the ground while reversing:
red, yellow and green by distance, visible in every camera and the mirrors.
In a forward gear, two square dots show where the truck's front corners
will be in a moment (red left, green right), to help with lane position.

This package contains only the models. They are drawn by the
ats_reverse_overlay.dll plugin, which must be installed separately into
the game's bin/win_x64/plugins folder.

Line models derived from "Reverse Posture Assistant For ATS" by yyysheng (MIT).
'''.encode()


def model_files():
    files = {}
    pmg = make_pmg(HALF_WIDTH, BASE_HALF_LENGTH, 0.26)
    dot_pmg = make_pmg(DOT_TILE / 2, DOT_TILE / 2, 0.15)
    for name, rgb in COLOURS.items():
        dds = f'/{MODEL_DIR}/line_{name}.dds'
        tobj = f'/{MODEL_DIR}/line_{name}.tobj'
        files[dds[1:]] = make_dds(rgb)
        files[tobj[1:]] = make_tobj(dds)
        files[MATERIAL_PATHS[name][1:]] = make_material(rgb, tobj)
        files[f'{MODEL_DIR}/line_{name}.pmd'] = make_pmd(MATERIAL_PATHS[name])
        files[f'{MODEL_DIR}/line_{name}.pmg'] = pmg
    for name in DOT_COLOURS:
        files[f'{MODEL_DIR}/dot_{name}.pmd'] = make_pmd(MATERIAL_PATHS[name])
        files[f'{MODEL_DIR}/dot_{name}.pmg'] = dot_pmg
    return files


def package_files(workshop):
    icon = ICON.read_bytes() if ICON.exists() else None
    files = model_files()
    files['manifest.sii'] = manifest(workshop, icon is not None)
    files['description.txt'] = DESCRIPTION
    if icon is not None:
        files['mod_icon.jpg'] = icon
    return files, icon is not None


def write_tree(root, files):
    import shutil
    if root.exists():
        shutil.rmtree(root)
    for path, data in files.items():
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


VERSIONS_SII = '''SiiNunit
{
package_version_info : .universal {
 package_name: "universal"
}
}
'''


def build_scs_stage():
    # tools/pack_scs.ps1 zips this with .NET's ZipFile (deflate); the game
    # rejects stored zip entries.
    files, _ = package_files(workshop=False)
    write_tree(STAGE, files)
    print(f'staged {len(files)} files in {STAGE}')


def build_workshop():
    # Uploader layout: the selected folder may contain only versions.sii and
    # version packages. One universal package serves every game version.
    upload = WORKSHOP_DIR / 'upload'
    files, has_icon = package_files(workshop=True)
    write_tree(upload / 'universal', files)
    (upload / 'versions.sii').write_text(VERSIONS_SII)
    print(f'wrote {upload} ({len(files)} files in universal/)')
    if not has_icon:
        print(f'WARNING: {ICON.relative_to(ROOT)} is missing; the uploader requires a '
              '276x162 JPEG icon. Add it and run this again.')


def main():
    import sys
    target = sys.argv[1] if len(sys.argv) > 1 else 'scs'
    if target in ('scs', 'all'):
        build_scs_stage()
    if target in ('workshop', 'all'):
        build_workshop()


if __name__ == '__main__':
    main()
