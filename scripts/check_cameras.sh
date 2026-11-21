#!/usr/bin/env bash
# Probes every camera in the active config with ffprobe (RTSP over TCP).
# Usage: scripts/check_cameras.sh [config.json]
# Credentials come from config/.env (RG_CAMERA_USER / RG_CAMERA_PASSWORD, RG_CAMn_*); they are not printed.
set -eo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -f "$ROOT/config/.env" ]]; then set -a; source "$ROOT/config/.env"; set +a; fi

CONFIG="${1:-}"
if [[ -z "$CONFIG" ]]; then
  for c in "${RG_CONFIG:-}" "$HOME/.config/RoboGuide/Robo Guide/robo_guide.json" "$ROOT/config/robo_guide.json" "$ROOT/config/robo_guide.example.json"; do
    if [[ -n "$c" && -f "$c" ]]; then CONFIG="$c"; break; fi
  done
fi
echo "Config: $CONFIG"
command -v ffprobe >/dev/null || { echo "ffprobe missing: sudo apt install ffmpeg" >&2; exit 1; }

python3 -I - "$CONFIG" <<'PY' | while IFS=$'\t' read -r idx name url redacted; do
import json, os, sys, urllib.parse
cfg = json.load(open(sys.argv[1]))
for i, c in enumerate(cfg.get("cameras", []), start=1):
    if c.get("url"):
        url = red = c["url"]
    elif c.get("host"):
        user = os.environ.get(f"RG_CAM{i}_USER") or os.environ.get("RG_CAMERA_USER", "")
        pw = os.environ.get(f"RG_CAM{i}_PASSWORD") or os.environ.get("RG_CAMERA_PASSWORD", "")
        q = lambda s: urllib.parse.quote(s, safe="")
        auth = (q(user) + (":" + q(pw) if pw else "") + "@") if user else ""
        rauth = (q(user) + (":***" if pw else "") + "@") if user else ""
        path = c.get("stream", "stream2").lstrip("/")
        url = f"rtsp://{auth}{c['host']}:{c.get('port', 554)}/{path}"
        red = f"rtsp://{rauth}{c['host']}:{c.get('port', 554)}/{path}"
    else:
        continue
    print(f"{i}\t{c.get('name', '')}\t{url}\t{red}")
PY
  printf '\n[%s] %s\n    %s\n' "$idx" "$name" "$redacted"
  if out=$(timeout 15 ffprobe -v error -rtsp_transport tcp -select_streams v:0 \
             -show_entries stream=codec_name,width,height,avg_frame_rate -of csv=p=0 "$url" 2>&1); then
    echo "    OK  $out"
  else
    echo "    FAILED  $(echo "$out" | head -1 | sed -E 's#rtsp://[^@ ]*@#rtsp://***@#g')"
  fi
done
