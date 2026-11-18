#!/bin/bash
# Build the Switch homebrew launcher with devkitA64, libnx, and SDL2.
set -eu
cd "$(dirname "$0")"
DKP=$(cygpath -u "$BETRAYAL_DEVKITPRO")
export DEVKITPRO=$(cygpath -m "$DKP")
CC="$DKP/devkitA64/bin/aarch64-none-elf-gcc.exe"
M=external/miniJVM/minijvm/c
O="$1/switch-obj"
SD="$1/switch"
ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
INC=(-Isrc/switch/compat "-I$DKP/portlibs/switch/include" "-I$DKP/libnx/include" -I"$M"/{jvm,utils,utils/sljit,utils/https,utils/mimalloc/include,utils/https/mbedtls/include} -Iexternal/stb -Iexternal/minimp3)
mkdir -p "$O" "$SD"

compila() {  # fuente, objeto, banderas extra: solo si el fuente es mas nuevo
    echo "  cc $1"
    "$CC" -c -O2 -w $ARCH -ffunction-sections -D__SWITCH__ -DSLJIT_HAVE_CONFIG_PRE=1 "${INC[@]}" "$1" -o "$2"
}
for f in $M/jvm/*.c $M/utils/*.c; do
    b=$(basename $f .c); [ $b = os_win ] && continue
    compila $f $O/$b.o
done
compila $M/utils/sljit/sljitLir.c $O/sljitLir.o
compila src/switch/compat/compat.c $O/compat.o
compila src/switch/jit_mem.c $O/jit_mem.o
compila src/launcher.c $O/gownx_launcher.o
compila src/nat/nat.c $O/gownx_nat.o
compila src/nat/audio.c $O/gownx_audio.o
compila src/nat/controles.c $O/gownx_controles.o
compila src/nat/perfil.c $O/gownx_perfil.o
compila src/nat/puentes.c $O/gownx_puentes.o
compila src/switch/main_switch.c $O/main_switch.o

# --allow-multiple-definition: tinycthread.c (miniJVM) define thrd_*/tss_* igual que newlib; ganan los nuestros.
"$CC" "-specs=$DEVKITPRO/libnx/switch.specs" $ARCH -Wl,--allow-multiple-definition "$O"/*.o "-L$DKP/portlibs/switch/lib" "-L$DKP/libnx/lib" \
    -lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm -lstdc++ -o "$O/gow_nx.elf"
"$DKP/tools/bin/nacptool.exe" --create "God of War: Betrayal" "gow_nx" "0.2" "$O/gow_nx.nacp"
"$DKP/tools/bin/elf2nro.exe" "$O/gow_nx.elf" "$SD/gow_nx.nro" "--nacp=$O/gow_nx.nacp"
