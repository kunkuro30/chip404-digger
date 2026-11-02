#!/data/data/com.termux/files/usr/bin/bash
# Chip404 — gali PoW dari HP sendiri (Termux). 100% perangkat sendiri.
# Pakai sambil HP DICAS (panas + boros baterai). Hentikan kapan saja: Ctrl+C.
# Pemakaian: bash termux_grind.sh [bits] [offset]
#   bits   : target bit (default 45)
#   offset : pangkal partisi HP ini (default 3000000001) — jangan diubah
#            kecuali menjalankan lebih dari satu perangkat.
set -e
BITS="${1:-45}"
OFFSET="${2:-3000000001}"
SALT="445aa0838438a376563c290a2c15cf7ddea0883854555b68b0e01efefafff39a"
WALLET="757B2d9D9e0E456B080d671596Af184CA758eC9a"

command -v clang >/dev/null 2>&1 || pkg install -y clang
command -v curl >/dev/null 2>&1 || pkg install -y curl

if [ ! -x ./solver2 ]; then
  echo "Mengambil solver dari repo sendiri..."
  curl -sL -o solver2.c https://raw.githubusercontent.com/kunkuro30/chip404-digger/main/solver2.c
  clang -O3 -march=native -pthread -o solver2 solver2.c
fi

echo "Self-test dulu (wajib cocok dua baris hash resmi)..."
./solver2 --test "$SALT" "$WALLET" | tee test.log
grep -q "3aae4333589750d8e637262e38e5838a2d9a869ad7cd7ab92d100797c9e69997" test.log
grep -q "a84cf489abca3349075e8c0507105dfdefe103faefbdee21cd46a01ea9df0012" test.log
echo "SELF-TEST OK — mulai gali ${BITS} bit dari offset ${OFFSET} (8 thread)."
echo "Kalau muncul baris 'FOUND nonce=...' — screenshot & kirim ke Muse SEGERA."
termux-wake-lock 2>/dev/null || true
# stride 32: 8 thread lokal terpisah dari partisi armada GitHub
./solver2 "$SALT" "$WALLET" "$BITS" 8 30 "$OFFSET" 32 2>&1 | tee grind-phone.log
