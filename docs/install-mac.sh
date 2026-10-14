#!/bin/bash
# Kinetik Jungle (KJNG-2) : installation sur macOS (VST3 + Audio Unit), pour l'utilisateur courant.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
NAME="Kinetik Jungle"

echo "Installation de $NAME..."
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/$NAME.vst3" "$AU/$NAME.component"
cp -R "$DIR/$NAME.vst3" "$VST3/"
cp -R "$DIR/$NAME.component" "$AU/"

# Le plugin n'est pas signé par Apple : on retire la "quarantaine" posée par le navigateur,
# sinon macOS refuse de le charger.
xattr -dr com.apple.quarantine "$VST3/$NAME.vst3" "$AU/$NAME.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$DIR/$NAME.app" 2>/dev/null || true

# Ancienne version : J-Slice est le même plugin sous son ancien nom (mêmes identifiants),
# les deux ne doivent pas être installés en même temps.
if [ -e "$VST3/J-Slice.vst3" ] || [ -e "$AU/J-Slice.component" ]; then
  rm -rf "$VST3/J-Slice.vst3" "$AU/J-Slice.component"
  echo "Ancienne version J-Slice retirée (Kinetik Jungle la remplace, tes projets Ableton continuent de marcher)."
fi

# Force macOS à relire la liste des Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "OK !"
echo "  VST3        -> $VST3/$NAME.vst3"
echo "  Audio Unit  -> $AU/$NAME.component"
echo ""
echo "Ableton Live : Settings > Plug-Ins > activer 'Use Audio Units' et/ou 'Use VST3 Plug-In System Folders',"
echo "puis 'Rescan'. $NAME apparait dans Plug-Ins > Kozzzmo."
