# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

"""Upload files to the board's SD card over the USB serial console.

  python tools/sd_put.py [--port COM22] <local_file> <device_path> [<local> <device> ...]

Device path is absolute on the device, e.g. /sd/story/llm/model.bin.
Also usable with --manifest to push the standard asset set (see ASSETS).
"""
import argparse
import os
import sys
import time
import zlib

import serial

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = [
    ("models_out/llm8m_v8/embed/model_neo_q4.bin", "/sd/story/llm8m_v8/model.bin"),
    ("models_out/llm8m_v8/embed/tok_neo.bin", "/sd/story/llm8m_v8/tok.bin"),
    ("models_out/llm8m_v8/embed/name.txt", "/sd/story/llm8m_v8/name.txt"),
    ("models_out/llm8m_v7/embed/model_neo_q4.bin", "/sd/story/llm8m_v7/model.bin"),
    ("models_out/llm8m_v7/embed/tok_neo.bin", "/sd/story/llm8m_v7/tok.bin"),
    ("models_out/llm8m_v7/embed/name.txt", "/sd/story/llm8m_v7/name.txt"),
    ("models_out/llm8m_v6/embed/model_neo_q4.bin", "/sd/story/llm8m_v6/model.bin"),
    ("models_out/llm8m_v6/embed/tok_neo.bin", "/sd/story/llm8m_v6/tok.bin"),
    ("models_out/llm8m_v6/embed/name.txt", "/sd/story/llm8m_v6/name.txt"),
    ("models_out/llm8m_v5/embed/model_neo_q4.bin", "/sd/story/llm8m_v5/model.bin"),
    ("models_out/llm8m_v5/embed/tok_neo.bin", "/sd/story/llm8m_v5/tok.bin"),
    ("models_out/llm8m_v5/embed/name.txt", "/sd/story/llm8m_v5/name.txt"),
    ("models_out/llm8m_v4/embed/model_neo_q4.bin", "/sd/story/llm8m_v4/model.bin"),
    ("models_out/llm8m_v4/embed/tok_neo.bin", "/sd/story/llm8m_v4/tok.bin"),
    ("models_out/llm8m_v4/embed/name.txt", "/sd/story/llm8m_v4/name.txt"),
    ("models_out/llm8m_kid/embed/model_neo_q4.bin", "/sd/story/llm8m_kid/model.bin"),
    ("models_out/llm8m_kid/embed/tok_neo.bin", "/sd/story/llm8m_kid/tok.bin"),
    ("models_out/llm8m_kid/embed/name.txt", "/sd/story/llm8m_kid/name.txt"),
    ("models_out/llm8m_kidonly/embed/model_neo_q4.bin", "/sd/story/llm8m_kidonly/model.bin"),
    ("models_out/llm8m_kidonly/embed/tok_neo.bin", "/sd/story/llm8m_kidonly/tok.bin"),
    ("models_out/llm8m_kidonly/embed/name.txt", "/sd/story/llm8m_kidonly/name.txt"),
    ("models_out/llm8m/model.bin", "/sd/story/llm8m/model.bin"),
    ("models_out/llm8m/tok.bin", "/sd/story/llm8m/tok.bin"),
    ("models_out/llm3m/embed/model_neo_q4.bin", "/sd/story/llm/model.bin"),
    ("models_out/llm3m/embed/tok_neo.bin", "/sd/story/llm/tok.bin"),
    (".refs/conformer-stt-s3/checkpoints/model.bin", "/sd/story/stt/model.bin"),
    (".refs/esp-picotts/pico/lang/en-US_ta.bin", "/sd/story/tts/en-US_ta.bin"),
    # Second voice, for the demo's question/answer contrast.
    (".refs/esp-picotts/pico/lang/en-GB_ta.bin", "/sd/story/tts/en-GB_ta.bin"),
    (".refs/esp-picotts/pico/lang/en-GB_kh0_sg.bin", "/sd/story/tts/en-GB_kh0_sg.bin"),
    (".refs/esp-picotts/pico/lang/en-US_lh0_sg.bin", "/sd/story/tts/en-US_lh0_sg.bin"),
]


def wait_line(ser, prefixes, timeout):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        c = ser.read(1)
        if not c:
            continue
        if c == b"\n":
            line = buf.decode("utf-8", "replace").strip()
            buf = b""
            for p in prefixes:
                if line.startswith(p):
                    return line
        else:
            buf += c
    raise TimeoutError(f"timeout waiting for {prefixes}")


def put(ser, local, remote):
    data = open(local, "rb").read()
    crc = zlib.crc32(data) & 0xFFFFFFFF
    ser.reset_input_buffer()
    ser.write(f"put {remote} {len(data)} {crc:08x}\n".encode())
    line = wait_line(ser, ["PUT READY", "PUT ERR"], 10)
    if line.startswith("PUT ERR"):
        raise RuntimeError(line)
    t0 = time.time()
    CH = 32 * 1024
    for off in range(0, len(data), CH):
        ser.write(data[off:off + CH])
        line = wait_line(ser, ["ACK", "PUT ERR"], 20)
        if line.startswith("PUT ERR"):
            raise RuntimeError(line)
        done = int(line.split()[1])
        sys.stdout.write(f"\r  {remote}: {done * 100 // len(data)}%  {done / 1024 / (time.time() - t0):.0f} KB/s ")
        sys.stdout.flush()
    line = wait_line(ser, ["PUT DONE", "PUT ERR"], 20)
    print("\n  " + line)
    if not line.startswith("PUT DONE"):
        raise RuntimeError(line)
    wait_line(ser, ["OK"], 5)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM22")
    ap.add_argument("--manifest", action="store_true", help="push the standard asset set")
    ap.add_argument("--only", default="", help="substring filter for --manifest")
    ap.add_argument("pairs", nargs="*")
    a = ap.parse_args()
    pairs = []
    if a.manifest:
        pairs = [(os.path.join(ROOT, l), r) for l, r in ASSETS if a.only in r]
    pairs += list(zip(a.pairs[0::2], a.pairs[1::2]))
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = a.port, 115200, 0.1
    ser.dtr = ser.rts = False
    ser.open()
    for l, r in pairs:
        if not os.path.exists(l):
            print(f"missing local file {l}")
            sys.exit(1)
        put(ser, l, r)
    ser.close()


if __name__ == "__main__":
    main()
