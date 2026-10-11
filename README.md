# J-Slice

**Slicer + générateur de rythmiques jungle / drum and bass** (VST3 pour Ableton Live et autres DAW, Windows).

Tu charges un break de batterie, J-Slice le découpe automatiquement aux bons endroits, reconnaît
les kicks / snares / hats / ghost notes, puis génère des séquences dans le style choisi (Jungle 94,
Ragga, Rinse-Out Choppage, Two-Step, Rollers, Liquid, Neurofunk, Halftime, Drumfunk, Breakcore),
synchronisées au tempo d'Ableton.

![J-Slice (skin NEON)](docs/screenshot.png)
![J-Slice (skin MIDNIGHT)](docs/screenshot-midnight.png)

## Télécharger et installer

Page **Releases** du dépôt → **latest** (https://github.com/Kozzzmo/J-Slice/releases/tag/latest).

### Windows
1. Télécharge `J-Slice-Windows.zip`, dézippe.
2. Copie **tout le dossier** `J-Slice.vst3` dans `C:\Program Files\Common Files\VST3`.
3. Ableton Live 12 : *Settings → Plug-Ins* → **Use VST3 Plug-In System Folders** → **Rescan**.

### Mac (Apple Silicon et Intel, macOS 11+) : VST3 et Audio Unit
1. Télécharge `J-Slice-macOS.zip`, dézippe.
2. Ouvre le **Terminal**, tape `bash ` (avec un espace), glisse `install.sh` du dossier dans la
   fenêtre, puis Entrée. Le script installe le VST3 et l'AU et lève le blocage de macOS (le plugin
   n'est pas signé par Apple).
3. Ableton Live 12 : *Settings → Plug-Ins* → **Use Audio Units** et/ou **Use VST3 Plug-In System
   Folders** → **Rescan**.

Puis *Plug-Ins → Kozzzmo → J-Slice* : glisse-le sur une piste MIDI. Les zips contiennent aussi une
version autonome (`J-Slice.exe` / `J-Slice.app`) pour tester sans Ableton.

## Prise en main en 30 secondes

1. **BROWSE** (ou clic sur le nom du break, ou glisser-déposer un fichier sur le plugin) → choisis un
   break. Avec *Load on click*, les flèches haut/bas chargent les fichiers un par un. **+ FAV**
   mémorise un dossier dans tes favoris.
2. Vérifie **BREAK BPM** et **BARS** (glisser, ou double-clic pour taper la valeur ; /2, x2,
   **AUTO** revient à la détection).
3. Choisis un **STYLE** à droite : ça charge ses réglages et génère un pattern.
4. Lance Ableton (ou **PLAY**). **GENERATE** = nouveau pattern, **MUTATE** = variations,
   **FILL!** = fill à la mesure suivante.

## L'interface

### En-tête
- **SWING**, **PITCH** : glisser haut/bas, molette, double-clic pour taper.
- **SYNC** (allumé par défaut) : J-Slice suit le tempo et la position de mesure d'Ableton ; le
  break est recalé sur le tempo du projet. Éteint : J-Slice tourne à son propre tempo (case **BPM**).
- **BPM** : affiche le tempo utilisé (HOST = celui d'Ableton). Le modifier passe en tempo libre.
- **SKIN** (en bas à droite) : NEON, MIDNIGHT ou AMEN.

### Écran du break
- Étiquettes colorées : **K** kick, **S** snare, **H** hat/cymbale, **G** ghost note.
- Clic : écouter une slice. Glisser un marqueur : le déplacer.
- **Double-clic sur une étiquette (bande du haut) : supprimer la slice.**
- Double-clic dans la forme d'onde : ajouter un marqueur. Clic droit : classe, supprimer.
- **SLICER** *Smart* / *Transient* / *Grid*, **SENSITIVITY**, **GRID**, **PRE-ROLL** : la découpe se
  met à jour **en direct** pendant que tu bouges le réglage.

### Séquenceur (jusqu'à 128 pas + mesure de FILL)
| Geste | Action |
|------|--------|
| Clic | ajouter / effacer une frappe |
| **Clic droit** | **ajouter une frappe à l'envers** (ou inverser une frappe existante) |
| **Clic maintenu + gauche / droite** | **raccourcir / rallonger** la frappe |
| **Clic maintenu + haut / bas** | **re-triggers** (x1 à x8 dans le pas) |
| Ctrl + glisser haut/bas | vélocité |
| Molette | changer de slice |
| Shift + clic | verrouiller (gardé quand on régénère) |
| Alt + clic | écouter |
| Shift + clic droit | menu (slice, pitch, longueur, vélocité, effets…) |
| **Flèches gauche / droite** | **slice précédente / suivante sur le pas sélectionné** (le dernier pas cliqué) |

#### Effets par pas
Les onglets au-dessus de la grille choisissent ce que le clic modifie :
- **NOTES** : édition normale des frappes.
- **DELAY** / **REVERB** : clique ou glisse sur des pas pour envoyer **seulement ces frappes** dans
  le delay ou la reverb (« throws » façon dub). L'effet s'allume tout seul. Sa **SOURCE**
  (onglet SPACE) : *All* (tout le break), *Snares+marked* (snares + pas marqués) ou *Marked only*
  (uniquement les pas marqués).
- **LO-FI** : ces frappes seulement passent en 8 kHz / 5 bits.
- **FILTER** : ces frappes seulement passent dans un filtre « téléphone ».
- **CLEAR FX** retire l'effet du mode courant de tous les pas (en mode NOTES : tous les effets).
- Dans la grille, des petits carrés colorés en bas à gauche d'un pas indiquent ses effets.

- **L:64** avec **-** / **+** : longueur du pattern en pas (16 = 1 mesure ; shift = une mesure).
- **FILL** (bouton à gauche de la ligne de fill) : éteint par défaut. Allumé, la ligne FILL remplace
  la dernière mesure **toutes les 2 / 4 / 8 mesures** (menu à côté de la longueur).
- **SEED**, **UNDO / REDO**, **-LOCKS**, **SEQ** (off = jouer les slices au clavier, C1 = slice 1).
- **KEEP GROOVE ON LOAD** : en changeant de break, le pattern est conservé et chaque frappe est
  remplacée par une frappe du même type.

### Macros (à droite)
**COMPLEXITY** (découpe, coupes en 1/16, rolls, répétitions, fills), **ENERGY** (densité, ghosts,
hats), **CHAOS** (bégaiements, reverse, pitch, frappes courtes, variations). **À 0 (par défaut) le
style est joué tel quel** ; en montant, les réglages correspondants sont poussés jusqu'au maximum.

### Onglet GENERATOR
Density, Backbone (respect du squelette kick/snare du style), Ghosts, Hats, Chop, 16th cut, Repeats,
Rolls, Stutter, Reverse, Pitch %, Pitch range, Short, Variation, Fill amount, Humanize.

### Onglet PLAYBACK
- **TEMPO MODE** :
  - *Repitch (vintage)* : le break est accéléré comme sur un sampler des années 90 (le pitch monte) ;
  - *Slice (original speed)* : chaque frappe garde sa hauteur et sa vitesse ;
  - *Cyclic stretch (Akai)* : time-stretch métallique façon Akai S950/S1000 ;
  - *Stretch (clean)* : time-stretch propre, la hauteur ne bouge pas et le break suit le tempo.
  **GRAIN** règle la taille des cycles des deux modes stretch.
- **VOICES** Mono/Poly, **SLICE TAIL** Chunk (le break continue jusqu'à la frappe suivante) /
  Hit (s'arrête à la fin de la slice), **ATTACK**, **TIGHT**, **VELOCITY**.
- **NEXT BAR** : un nouveau pattern démarre proprement à la mesure suivante.

### Onglet COLOUR (chaque section a son bouton on/off)
- **VINTAGE** : réduction de bits et de fréquence d'échantillonnage (12 bits / 26 kHz).
- **FILTER** : passe-bas / passe-bande / passe-haut résonant.
- **DRIVE** : saturation.
- **SQUASH** : compression parallèle (le classique pour faire « claquer » un Amen) :
  Amount, Attack, Release, Mix.

### Onglet SPACE
- **DELAY** synchronisé au tempo (1/16 à 1/2, triolets, pointés), **ping-pong**, Feedback, Tone,
  Level. **SOURCE** : tout le break ou **seulement les snares** (le dub-delay sur la caisse claire
  typique du ragga jungle).
- **REVERB** : Size, Damp, Width, Level, source tout ou **snares seulement**.
- **OUTPUT**.

## Contrôle MIDI et automation

| Note | Action |
|------|--------|
| C1 (36) et au-dessus | joue la slice 1, 2, 3… (vélocité respectée) |
| C0 (24) | GENERATE |
| C#0 (25) | MUTATE |
| D0 (26) | FILL à la mesure suivante |
| D#0 (27) | UNDO |
| E0 (28) | séquenceur on/off |

Tous les paramètres sont automatisables. *Trigger: generate / mutate / fill* sont des paramètres
"boutons" pour Push, un contrôleur MIDI ou une enveloppe de clip.

Astuce : **SEQ off** + un clip MIDI = tu joues toi-même les slices dans Ableton.

## Le break et le projet Ableton

Le fichier audio est **intégré dans le projet** (copie FLAC dans l'état du plugin) : si tu déplaces
ou supprimes le WAV, ou ouvres le projet sur un autre ordinateur, le break est toujours là.
Les dossiers favoris sont communs à toutes les instances.

## Comment ça marche

Voir [docs/RECHERCHE.md](docs/RECHERCHE.md) : la recherche sur la jungle / DnB (breaks, chopping,
samplers Akai, patterns two-step / halftime / neuro, algorithme de découpe de Nick Collins) et
comment chaque point a été traduit dans le plugin.

Organisation du code :

| Fichier | Rôle |
|---------|------|
| `Source/Core/Analyzer.*` | détection d'attaques multi-bandes, tempo, découpe Smart/Transient/Grid, classification |
| `Source/Core/Generator.*` | styles, génération (découpe Collins + squelette + gabarits + ornements), mutation, remap |
| `Source/Core/Engine.*` | lecteur temps réel : séquenceur synchronisé, voix (repitch / slice / cyclic), effets |
| `Source/Core/Exporter.h` | export MIDI et rendu audio |
| `Source/PluginProcessor.*` | paramètres, état, chargement, historique |
| `Source/PluginEditor.*`, `Source/UI/*` | interface |
| `Tests/TestMain.cpp` | tests automatiques (break synthétique, tempo, découpe, classes, génération, rendu) |
| `Tests/Harness.cpp` | test d'intégration du plugin complet + captures d'écran (Linux) |

## Compiler soi-même

Pré-requis : CMake ≥ 3.22, Visual Studio 2022 (Windows) ou Xcode (Mac). JUCE 8 est téléchargé
automatiquement. Sur Mac : `cmake -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` puis
`cmake --build build --config Release --target JSlice_AU JSlice_VST3`.

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target JSlice_VST3
```

Chaque push sur `main` lance la compilation sur GitHub Actions et met à jour la release **latest**.

## Licence

Polices intégrées : Instrument Serif et IBM Plex Mono (SIL Open Font License, voir `Resources/Fonts`).
J-Slice utilise le framework [JUCE](https://juce.com) (licence AGPLv3 ou licence commerciale JUCE ;
usage personnel gratuit). Si tu distribues le plugin, respecte les conditions de la licence JUCE.
