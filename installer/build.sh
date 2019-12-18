#!/bin/bash

mkdir -p ROOT/tmp/SkyPortalWiFi_X2/
cp "../SkyPortalWiFi.ui" ROOT/tmp/SkyPortalWiFi_X2/
cp "../mountlist SkyPortalWiFi.txt" ROOT/tmp/SkyPortalWiFi_X2/
cp "../build/Release/libSkyPortalWiFi.dylib" ROOT/tmp/SkyPortalWiFi_X2/

PACKAGE_NAME="SkyPortalWiFi_X2.pkg"
BUNDLE_NAME="org.rti-zone.SkyPortalWiFiX2"


if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
