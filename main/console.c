// Line-based serial console over USB-Serial/JTAG, used to drive tests from
// the host (tools/serial_cmd.py).
#include "console.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <stdio.h>
#include <string.h>

void console_init(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 1024;
    usb_serial_jtag_driver_install(&cfg);
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);
}

bool console_readline(char *buf, size_t cap, uint32_t timeout_ms)
{
    static char acc[160];
    static size_t n;
    uint8_t c;
    uint32_t waited = 0;
    while (1) {
        int r = usb_serial_jtag_read_bytes(&c, 1, pdMS_TO_TICKS(10));
        if (r <= 0) {
            waited += 10;
            if (timeout_ms && waited >= timeout_ms) return false;
            continue;
        }
        if (c == '\r') continue;
        if (c == '\n') {
            acc[n] = 0;
            strncpy(buf, acc, cap - 1);
            buf[cap - 1] = 0;
            n = 0;
            return true;
        }
        if (n < sizeof acc - 1) acc[n++] = (char)c;
    }
}
