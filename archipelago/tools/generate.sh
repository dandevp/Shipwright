#!/bin/sh
# Generate a multiworld with this repo's apworld.
#   [AP_DIR=...] [AP_YAMLS=dir] archipelago/tools/generate.sh [extra Generate.py args]
# AP_DIR is an Archipelago checkout (0.6.7+) with a .venv and worlds/oot_soh linked to archipelago/oot_soh.
# Players are the YAMLs in $AP_YAMLS (default: archipelago/tools/yamls), output goes to $AP_OUT
# (default: <AP_DIR>/output-soh).
set -e
here=$(cd "$(dirname "$0")" && pwd)
AP_DIR=${AP_DIR:-$HOME/code/ship/Archipelago}
AP_OUT=${AP_OUT:-$AP_DIR/output-soh}
yamls=${AP_YAMLS:-$here/yamls}
ln -sfn "$here/../oot_soh" "$AP_DIR/worlds/oot_soh"
mkdir -p "$AP_OUT"
cd "$AP_DIR"
. .venv/bin/activate
# stdin closed: on an error Generate.py waits for Enter otherwise
exec python Generate.py --player_files_path "$yamls" --outputpath "$AP_OUT" "$@" </dev/null
