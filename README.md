# ravenn — clipper / saturation de mastering (VST3 · AU · Standalone)

## Chaîne du signal

1. **IN** (−15…+15 dB) — gain d'entrée.
2. **EQ** en position *PRE* : Tilt (pivot 1 kHz), Low (100 Hz), Air (12 kHz).
3. **STEREO** : Width (0–200 %), Mono Bass (graves en mono sous 20–300 Hz).
4. *Sur-échantillonnage x1…x16 (FIR) pour tous les étages non linéaires, ou zéro latence avec NO LATENCY :*
   - **THE MOUTH** : graves (< 300 Hz) dans une distorsion asymétrique *à niveau constant*
     (elle mord pareil à −30 dB qu'à 0 dB). Grain dès 20 %, carré et agressif à 100 %.
   - **THE MASK** : saturation à niveau constant + remontée du Side (jusqu'à +7 dB) dans la bande
     choisie (MID 70 Hz–1,1 kHz · HI MID 600 Hz–2,6 kHz · HIGH > 1,5 kHz · FULL). Le centre reste intact.
   - **BASS DEPTH** : harmoniques des graves + bosse de sub à 55 Hz.
   - **ANALOG** : transfo, aigus adoucis, saturation douce, crosstalk. Aucun bruit ajouté.
   - **CLIPPER** : HARD / SOFT / ANALOG, sur tout le spectre ou une seule bande (LOW < 200 Hz,
     MID 200 Hz–4 kHz, HIGH > 4 kHz).
5. **EQ** en position *POST*.
6. Plafond de sécurité : jamais au-dessus de 0 dBFS.
7. **AUTO GAIN**, **MIX**, **DELTA**, puis **OUT**.

**LINK** relie In et Clipper en sens inverse. **NO LATENCY** coupe le sur-échantillonnage (0 sample).

## Auto master
Choisis une cible (STREAM −14 · CLUB −9 · LOUD −7 LUFS), lance la lecture et clique **AUTO MASTER** :
1. ravenn écoute 6 s de musique (loudness, crêtes, équilibre spectral sur 5 bandes, corrélation stéréo,
   stéréo dans le grave) ;
2. il règle EQ, couleur, stéréo, mono bass, clipper SOFT et le gain ;
3. il mesure le résultat et ajuste le niveau jusqu'à la cible (jusqu'à 4 passes).
Tu peux tout retoucher ensuite ; A/B pour comparer avec ton réglage d'avant.

## Mesures
LUFS momentané / court terme / intégré, True Peak, barres LED In / Out / GR, oscilloscope du clipper.

## Installer
- **Windows** : `ravenn-1.0.0-windows-setup.exe` → VST3 dans `C:\Program Files\Common Files\VST3`,
  application dans `C:\Program Files\ravenn`.
- **macOS** : `ravenn-1.0.0-macos.pkg` → VST3, AU et application. Le paquet n'est pas signé par Apple :
  au premier lancement, ouvre **Réglages Système › Confidentialité et sécurité › Ouvrir quand même**
  (ou clic droit › Ouvrir). Les plugins se chargent ensuite normalement dans le DAW.

## Compiler
Windows (VS 2022 Build Tools + CMake) :
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ravenn_VST3 ravenn_Standalone
```
macOS : automatique via GitHub Actions (`.github/workflows/build.yml`) : à chaque push, les installeurs
Windows et Mac sont produits dans l'onglet *Actions › Artifacts*.
