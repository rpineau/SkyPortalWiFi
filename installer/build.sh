#!/bin/bash

mkdir -p ROOT/tmp/SkyPortalWiFi_X2/
cp "../SkyPortalWiFi.ui" ROOT/tmp/SkyPortalWiFi_X2/
cp "../mountlist SkyPortalWiFi.txt" ROOT/tmp/SkyPortalWiFi_X2/
cp "../build/Release/libSkyPortalWiFi.dylib" ROOT/tmp/SkyPortalWiFi_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.SkyPortalWiFi_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 SkyPortalWiFi_X2.pkg
pkgutil --check-signature ./SkyPortalWiFi_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.SkyPortalWiFi_X2 --scripts Scritps --version 1.0 SkyPortalWiFi_X2.pkg
fi

rm -rf ROOT
