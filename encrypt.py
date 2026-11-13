key = 0x5A

with open("fsociety.dll", "rb") as f:
    data = bytearray(f.read())

for i in range(len(data)):
    data[i] ^= key

with open("fsociety.enc", "wb") as f:
    f.write(data)

print("[+] fsociety.dll chiffré dans fsociety.enc")