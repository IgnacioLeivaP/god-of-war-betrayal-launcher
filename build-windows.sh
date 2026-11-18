#!/bin/bash
# Build the Windows launcher with MinGW-w64 and SDL2.
set -eu
cd "$(dirname "$0")"
MINGW=$(cygpath -u "$BETRAYAL_MINGW")
export PATH="$MINGW/bin:$PATH"
M=external/miniJVM/minijvm/c
O="$1/windows-obj"
SD="$1/windows"
INC="-I$M/jvm -I$M/utils -I$M/utils/sljit -I$M/utils/https -I$M/utils/mimalloc/include -I$M/utils/https/mbedtls/include -Iexternal/stb -Iexternal/minimp3"
mkdir -p "$O" "$SD"

compila() {
    echo "  cc $1"
    gcc -c -O2 -w $INC "$1" -o "$2"
}

# --- miniJVM (todo menos su main.c, que sustituye src/launcher.c)
for f in $(find $M -name "*.c" | grep -v "sljit\|mimalloc\|cmake-\|/main.c"); do
    compila $f $O/$(echo $f | sed "s#$M/##; s#/#_#g; s#\.c\$#.o#")
done
compila $M/utils/sljit/sljitLir.c $O/sljitLir.o
compila $M/utils/mimalloc/src/static.c $O/mimalloc.o

# --- lo nuestro
compila src/launcher.c $O/gownx_launcher.o
compila src/nat/nat.c $O/gownx_nat.o
compila src/nat/audio.c $O/gownx_audio.o
compila src/nat/controles.c $O/gownx_controles.o
compila src/nat/perfil.c $O/gownx_perfil.o
compila src/nat/puentes.c $O/gownx_puentes.o
compila src/pc/main_pc.c $O/gownx_main_pc.o
gcc -o $SD/gow_nx.exe $O/*.o -lSDL2 -lpthread -lm -lws2_32
for d in SDL2.dll libwinpthread-1.dll; do cp "$MINGW/bin/$d" "$SD/"; done
