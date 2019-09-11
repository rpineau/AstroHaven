#!/bin/bash

mkdir -p ROOT/tmp/AstroHaven_X2/
cp "../domelist AstroHaven.txt" ROOT/tmp/AstroHaven_X2/
cp "../build/Release/libAstroHaven.dylib" ROOT/tmp/AstroHaven_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.AstroHaven_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 AstroHaven_X2.pkg
pkgutil --check-signature ./AstroHaven_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.AstroHaven_X2 --scripts Scripts --version 1.0 AstroHaven_X2.pkg
fi

rm -rf ROOT
