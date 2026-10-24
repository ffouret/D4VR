#!/usr/bin/env bash
set -euo pipefail
task_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ "$#" -eq 0 ]; then
    exec python3 "$task_root/tools/d4vr.py" run
fi
exec python3 "$task_root/tools/d4vr.py" "$@"
