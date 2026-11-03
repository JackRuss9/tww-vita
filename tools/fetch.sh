# Shared by build.sh and build-pc.sh (sourced): downloads the decompilation
# and the Sunshine PC port at the versions this project was made for,
# applies this project's patches to them, and extracts from your disc image
# the data the game's code embeds.

TWW_URL=https://github.com/zeldaret/tww.git
TWW_COMMIT=9cbb155cb805ec0546a167926ef71542c485cf0a
SMS_PORT_URL=https://github.com/chasem-dev/sms-pc-port.git
SMS_PORT_COMMIT=fb2a93ba014cd5888c85c00a5e32412328d81fa0

# Applies patches/<name>/*.patch to a clean checkout of <dir> at <commit>
# whenever they change.
apply_patches() {
  local dir=$1 commit=$2 name=$3
  local want have
  want=$(cat patches/"$name"/*.patch | sha256sum | cut -d' ' -f1)
  have=$(cat "$dir/.tww-vita-patches" 2>/dev/null || true)
  if [[ "$want" != "$have" ]]; then
    echo "== Applying patches/$name to a clean $dir"
    git -C "$dir" reset -q --hard "$commit"
    git -C "$dir" clean -q -fd -e orig -e build -e .tww-vita-patches
    for p in patches/"$name"/*.patch; do
      echo "   $p"
      git -C "$dir" apply --whitespace=nowarn "../$p"
    done
    echo "$want" > "$dir/.tww-vita-patches"
  fi
}

fetch_sources() {
  if [[ ! -d tww/.git ]]; then
    echo "== Downloading the Wind Waker decompilation (tww)"
    git clone "$TWW_URL" tww
    git -C tww checkout -q "$TWW_COMMIT"
  fi
  if [[ ! -d sms-port/.git ]]; then
    echo "== Downloading the Sunshine PC port (sms-port) and its decompilation"
    git clone "$SMS_PORT_URL" sms-port
    git -C sms-port checkout -q "$SMS_PORT_COMMIT"
    git -C sms-port submodule update --init --recursive
  fi
  apply_patches tww "$TWW_COMMIT" tww
  apply_patches sms-port "$SMS_PORT_COMMIT" sms-port
}

# The textures, display lists and vertex tables the game's code embeds
# (tww/build/GZLE01/include/assets/*.h), extracted from your own disc image
# with the decomp's tools (it checks the image is GZLE01 Rev 0).
extract_assets() {
  local inc=tww/build/GZLE01/include/assets
  if [[ "${TWW_PLACEHOLDER_ASSETS:-OFF}" == "ON" ]]; then
    return
  fi
  if [[ -f tww/build/GZLE01/.assets-done ]]; then
    return
  fi
  mkdir -p tww/orig/GZLE01
  if ! ls tww/orig/GZLE01/* >/dev/null 2>&1; then
    [[ -n "${TWW_ISO:-}" ]] || {
      echo "ERROR: the first build needs your disc image: TWW_ISO=/path/to/GZLE01.iso ./build-pc.sh"; exit 1; }
    [[ -f "$TWW_ISO" ]] || { echo "ERROR: $TWW_ISO does not exist."; exit 1; }
    ln -s "$(realpath "$TWW_ISO")" "tww/orig/GZLE01/$(basename "$TWW_ISO")"
  fi
  command -v ninja >/dev/null || { echo "ERROR: ninja is not installed (sudo apt install ninja-build)."; exit 1; }
  echo "== Extracting the game code's embedded data from your disc image"
  (cd tww && python3 configure.py --version GZLE01 >/dev/null && ninja build/GZLE01/config.json)
  # (the conversion steps exist once config.json does)
  (cd tww && python3 configure.py --version GZLE01 >/dev/null)
  local targets
  targets=$(cd tww && ninja -t targets all | grep -oE '^build/GZLE01/include/assets/[^:]+' || true)
  if [[ -n "$targets" ]]; then
    (cd tww && ninja $targets)
  fi
  local n
  n=$(ls "$inc" 2>/dev/null | wc -l)
  [[ "$n" -gt 100 ]] || { echo "ERROR: only $n files in $inc after extracting (see above)."; exit 1; }
  echo "   $n files in $inc"
  touch tww/build/GZLE01/.assets-done
}
