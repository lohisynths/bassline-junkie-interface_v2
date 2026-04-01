import termios
import select
import os

PORT = "/dev/ttyUSB0"
BAUD = termios.B1000000


def open_serial(port: str) -> int:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

    attrs = termios.tcgetattr(fd)

    # raw mode
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CREAD | termios.CLOCAL | termios.CS8
    attrs[3] = 0

    # set baudrate (portable way)
    attrs[4] = BAUD  # input speed
    attrs[5] = BAUD  # output speed

    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 1

    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def midi_len(status: int) -> int:
    if 0x80 <= status <= 0xBF:
        return 3
    if 0xC0 <= status <= 0xDF:
        return 2
    if 0xE0 <= status <= 0xEF:
        return 3
    if status == 0xF2:
        return 3
    if status in (0xF1, 0xF3):
        return 2
    return 1


def parse_midi(byte_stream):
    running = None
    msg = []

    for b in byte_stream:

        # realtime messages (can appear anywhere)
        if 0xF8 <= b <= 0xFF:
            yield [b]
            continue

        # status byte
        if b & 0x80:
            running = b
            msg = [b]

            if midi_len(b) == 1:
                yield msg
                msg = []
                if b >= 0xF0:
                    running = None
            continue

        # data byte
        if running is None:
            continue

        if not msg:
            msg = [running]

        msg.append(b)

        if len(msg) == midi_len(msg[0]):
            yield msg

            if 0x80 <= msg[0] <= 0xEF:
                msg = []  # running status continues
            else:
                msg = []
                running = None


def describe(msg):
    s = msg[0]

    if 0x90 <= s <= 0x9F and len(msg) == 3:
        ch = (s & 0x0F)
        note, vel = msg[1], msg[2]
        return f"NOTE_ON  ch={ch} note={note} vel={vel}"

    if 0x80 <= s <= 0x8F and len(msg) == 3:
        ch = (s & 0x0F)
        note, vel = msg[1], msg[2]
        return f"NOTE_OFF ch={ch} note={note} vel={vel}"

    if 0xB0 <= s <= 0xBF and len(msg) == 3:
        ch = (s & 0x0F)
        cc, val = msg[1], msg[2]
        return f"CC       ch={ch} cc={cc} val={val}"

    if 0xC0 <= s <= 0xCF and len(msg) == 2:
        ch = (s & 0x0F)
        return f"PROGRAM  ch={ch} program={msg[1]}"

    if s == 0xF8:
        return "CLOCK"
    if s == 0xFA:
        return "START"
    if s == 0xFC:
        return "STOP"

    return "RAW " + " ".join(f"{b:02X}" for b in msg)


def read_bytes(fd):
    while True:
        r, _, _ = select.select([fd], [], [], 1.0)
        if fd in r:
            data = os.read(fd, 256)
            for b in data:
                yield b


def main():
    fd = open_serial(PORT)
    print(f"Listening on {PORT} @1000000")

    try:
        for msg in parse_midi(read_bytes(fd)):
            print(describe(msg))
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
