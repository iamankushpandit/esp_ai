# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

"""Talk to the board's serial console.

  python tools/serial_cmd.py [--port COM22] [--reset] [--wait SECONDS] [--until TEXT] [cmd ...]

--reset   : pulse RTS to reboot, then capture boot log until --until/--wait.
Each cmd is sent as a line; output is printed until the board prints "OK"
(or the per-command timeout expires).
"""
import argparse
import sys
import time

import serial


def read_until(ser, needle, timeout):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            sys.stdout.write(chunk.decode("utf-8", "replace"))
            sys.stdout.flush()
            buf += chunk
            if needle and needle.encode() in buf:
                return True
        else:
            time.sleep(0.02)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM22")
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--wait", type=float, default=8)
    ap.add_argument("--until", default=None)
    ap.add_argument("--timeout", type=float, default=60, help="per-command timeout")
    ap.add_argument("cmds", nargs="*")
    a = ap.parse_args()

    ser = serial.Serial()
    ser.port = a.port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.dtr = False
    ser.rts = False
    ser.open()
    if a.reset:
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        ser.close()
        # USB-Serial/JTAG re-enumerates on reset
        for _ in range(100):
            time.sleep(0.1)
            try:
                ser.open()
                break
            except serial.SerialException:
                pass
        read_until(ser, a.until, a.wait)
    else:
        ser.reset_input_buffer()
    for c in a.cmds:
        ser.write((c + "\n").encode())
        ok = read_until(ser, "\nOK", a.timeout)
        if not ok:
            print(f"\n[serial_cmd] timeout waiting for OK after '{c}'")
    ser.close()


if __name__ == "__main__":
    main()
