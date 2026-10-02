# Recherche : comment se fabrique la rythmique jungle / drum and bass

Ce document résume ce que j'ai appris avant de concevoir J-Slice, et montre comment chaque point
se traduit dans le plugin. Les sources sont listées à la fin.

## 1. Le matériau : les breaks de batterie

- La jungle (début des années 90, Royaume-Uni) est née en prenant des solos de batterie funk/soul
  ("breaks") comme l'Amen (The Winstons, 1969) ou le Think (Lyn Collins), en les accélérant vers
  160-175 BPM, puis en les découpant coup par coup pour les réarranger librement.
- Un break typique dure 1, 2 ou 4 mesures, joué entre ~85 et ~140 BPM à l'origine, avec des
  grosses caisses, des caisses claires, des cymbales et beaucoup de **ghost notes** (petites
  frappes de caisse claire très douces) qui donnent le "swing" humain.
- L'Amen a été samplé des milliers de fois ; les compilations *Ultimate Breaks and Beats* (1986)
  ont mis ces breaks à disposition des producteurs.

**Dans J-Slice :** l'analyse suppose un break "propre" (bouclé, nombre entier de mesures). Le
tempo d'origine et le nombre de mesures sont détectés automatiquement et modifiables à la main
(BREAK BPM, /2, x2, BARS, AUTO).

## 2. Le découpage ("chopping")

- Deux approches classiques : découper en divisions régulières (1/8, 1/16), ou découper sur les
  transitoires (les attaques des frappes). Les samplers des DAW (Simpler, Slicex…) proposent les deux.
- Les producteurs ne découpent **pas** sur chaque micro-transitoire : ils isolent les frappes qui
  comptent (kick, snare, hats, ghosts audibles) et gardent parfois des morceaux plus longs (un
  kick + le charley qui suit). Un conseil récurrent de la scène : ne pas couper pile sur le
  transitoire mais très légèrement avant, sinon l'attaque perd son punch.
- Les "flams" (deux frappes quasi simultanées) doivent rester dans la même slice.

**Dans J-Slice (mode Smart) :**
1. Détection d'attaques multi-bandes ("spectral flux" normalisé séparément dans 4 bandes :
   kick 30-150 Hz, corps de caisse claire 150-500 Hz, timbre 500 Hz-4 kHz, cymbales > 4 kHz),
   pour qu'un charley discret soit aussi visible qu'une grosse caisse.
2. Position affinée à l'échantillon près puis recalée sur un passage par zéro, avec un
   **pré-roll** réglable (1,5 ms par défaut) pour garder l'attaque intacte.
3. Sélection "musicale" : on connaît le tempo, donc la grille de doubles-croches. Au maximum une
   frappe significative par case de 1/16 (la plus forte et la mieux placée), les flams sont
   fusionnés, et les ghost notes entre deux cases ne sont gardées que si la sensibilité est haute
   et qu'elles sont assez fortes.
4. Modes Transient (toutes les attaques au-dessus du seuil) et Grid (divisions égales recalées
   sur les attaques voisines) pour les cas particuliers. Chaque marqueur se déplace, s'ajoute ou
   se supprime à la souris.

## 3. Reconnaître kick, snare, hat, ghost

- Les études de transcription de batterie classent les frappes par la répartition de leur énergie
  spectrale (grave = kick, médium bruité = caisse claire, aigu = cymbales).
- Difficulté propre aux breaks : la queue de la frappe précédente se mélange à la suivante.

**Dans J-Slice :** le spectre de l'attaque **moins** le spectre juste avant la coupe (pour retirer
la résonance du coup précédent), puis des scores kick/snare/hat. Une légère connaissance du rythme
s'y ajoute (kick plutôt sur le 1, snare sur les temps 2 et 4). Une snare nettement plus faible que
les autres devient une ghost note. Sur le break de test, les kicks et snares sont reconnus à 100 %.
Clic droit sur une slice pour corriger la classe à la main.

## 4. Le tempo : repitch, time-stretch et le son "Akai"

- Méthode historique : on accélère le break (le pitch monte) jusqu'au tempo voulu. C'est une
  grande part du son jungle.
- Les samplers Akai S950 / S1000 / S3000 avaient un time-stretch "cyclique" très grossier qui
  produisait un son métallique, granuleux, devenu une signature (l'outil gratuit Akaizer existe
  uniquement pour l'imiter). Le S900/S950 travaillait en 12 bits avec une fréquence
  d'échantillonnage variable et des filtres analogiques, ce qui colore aussi le son.

**Dans J-Slice :** trois modes de tempo
- **Repitch (vintage)** : vitesse et hauteur suivent le tempo du projet.
- **Slice (original speed)** : chaque slice est jouée à sa vitesse d'origine, recalée sur la
  grille (comme un fichier REX).
- **Cyclic stretch (Akai)** : time-stretch par cycles répétés (longueur de cycle réglable), pour
  le son métallique des années 90.
Plus une section "Vintage sampler" : réduction de bits (12 bits par défaut) et de fréquence
d'échantillonnage, filtre résonant, saturation.

## 5. Les patterns : ce qui fait sonner "jungle" ou "DnB"

- **Two-step** (DnB classique à partir de ~1995) : snare sur les temps 2 et 4, kick sur le 1 et
  le second kick repoussé juste avant la deuxième snare.
- **Neurofunk / techstep** : comme le two-step mais le second kick tombe sur la dernière
  double-croche avant le temps 3.
- **Halftime** : une seule snare par mesure, sur le temps 3.
- **Jungle / Amen chops** : rythmes polyrythmiques très découpés, roulements de caisse claire,
  bégaiements ("stutters"), caisse claire inversée qui mène à la vraie caisse claire, slices
  repitchées, petits silences entre les frappes (raccourcir des notes de 1/64 ou 1/128).
- Les ghost notes très douces (vélocité ~15-30 %) et le "shuffle" de doubles-croches font la
  différence entre un beat rigide et un beat qui roule.
- Variation : on change de pattern toutes les 4 ou 8 mesures, avec des fills.

## 6. L'algorithme de découpe de Nick Collins (BBCut)

Nick Collins a modélisé dès 2001 le découpage jungle ("breakbeat science") :
- La phrase (1 ou 2 mesures) est découpée en **blocs de longueur impaire** (en croches : 3+3+2
  est la découpe la plus typique), qui créent de la syncope avant de "retomber" sur le temps fort.
- Un bloc peut être **répété** (comme si on relançait le break sur un contretemps).
- La phrase peut finir par un **stutter** (une toute petite portion répétée).
- Les blocs courts sont préférés : des blocs trop longs révèlent trop le break d'origine.
- Pour les beats "programmés", il utilise des **gabarits de probabilité** par double-croche
  (la snare des temps 2 et 4 toujours présente, le reste probabiliste) et une structure AAAB
  (trois variations légères puis un fill).

**Dans J-Slice, le générateur combine les deux :**
1. découpe Collins du break (blocs impairs 1/3/5, répétitions, sauts vers un autre endroit du
   break avec la probabilité **Chop**, coupes en 1/8 ou 1/16 selon **16th cuts**, fin en
   **Stutter**) ;
2. squelette du style (**Backbone**) : impose kick/snare aux bonnes places (two-step, neuro,
   halftime…) ;
3. gabarits de probabilité du style pour ajouter kicks, snares, **Ghosts** et **Hats** ;
4. ornements : **Rolls** (plutôt en fin de mesure), snare inversée avant la snare
   (**Reverse**), **Pitch** par frappe, frappes raccourcies (**Short hits**), **Humanize** ;
5. motif de 2 mesures répété sur 8 mesures avec variations plus fortes en fin de phrase
   (**Variation**), et une mesure de **Fill** séparée (roulement, bégaiement accéléré ou
   "dropout").

## Sources

- KAN Samples, *How to Chop the Amen Break* : https://kansamples.com/blogs/learn/how-to-chop-amen-break
- eMastered, *How to Make Jungle Music* : https://emastered.com/blog/how-to-make-jungle-music
- EDMProd, *How to Make Jungle Music* : https://www.edmprod.com/how-to-make-jungle-music/
- EDMProd, *How To Make Drum & Bass* : https://www.edmprod.com/how-to-make-drum-and-bass/
- MusicRadar, *How to program 6 different jungle and drum 'n' bass grooves* : https://www.musicradar.com/how-to/program-6-different-jungle-6-dnb-grooves
- Unison, *DnB drum patterns* : https://unison.audio/dnb-drum-patterns/
- Dogs On Acid, *Jungle/Amen break programming* : https://www.dogsonacid.com/threads/jungle-amen-break-programming.787380/
- Nick Collins, *Algorithmic Composition Methods for Breakbeat Science* : https://composerprogrammer.com/research/acmethodsforbbsci.pdf
- BBenCut (portage de BBCut pour Ableton) : https://github.com/bencodec/BBenCut
- The Akaizer Project : https://the-akaizer-project.blogspot.com/
- Wikipedia, *Amen break*, *Akai S900*, *Akai S1000* : https://en.wikipedia.org/wiki/Amen_break
- Bello et al. / travaux sur la détection d'attaques par "spectral flux" (synthèse) : https://arxiv.org/pdf/2006.10553
