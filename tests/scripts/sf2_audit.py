#!/usr/bin/env python3
"""SF2 structure audit for the harmonium SoundFont.

Parses the RIFF/sfbk container directly (pure stdlib) and reports:
INFO metadata, presets/banks, preset zones, instruments, per-zone
key/velocity ranges, samples (root keys, loop points, modes), volume
envelope parameters, LFO settings, and modulators. Answers the Phase 1
audit questions: key coverage 36-84, velocity-to-filter/attack modulators,
LFO vibrato, attack/release values.

Usage: sf2_audit.py [path/to/harmonium.sf2]

Default: the committed provenance copy of the upstream font,
<repo>/plugins/harmonium/soundfonts/harmonium_original.sf2 (resolved
relative to this script's own location, so the script works from any CWD).
"""

import os
import struct
import sys

# Default audit target: the in-repo provenance copy (see
# plugins/harmonium/soundfonts/README.md).
DEFAULT_SF2 = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))),
    "plugins", "harmonium", "soundfonts", "harmonium_original.sf2")

GEN = {
    0: "startAddrsOffset", 1: "endAddrsOffset", 2: "startloopAddrsOffset",
    3: "endloopAddrsOffset", 4: "startAddrsCoarseOffset", 5: "modLfoToPitch",
    6: "vibLfoToPitch", 7: "modEnvToPitch", 8: "initialFilterFc",
    9: "initialFilterQ", 10: "modLfoToFilterFc", 11: "modEnvToFilterFc",
    12: "endAddrsCoarseOffset", 13: "modLfoToVolume", 15: "chorusSend",
    16: "reverbSend", 17: "pan", 21: "delayModLFO", 22: "freqModLFO",
    23: "delayVibLFO", 24: "freqVibLFO", 25: "delayModEnv",
    26: "attackModEnv", 27: "holdModEnv", 28: "decayModEnv",
    29: "sustainModEnv", 30: "releaseModEnv", 33: "delayVolEnv",
    34: "attackVolEnv", 35: "holdVolEnv", 36: "decayVolEnv",
    37: "sustainVolEnv", 38: "releaseVolEnv", 41: "instrument",
    43: "keyRange", 44: "velRange", 45: "startloopAddrsCoarseOffset",
    46: "keynum", 47: "velocity", 48: "initialAttenuation",
    50: "endloopAddrsCoarseOffset", 51: "coarseTune", 52: "fineTune",
    53: "sampleID", 54: "sampleModes", 56: "scaleTuning",
    57: "exclusiveClass", 58: "overridingRootKey",
}
TIME_GENS = {25, 26, 27, 28, 30, 33, 34, 35, 36, 38}  # timecents
DEFAULTS = {
    34: -12000, 35: -12000, 36: -12000, 37: 0, 38: -12000,
    24: 0, 6: 0, 22: 0, 13: 0, 8: 13500, 9: 0, 48: 0, 17: 0,
    16: 200, 15: 0, 56: 100, 54: 0,
}
GEN_CTRL = {0: "no controller", 1: "note-on velocity", 2: "note-on key number",
            3: "poly pressure", 4: "channel pressure", 5: "pitch wheel",
            6: "pitch wheel sensitivity", 7: "link"}
MOD_SRC_TYPE = {0: "linear", 1: "concave", 2: "convex", 3: "switch"}


def chunks(data, pos, end):
    """Iterate (id, start, length) of RIFF subchunks (padded to even)."""
    while pos + 8 <= end:
        cid = data[pos:pos + 4].decode("ascii", "replace")
        (length,) = struct.unpack_from("<I", data, pos + 4)
        yield cid, pos + 8, length
        pos += 8 + length + (length & 1)


def list_chunks(data, start, length):
    """Subchunks of a LIST chunk (skipping the 4-byte list type)."""
    return list(chunks(data, start + 4, start + length))


def timecents(val):
    return 2.0 ** (val / 1200.0)


def read_zones(data, bag, gen, mod):
    """Zones from (start, len) of bag/gen/mod chunk triples.

    Zone i spans generators bag[i].genidx .. bag[i+1].genidx-1 (same for
    mods); the extra terminal bag record delimits the last zone, so the
    terminal gen/mod records are never included.
    """
    (bag_s, bag_l), (gen_s, gen_l), (mod_s, mod_l) = bag, gen, mod
    bags = [struct.unpack_from("<HH", data, bag_s + 4 * i)
            for i in range(bag_l // 4)]
    gens = [struct.unpack_from("<HH", data, gen_s + 4 * i)
            for i in range(gen_l // 4)]
    mods = [struct.unpack_from("<HHhHH", data, mod_s + 10 * i)
            for i in range(mod_l // 10)]
    zones = []
    for zi in range(len(bags) - 1):
        g0, m0 = bags[zi]
        g1, m1 = bags[zi + 1]
        zgens = {}
        for gi in range(g0, g1):
            op, amount = gens[gi]
            if op in (43, 44):  # key/vel range: lo/hi bytes
                zgens[op] = (amount & 0xFF, (amount >> 8) & 0xFF)
            else:
                v = amount - 0x10000 if amount >= 0x8000 else amount
                zgens[op] = v if op != 58 or 0 <= v < 128 else None
        zones.append({"gens": zgens,
                      "mods": [mods[mi] for mi in range(m0, m1)]})
    return zones


def gen_str(op, val):
    name = GEN.get(op, f"op{op}")
    if op in (43, 44):
        return f"{name}={val[0]}..{val[1]}"
    if op in TIME_GENS:
        return f"{name}={val} ({timecents(val)*1000:.1f} ms)"
    if op == 37:
        return f"{name}={val} ({val/10:.1f} dB attn)"
    if op == 48:
        return f"{name}={val} ({val/10:.1f} dB)"
    if op == 24:
        return f"{name}={val} ({timecents(val)*8.176:.2f} Hz)"
    if op == 17:
        return f"{name}={val} ({val/10:.0f}%)"
    return f"{name}={val}"


def range_str(zgens, op):
    return f"{zgens[op][0]}-{zgens[op][1]}" if op in zgens else "0-127"


def mod_str(mod):
    src, dest, amt, asrc, trans = mod
    stype = MOD_SRC_TYPE.get(src & 0x3F, f"type{src & 0x3F}")
    flags = [stype,
             "bipolar" if src & 0x80 else "unipolar",
             "descending" if src & 0x100 else "ascending",
             f"CC{src >> 10}" if src & 0x200
             else GEN_CTRL.get(src >> 10, f"gen{src >> 10}")]
    return (f"src=[{'/'.join(flags)}] dest={GEN.get(dest, dest)} "
            f"amount={amt} amtSrc=0x{asrc:04X} trans={trans}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SF2
    data = open(path, "rb").read()
    if data[:4] != b"RIFF" or data[8:12] != b"sfbk":
        raise SystemExit("not an SF2 (RIFF/sfbk) file")

    info, sdta, pdta = {}, None, {}
    for cid, start, length in chunks(data, 12, len(data)):
        if cid != "LIST":
            continue
        ltype = data[start:start + 4]
        if ltype == b"INFO":
            for sub, s, ln in list_chunks(data, start, length):
                info[sub] = data[s:s + ln].split(b"\x00")[0].decode(
                    "ascii", "replace")
        elif ltype == b"sdta":
            sdta = (start, length)
        elif ltype == b"pdta":
            for sub, s, ln in list_chunks(data, start, length):
                pdta[sub] = (s, ln)

    print(f"=== {path} ({len(data)/1e6:.2f} MB) ===")
    print("INFO:")
    for key in ("INAM", "ICRD", "IENG", "IPRD", "ICOP", "ICMT", "ISFT"):
        if key in info:
            print(f"  {key}: {info[key]}")

    smpl_len = 0
    if sdta:
        for sub, s, ln in list_chunks(data, sdta[0], sdta[1]):
            if sub == "smpl":
                smpl_len = ln

    # phdr: name[20] preset bank bagidx library genre morphology
    ph_s, ph_l = pdta["phdr"]
    presets = []
    for i in range(ph_l // 38):
        raw = data[ph_s + 38 * i: ph_s + 38 * (i + 1)]
        name = raw[:20].split(b"\x00")[0].decode("ascii", "replace")
        preset, bank, bagidx = struct.unpack_from("<HHH", raw, 20)
        presets.append((bank, preset, name, bagidx))

    # inst: name[20] bagidx
    in_s, in_l = pdta["inst"]
    insts = []
    for i in range(in_l // 22):
        raw = data[in_s + 22 * i: in_s + 22 * (i + 1)]
        name = raw[:20].split(b"\x00")[0].decode("ascii", "replace")
        (bagidx,) = struct.unpack_from("<H", raw, 20)
        insts.append((name, bagidx))

    # shdr: name[20] start end startloop endloop rate pitch corr link type
    sh_s, sh_l = pdta["shdr"]
    samples = []
    for i in range(sh_l // 46):
        raw = data[sh_s + 46 * i: sh_s + 46 * (i + 1)]
        name = raw[:20].split(b"\x00")[0].decode("ascii", "replace")
        samples.append((name,) + struct.unpack_from("<IIIIIbBHH", raw, 20))

    p_zones = read_zones(data, pdta["pbag"], pdta["pgen"], pdta["pmod"])
    i_zones = read_zones(data, pdta["ibag"], pdta["igen"], pdta["imod"])

    print(f"\npresets: {len(presets)-1} (+terminal)")
    for pi, (bank, preset, name, bagidx) in enumerate(presets[:-1]):
        print(f"  bank {bank} preset {preset:3d}: {name!r}")
        for z in p_zones[bagidx:presets[pi + 1][3]]:
            g = z["gens"]
            extras = [gen_str(op, v) for op, v in sorted(g.items())
                      if op not in (41, 43, 44)]
            print(f"    zone -> instrument [{g.get(41)}] "
                  f"keys {range_str(g, 43)} vel {range_str(g, 44)}"
                  + ("  " + " ".join(extras) if extras else ""))
            for m in z["mods"]:
                print(f"    pmod: {mod_str(m)}")

    print(f"\nsamples: {len(samples)-1} (+terminal), smpl chunk "
          f"{smpl_len//2} frames")
    for s in samples[:-1]:
        (name, start, end, lo, hi, rate, pitch, corr, link, stype) = s
        dur = (end - start) / rate
        loop = f" loop {lo}-{hi} ({(hi-lo)/rate*1000:.0f} ms)" if hi > lo \
            else " no-loop"
        print(f"  {name!r}: {dur:.1f} s @ {rate} Hz, root={pitch}, "
              f"corr={corr} cents,{loop}, type={stype}")

    print(f"\ninstruments: {len(insts)-1} (+terminal)")
    key_lo, key_hi = 127, 0
    vel_layers = []
    for idx, (iname, bagidx) in enumerate(insts[:-1]):
        print(f"  [{idx}] {iname!r}")
        end_idx = insts[idx + 1][1]
        for z in i_zones[bagidx:end_idx]:
            g = z["gens"]
            if 53 not in g:
                desc = ", ".join(gen_str(op, v) for op, v in sorted(g.items()))
                print(f"    global zone: {desc or '(empty)'}")
                continue
            sample = samples[g[53]]
            kr = g.get(43, (0, 127))
            vr = g.get(44, (0, 127))
            key_lo = min(key_lo, kr[0])
            key_hi = max(key_hi, kr[1])
            if vr != (0, 127):
                vel_layers.append((kr, vr))
            parts = [gen_str(op, v) for op, v in sorted(g.items())
                     if op != 53]
            print(f"    sample={sample[0]!r} keys {kr[0]}-{kr[1]} "
                  f"vel {vr[0]}-{vr[1]}")
            print(f"      {', '.join(parts) if parts else '(no generators)'}")
            for m in z["mods"]:
                print(f"      imod: {mod_str(m)}")

    print(f"\nkey coverage (instrument zones): {key_lo}-{key_hi} "
          f"({'COVERS' if key_lo <= 36 and key_hi >= 84 else 'GAP vs'} "
          f"target 36-84)")
    if vel_layers:
        print("velocity layers (zones with explicit velRange):")
        for kr, vr in vel_layers:
            print(f"  keys {kr[0]}-{kr[1]}: vel {vr[0]}-{vr[1]}")
    else:
        print("velocity layers: NONE (all zones full velocity range)")


if __name__ == "__main__":
    main()
