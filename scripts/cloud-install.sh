#!/usr/bin/env bash
set -euo pipefail
# User-owned JUCE toolchain overlay; leaves /workspace/synth unchanged.
synth_env=/workspace/.synth-env
mkdir -p "$synth_env"/{apt/lists/partial,apt/archives/partial,apt/empty,sysroot,cache}
python3 -m venv "$synth_env/tools"
"$synth_env/tools/bin/python" -m pip --cache-dir "$synth_env/cache/pip" install 'cmake==3.31.10' 'ninja==1.13.0'
cat > "$synth_env/apt/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie-updates main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://security.debian.org/debian-security trixie-security main
SOURCES
cat > "$synth_env/apt/apt.conf" <<'APTCONF'
Dir::Etc::parts "/workspace/.synth-env/apt/empty";
Dir::Etc::main "/dev/null";
Dir::Etc::sourcelist "/workspace/.synth-env/apt/sources.list";
Dir::Etc::sourceparts "/workspace/.synth-env/apt/empty";
Dir::State::lists "/workspace/.synth-env/apt/lists";
Dir::Cache::archives "/workspace/.synth-env/apt/archives";
APT::Get::List-Cleanup "0";
Acquire::Languages "none";
APTCONF
export APT_CONFIG="$synth_env/apt/apt.conf"
export https_proxy="${https_proxy:-$HTTPS_PROXY}"
export http_proxy="${http_proxy:-$HTTP_PROXY}"
/usr/bin/apt-get update
synth_packages="$(mktemp -d "$synth_env/apt/selection.XXXXXX")"
(
  cd "$synth_packages"
  /usr/bin/apt-get download \
    libasound2-dev libasound2t64 libfreetype-dev libfreetype6 \
    libfontconfig-dev libfontconfig1 libexpat1-dev libexpat1 uuid-dev libuuid1 \
    libx11-dev libx11-6 libxcomposite-dev libxcomposite1 libxcursor-dev libxcursor1 \
    libxext-dev libxext6 libxinerama-dev libxinerama1 libxrandr-dev libxrandr2 \
    libxrender-dev libxrender1 libxi-dev libxi6 libxfixes-dev libxfixes3 \
    libxcb1-dev libxcb1 libxau-dev libxau6 libxdmcp-dev libxdmcp6 \
    libpng-dev libpng16-16t64 libbz2-dev libbz2-1.0 libbrotli-dev libbrotli1 \
    zlib1g-dev zlib1g x11proto-dev xorg-sgml-doctools xtrans-dev
  for synth_deb in *.deb; do dpkg-deb -x "$synth_deb" "$synth_env/sysroot"; done
)
rm -- "$synth_packages/"*.deb
rmdir -- "$synth_packages"
unset APT_CONFIG
if [ -d "$synth_env/juce" ]; then
  test "$(cat "$synth_env/juce/.synth-source-commit")" = 29396c22c93392d6738e021b83196283d6e4d850
  test -f "$synth_env/juce/modules/juce_core/juce_core.h"
else
  curl --fail --location --retry 2 --output "$synth_env/JUCE-8.0.12.tar.gz" \
    https://codeload.github.com/juce-framework/JUCE/tar.gz/29396c22c93392d6738e021b83196283d6e4d850
  printf '%s  %s\n' 97cc01e0fe15278f6edadc895a3cf0aac2fd98bcd023823384d5ca7025beb91c "$synth_env/JUCE-8.0.12.tar.gz" | sha256sum -c -
  synth_juce_stage="$(mktemp -d "$synth_env/juce-stage.XXXXXX")"
  tar -xzf "$synth_env/JUCE-8.0.12.tar.gz" --strip-components=1 -C "$synth_juce_stage"
  test -f "$synth_juce_stage/modules/juce_core/juce_core.h"
  printf '%s\n' 29396c22c93392d6738e021b83196283d6e4d850 > "$synth_juce_stage/.synth-source-commit"
  mv -- "$synth_juce_stage" "$synth_env/juce"
fi
cat > "$synth_env/activate.sh" <<'ACTIVATE'
# Source this file in every build/test shell.
export SYNTH_ENV=/workspace/.synth-env
export PATH="$SYNTH_ENV/tools/bin:$PATH"
export JUCE_SOURCE_DIR="$SYNTH_ENV/juce"
export PKG_CONFIG_SYSROOT_DIR="$SYNTH_ENV/sysroot"
export PKG_CONFIG_LIBDIR="$SYNTH_ENV/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$SYNTH_ENV/sysroot/usr/share/pkgconfig"
export CPATH="$SYNTH_ENV/sysroot/usr/include:$SYNTH_ENV/sysroot/usr/include/x86_64-linux-gnu${CPATH:+:$CPATH}"
export LIBRARY_PATH="$SYNTH_ENV/sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
export LD_LIBRARY_PATH="$SYNTH_ENV/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CMAKE_PREFIX_PATH="$SYNTH_ENV/sysroot/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export XDG_CACHE_HOME="$SYNTH_ENV/cache"
ACTIVATE
source "$synth_env/activate.sh"
cmake --version
ninja --version
pkg-config --modversion alsa freetype2 fontconfig x11 xext xinerama xrandr xcursor


# Build and validate the instrument in this checkout.
export CMAKE_BUILD_PARALLEL_LEVEL=4
cmake -S /workspace/synth -B /workspace/.synth-env/synth-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/.synth-env/synth-build --target SynthTests Sixth_Standalone -j4
ctest --test-dir /workspace/.synth-env/synth-build --output-on-failure
