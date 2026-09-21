#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# The collaborator workspace has URL-less legacy gitlinks. They are unused.
git submodule sync -- "external dependencies/OpenIGTLink" "external dependencies/ws_smartneedle" "external dependencies/SmartNeedleIGTL-3DSlicer"
git -c submodule.recurse=false submodule update --init --checkout -- "external dependencies/OpenIGTLink" "external dependencies/ws_smartneedle" "external dependencies/SmartNeedleIGTL-3DSlicer"
