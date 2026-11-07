#!/usr/bin/env bash
# Regenerates every wiring diagram of the notices from its WireViz source:
# docs/notice/phase-N/*.yml -> .gv (layout input, checked by the CI) and .svg.
set -euo pipefail
cd "$(dirname "$0")/.."
shopt -s nullglob
for source in docs/notice/phase-*/*.yml; do
  wireviz -f gs "$source" >/dev/null
  echo "schéma : ${source%.yml}.svg"
done
