# ravenn — suite de mastering (VST3 · AU · Standalone)

Interface en 5 pages : **CORE**, **TONE & SPACE**, **SATURATION & CHARACTER**, **LIMITER & OUTPUT**,
**VOICE & REFERENCE**. Spectre, mesures et auto master restent toujours visibles.

## Chaîne du signal

1. **IN** (−15…+15 dB).
2. **EQ PRE** : Tilt, Low, Air · **EQ MATCH** (10 octaves, calé sur une référence).
3. **VOICE SIDECHAIN** : la voix branchée sur l'entrée sidechain creuse la prod sur ses fréquences
   (6 bandes de 250 Hz à 8 kHz, jusqu'à −9 dB), en temps réel.
4. **STEREO** : Width, Mono Bass · **VOICE** : présence ±6 dB sur le centre (0,8–6 kHz) ·
   **3D DEPTH** : côtés décorrélés dans les aigus + centre avancé (compatible mono).
5. **DYNAMICS** : OTT 3 bandes (fréquences de coupure et quantité par bande réglables, TIME),
   **PUNCH** (attaques), **SPACE** (toute petite réverb, le grave n'y passe pas).
6. *Sur-échantillonnage x1…x16 pour les étages non linéaires (ou zéro latence avec NO LATENCY) :*
   - **THE MOUTH** · **BODY** · **THE MASK** : calés par mesure sur MONSTER (voir plus bas).
   - **BASS DEPTH**, **MULTIBAND SATURATION** (3 bandes, 7 styles : Clean Tube, Warm Tube, Tape,
     Transistor, Amp, Rectify, Foldback), **CHARACTER** (Tape 15 / 30 ips, Cassette, Tube Console,
     Class-A Console, Transformer, Vinyl), **ANALOG**, **FRESH AIR** (Mid Air 5 kHz, High Air 11 kHz,
     dynamique : recule sur les sifflantes), **CLIPPER** (Hard / Soft / Tube, plein spectre ou une bande).
7. **EQ POST**, pleurage (WOW), craquements (CRACKLE, à 0 par défaut : aucun bruit).
8. **LIMITER** : brickwall à anticipation, true peak (interpolation 4x), relâchement adaptatif,
   9 styles (Transparent, Punchy, Dynamic, Allround, Aggressive, Modern, Bus, Safe, Stealth),
   Gain, Ceiling, Lookahead, Attack, Release, Link transitoires / relâchement, Soft Clip, Unity, Dither.
9. **AUTO GAIN**, **MIX**, **DELTA**, **OUT**, **BYPASS**.

**DELTA** : écoute uniquement la différence entre le son traité et l'original, ramenés au même volume :
ce que ravenn ajoute (grain, harmoniques, air) ou retire (pics). Le spectre affiche alors cette différence.

**FATTEN** (compression écrasante + saturation, COLOR sombre/brillant), **IMAGER 3 bandes**, **STABILIZER** (EQ dynamique vers un équilibre de master moderne ou celui de la référence), **VOICE FOCUS** (fait ressortir la voix d'un mix, sans sidechain).

**MULTIBAND** : compresseur 4 bandes (seuil, ratio, attaque, relâchement, gain, solo, bypass, 3 coupures réglables, knee, mix) + EQ dynamique 2 bandes flottantes (fréquence, Q, seuil, plage ±12 dB), dans l'esprit de Pro-MB et du C6 (conception originale).

**SPECTRAL** : équilibreur de spectre STFT (~60 zones, cible Modern / Pink / Référence, Cut Only) et dompteur de résonances ; **DE-ESSER** à bande séparée (Freq, Threshold, Range, Listen). Multibande en vrai Linkwitz-Riley, coupures déplaçables à la souris sur le spectre, Auto Gain. Limiteur : modes Stealth Tight / Balanced / Harmonics 1 / Harmonics 2 et vue entrée + réduction. Clipper SOFT à genou réglable et mode M/S. Low / High Cut, filtres de la 3D et de Space, vectorscope stéréo, LUFS long terme (30 s), slots A/B/C/D, presets par catégorie.

**AAX** : option de compilation -DRAVENN_AAX=ON -DAAX_SDK_PATH=... (SDK Avid requis, signature PACE pour Pro Tools).

**1.6** : multibande éditable directement sur le spectre (points de seuil, molette = ratio, double-clic = bypass, bandes dynamiques par double-clic), page des presets (catégories + recherche), auto de-esser, 10 courbes pour l'équilibreur, auto master qui règle tous les modules (annulable en une fois), copie A/B/C/D par menu, PLR.

**1.7** : AUTO STEM (8 types de pistes, style, intensité), MATCH REFERENCE (A = ton réglage, B/C/D = 3 dosages) et liste CHANGES (avant → après, enregistrable en preset), GLUE (compresseur de bus + VU-mètre), jauge du clipper avec fréquences réglables, de-esser avec bande FREQ → FREQ HIGH et vue façon Pro-DS (l'auto bouge ses potards), plages de voix et plages 3D (presets + Q), stéréo 2D / nuage 3D / cascade 3D, écran VHS d'aide au survol, 8 langues (EN, FR, DE, JA, ZH, PT, ES, NL), presets par stem, export de preset, flèches synchronisées dans la page des presets.

**1.8** : The Mask refait (ouverture du Side sans saturation, démasquage, garde mono), Kick Presence, AUTO IN à l'insertion, auto master plus propre jusqu'à 0 dB (-0,1 dBTP), auto gain du glue + détection plus douce, pentes 12/24/48 dB/oct (multibande, saturation, clipper, largeur), plages de largeur réglables, de-esser ON/OFF + AUTO DE-ESS ponctuel, waveform du clipper, indicateurs de la vue stéréo, presets de voix + RESET, vitesses affichées SLOW/FAST, Body coupé par défaut, logo net, correction du plantage à la fermeture (FL Studio).

**RESET** (tous les paramètres, annulable), **UNDO / REDO** (100 étapes minimum), A/B, presets (dossier ouvrable, import de fichiers `.preset`).

### Mouth, Mask, Body
Calés avec `Tools/Probe.cpp`, un banc qui charge un VST3 et mesure sa réponse (harmoniques à −30 / −24 /
−18 / −12 dBFS, gain Mid/Side par octave), puis ajustement numérique des courbes. Mask : gain et H3 à
±0,5 dB de −30 à −18 dBFS ; Mouth : H2 / H3 / H4 à ±1–2 dB. Le code de MONSTER n'a pas été lu.
La saturation multibande et le limiteur sont des conceptions originales (aucun code de FabFilter,
d'iZotope ou d'un autre éditeur n'a été lu ou copié).

## Auto master
Cibles : **0 LUFS (max)**, −4 Extreme, −7 Loud, −9 Club, −11 Spotify Loud, −14 Streaming, −16 Apple Music,
−23 EBU R128 (TV), **REFERENCE**. Lance la lecture et clique **AUTO MASTER** : 6 s d'écoute, réglages
(EQ, couleur, stéréo, limiteur : −1 dBTP pour le streaming / la TV, −0,3 dB pour le club), puis
ajustement du niveau. En mode REFERENCE, il vise le loudness de la référence importée, recopie son
équilibre par octave (EQ MATCH, corrigé en boucle fermée en mesurant la sortie) et sa largeur stéréo.

## Mesures
LUFS M / S / I, True Peak, barres In / Out / Clip / Lim, oscilloscope du clipper, spectre entrée /
sortie / référence avec échelle en dB qui suit le niveau.

## Installer
- **Windows** : `ravenn-1.8.0-windows-setup.exe`.
- **macOS** : `ravenn-1.8.0-macos.pkg` (non signé par Apple : *Réglages Système › Confidentialité et
  sécurité › Ouvrir quand même*).

Le sidechain voix : dans le DAW, active l'entrée sidechain de ravenn sur la piste prod et envoie-y la voix.

## Compiler
```
cmake -B build -A x64
cmake --build build --config Release --target ravenn_VST3 ravenn_Standalone ravennTest
```
GitHub Actions produit les installeurs Windows et Mac à chaque push.
