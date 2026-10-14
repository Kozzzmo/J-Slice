#!/bin/bash
# J-Slice : installation sur macOS (VST3 + Audio Unit), pour l'utilisateur courant.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"

echo "Installation de J-Slice..."
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/J-Slice.vst3" "$AU/J-Slice.component"
cp -R "$DIR/J-Slice.vst3" "$VST3/"
cp -R "$DIR/J-Slice.component" "$AU/"

# Le plugin n'est pas signé par Apple : on retire la "quarantaine" posée par le navigateur,
# sinon macOS refuse de le charger.
xattr -dr com.apple.quarantine "$VST3/J-Slice.vst3" "$AU/J-Slice.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$DIR/J-Slice.app" 2>/dev/null || true

# Force macOS à relire la liste des Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "OK !"
echo "  VST3        -> $VST3/J-Slice.vst3"
echo "  Audio Unit  -> $AU/J-Slice.component"
echo ""
echo "Ableton Live : Settings > Plug-Ins > activer 'Use Audio Units' et/ou 'Use VST3 Plug-In System Folders',"
echo "puis 'Rescan'. J-Slice apparait dans Plug-Ins > Kozzzmo."
