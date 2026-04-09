#!/bin/sh

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <version> [--git-tag]" >&2
    exit 1
fi

VERSION="$1"
CREATE_GIT_TAG=0

if [ "$#" -eq 2 ]; then
    if [ "$2" != "--git-tag" ]; then
        echo "Unknown option: $2" >&2
        echo "Usage: $0 <version> [--git-tag]" >&2
        exit 1
    fi
    CREATE_GIT_TAG=1
fi

MAJOR_VERSION="${VERSION%%.*}"
DIST_DIR="dist/$VERSION"
TAG_NAME="v$VERSION"

echo "Building libraries for version $VERSION"
make clean
make static
make shared
make dynamic

mkdir -p "$DIST_DIR"

STATIC_VERSIONED="libhm-$VERSION.a"
SHARED_VERSIONED="libhm.so.$VERSION"
SHARED_MAJOR="libhm.so.$MAJOR_VERSION"
DYNAMIC_VERSIONED="libhm.$VERSION.dylib"
DYNAMIC_MAJOR="libhm.$MAJOR_VERSION.dylib"

cp -f libhm.a "$DIST_DIR/$STATIC_VERSIONED"
cp -f libhm.so "$DIST_DIR/$SHARED_VERSIONED"
cp -f libhm.dylib "$DIST_DIR/$DYNAMIC_VERSIONED"

ln -sfn "$STATIC_VERSIONED" "$DIST_DIR/libhm.a"
ln -sfn "$SHARED_VERSIONED" "$DIST_DIR/$SHARED_MAJOR"
ln -sfn "$SHARED_MAJOR" "$DIST_DIR/libhm.so"
ln -sfn "$DYNAMIC_VERSIONED" "$DIST_DIR/$DYNAMIC_MAJOR"
ln -sfn "$DYNAMIC_MAJOR" "$DIST_DIR/libhm.dylib"

echo "Created versioned libraries in $DIST_DIR"

if [ "$CREATE_GIT_TAG" -eq 1 ]; then
    if git rev-parse "$TAG_NAME" >/dev/null 2>&1; then
        echo "Git tag $TAG_NAME already exists" >&2
        exit 1
    fi

    git tag -a "$TAG_NAME" -m "Release $TAG_NAME"
    echo "Created git tag $TAG_NAME"
fi

echo "Done"
