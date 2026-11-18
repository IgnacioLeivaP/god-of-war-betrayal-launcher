# God of War: Betrayal Launcher

Build a Windows launcher or a Nintendo Switch homebrew package for the original Java ME release of God of War: Betrayal. Setup runs on Windows. The game JAR stays on your machine and is copied unchanged into the output.

## Required game file

Place your own **Sony Pictures v1.4.62 (a1)** JAR at `inputs/betrayal.jar`, or pass the path with `--jar`. This setup checks the exact revision before building.

```text
SHA-256  E050810F34F554F2EF1B0A60191F684DA8BD7AFEED1206961B58A489380D5C0E
```

The script builds the original game configuration. The runtime plays WAV and MP3. Original sounds stored as MIDI or other phone-specific formats may be silent.

The separate WS146 enhancement project uses PSP audio from **Ghost of Sparta** and **Chains of Olympus**, an already-modified Betrayal JAR, and additional assets and audio processing. This checkout has no reproducible builder for that version from retail dumps alone. You can leave the PSP files separate when using this launcher.

## Install build tools

- Install Python 3.9 or newer, Git for Windows with Git Bash, and a JDK with `javac` and `jar`. The development checkout uses JDK 25. Put the JDK tools on PATH or set `JAVA_HOME`.
- For the **Windows** target, install MSYS2. In its **MINGW64** shell, install the compiler and SDL2 with `pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2`. Setup defaults to `C:\msys64\mingw64`. Set `BETRAYAL_MINGW` to the MINGW64 directory if you installed it elsewhere. [MSYS2 SDL2 package](https://packages.msys2.org/packages/mingw-w64-x86_64-SDL2)
- For the **Switch** target, install devkitPro with `switch-dev` and `switch-sdl2`. Setup defaults to `C:\devkitPro`. Set `BETRAYAL_DEVKITPRO` to your installation directory if needed. The Switch needs an existing homebrew setup.
- The first build fetches the pinned miniJVM source from GitHub and applies the included compatibility patch.

## Windows

1. In PowerShell, open this repository folder and run `py -3 setup.py`.
2. Open `dist\windows\gow_nx.exe`. Keep `juego.jar`, `minijvm_rt.jar`, `gownx.jar`, `SDL2.dll`, and `libwinpthread-1.dll` beside it. Close the game before rebuilding.
3. Use the arrow keys to move and navigate, Z to attack or confirm, X to jump, S and Enter for the left and right soft keys, and E and R for the phone’s * and # keys. The number keys also work. You can configure an SDL-compatible controller in the generated `controles.txt`.

## Nintendo Switch

1. Run `py -3 setup.py --target switch` in PowerShell. Use `--target both` to build both platforms in one run.
2. Copy the `switch` folder from `dist\switch-sd\` to the root of the SD card. After copying, the SD card should contain:

   ```text
   switch/gow_nx/
   ├── gow_nx.nro
   ├── minijvm_rt.jar
   ├── gownx.jar
   └── juego.jar
   ```

3. Launch the app from the Homebrew Menu. Use the D-pad or left stick to move, A or Y to attack or confirm, B to jump, and X and + for the left and right soft keys. The app creates its log, `controles.txt`, and `rms/` save directory alongside the NRO. Back up those files before replacing an older installation, especially if you used WS146.
4. If the JIT causes trouble, create an empty file named `interprete` (no extension) beside the NRO to run in interpreter mode.

An earlier version of this runtime was tested on Switch hardware with WS146. That test does not establish compatibility for this original-JAR package.

## Files in this repository

- `setup.py` and the two build scripts compile and stage the selected targets.
- `src/` and `java/` contain the native launcher and Java ME compatibility layer.
- `external/` contains the miniJVM revision and patch, its upstream license notice, and the stb_image and minimp3 headers with their upstream notices.
- `inputs/`, `dist/`, and the downloaded miniJVM checkout are local to each user. Keep commercial JARs, PSP dumps, extracted assets, and generated packages out of Git.

## AI usage disclosure

OpenAI Codex helped prepare the public snapshot, adapt the existing build scripts, and write and test the setup instructions.
