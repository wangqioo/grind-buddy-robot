#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/firmware/k230-vision"

python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
