#!/bin/bash
# Construit l'installeur macOS de ravenn (VST3 + AU + application).
# Usage : packaging/macos/package.sh <dossier artefacts Release> <dossier de sortie>
set -euo pipefail

ART="$1"
OUT="$2"
VER="1.1.0"
STAGE="$(mktemp -d)"
mkdir -p "$OUT" "$STAGE/vst3" "$STAGE/au" "$STAGE/app"

cp -R "$ART/VST3/ravenn.vst3"        "$STAGE/vst3/"
cp -R "$ART/AU/ravenn.component"     "$STAGE/au/"
cp -R "$ART/Standalone/ravenn.app"   "$STAGE/app/"

# Signature ad-hoc (obligatoire sur Apple Silicon)
for b in "$STAGE/vst3/ravenn.vst3" "$STAGE/au/ravenn.component" "$STAGE/app/ravenn.app"; do
    codesign --force --deep --sign - "$b"
done

make_pkg () {
    local name="$1" location="$2" id="$3"
    pkgbuild --analyze --root "$STAGE/$name" "$STAGE/$name.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/$name.plist"
    pkgbuild --root "$STAGE/$name" --component-plist "$STAGE/$name.plist" \
             --install-location "$location" --identifier "$id" --version "$VER" "$STAGE/$name.pkg"
}

make_pkg vst3 "/Library/Audio/Plug-Ins/VST3"       com.ravenn.vst3
make_pkg au   "/Library/Audio/Plug-Ins/Components" com.ravenn.au
make_pkg app  "/Applications"                      com.ravenn.app

productbuild --package "$STAGE/vst3.pkg" --package "$STAGE/au.pkg" --package "$STAGE/app.pkg" \
             "$OUT/ravenn-$VER-macos.pkg"

# Archive zip en secours (installation manuelle)
( cd "$STAGE" && mkdir -p zip && cp -R vst3/ravenn.vst3 au/ravenn.component app/ravenn.app zip/ \
  && cd zip && ditto -c -k --sequesterRsrc --keepParent . "$OUT/ravenn-$VER-macos.zip" )

echo "OK : $OUT/ravenn-$VER-macos.pkg"
