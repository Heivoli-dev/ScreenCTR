#!/usr/bin/env python3
"""ScreenCTR end-to-end test against tools/mock_console.

This client re-implements the wire protocol from protocol/PROTOCOL.md in
Python (independently of the C library) and checks: handshake, both video
streams (decoded by Pillow), frame skipping of a static screen, PING/PONG,
quality change, remote input relay to a fake Rosalina, the 250 ms release
watchdog, release on BYE, busy rejection and denied authorisation.

usage: e2e_test.py <path to mock_console> [--loss 0.05]
"""
import io, os, socket, struct, subprocess, sys, time
from PIL import Image

MOCK = sys.argv[1]
LOSS = float(sys.argv[sys.argv.index("--loss") + 1]) if "--loss" in sys.argv else 0.0
BASE = 20000 + os.getpid() % 20000
CTRL, VIDEO, INPUT, ROSA = BASE, BASE + 1, BASE + 2, BASE + 3
results = []

def check(name, cond, detail=""):
    results.append(cond)
    print(f"{'OK  ' if cond else 'FAIL'} {name}{(' — ' + detail) if detail else ''}")

def ctrl(t, payload=b""):
    return struct.pack("<HB", len(payload) + 1, t) + payload

def hello(name="e2e-test", want_input=True, cid=b"\x42" * 16):
    return ctrl(0x01, struct.pack("<IHHH", 0x52544353, 1, 1 if want_input else 0, VIDEO) + name.encode().ljust(32, b"\0") + cid)

class Ctrl:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", CTRL), timeout=3)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.buf = b""
    def send(self, data): self.s.sendall(data)
    def recv_msg(self, timeout=3.0):
        end = time.time() + timeout
        while True:
            if len(self.buf) >= 2:
                (l,) = struct.unpack_from("<H", self.buf)
                if len(self.buf) >= l + 2:
                    t, p = self.buf[2], self.buf[3:l + 2]
                    self.buf = self.buf[l + 2:]
                    return t, p
            left = end - time.time()
            if left <= 0: return None, None
            self.s.settimeout(left)
            try:
                d = self.s.recv(4096)
            except socket.timeout:
                return None, None
            if not d: return "closed", b""
            self.buf += d
    def wait_for(self, t, timeout=3.0):
        end = time.time() + timeout
        while time.time() < end:
            mt, p = self.recv_msg(end - time.time())
            if mt == t: return p
            if mt in (None, "closed"): return None
        return None

def start_mock(*extra):
    p = subprocess.Popen([MOCK, "--port", str(CTRL), "--input-port", str(INPUT), "--rosalina-port", str(ROSA),
                          "--loss", str(LOSS), *extra], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(0.3)
    return p

def receive_video(sock, token, seconds):
    frames = {0: [], 1: []}
    cur = {0: None, 1: None}
    end = time.time() + seconds
    sock.settimeout(0.05)
    while time.time() < end:
        try:
            d, _ = sock.recvfrom(2048)
        except socket.timeout:
            continue
        magic, ver, scr, tok, fid, idx, cnt, cap = struct.unpack_from("<HBBIIHHI", d)
        if magic != 0x4353 or ver != 1 or tok != token: continue
        c = cur[scr]
        if c is None or ((fid - c["id"]) & 0xFFFFFFFF) < 0x80000000 and fid != c["id"]:
            c = cur[scr] = {"id": fid, "cnt": cnt, "parts": {}}
        if fid != c["id"]: continue
        c["parts"][idx] = d[20:]
        if len(c["parts"]) == c["cnt"]:
            data = b"".join(c["parts"][i] for i in range(c["cnt"]))
            frames[scr].append((fid, data, time.time()))
            cur[scr] = {"id": fid, "cnt": 0, "parts": {}, "done": True}
    return frames

def main():
    vsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); vsock.bind(("0.0.0.0", VIDEO))
    vsock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    rsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); rsock.bind(("127.0.0.1", ROSA))
    isock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mock = start_mock()
    try:
        # ---- handshake
        c = Ctrl(); c.send(hello())
        check("AUTH_PENDING reçu", c.wait_for(0x02) is not None)
        w = c.wait_for(0x03)
        check("WELCOME reçu", w is not None and len(w) == 10)
        token, ver, iport, allowed, model = struct.unpack("<IHHBB", w)
        check("contrôle autorisé + port commandes", allowed == 1 and iport == INPUT)

        # ---- video
        fr = receive_video(vsock, token, 3.0)
        top, bot = fr[0], fr[1]
        fps_top = len(top) / 3.0
        check("flux écran supérieur ≥ 25 FPS", fps_top >= 25 * (1 - 3 * LOSS) if LOSS else fps_top >= 25, f"{fps_top:.1f} FPS")
        ok_dec = True
        for scr, lst, size in ((0, top, (400, 240)), (1, bot, (320, 240))):
            for _, data, _ in lst[:20]:
                im = Image.open(io.BytesIO(data)); im.load()
                ok_dec &= im.size == size and im.format == "JPEG"
        check("JPEG décodables (Pillow) aux bonnes tailles", ok_dec and len(top) > 0 and len(bot) > 0)
        # bottom screen is static: sent once, repeated at +100/+300 ms, then ~1x/s
        check("écran statique : 1 envoi + répétitions, ≈1 image/s ensuite", 1 <= len(bot) <= 6, f"{len(bot)} images en 3 s")
        # animated content check: white box moves 4 px/frame on the top screen
        if len(top) >= 2:
            def box_x(data):
                im = Image.open(io.BytesIO(data)).convert("L")
                row = [im.getpixel((x, 120)) for x in range(400)]
                xs = [x for x, v in enumerate(row) if v > 240]
                return xs[0] if xs else -1
            (i1, d1, _), (i2, d2, _) = top[0], top[-1]
            dx = box_x(d2) - box_x(d1)
            expect = (4 * (i2 - i1)) % 360
            check("contenu correct (déplacement mesuré = attendu)", abs(dx - expect) <= 2 or abs(dx - expect) >= 358, f"{dx} px vs {expect} px")
        sizes70 = sum(len(d) for _, d, _ in top) / max(1, len(top))

        # ---- ping
        t0 = time.time(); c.send(ctrl(0x20, struct.pack("<II", 7, 1234)))
        p = c.wait_for(0x21)
        rtt = (time.time() - t0) * 1000
        check("PONG identique au PING", p == struct.pack("<II", 7, 1234), f"RTT {rtt:.1f} ms")
        st = c.wait_for(0x30, 2.5)
        if st:
            f0, f1, e0, e1, bps, skipped, _ = struct.unpack("<HHHHIHH", st)
            check("STATS reçues", f0 > 0, f"{f0/10:.1f} FPS haut, encodage {e0} µs, {bps/1024:.0f} Kio/s, {skipped} ignorées")
        else:
            check("STATS reçues", False)

        # ---- quality change
        c.send(ctrl(0x10, bytes([20, 30, 3, 0])))
        time.sleep(0.3); vsock.settimeout(0.01)
        try:
            while True: vsock.recvfrom(2048)
        except socket.timeout:
            pass
        fr = receive_video(vsock, token, 1.0)
        sizes20 = sum(len(d) for _, d, _ in fr[0]) / max(1, len(fr[0]))
        check("qualité 20 → images plus petites", sizes20 < sizes70 * 0.8, f"{sizes70:.0f} → {sizes20:.0f} octets")

        # ---- input relay
        def inp(seq, buttons=0, cx=0, cy=0, touch=None, tok=token):
            flags, tx, ty = (1, *touch) if touch else (0, 0, 0)
            return struct.pack("<HBBIIIhhhhHH", 0x4953, 1, flags, tok, seq, buttons, cx, cy, 0, 0, tx, ty)
        rsock.settimeout(0.5)
        def drain():
            rsock.settimeout(0.01)
            try:
                while True: rsock.recvfrom(64)
            except socket.timeout:
                pass
            rsock.settimeout(0.5)
        drain()
        isock.sendto(inp(1, buttons=1), ("127.0.0.1", INPUT))   # A
        d, _ = rsock.recvfrom(64)
        pad, touch, circle, cpp, special = struct.unpack("<IIIII", d)
        check("A relayé à Rosalina (actif bas)", pad == 0xFFE and touch == 0x02000000 and circle == 0x7FF7FF, f"pad={pad:#x}")
        isock.sendto(inp(2, touch=(160, 120), cy=32767), ("127.0.0.1", INPUT))
        d, _ = rsock.recvfrom(64)
        pad, touch, circle, cpp, special = struct.unpack("<IIIII", d)
        check("tactile + Circle Pad convertis", touch == (1 << 24) | ((120 * 4095 // 239) << 12) | (160 * 4095 // 319) and (circle >> 12) == 0xFFE,
              f"touch={touch:#x} circle={circle:#x}")
        isock.sendto(inp(1, buttons=2), ("127.0.0.1", INPUT))   # stale seq
        isock.sendto(inp(3, buttons=2, tok=token ^ 1), ("127.0.0.1", INPUT))  # wrong token
        # Only a neutral packet (from the 250 ms watchdog) may arrive, never B (0xFFD)
        stale_ignored, end = True, time.time() + 0.4
        rsock.settimeout(0.05)
        while time.time() < end:
            try:
                d, _ = rsock.recvfrom(64)
                if struct.unpack_from("<I", d)[0] & 0x2 == 0: stale_ignored = False
            except socket.timeout:
                pass
        rsock.settimeout(0.5)
        check("paquets périmés / mauvais jeton ignorés", stale_ignored)
        # watchdog
        isock.sendto(inp(10, buttons=1), ("127.0.0.1", INPUT)); rsock.recvfrom(64)
        t0 = time.time(); rsock.settimeout(1.0)
        d, _ = rsock.recvfrom(64); dt = (time.time() - t0) * 1000
        check("relâchement automatique après 250 ms sans paquet",
              d == struct.pack("<IIIII", 0xFFF, 0x02000000, 0x7FF7FF, 0x80800081, 0) and 230 <= dt <= 400, f"{dt:.0f} ms")

        # ---- busy
        c2 = Ctrl(); c2.send(hello("intrus"))
        r = c2.wait_for(0x04)
        check("2e PC refusé (occupé)", r == bytes([3]))

        # ---- BYE releases input
        drain()
        isock.sendto(inp(20, buttons=1), ("127.0.0.1", INPUT)); rsock.recvfrom(64)
        c.send(ctrl(0x7F)); t0 = time.time()
        d, _ = rsock.recvfrom(64)
        check("BYE → commandes relâchées immédiatement", d[:4] == struct.pack("<I", 0xFFF) and time.time() - t0 < 0.2)
    finally:
        mock.terminate(); out = mock.communicate(timeout=3)[0]

    # ---- denied authorisation
    mock = start_mock("--deny")
    try:
        c = Ctrl(); c.send(hello())
        c.wait_for(0x02)
        check("refus sur la console → REJECT 2", c.wait_for(0x04) == bytes([2]))
    finally:
        mock.terminate(); mock.communicate(timeout=3)

    print("\n--- journal de la fausse console ---\n" + out.strip())
    print(f"\n{sum(results)}/{len(results)} vérifications réussies (perte simulée {LOSS*100:.0f} %)")
    sys.exit(0 if all(results) else 1)

if __name__ == "__main__":
    main()
