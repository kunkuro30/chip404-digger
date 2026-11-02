#!/data/data/com.termux/files/usr/bin/bash
# Chip404 — pengawas FOUND untuk gali HP (Termux).
# Jalankan di SESI TERMUX BARU, jangan ganggu sesi gali yang sedang jalan.
# Memantau grind-phone.log; begitu baris FOUND muncul, langsung POST ke
# webhook agen. Nonce terikat wallet agen — disiarkan pun aman, agen
# memverifikasi ulang (keccak + simulasi 2 RPC) sebelum bertindak.
# Pemakaian: bash termux_watch.sh <webhook-url>
set -u
URL="${1:-}"
if [ -z "$URL" ]; then
  echo "Pemakaian: bash termux_watch.sh <webhook-url>"
  exit 1
fi

find_log() {
  for c in "$HOME/grind-phone.log" "./grind-phone.log" "$HOME/chip404/grind-phone.log"; do
    if [ -f "$c" ]; then LOG="$c"; return 0; fi
  done
  f="$(find "$HOME" -maxdepth 3 -name 'grind-phone.log' 2>/dev/null | head -1)"
  if [ -n "$f" ]; then LOG="$f"; return 0; fi
  return 1
}

LOG=""
echo "Pengawas FOUND aktif — memantau log gali tiap 20 detik."
echo "Biarkan sesi ini terbuka. Ctrl+C untuk berhenti."
while true; do
  if find_log; then
    if grep -q "FOUND nonce=" "$LOG" 2>/dev/null; then
      LINE="$(grep "FOUND nonce=" "$LOG" | tail -1)"
      echo "FOUND terdeteksi: $LINE"
      echo "Mengirim ke agen..."
      ok=0
      for i in $(seq 1 20); do
        code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 15 -X POST -H 'Content-Type: text/plain' -d "redmi-phone $LINE" "$URL" || echo 000)"
        if [ "$code" = "200" ]; then ok=1; break; fi
        sleep 10
      done
      if [ "$ok" = "1" ]; then
        echo "TERKIRIM. Agen akan memverifikasi & menyiapkan mint."
      else
        echo "Gagal kirim setelah 20 percobaan — screenshot baris FOUND dan kirim manual ke Muse."
      fi
      exit 0
    fi
  fi
  sleep 20
done
