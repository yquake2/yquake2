#!/bin/sh

# ----

CURLVER="8.21.0"
OPENALSOFTVER="1.25.2"
SDL3VER="3.4.14"
VULKANSDKVER="1.4.357.0"
W64DEVKITVER="2.9.1"

# ----

error() {
	if [ -e $WRKDIR ] ; then
		rm -rf $WRKDIR
	fi

	echo "$@" 1>&2
	exit 1
}

usage() {
	echo "Usage: ./$SCRIPTNAME outdir" 1>&2
	exit 2
}

# ----

# Out name for error messages.
SCRIPTNAME=$0

# Output directory.
if [ -z $1 ] ; then
	usage
fi
OUTDIR=$1

# Work dir.
WRKDIR=$(mktemp -d)
mkdir -p $WRKDIR/download $WRKDIR/buildenv $WRKDIR/buildenv/dep $WRKDIR/buildenv/home

# --

# w64devkit (32-bit).
curl --follow -o $WRKDIR/download/w64devkit-x86.exe https://github.com/skeeto/w64devkit/releases/download/v$W64DEVKITVER/w64devkit-x86-$W64DEVKITVER.7z.exe
if [ $? -ne 0 ] ; then
	error "Couldn't download w64devkit-x86"
fi
$WRKDIR/download/w64devkit-x86.exe -y
mv $WRKDIR/download/w64devkit $WRKDIR/buildenv/w64devkit-x86

# w64devkit (64-bit).
curl --follow -o $WRKDIR/download/w64devkit-x64.exe https://github.com/skeeto/w64devkit/releases/download/v$W64DEVKITVER/w64devkit-x64-$W64DEVKITVER.7z.exe
if [ $? -ne 0 ] ; then
	error "Couldn't download w64devkit-x64"
fi
$WRKDIR/download/w64devkit-x64.exe -y
mv $WRKDIR/download/w64devkit $WRKDIR/buildenv/w64devkit-x64

# curl.
curl --follow -o $WRKDIR/download/curl.tar.gz https://curl.se/download/curl-$CURLVER.tar.gz
if [ $? -ne 0 ] ; then
	error "Couldn't download curl"
fi
tar -xf $WRKDIR/download/curl.tar.gz -C $WRKDIR/buildenv/dep
mv $WRKDIR/buildenv/dep/curl-$CURLVER $WRKDIR/buildenv/dep/curl

# openal-soft.
curl --follow -o $WRKDIR/download/openal-soft.zip https://github.com/kcat/openal-soft/releases/download/$OPENALSOFTVER/openal-soft-$OPENALSOFTVER-bin.zip
if [ $? -ne 0 ] ; then
	error "Couldn't download openal-soft"
fi
cd $WRKDIR/buildenv/dep
unzip $WRKDIR/download/openal-soft.zip
cd -
mv $WRKDIR/buildenv/dep/openal-soft-$OPENALSOFTVER-bin $WRKDIR/buildenv/dep/openal-soft

# SDL3.
curl --follow -o $WRKDIR/download/SDL3.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-$SDL3VER/SDL3-devel-$SDL3VER-mingw.tar.gz
if [ $? -ne 0 ] ; then
	error "Couldn't download SDL3"
fi
tar -xf $WRKDIR/download/SDL3.tar.gz -C $WRKDIR/buildenv/dep
mv $WRKDIR/buildenv/dep/SDL3-$SDL3VER $WRKDIR/buildenv/dep/SDL3

# Vulkan-SDK.
curl --follow -o $WRKDIR/download/vulkan-sdk.tar.gz https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-$VULKANSDKVER.tar.gz
if [ $? -ne 0 ] ; then
	error "Couldn't download Vulkan-SDK"
fi
tar -xvf $WRKDIR/download/vulkan-sdk.tar.gz -C $WRKDIR/buildenv/dep
mv $WRKDIR/buildenv/dep/Vulkan-Headers-vulkan-sdk-$VULKANSDKVER $WRKDIR/buildenv/dep/vulkan-sdk

# --

# Set home dir.
echo "home = ..\\home" >> $WRKDIR/buildenv/w64devkit-x86/w64devkit.ini
echo "home = ..\\home" >> $WRKDIR/buildenv/w64devkit-x64/w64devkit.ini

# Create bashrc.
cat > $WRKDIR/buildenv/home/.profile << 'EOL'
export PS1="\w\$ "
export CPATH="$HOME/../dep/curl/include;$HOME/../dep/openal-soft/include;$HOME/../dep/vulkan-sdk/include"
export PKG_CONFIG_PATH="$HOME/../dep/SDL3/$(uname -m)-w64-mingw32/lib/pkgconfig"
EOL

# Create startscripts.
echo "w64devkit-x86\\w64devkit.exe" > $WRKDIR/buildenv/start_x86.bat
echo "w64devkit-x64\\w64devkit.exe" > $WRKDIR/buildenv/start_x64.bat

# --

# Move and cleanup.
mv $WRKDIR/buildenv $OUTDIR
rm -rf $WRKDIR/
