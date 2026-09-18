#!/usr/bin/env python3
"""Derive harmonium_v2.sf2 / harmonium_v3.sf2 from the original harmonium
SoundFont (the original is never modified).

v2 (Phase 3, default): adds a second preset (bank 0, program 1, "harmonium
double") backed by a NEW instrument that duplicates every key zone of the
original instrument with fineTune = +D cents. The original preset 0 /
instrument 0 are copied byte-identically, so preset 0 renders the same sound
as today.

v3 (Phase 6, --click): everything v2 has, PLUS a key-click/chiff layer:
  - a synthesized ~40 ms mechanical key-noise burst appended to sdta
    (16-bit mono 22050 Hz, matching the font; brown-noise-ish, bandpassed
    700-4000 Hz, ~-8 dBFS peak, 2/15 ms fades — all parameters are module
    constants below)
  - a new shdr entry for it (NO loop, sample rate/type correct)
  - a "key click" instrument with ONE zone covering keys 21-108 (wide —
    the click is mechanical noise, not pitched) referencing the click
    sample with a SELF-ENDING volume envelope:
      attackVolEnv ~1 ms, holdVolEnv ~0, decayVolEnv ~40 ms,
      sustainVolEnv 1000 cB (= 100 dB attenuation = fully closed),
      releaseVolEnv ~15 ms.
    Self-end math: the envelope reaches its (silent) sustain level
    1 + 0 + 40 ms after onset, and the unlooped 40 ms sample runs out of
    data at the same time — either mechanism ends the voice <= 60 ms.
    THIS IS THE CRITICAL POINT: sustainVolEnv is an attenuation (0 = hold
    full level, higher = quieter), so a LOW sustain value would sustain the
    click forever; 1000 cB closes it. Verified by render (see
    tests/RESULTS.md Phase 6: measured self-end).
    The zone also fixes keynum = 60 (gen 46) so the burst plays at the same
    rate on every key (a key click is unpitched mechanical noise).
  - preset 2 "key click" (bank 0, program 2) -> the click instrument.

All Phase 3 surgery rules apply: bag/gen index renumbering, EOI/EOP
terminal records kept, chunk + RIFF sizes recomputed; preset 0's zones stay
byte-identical. The 6.6 MB reed sample data is referenced, not duplicated
(v3 grows only by the ~1.8 KB click sample + ~110 bytes of pdta/shdr
records; regenerating v2 without --click is byte-identical to the committed
file).

Zone generator order follows the SF2 practice of keyRange (43) first,
velRange (44) second; the remaining generators are sorted by operator
number, which places fineTune (52) before sampleID (53).

Gen-op numbers verified against the SF2 2.x spec and fluidsynth's gen.h:
41=GEN_INSTRUMENT, 43=GEN_KEYRANGE, 44=GEN_VELRANGE, 46=GEN_KEYNUM,
51=GEN_COARSETUNE, 52=GEN_FINETUNE (range -99..+99 cents per the spec —
the task brief said "fineTune = gen 51"; gen 51 is coarseTune, 52 is
fineTune), 53=GEN_SAMPLEID, 54=GEN_SAMPLEMODES,
34/35/36/37/38 = attack/hold/decay/sustain/releaseVolEnv (times in
timecents, sustain in cB attenuation).

Usage:
  derive_sf2.py <original.sf2> <output.sf2> [detune_cents] [preset_name]
                [--click]

Defaults: detune_cents=4, preset_name="harmonium double".

  # v2 (Phase 3, exactly as committed):
  derive_sf2.py original.sf2 harmonium_v2.sf2
  # v3 (Phase 6):
  derive_sf2.py original.sf2 harmonium_v3.sf2 --click
"""

import struct
import sys

# pdta record formats (SF2 spec 2.x)
PHDR = "<20sHHHIII"    # name[20] preset bank bagIdx library genre morphology
BAG = "<HH"            # genIdx modIdx
PMOD_IMOD = "<HHhHH"   # src dest amount amtSrcOper transOper
PGEN_IGEN = "<HH"      # genOper amount
INST = "<20sH"         # name[20] bagIdx
SHDR = "<20sIIIIIbBHH"  # name start end startLoop endLoop sampleRate pitch correction link type

GEN_INSTRUMENT = 41
GEN_KEYRANGE = 43
GEN_VELRANGE = 44
GEN_KEYNUM = 46
GEN_FINETUNE = 52
GEN_SAMPLEID = 53

# --- Phase 6 key-click sample constants (tuned by ear-proxy renders; see
# tests/RESULTS.md Phase 6). The sample is synthesized with numpy (sox is
# not installable on this machine) and embedded into the derived font. ---
CLICK_RATE = 22050          # Hz, matches the reed samples in the font
CLICK_MS = 40.0             # burst length
CLICK_HIGHPASS_HZ = 700.0   # bandpass edges (FFT-domain, raised-cosine
CLICK_LOWPASS_HZ = 4000.0   #   transitions, see synth_click_pcm)
CLICK_GAIN_DB = -8.0        # peak level (dBFS) before the envelope
CLICK_FADEIN_MS = 2.0
CLICK_FADEOUT_MS = 15.0
CLICK_SEED = 20260919       # deterministic sample -> regenerable font

# --- Phase 6 key-click envelope constants (SF2 generators in the click
# zone). Chosen for a self-ending voice: attack 1 ms + hold 0 + decay 40 ms
# reaches sustainVolEnv (1000 cB = 100 dB down = silence) <= 41 ms after
# onset, and the unlooped sample runs out of data at 40 ms. releaseVolEnv
# 15 ms only matters if a noteoff arrives early. See the module docstring. ---
CLICK_KEYS = (21, 108)      # wide zone: mechanical noise, not pitched
CLICK_KEYNUM = 60           # gen 46: fixed playback rate on every key
CLICK_ATTACK_MS = 1.0
CLICK_HOLD_MS = 0.0         # encoded as -32768 timecents (= 0 seconds)
CLICK_DECAY_MS = 40.0
CLICK_SUSTAIN_CB = 1000     # 100 dB attenuation = fully closed (silent)
CLICK_RELEASE_MS = 15.0
CLICK_INITIAL_ATTENUATION_CB = 0   # coarse level trim; plugin CC7/velocity
                                   # do the fine level (tuned by render)


def ms_to_timecents(ms):
    """SF2 envelope times are timecents: tc = 1200*log2(seconds)."""
    import math
    return int(round(1200.0 * math.log2(ms / 1000.0)))


def synth_click_pcm():
    """Synthesize the key-click burst as an int16 LE numpy array.

    Brown-noise-ish burst: cumulative white noise (detrended), FFT-domain
    bandpass 700-4000 Hz with raised-cosine edges (a brick wall on a 40 ms
    window rings; the cosine transitions keep it clean), peak-normalized to
    CLICK_GAIN_DB, short fades. Fully deterministic (fixed seed) so the
    font is regenerable bit-for-bit.
    """
    import numpy as np
    rng = np.random.default_rng(CLICK_SEED)
    n = int(round(CLICK_RATE * CLICK_MS / 1000.0))
    brown = np.cumsum(rng.standard_normal(n))
    brown -= np.linspace(brown[0], brown[-1], n)   # detrend (kill DC drift)
    brown /= np.abs(brown).max() + 1e-12

    freqs = np.fft.rfftfreq(n, 1.0 / CLICK_RATE)

    def edge(f, f0, f1):
        if f <= f0:
            return 0.0
        if f >= f1:
            return 1.0
        return 0.5 - 0.5 * np.cos(np.pi * (f - f0) / (f1 - f0))

    trans = 200.0
    mask = np.array([edge(f, CLICK_HIGHPASS_HZ, CLICK_HIGHPASS_HZ + trans)
                     * (1.0 - edge(f, CLICK_LOWPASS_HZ - trans, CLICK_LOWPASS_HZ))
                     for f in freqs])
    y = np.fft.irfft(np.fft.rfft(brown) * mask, n)
    y /= np.abs(y).max() + 1e-12
    y *= 10.0 ** (CLICK_GAIN_DB / 20.0)

    fi = max(1, int(round(CLICK_RATE * CLICK_FADEIN_MS / 1000.0)))
    fo = max(1, int(round(CLICK_RATE * CLICK_FADEOUT_MS / 1000.0)))
    y[:fi] *= np.linspace(0.0, 1.0, fi)
    y[-fo:] *= np.linspace(1.0, 0.0, fo)
    return np.round(y * 32767.0).astype("<i2")


def parse(data):
    """Split an SF2 into (info_bytes, sdta_bytes, {pdta_chunk: bytes})."""
    assert data[:4] == b"RIFF" and data[8:12] == b"sfbk", "not an SF2 file"
    info = sdta = None
    pdta = {}
    pos, end = 12, len(data)
    while pos + 8 <= end:
        cid = data[pos:pos + 4]
        (ln,) = struct.unpack_from("<I", data, pos + 4)
        body = data[pos + 8:pos + 8 + ln]
        if cid == b"LIST":
            ltype, payload = body[:4], body[4:]
            if ltype == b"INFO":
                info = body
            elif ltype == b"sdta":
                sdta = body
            elif ltype == b"pdta":
                pdta = {sub: payload[s:s + ln2]
                        for sub, s, ln2 in _chunks(payload)}
        pos += 8 + ln + (ln & 1)
    assert info is not None and sdta is not None and pdta, "malformed sf2"
    return info, sdta, pdta


def _chunks(payload):
    pos, end = 0, len(payload)
    while pos + 8 <= end:
        cid = payload[pos:pos + 4]
        (ln,) = struct.unpack_from("<I", payload, pos + 4)
        yield cid, pos + 8, ln
        pos += 8 + ln + (ln & 1)


def records(blob, fmt):
    sz = struct.calcsize(fmt)
    assert len(blob) % sz == 0, f"chunk not a multiple of record size {sz}"
    return [struct.unpack_from(fmt, blob, sz * i)
            for i in range(len(blob) // sz)]


def pack(blob_fmt, recs):
    return b"".join(struct.pack(blob_fmt, *r) for r in recs)


def u16(v):
    """Generator amounts are unsigned 16-bit LE; wrap signed values."""
    return v & 0xFFFF


def zone_key(op):
    """Generator ordering: ranges first, then by operator number."""
    return (0 if op in (GEN_KEYRANGE, GEN_VELRANGE) else 1, op)


def main():
    args = [a for a in sys.argv[1:] if a != "--click"]
    click_mode = "--click" in sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        sys.exit(1)
    src_path = args[0]
    dst_path = args[1]
    detune = float(args[2]) if len(args) > 2 else 4.0
    if not -99.0 <= detune <= 99.0:
        raise SystemExit("detune cents must be within the SF2 spec range "
                         "-99..+99")
    detune_i = int(round(detune))
    preset_name = (args[3] if len(args) > 3 else "harmonium double")
    preset_name = preset_name.encode("ascii")[:20]

    data = open(src_path, "rb").read()
    info, sdta, pdta = parse(data)

    phdr = records(pdta[b"phdr"], PHDR)
    pbag = records(pdta[b"pbag"], "<HH")
    pgen = records(pdta[b"pgen"], "<HH")
    inst = records(pdta[b"inst"], INST)
    ibag = records(pdta[b"ibag"], "<HH")
    igen = records(pdta[b"igen"], "<HH")
    shdr = pdta[b"shdr"]

    assert len(phdr) == 2 and len(inst) == 2, \
        f"expected 1 preset / 1 instrument, got {len(phdr) - 1}/{len(inst) - 1}"

    # --- unpack the original instrument's zones (bags 0 .. inst[1].bagIdx) --
    inst0_end_bag = inst[1][1]           # EOI bag index = zone count
    zones = []                           # list of [(op, amount), ...]
    for zi in range(inst0_end_bag):
        g0 = ibag[zi][0]
        g1 = ibag[zi + 1][0]
        zones.append([(op, amt) for op, amt in igen[g0:g1]])

    # global zone (no sampleID) is emitted once; sample zones are doubled.
    global_zone = [z for z in zones if all(op != GEN_SAMPLEID
                                            for op, _ in z)]
    sample_zones = [z for z in zones if any(op == GEN_SAMPLEID
                                             for op, _ in z)]
    assert len(global_zone) == 1, "expected exactly one global zone"
    global_zone = global_zone[0]

    def tuned(zone):
        """Copy of a zone's generator list with fineTune = +D cents."""
        out = []
        for op, amt in zone:
            if op == GEN_FINETUNE:
                # signed generator amount stored little-endian as u16
                amt = struct.unpack("<h", struct.pack("<H", amt))[0]
                out.append((op, amt + detune_i))
            else:
                out.append((op, amt))
        if not any(op == GEN_FINETUNE for op, _ in out):
            out.append((GEN_FINETUNE, detune_i))
        out.sort(key=lambda g: zone_key(g[0]))
        return out

    double_zones = [global_zone]
    for z in sample_zones:
        double_zones.append(z)          # reed A: original, unchanged
        double_zones.append(tuned(z))   # reed B: +D cents

    # --- sdta / shdr: append the click sample (Phase 6, --click) -----------
    # The click PCM is appended to the smpl chunk (after 8 zero frames of
    # interpolation headroom beyond the header's end point); a new shdr
    # record is inserted BEFORE the terminal record so existing sampleID
    # indices stay valid.
    click_pcm = b""
    click_sample_idx = None
    if click_mode:
        import numpy as np
        pcm = synth_click_pcm()
        click_pcm = pcm.tobytes()
        # parse the existing smpl chunk inside sdta
        sdta_body = sdta[4:]              # skip the 'sdta' list type
        smpl = None
        for cid, off, ln in _chunks(sdta_body):
            if cid == b"smpl":
                smpl = sdta_body[off:off + ln]
        assert smpl is not None, "no smpl chunk in source sdta"
        n_frames = len(pcm) // 2
        existing_frames = len(smpl) // 2
        shdr_recs = records(shdr, SHDR)
        click_sample_idx = len(shdr_recs) - 1   # after the last real sample,
                                                # before the terminal record
        start = existing_frames
        end = start + n_frames
        # name start end startLoop endLoop rate pitch corr link type
        # (mono sample, type=1; NO loop: startLoop=endLoop=end and the
        # click zone relies on the envelope + end-of-data to self-end)
        click_shdr = (b"KeyClick".ljust(20, b"\0"), start, end, end, end,
                      CLICK_RATE, 60, 0, 0, 1)
        shdr_new = pack(SHDR, shdr_recs[:click_sample_idx]
                        + [click_shdr] + [shdr_recs[-1]])
        headroom = b"\0\0" * 8              # interpolation headroom past end
        smpl_new = smpl + click_pcm + headroom
        sdta_new_body = b"".join(
            (cid if cid != b"smpl" else b"smpl")
            + struct.pack("<I", len(smpl_new) if cid == b"smpl" else ln)
            + (smpl_new if cid == b"smpl" else sdta_body[off:off + ln])
            for cid, off, ln in _chunks(sdta_body))
        sdta_new = b"sdta" + sdta_new_body
    else:
        shdr_new = shdr
        sdta_new = sdta

    # --- igen/ibag: inst0 keeps its zone bags (0 .. inst0_end_bag-1)
    # unchanged — its original terminal bag doubles as the first bag of the
    # double instrument (bag 15 = its global zone), then 28 sample zones (14
    # originals + 14 +D-cent copies). With --click, the click instrument's
    # single zone follows. Each instrument's original terminal bag doubles
    # as the next instrument's first bag (exactly like the bag shared
    # between an empty global zone and the next zone). The original
    # terminal igen record is dropped and re-appended at the end.
    igen_new = list(igen[:-1])              # drop original terminal gen
    ibag_new = list(ibag[:inst0_end_bag])
    gen_idx = len(igen_new)
    for z in double_zones:
        ibag_new.append((gen_idx, 0))
        igen_new.extend((op, u16(amt)) for op, amt in z)
        gen_idx += len(z)

    if click_mode:
        click_zone = [
            (GEN_KEYRANGE, CLICK_KEYS[0] | (CLICK_KEYS[1] << 8)),
            (34, ms_to_timecents(CLICK_ATTACK_MS)),    # attackVolEnv
            (35, -32768 if CLICK_HOLD_MS <= 0
             else ms_to_timecents(CLICK_HOLD_MS)),     # holdVolEnv (~0)
            (36, ms_to_timecents(CLICK_DECAY_MS)),     # decayVolEnv
            (37, CLICK_SUSTAIN_CB),                    # sustainVolEnv (cB!)
            (38, ms_to_timecents(CLICK_RELEASE_MS)),   # releaseVolEnv
            (48, CLICK_INITIAL_ATTENUATION_CB),        # initialAttenuation
            (GEN_KEYNUM, CLICK_KEYNUM),                # fixed playback rate
            (GEN_SAMPLEID, click_sample_idx),
        ]
        click_zone.sort(key=lambda g: zone_key(g[0]))
        ibag_new.append((gen_idx, 0))       # click instrument's zone bag
        igen_new.extend((op, u16(amt)) for op, amt in click_zone)
        gen_idx += len(click_zone)

    ibag_new.append((gen_idx, 0))           # terminal bag
    igen_new.append((0, 0))                 # terminal gen record

    inst_new = [
        (inst[0][0], inst[0][1]),           # "harmonium" @ bag 0
        (preset_name.ljust(20, b"\0"), inst0_end_bag),
    ]
    if click_mode:
        inst_new.append((b"key click".ljust(20, b"\0"),
                         inst0_end_bag + len(double_zones)))
    inst_new.append((b"EOI".ljust(20, b"\0"), len(ibag_new) - 1))

    # --- presets: keep preset 0's zones byte-identical (including its
    # empty global zone); append one zone per new preset -> its instrument,
    # inserted before pgen's terminal record. An empty preset global zone
    # shares its bag genIdx with the following zone, so each new preset's
    # first bag reuses the previous terminal pbag record and the new
    # terminal pbag record gets the terminal pgen index.
    eop = phdr[1]
    extra_presets = [(preset_name, 1)]
    if click_mode:
        extra_presets.append((b"key click", 2))
    phdr_new = [phdr[0]]
    pbag_new = list(pbag)
    pgen_new = list(pgen)
    for name, prog in extra_presets:
        zone_gen_idx = len(pgen_new) - 1            # current terminal gen
        pgen_new.insert(zone_gen_idx, (GEN_INSTRUMENT, prog))
        phdr_new.append((name.ljust(20, b"\0"), prog, 0,
                         len(pbag_new) - 1) + phdr[0][4:])
        pbag_new.append((len(pgen_new) - 1, 0))     # new terminal bag
    phdr_new.append((eop[0], eop[1], eop[2], len(pbag_new) - 1) + eop[4:])
    pmod_new = records(pdta[b"pmod"], PMOD_IMOD)  # keep terminal record(s)

    # --- serialize pdta (spec-canonical chunk order) ------------------------
    pdta_out = b"".join([
        b"phdr" + struct.pack("<I", len(ph := pack(PHDR, phdr_new))) + ph,
        b"pbag" + struct.pack("<I", len(pb := pack("<HH", pbag_new))) + pb,
        b"pmod" + struct.pack("<I", len(pm := pack(PMOD_IMOD, pmod_new))) + pm,
        b"pgen" + struct.pack("<I", len(pg := pack("<HH", pgen_new))) + pg,
        b"inst" + struct.pack("<I", len(inb := pack(INST, inst_new))) + inb,
        b"ibag" + struct.pack("<I", len(ib := pack("<HH", ibag_new))) + ib,
        b"imod" + struct.pack("<I", len(im := pack(PMOD_IMOD,
                                                   records(pdta[b"imod"],
                                                           PMOD_IMOD)))) + im,
        b"igen" + struct.pack("<I", len(ig := pack("<HH", igen_new))) + ig,
        b"shdr" + struct.pack("<I", len(shdr_new)) + shdr_new,
    ])

    # --- assemble RIFF/sfbk -------------------------------------------------
    def list_chunk(ltype, payload):
        body = ltype + payload
        return b"LIST" + struct.pack("<I", len(body)) + body

    riff_payload = b"sfbk" + list_chunk(b"INFO", info[4:]) \
        + list_chunk(b"sdta", sdta_new[4:]) + list_chunk(b"pdta", pdta_out)
    out = b"RIFF" + struct.pack("<I", len(riff_payload)) + riff_payload
    with open(dst_path, "wb") as f:
        f.write(out)

    print(f"wrote {dst_path} ({len(out)} bytes; original {len(data)} bytes, "
          f"delta +{len(out) - len(data)} — reed sample data reused, "
          f"not copied)")
    print(f"presets: {len(phdr_new) - 1}  instruments: {len(inst_new) - 1}  "
          f"detune: +{detune_i} cents on preset 1 duplicate zones"
          + ("  + key click sample/instrument/preset 2" if click_mode else ""))
    print(f"ibag {len(ibag)} -> {len(ibag_new)}  "
          f"igen {len(igen)} -> {len(igen_new)}"
          + (f"  smpl +{len(click_pcm) // 2} frames" if click_mode else ""))

    if click_mode:
        # ear-proxy audition file (sox is unavailable on this machine)
        try:
            import wave
            import numpy as np
            with wave.open("/tmp/opencode/keyclick_22050.wav", "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(CLICK_RATE)
                w.writeframes(click_pcm)
            print("wrote /tmp/opencode/keyclick_22050.wav (ear-proxy audition)")
        except OSError as e:
            print(f"note: could not write audition wav: {e}")


if __name__ == "__main__":
    main()
