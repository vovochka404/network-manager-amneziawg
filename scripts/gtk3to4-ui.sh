#!/bin/sh
# Convert the GTK3 (Glade) dialog into the GTK4 form for the GTK4 editor.
# Usage: gtk3to4-ui.sh <gtk4-builder-tool> <input.ui> <output.ui>
#
# Same approach as NetworkManager-openvpn/-vpnc (gtk4-builder-tool simplify
# --3to4), with the GTK3-only single-line properties that --3to4 leaves in
# place removed first, and a check for constructs it cannot convert.
set -eu
tool=$1
in=$2
out=$3
tmp="$out.tmp"

mkdir -p "$(dirname "$out")"
rm -f "$tmp" "$tmp.in.ui" "$tmp.log"
trap 'rm -f "$tmp" "$tmp.in.ui" "$tmp.log"' EXIT

grep -v -e 'name="can-default"' -e 'name="can_default"' \
        -e 'name="shadow-type"' -e 'name="shadow_type"' "$in" > "$tmp.in.ui"
status=0
"$tool" simplify --3to4 "$tmp.in.ui" > "$tmp" 2> "$tmp.log" || status=$?
cat "$tmp.log" >&2
if [ "$status" -ne 0 ]; then
    echo "$in: $tool simplify --3to4 failed (exit $status)" >&2
    exit 1
fi

# --3to4 keeps properties it does not know and only warns about them
failed=0
if grep -qi 'not found' "$tmp.log"; then
    failed=1
fi
if grep -n -e '<packing>' -e 'GtkButtonBox' -e 'name="type[-_]hint"' "$tmp" >&2; then
    failed=1
fi
if [ "$failed" -ne 0 ]; then
    echo "$in: GTK3-only constructs survived --3to4 (see above); fix the source .ui" >&2
    exit 1
fi
mv "$tmp" "$out"
