#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cross-builds librespot for Sailfish OS aarch64 so Wave can play audio on the phone.
#
# The Sailfish SDK ships Rust 1.75, but librespot 0.8 needs 1.85 or newer, so Rust runs
# on the host (install it with rustup and add the aarch64-unknown-linux-gnu target).
# The SDK build engine is an i686 container, so only C compiling and linking are
# forwarded into it through sb2, which maps the Sailfish OS target sysroot.
# /home is shared between the host and the engine.
#
# Output: build/deps/librespot/target/aarch64-unknown-linux-gnu/release/librespot,
# which wave.pro installs to /usr/share/wave/bin/ when it exists.

set -eu

LIBRESPOT_VERSION=${LIBRESPOT_VERSION:-v0.8.0}
SFOS_TARGET=${SFOS_TARGET:-SailfishOS-5.1.0.11-aarch64}
CARGO=${CARGO:-$HOME/.cargo/bin/cargo}

root=$(cd "$(dirname "$0")/.." && pwd)
deps=$root/build/deps
cross=$deps/cross
src=$deps/librespot

mkdir -p "$cross/lib" "$cross/pkgconfig" "$cross/tmp"

if [ ! -d "$src" ]; then
    git clone --depth 1 --branch "$LIBRESPOT_VERSION" \
        https://github.com/librespot-org/librespot.git "$src"
fi

for tool in gcc ar; do
    cat > "$cross/aarch64-$tool" <<EOF
#!/bin/sh
exec sfdk engine exec sh -c 'cd "\$0" && exec sb2 -t $SFOS_TARGET $tool "\$@"' "\$PWD" "\$@"
EOF
    chmod +x "$cross/aarch64-$tool"
done

# The target only has PulseAudio's runtime libraries, so provide link-time names
# and pkg-config files. sb2 maps /usr/lib64 to the target sysroot.
for lib in pulse pulse-simple; do
    ln -sfn "/usr/lib64/lib$lib.so.0" "$cross/lib/lib$lib.so"
    printf 'Name: lib%s\nDescription: Sailfish OS lib%s (link stub)\nVersion: 17.0\nLibs: -L%s/lib -l%s\nCflags:\n' \
        "$lib" "$lib" "$cross" "$lib" > "$cross/pkgconfig/lib$lib.pc"
done

cd "$src"
CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=$cross/aarch64-gcc \
CC_aarch64_unknown_linux_gnu=$cross/aarch64-gcc \
AR_aarch64_unknown_linux_gnu=$cross/aarch64-ar \
PKG_CONFIG_ALLOW_CROSS=1 \
PKG_CONFIG_LIBDIR=$cross/pkgconfig \
PKG_CONFIG_PATH= \
TMPDIR=$cross/tmp \
    "$CARGO" build --release --target aarch64-unknown-linux-gnu \
        --no-default-features --features rustls-tls-webpki-roots,pulseaudio-backend

echo "Built $src/target/aarch64-unknown-linux-gnu/release/librespot"
