from struct import *
import binascii
import re
import sys

format = "<QBhhhB"
entry_bytes = 16
accel_coeff = 0.004 * 9.81

def calc_checksum(data):
    parity = data[0]
    for i in range(1, len(data) - 1):
        parity = parity ^ data[i]

    return parity

if __name__ == "__main__":
    read_bytes = 0
    success = True
    with open("log.hex", "r") as f:
        print("timestamp", "type", "accel_x", "accel_y", "accel_z", sep=",")
        for line in f:
            if line.startswith("start"):
                matches = re.match(r"start\s+(\d+)", line)
                log_bytes = int(matches.group(1))
            elif line.startswith("end"):
                pass
            else:
                bytes = binascii.unhexlify(line.strip().replace(" ", ""))
                if len(bytes) > 0:
                    checksum = calc_checksum(bytes)
                    unpacked = unpack(format, bytes)
                    if checksum != unpacked[5]:
                        print("checksums don't match!", file=sys.stderr)
                        success = False

                    print(unpacked[0], unpacked[1], round(unpacked[2] * accel_coeff, 2),
                                                    round(unpacked[3] * accel_coeff, 2),
                                                    round(unpacked[4] * accel_coeff, 2), sep=",")
                    read_bytes += entry_bytes
        if read_bytes != log_bytes:
            print("error: parsed %d bytes, expected %d" % (read_bytes, log_bytes), file=sys.stderr)
            exit(1)
        else:
            if success:
                print("parsed %d bytes successfully" % read_bytes, file=sys.stderr)
            else:
                print("error: parsed %d bytes, but checksum errors were found" % read_bytes, file=sys.stderr)
                exit(1)
