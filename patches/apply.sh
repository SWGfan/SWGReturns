#!/usr/bin/env bash
# Re-applies our engine3 fixes after any `git submodule update`.
#
# engine3 is an upstream submodule, so anything we change there is wiped
# whenever it is updated. Carrying the fixes as patch files and applying
# them on every build turns a silent time-bomb into a routine step.
#
# Idempotent: a patch already applied is detected and skipped.
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE3="$REPO/MMOCoreORB/utils/engine3"
rc=0
shopt -s nullglob
for p in "$REPO"/patches/*.patch; do
    name="$(basename "$p")"
    if git -C "$ENGINE3" apply --check --reverse "$p" >/dev/null 2>&1; then
        echo "  already applied: $name"
    elif git -C "$ENGINE3" apply "$p" >/dev/null 2>&1; then
        echo "  APPLIED: $name"
    else
        echo "  !! FAILED to apply: $name  (upstream may have changed this code)"
        rc=1
    fi
done
exit $rc
