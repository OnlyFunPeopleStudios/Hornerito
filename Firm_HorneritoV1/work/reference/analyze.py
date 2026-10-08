import struct, sys, re

path = sys.argv[1]
data = open(path, 'rb').read()
print("file size:", len(data))

# --- partition table @0x8000 (ESP-IDF entries: magic 0x50AA, 32 bytes) ---
print("\n--- partition table @0x8000 ---")
off = 0x8000
for i in range(20):
    e = data[off + i*32: off + i*32 + 32]
    if len(e) < 32:
        break
    magic, ptype, subtype = struct.unpack('<HBB', e[0:4])
    if magic != 0x50AA:
        break
    pos, size = struct.unpack('<QQ', e[8:24])
    label = e[12-4:12+16].split(b'\x00')[0] if False else e[4+8+0:0]  # placeholder
    label = e[12:28].split(b'\x00')[0].decode('latin1')
    print(f"  {label:16s} type={ptype} subtype={subtype} offset=0x{pos:x} size=0x{size:x} ({size//1024}K)")

# --- FAT / filesystem signatures ---
print("\n--- FAT signatures ---")
for m in re.finditer(b'MSDOS5.0|MSDOS4.1|FAT32   |FAT16   ', data):
    print(f"  {m.group()!r} @0x{m.start():x}")

# --- interesting ASCII strings ---
print("\n--- interesting strings ---")
pats = [b'.mp3', b'.MP3', b'.wav', b'.flac', b'music', b'Music', b'MUSIC',
        b'SD', b'sdcard', b'spiffs', b'FAT', b'Play', b'Pause', b'Next',
        b'Volume', b'Folder', b'TF card', b'microSD']
found = {}
for p in pats:
    idxs = [m.start() for m in re.finditer(re.escape(p), data)][:5]
    if idxs:
        found[p.decode('latin1', 'replace')] = idxs
for k, v in found.items():
    print(f"  {k!r}: {[hex(x) for x in v]}")

# --- printable ASCII strings >= 5 chars, sample ---
print("\n--- sample strings (UI-ish) ---")
strs = re.findall(rb'[\x20-\x7e]{6,}', data)
seen = 0
for s in strs:
    t = s.decode('latin1')
    if re.search(r'(?i)(menu|play|pause|next|prev|volume|music|file|setting|touch|screen|version|wifi|battery|sd |card|error|ok$|back|exit)', t):
        print("   ", t)
        seen += 1
        if seen > 80:
            break
print("total ascii strings >=6:", len(strs))
