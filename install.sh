#!/usr/bin/env bash
set -euo pipefail
PREFIX="/usr/local"
if [ "${1:-}" != "" ]; then PREFIX="$1"; fi
BIN_DIR="$PREFIX/bin"
SCROLL_DIR="$PREFIX/lib/ardent/scrolls"
EX_DIR="$PREFIX/share/ardent/examples"

echo "Installing Ardent to $PREFIX";
mkdir -p "$BIN_DIR" "$SCROLL_DIR" "$EX_DIR"
# Expect build artifact ./ardent (user should run cmake --build . beforehand)
if [ ! -f "ardent" ]; then
  echo "Error: 'ardent' binary not found. Run: cmake -S . -B build && cmake --build build --target ardent";
  exit 1;
fi
install -m 0755 ardent "$BIN_DIR/ardent"
cp "$BIN_DIR/ardent" "$BIN_DIR/ardentc"
cp "$BIN_DIR/ardent" "$BIN_DIR/oracle"
cp examples/*.ardent "$EX_DIR" 2>/dev/null || true

# Export ARDENT_HOME suggestion
if ! grep -q 'ARDENT_HOME' "$HOME/.bashrc" 2>/dev/null; then
  echo "export ARDENT_HOME=$PREFIX/lib/ardent" >> "$HOME/.bashrc"
  echo "Appended ARDENT_HOME to ~/.bashrc"
fi

cat <<EOF
"The Scholar's Ink now flows through your system."
Ardent installed at: $PREFIX
Add to PATH (if not already): export PATH="$BIN_DIR:\$PATH"
Standard scrolls root: \$ARDENT_HOME/scrolls
Run: ardent --demo   (showcase)\n     ardent --scrolls (list stdlib)
EOF
