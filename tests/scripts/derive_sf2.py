#!/usr/bin/env python3
"""Derive harmonium_v2.sf2 — a double-reed detuned variant of the original
harmonium SoundFont (Phase 3: 2-reed layering, the signature slow beating).

Binary RIFF/sfbk surgery on the ORIGINAL font (the original is never
modified): adds a second preset (bank 0, program 1, "harmonium double")
backed by a NEW instrument that duplicates every key zone of the original
instrument with fineTune = +D cents. The original preset 0 / instrument 0
are copied byte-identically, so preset 0 renders the same sound as today.

SF2 structure created (record counts for this font, 1 preset / 1 instrument
/ 14 zones + 1 global zone in):

  phdr: [harmonium|bag 0] [harmonium double|bag 1] [EOP|bag 2]
  pbag: zone0(gen none) zone1(GEN_INSTRUMENT=0) zone2(GEN_INSTRUMENT=1) + terminal
        (modIdx 0 everywhere; pmod keeps its single terminal record)
  pgen: (41,0) (41,1) (0,0 terminal)
  inst: [harmonium|bag 0] [harmonium double|bag 15] [EOI|bag 44]
  ibag: inst0 keeps bags 0..14 unchanged (15 zones); inst0's original
        terminal bag becomes the first bag of the double instrument (bag 15
        = its global zone), then 28 sample zones (14 originals + 14 +D-cent
        copies), terminal bag 44
  igen: inst0's zone generators unchanged (indices 0..30); the double
        global zone's 3 gens; per key zone: original gens, then a tuned
        copy with fineTune inserted (op 52, signed cents); terminal gen
        (0,0). The original terminal igen record is re-appended at the end.
  imod/pmod/shdr/sdta/INFO: unchanged (the 6.6 MB sample data is REFERENCED,
        not duplicated — the derived file grows only by ~1 KB)

Zone generator order follows the SF2 practice of keyRange (43) first,
velRange (44) second; the remaining generators are sorted by operator
number, which places fineTune (52) before sampleID (53).

Gen-op numbers verified against the SF2 2.x spec and fluidsynth's gen.h:
41=GEN_INSTRUMENT, 43=GEN_KEYRANGE, 44=GEN_VELRANGE, 51=GEN_COARSETUNE,
52=GEN_FINETUNE (range -99..+99 cents per the spec — the task brief said
"fineTune = gen 51"; gen 51 is coarseTune, 52 is fineTune).

Usage:
  derive_sf2.py <original.sf2> <output.sf2> [detune_cents] [preset_name]

Defaults: detune_cents=4, preset_name="harmonium double".
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
GEN_FINETUNE = 52
GEN_SAMPLEID = 53


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


def zone_key(op):
    """Generator ordering: ranges first, then by operator number."""
    return (0 if op in (GEN_KEYRANGE, GEN_VELRANGE) else 1, op)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    src_path = sys.argv[1]
    dst_path = sys.argv[2]
    detune = float(sys.argv[3]) if len(sys.argv) > 3 else 4.0
    if not -99.0 <= detune <= 99.0:
        raise SystemExit("detune cents must be within the SF2 spec range "
                         "-99..+99")
    detune_i = int(round(detune))
    preset_name = (sys.argv[4] if len(sys.argv) > 4 else "harmonium double")
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

    # --- igen/ibag: inst0 keeps its zone bags (0 .. inst0_end_bag-1)
    # unchanged — its original terminal bag doubles as the first bag of the
    # double instrument (the boundary bag between two instruments, exactly
    # like the bag shared between an empty global zone and the next zone).
    # The original terminal igen record is dropped and re-appended at the
    # end, after the double instrument's generators.
    igen_new = list(igen[:-1])              # drop original terminal gen
    ibag_new = list(ibag[:inst0_end_bag])
    gen_idx = len(igen_new)
    for z in double_zones:
        ibag_new.append((gen_idx, 0))
        igen_new.extend(z)
        gen_idx += len(z)
    ibag_new.append((gen_idx, 0))           # terminal bag
    igen_new.append((0, 0))                 # terminal gen record

    inst_new = [
        (inst[0][0], inst[0][1]),           # "harmonium" @ bag 0
        (preset_name.ljust(20, b"\0"), inst0_end_bag),
        (b"EOI".ljust(20, b"\0"), len(ibag_new) - 1),
    ]

    # --- presets: keep preset 0's zones byte-identical (including its
    # empty global zone); append one zone for preset 1 -> instrument 1,
    # inserted before pgen's terminal record. An empty preset global zone
    # shares its bag genIdx with the following zone, so the new zone's bag
    # genIdx is the index where (41, 1) is inserted.
    eop = phdr[1]
    p1_gen_idx = len(pgen) - 1          # before the terminal gen record
    phdr_new = [
        phdr[0],
        (preset_name.ljust(20, b"\0"), 1, 0, len(pbag) - 1) + phdr[0][4:],
        (eop[0], eop[1], eop[2], len(pbag)) + eop[4:],
    ]
    pbag_new = list(pbag[:-1]) + [(p1_gen_idx, 0), (len(pgen), 0)]
    pgen_new = list(pgen[:-1]) + [(GEN_INSTRUMENT, 1), (0, 0)]
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
        b"shdr" + struct.pack("<I", len(shdr)) + shdr,
    ])

    # --- assemble RIFF/sfbk (INFO and sdta reused verbatim) -----------------
    def list_chunk(ltype, payload):
        body = ltype + payload
        return b"LIST" + struct.pack("<I", len(body)) + body

    riff_payload = b"sfbk" + list_chunk(b"INFO", info[4:]) \
        + list_chunk(b"sdta", sdta[4:]) + list_chunk(b"pdta", pdta_out)
    out = b"RIFF" + struct.pack("<I", len(riff_payload)) + riff_payload
    with open(dst_path, "wb") as f:
        f.write(out)

    print(f"wrote {dst_path} ({len(out)} bytes; original {len(data)} bytes, "
          f"delta +{len(out) - len(data)} — sample data reused, not copied)")
    print(f"presets: {len(phdr_new) - 1}  instruments: {len(inst_new) - 1}  "
          f"detune: +{detune_i} cents on preset 1 duplicate zones")
    print(f"ibag {len(ibag)} -> {len(ibag_new)}  "
          f"igen {len(igen)} -> {len(igen_new)}")


if __name__ == "__main__":
    main()
