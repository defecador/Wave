#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cross-builds librespot for Sailfish OS so Wave can play audio on the phone.
# Pass the architecture as the first argument: aarch64 (default) or armv7hl.
#
# The Sailfish SDK ships Rust 1.75, but librespot 0.8 needs 1.85 or newer, so Rust runs
# on the host (install it with rustup and add the matching target). The SDK build engine
# is an i686 container, so only C compiling and linking are forwarded into it, through
# sb2, which maps the Sailfish OS target sysroot. /home is shared between the two.
#
# The engine is normally reached with "sfdk engine exec". Where that does not work --
# a Docker engine the user may not talk to, for instance -- set ENGINE_SSH=1 to go
# straight in over SSH instead.
#
# Output: build/deps/librespot/target/<rust triple>/release/librespot, which wave.pro
# installs to /usr/share/wave/bin/ when it exists.

set -eu

ARCH=${1:-aarch64}
LIBRESPOT_VERSION=${LIBRESPOT_VERSION:-v0.8.0}
SFOS_RELEASE=${SFOS_RELEASE:-SailfishOS-5.1.0.11EA}
CARGO=${CARGO:-$HOME/.cargo/bin/cargo}

case $ARCH in
    aarch64)
        RUST_TARGET=aarch64-unknown-linux-gnu
        TARGET_LIBDIR=/usr/lib64
        ;;
    armv7hl)
        RUST_TARGET=armv7-unknown-linux-gnueabihf
        TARGET_LIBDIR=/usr/lib
        ;;
    *)
        echo "Unknown architecture: $ARCH (use aarch64 or armv7hl)" >&2
        exit 1
        ;;
esac

SFOS_TARGET=${SFOS_TARGET:-$SFOS_RELEASE-$ARCH}
# Cargo spells the triple in upper case with underscores for the linker, and in
# lower case with underscores for the compiler.
upper=$(echo "$RUST_TARGET" | tr 'a-z-' 'A-Z_')
lower=$(echo "$RUST_TARGET" | tr '-' '_')

root=$(cd "$(dirname "$0")/.." && pwd)
deps=$root/build/deps
cross=$deps/cross/$ARCH
src=$deps/librespot

mkdir -p "$cross/lib" "$cross/pkgconfig" "$cross/tmp"

if [ ! -d "$src" ]; then
    git clone --depth 1 --branch "$LIBRESPOT_VERSION" \
        https://github.com/librespot-org/librespot.git "$src"
fi

ENGINE_SSH=${ENGINE_SSH:-0}
ENGINE_SSH_KEY=${ENGINE_SSH_KEY:-$HOME/SailfishOS/vmshare/ssh/private_keys/sdk}
ENGINE_SSH_PORT=${ENGINE_SSH_PORT:-2222}
ENGINE_SSH_HOST=${ENGINE_SSH_HOST:-mersdk@127.0.0.1}

for tool in gcc ar; do
    wrapper=$cross/$ARCH-$tool
    if [ "$ENGINE_SSH" = 1 ]; then
        # Arguments go through a file, so quoting survives the trip over SSH.
        cat > "$wrapper" <<EOF
#!/bin/sh
args=\$(mktemp "$cross/tmp/args.XXXXXX")
for a in "\$@"; do printf '%s\0' "\$a"; done > "\$args"
ssh -i "$ENGINE_SSH_KEY" -p "$ENGINE_SSH_PORT" -o BatchMode=yes \\
    -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \\
    "$ENGINE_SSH_HOST" "cd '\$PWD' && exec sb2 -t $SFOS_TARGET xargs -0 $tool < '\$args'"
status=\$?
rm -f "\$args"
exit \$status
EOF
    else
        cat > "$wrapper" <<EOF
#!/bin/sh
exec sfdk engine exec sh -c 'cd "\$0" && exec sb2 -t $SFOS_TARGET $tool "\$@"' "\$PWD" "\$@"
EOF
    fi
    chmod +x "$wrapper"
done

# The target only has PulseAudio's runtime libraries, so provide link-time names
# and pkg-config files. sb2 maps the target's library directory to its sysroot.
for lib in pulse pulse-simple; do
    ln -sfn "$TARGET_LIBDIR/lib$lib.so.0" "$cross/lib/lib$lib.so"
    printf 'Name: lib%s\nDescription: Sailfish OS lib%s (link stub)\nVersion: 17.0\nLibs: -L%s/lib -l%s\nCflags:\n' \
        "$lib" "$lib" "$cross" "$lib" > "$cross/pkgconfig/lib$lib.pc"
done

cd "$src"
env \
    "CARGO_TARGET_${upper}_LINKER=$cross/$ARCH-gcc" \
    "CC_$lower=$cross/$ARCH-gcc" \
    "AR_$lower=$cross/$ARCH-ar" \
    PKG_CONFIG_ALLOW_CROSS=1 \
    "PKG_CONFIG_LIBDIR=$cross/pkgconfig" \
    PKG_CONFIG_PATH= \
    "TMPDIR=$cross/tmp" \
    "$CARGO" build --release --target "$RUST_TARGET" \
        --no-default-features --features rustls-tls-webpki-roots,pulseaudio-backend

echo "Built $src/target/$RUST_TARGET/release/librespot"
