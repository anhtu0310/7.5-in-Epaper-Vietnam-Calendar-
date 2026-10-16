#!/usr/bin/env python3
"""Print the packed C row initializer for the fixed BLE setup QR code."""

from reportlab.graphics.barcode import qrencoder

URL = "https://anhtu0310.github.io/VietCalendarTimeSync/"


def main() -> None:
    qr = qrencoder.QRCode(None, qrencoder.QRErrorCorrectLevel.Q)
    qr.addData(URL)
    qr.make()
    print(f"/* {URL} -- {qr.moduleCount}x{qr.moduleCount}, error correction Q */")
    for row in qr.modules:
        packed = []
        for start in range(0, len(row), 8):
            value = 0
            for bit, dark in enumerate(row[start : start + 8]):
                if dark:
                    value |= 0x80 >> bit
            packed.append(f"0x{value:02X}U")
        print("    {" + ", ".join(packed) + "},")


if __name__ == "__main__":
    main()
