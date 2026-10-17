"""Locate the plugin's engine functions in a new game build.

Uses a known-good build as reference. The patterns in src/plugin/engine.cpp
are the source of truth for which functions are needed; their locations in
the old executable are found by scanning it. Each function is then searched
for in the new executable with several independent methods:

  exact     the current pattern from engine.cpp matches once
  loose     a longer pattern taken from the old function, with struct offsets
            and 32-bit constants also wildcarded, matches once
  shape     instruction-shape fingerprint compared against every function
            in the new build's exception directory (.pdata)
  callers   functions that call it in the old build are matched by shape;
            their call at the same position votes for a target

A result is accepted when the methods that produced an answer agree. New
patterns are then generated that are unique in the new build and, where
possible, also match the old build, so they are more likely to survive the
next update. With --apply they are written into engine.cpp.

    python3 -I tools/re/find_in_build.py --old OLD.exe --new NEW.exe \\
        --insn-mask BIN --fnscan BIN [--apply] [--methods exact,loose,shape,callers]

The two helper binaries are built from tools/re/insn_mask.c and fnscan.c.
"""
import argparse
import collections
import difflib
import math
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pe import PE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ENGINE_CPP = ROOT / 'src' / 'plugin' / 'engine.cpp'
MIN_PATTERN = 40
MAX_INSNS = 120


# --------------------------------------------------------------------------
# Patterns

def read_engine_patterns(path=ENGINE_CPP):
    src = path.read_text()
    block = src[src.index('constexpr std::array patterns'):src.index('// Executables on which')]
    specs = re.findall(r'PatternSpec\{"(\w+)",\s*((?:"[^"]*"\s*)+)\}', block)
    return [(name, ''.join(re.findall(r'"([^"]*)"', parts)).split()) for name, parts in specs]


def compile_tokens(tokens):
    return re.compile(b''.join(b'.' if t == '??' else re.escape(bytes([int(t, 16)]))
                               for t in tokens), re.DOTALL)


def find_all(pe, regex, limit=3):
    va, text = pe.text()
    hits = []
    for m in regex.finditer(text):
        hits.append(va + m.start())
        if len(hits) >= limit:
            break
    return hits


def instructions(tool, exe_path, pe, rva, count, loose=False):
    args = [tool, exe_path, f'{pe.rva_to_off(rva):x}', str(count)]
    if loose:
        args.append('loose')
    out = subprocess.run(args, capture_output=True, text=True, check=True).stdout.split('\n')
    result = []
    for line in out:
        if not line or line == 'ERR':
            break
        length, mask = line.split()
        result.append((int(length), mask))
    return result


def tokens_from(data, mask):
    return ['??' if m == '?' else f'{b:02X}' for b, m in zip(data, mask)]


# --------------------------------------------------------------------------
# Fingerprints

class Fingerprints:
    def __init__(self, tool, exe_path, pe):
        lines = []
        for begin, end in pe.functions():
            try:
                lines.append(f'{begin:x} {pe.rva_to_off(begin):x} {end - begin:x}')
            except ValueError:
                continue
        out = subprocess.run([tool, exe_path], input='\n'.join(lines) + '\n',
                             capture_output=True, text=True, check=True).stdout
        self.funcs = {}
        for line in out.splitlines():
            head, _, calls = line.partition(';')
            parts = head.split()
            if len(parts) < 3:
                continue
            rva, size = int(parts[0], 16), int(parts[1], 16)
            toks = [int(t, 16) for t in parts[2].split(',') if t]
            call_list = [int(c, 16) for c in calls.strip().split(',') if c]
            self.funcs[rva] = (size, toks, call_list)
        self._hist = {}

    def hist(self, rva):
        if rva not in self._hist:
            self._hist[rva] = collections.Counter(self.funcs[rva][1])
        return self._hist[rva]

    def containing(self, rva):
        """Function whose range contains rva (linear, used rarely)."""
        for start, (size, _, _) in self.funcs.items():
            if start <= rva < start + size:
                return start
        return None


def cosine(a, b):
    dot = sum(v * b.get(k, 0) for k, v in a.items())
    na = math.sqrt(sum(v * v for v in a.values()))
    nb = math.sqrt(sum(v * v for v in b.values()))
    return dot / (na * nb) if na and nb else 0.0


def best_shape_matches(old_fp, old_rva, new_fp, top=3):
    if old_rva not in old_fp.funcs:
        return []
    size, toks, _ = old_fp.funcs[old_rva]
    target_hist = old_fp.hist(old_rva)
    candidates = []
    for rva, (nsize, ntoks, _) in new_fp.funcs.items():
        if not (size * 0.6 <= nsize <= size * 1.6 + 16):
            continue
        if abs(len(ntoks) - len(toks)) > max(8, len(toks) * 0.4):
            continue
        candidates.append((cosine(target_hist, new_fp.hist(rva)), rva))
    candidates.sort(reverse=True)
    scored = []
    for _, rva in candidates[:40]:
        ratio = difflib.SequenceMatcher(None, toks, new_fp.funcs[rva][1], autojunk=False).ratio()
        scored.append((ratio, rva))
    scored.sort(reverse=True)
    return scored[:top]


# --------------------------------------------------------------------------
# Methods

def method_exact(new_pe, tokens):
    hits = find_all(new_pe, compile_tokens(tokens))
    return hits[0] if len(hits) == 1 else None


def method_loose(args, old_pe, new_pe, old_rva):
    insns = instructions(args.insn_mask, args.old, old_pe, old_rva, MAX_INSNS, loose=True)
    off = old_pe.rva_to_off(old_rva)
    data, mask, last_unique = b'', '', None
    for length, m in insns:
        data += old_pe.data[off + len(data):off + len(data) + length]
        mask += m
        if len(data) < 24:
            continue
        hits = find_all(new_pe, compile_tokens(tokens_from(data, mask)), limit=2)
        if not hits:
            break  # the function diverged here; keep the last unique answer
        if len(hits) == 1:
            last_unique = hits[0]
            if len(data) >= MIN_PATTERN:
                return last_unique
    return last_unique


def method_shape(old_fp, new_fp, old_rva):
    matches = best_shape_matches(old_fp, old_rva, new_fp)
    if not matches:
        return None, 0.0, 0.0
    best = matches[0]
    margin = best[0] - (matches[1][0] if len(matches) > 1 else 0.0)
    # Require a clear winner.
    if best[0] >= 0.80 and margin >= 0.05:
        return best[1], best[0], margin
    return None, best[0], margin


def method_callers(old_fp, new_fp, old_rva, max_callers=8):
    votes = collections.Counter()
    callers = [(rva, calls) for rva, (_, _, calls) in old_fp.funcs.items() if old_rva in calls]
    for caller_rva, calls in callers[:max_callers * 4]:
        match, score, margin = method_shape(old_fp, new_fp, caller_rva)
        if match is None or score < 0.9:
            continue
        new_calls = new_fp.funcs[match][2]
        if len(new_calls) != len(calls):
            continue
        for index, target in enumerate(calls):
            if target == old_rva:
                votes[new_calls[index]] += 1
        if sum(votes.values()) >= max_callers:
            break
    if not votes:
        return None, 0
    target, count = votes.most_common(1)[0]
    total = sum(votes.values())
    return (target if count >= 2 and count / total >= 0.75 else None), count


# --------------------------------------------------------------------------
# Pattern generation

def make_pattern(args, new_pe, new_rva, old_pe=None, old_rva=None):
    """Shortest instruction-aligned pattern (>= MIN_PATTERN bytes) unique in
    the new build; bytes that differ from the old build are wildcarded so the
    pattern also matches there when the code lines up."""
    insns = instructions(args.insn_mask, args.new, new_pe, new_rva, MAX_INSNS)
    off = new_pe.rva_to_off(new_rva)
    old_off = old_pe.rva_to_off(old_rva) if old_pe else None
    data, mask = b'', ''
    for length, m in insns:
        start = len(data)
        chunk = new_pe.data[off + start:off + start + length]
        if old_off is not None:
            old_chunk = old_pe.data[old_off + start:old_off + start + length]
            m = ''.join('?' if (mm == '?' or a != b) else 'x' for mm, a, b in zip(m, chunk, old_chunk))
        data += chunk
        mask += m
        if len(data) < MIN_PATTERN or mask.count('x') < MIN_PATTERN * 0.6:
            continue
        regex = compile_tokens(tokens_from(data, mask))
        if find_all(new_pe, regex, limit=2) != [new_rva]:
            continue
        both = old_pe is None or find_all(old_pe, regex, limit=2) == [old_rva]
        return tokens_from(data, mask), both
    return None, False


def format_pattern(tokens, indent=' ' * 16, per_line=26):
    chunks = [tokens[i:i + per_line] for i in range(0, len(tokens), per_line)]
    lines = [' '.join(c) + (' ' if i + 1 < len(chunks) else '') for i, c in enumerate(chunks)]
    return '\n'.join(f'{indent}"{l}"' for l in lines)


def apply_patterns(results, note_lines):
    """Replace the pattern array in engine.cpp and the comment block above it."""
    src = ENGINE_CPP.read_text()
    start = src.index('constexpr std::array patterns{')
    end = src.index('\n};', start) + 3
    # Walk back over the contiguous // comment lines directly above the array.
    comment_start = start
    while True:
        prev = src.rfind('\n', 0, comment_start - 1)
        line = src[prev + 1:comment_start].strip()
        if not line.startswith('//'):
            break
        comment_start = prev + 1
    entries = [f'    PatternSpec{{"{name}",\n{format_pattern(tokens)}}},' for name, tokens in results]
    comment = ''.join(f'// {line}\n' for line in note_lines)
    block = comment + 'constexpr std::array patterns{\n' + '\n'.join(entries) + '\n};'
    ENGINE_CPP.write_text(src[:comment_start] + block + src[end:])


# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--old', required=True, help='known-good executable')
    ap.add_argument('--new', required=True, help='executable to search')
    ap.add_argument('--insn-mask', required=True)
    ap.add_argument('--fnscan', required=True)
    ap.add_argument('--methods', default='exact,loose,shape,callers')
    ap.add_argument('--apply', action='store_true', help='write new patterns into engine.cpp')
    args = ap.parse_args()
    methods = set(args.methods.split(','))

    old_pe, new_pe = PE(args.old), PE(args.new)
    specs = read_engine_patterns()
    need_fp = bool(methods & {'shape', 'callers'})
    if need_fp:
        print('fingerprinting old build...', file=sys.stderr)
        old_fp = Fingerprints(args.fnscan, args.old, old_pe)
        print('fingerprinting new build...', file=sys.stderr)
        new_fp = Fingerprints(args.fnscan, args.new, new_pe)

    failures = 0
    results = []
    cross = True
    for name, tokens in specs:
        old_hits = find_all(old_pe, compile_tokens(tokens))
        if len(old_hits) != 1:
            print(f'{name}: pattern does not match the old build uniquely ({len(old_hits)} hits); '
                  'is --old the build engine.cpp was made for?')
            failures += 1
            continue
        old_rva = old_hits[0]
        found = {}
        detail = []
        if 'exact' in methods:
            found['exact'] = method_exact(new_pe, tokens)
        if 'loose' in methods:
            found['loose'] = method_loose(args, old_pe, new_pe, old_rva)
        if 'shape' in methods:
            rva, score, margin = method_shape(old_fp, new_fp, old_rva)
            found['shape'] = rva
            detail.append(f'shape {score:.2f} (+{margin:.2f})')
        if 'callers' in methods:
            rva, votes = method_callers(old_fp, new_fp, old_rva)
            found['callers'] = rva
            detail.append(f'callers {votes} votes')

        answers = {k: v for k, v in found.items() if v is not None}
        distinct = set(answers.values())
        summary = ' '.join(f'{k}={v:#x}' if v is not None else f'{k}=-' for k, v in found.items())
        if len(distinct) != 1:
            verdict = 'NOT FOUND' if not distinct else 'CONFLICT'
            print(f'{name:24s} {verdict:9s} old={old_rva:#x} {summary}  [{", ".join(detail)}]')
            failures += 1
            continue
        new_rva = distinct.pop()
        agree = len(answers)
        if 'exact' not in answers and agree < 2:
            print(f'{name:24s} LOW       old={old_rva:#x} {summary}  [{", ".join(detail)}]  '
                  f'only one method found {new_rva:#x}; check it by hand')
            failures += 1
            continue
        pattern, both = make_pattern(args, new_pe, new_rva, old_pe, old_rva)
        if pattern is None:
            print(f'{name:24s} found at {new_rva:#x} but no unique pattern could be generated')
            failures += 1
            continue
        cross = cross and both
        print(f'{name:24s} {new_rva:#x} ({agree} method(s) agree) '
              f'old={old_rva:#x} {summary}  [{", ".join(detail)}]  '
              f'pattern={len(pattern)}B{" (matches both builds)" if both else " (new build only)"}')
        results.append((name, pattern))

    if failures:
        print(f'\n{failures} function(s) need attention; engine.cpp not changed.')
        return 1
    print('\nPatterns:')
    for name, pattern in results:
        print(f'  {name}: {" ".join(pattern)}')
    if args.apply:
        old_name, new_name = Path(args.old).name, Path(args.new).name
        note = [f'Generated by tools/re/find_in_build.py from {old_name} -> {new_name}.',
                'Each pattern matches exactly once in ' +
                ('both executables.' if cross else 'the new executable.')]
        apply_patterns(results, note)
        print(f'\nUpdated {ENGINE_CPP}. Add the new executable hash to verified_hashes '
              'after an in-game test.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
