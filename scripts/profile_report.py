#!/usr/bin/env python3
"""Summarize a System.profileStop() sample file.

usage: profile_report.py <unstripped.elf> <samples.bin> [top_n] [quickjs.c]

The ELF must be the exact build that produced the samples, linked without
stripping (make EE_STRIP=true). Prints the hottest functions and lines; when
the path to the matching quickjs.c is given, samples inside the interpreter
loop are also grouped by the bytecode handler (CASE(OP_x)) they fall in,
inlined helpers included.
"""
import collections
import os
import struct
import subprocess
import sys

ADDR2LINE = os.environ.get("ADDR2LINE", "mips64r5900el-ps2-elf-addr2line")


def resolve(elf, addrs):
    """addr -> [(func, file:line), ...] innermost first (inline chain)."""
    out = subprocess.run([ADDR2LINE, "-a", "-f", "-i", "-C", "-e", elf] + ["%x" % a for a in addrs],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    frames, current = {}, None
    i = 0
    while i < len(out):
        if out[i].startswith("0x"):
            current = int(out[i], 16)
            frames[current] = []
            i += 1
        else:
            frames[current].append((out[i], out[i + 1].split(" (")[0]))
            i += 2
    return frames


def main():
    elf, path = sys.argv[1], sys.argv[2]
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 40

    data = open(path, "rb").read()
    pcs = struct.unpack("<%dI" % (len(data) // 4), data)
    total = len(pcs)
    if not total:
        print("no samples")
        return

    handler_of = None
    if len(sys.argv) > 4:
        handler_of, current = {}, None
        for n, text in enumerate(open(sys.argv[4], encoding="utf-8", errors="replace"), 1):
            if "CASE(OP_" in text:
                current = text.strip().split("CASE(")[1].split(")")[0]
            handler_of[n] = current

    counts = collections.Counter(pcs)
    frames = resolve(elf, sorted(counts))

    funcs, lines, handlers = collections.Counter(), collections.Counter(), collections.Counter()
    for addr, n in counts.items():
        chain = frames.get(addr) or [("??", "??:0")]
        func, loc = chain[0]
        funcs[func] += n
        lines["%s  %s" % ("/".join(loc.split("/")[-2:]), func)] += n
        if handler_of is not None:
            outer_func, outer_loc = chain[-1]
            if outer_func == "JS_CallInternal" and outer_loc.split(":")[0].endswith("quickjs.c"):
                try:
                    handlers[handler_of.get(int(outer_loc.rsplit(":", 1)[1])) or "(prologue)"] += n
                except ValueError:
                    pass

    print("%d samples\n\n-- functions --" % total)
    for name, n in funcs.most_common(top):
        print("%6.2f%%  %6d  %s" % (100.0 * n / total, n, name))
    print("\n-- lines --")
    for name, n in lines.most_common(top):
        print("%6.2f%%  %6d  %s" % (100.0 * n / total, n, name))
    if handlers:
        print("\n-- bytecode handlers (JS_CallInternal, inlined helpers included) --")
        for name, n in handlers.most_common(top):
            print("%6.2f%%  %6d  %s" % (100.0 * n / total, n, name))


if __name__ == "__main__":
    main()
