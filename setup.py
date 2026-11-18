"""Build the Windows launcher and/or a Switch SD folder from a local MIDlet."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent
EXTERNAL = ROOT / "external"
JAR_HASH = "e050810f34f554f2ef1b0a60191f684da8bd7afeed1206961b58a489380d5c0e"
MINIJVM_UPSTREAM = "https://github.com/digitalgust/miniJVM.git"
WINDOWS_FILES = ("SDL2.dll", "libwinpthread-1.dll", "gow_nx.exe")
SWITCH_FILES = ("gow_nx.nro",)
COMMON_FILES = ("minijvm_rt.jar", "gownx.jar", "juego.jar")


def run(*args, cwd=ROOT, env=None, capture=False):
    result = subprocess.run(
        [str(arg) for arg in args], cwd=cwd, env=env, check=True,
        text=True, stdout=subprocess.PIPE if capture else None,
    )
    return result.stdout.strip() if capture else None


def program(name, fallback=None):
    if fallback and Path(fallback).is_file():
        return Path(fallback)
    found = shutil.which(name)
    if not found:
        raise ValueError(f"{name} is required. See the requirements in README.md.")
    return Path(found)


def prepare_minijvm(git):
    checkout = EXTERNAL / "miniJVM"
    revision = (EXTERNAL / "miniJVM.commit").read_text(encoding="ascii").strip()
    patch = EXTERNAL / "miniJVM.patch"
    if not checkout.exists():
        with tempfile.TemporaryDirectory(prefix="miniJVM-", dir=EXTERNAL) as staging:
            run(git, "init", "--quiet", cwd=staging)
            run(git, "config", "core.autocrlf", "false", cwd=staging)
            run(git, "remote", "add", "origin", MINIJVM_UPSTREAM, cwd=staging)
            run(git, "fetch", "--quiet", "--depth", "1", "origin", revision, cwd=staging)
            run(git, "checkout", "--quiet", "--detach", revision, cwd=staging)
            run(git, "apply", patch, cwd=staging)
            Path(staging).rename(checkout)
    if not (checkout / ".git").exists():
        raise ValueError("external/miniJVM must be the pinned Git checkout created by setup.")
    if run(git, "rev-parse", "HEAD", cwd=checkout, capture=True) != revision:
        raise ValueError(f"external/miniJVM is at a different revision. Expected {revision}.")
    run(git, "apply", "--reverse", "--check", patch, cwd=checkout)
    return checkout


def stage_game(source, destination):
    digest = hashlib.sha256()
    with source.open("rb") as original, destination.open("wb") as staged:
        for chunk in iter(lambda: original.read(65536), b""):
            digest.update(chunk)
            staged.write(chunk)
    if digest.hexdigest() != JAR_HASH:
        raise ValueError(
            "Unsupported JAR. Supply the original Sony Pictures v1.4.62 (a1) build. "
            "See the SHA-256 in README.md."
        )
    with zipfile.ZipFile(destination) as archive:
        if "GOWMIDlet.class" not in archive.namelist() or archive.testzip() is not None:
            raise ValueError("The game JAR is damaged or is missing GOWMIDlet.class.")


def build_java(javac, jar, sources, staging, name):
    classes = staging / name
    classes.mkdir()
    source_list = staging / (name + ".txt")
    source_list.write_text(
        "".join(f'"{path.relative_to(ROOT).as_posix()}"\n' for path in sorted(sources)),
        encoding="utf-8",
    )
    run(javac, "--release", "8", "-encoding", "UTF-8", "-nowarn",
        "-d", classes, "@" + str(source_list))
    run(jar, "cf", staging / (name + ".jar"), "-C", classes, ".")


def install(staging, target, directory, native_files):
    files = [(staging / name, name) for name in COMMON_FILES]
    files.extend((staging / target / name, name) for name in native_files)
    for source, _ in files:
        if not source.is_file() or not source.stat().st_size:
            raise ValueError(f"Build did not create {source.name}.")
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".install-", dir=directory) as temporary:
        for source, name in files:
            shutil.copyfile(source, Path(temporary) / name)
        for _, name in files:
            (Path(temporary) / name).replace(directory / name)
    print(f"Ready: {directory}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("windows", "switch", "both"), default="windows")
    parser.add_argument("--jar", type=Path, default=ROOT / "inputs" / "betrayal.jar",
                        help="Original v1.4.62 (a1) JAR. Default: inputs/betrayal.jar.")
    args = parser.parse_args()
    if os.name != "nt":
        parser.exit(1, "Setup currently runs on Windows.\n")

    try:
        if not args.jar.is_file():
            raise ValueError(f"Game JAR not found: {args.jar}")
        with tempfile.TemporaryDirectory(prefix=".build-", dir=ROOT) as temporary:
            staging = Path(temporary)
            stage_game(args.jar, staging / "juego.jar")
            env = os.environ.copy()
            program_files = Path(env.get("ProgramFiles", r"C:\Program Files"))
            git = program("git.exe", program_files / "Git/cmd/git.exe")
            bash = program("bash.exe", program_files / "Git/bin/bash.exe")
            java_home = env.get("JAVA_HOME")
            javac = program("javac.exe", Path(java_home) / "bin/javac.exe" if java_home else None)
            jar = program("jar.exe", javac.with_name("jar.exe"))
            mingw = Path(env.setdefault("BETRAYAL_MINGW", r"C:\msys64\mingw64"))
            devkit = Path(env.setdefault("BETRAYAL_DEVKITPRO", r"C:\devkitPro"))
            windows = args.target in ("windows", "both")
            switch = args.target in ("switch", "both")
            if windows and not (mingw / "bin/gcc.exe").is_file():
                raise ValueError(f"MinGW-w64 GCC was not found in {mingw}. See README.md.")
            if switch and not (devkit / "devkitA64/bin/aarch64-none-elf-gcc.exe").is_file():
                raise ValueError(f"devkitA64 was not found in {devkit}. See README.md.")
            env["BETRAYAL_DEVKITPRO"] = str(devkit)

            minijvm = prepare_minijvm(git)
            build_java(javac, jar, (minijvm / "minijvm/java/src/main/java").rglob("*.java"),
                       staging, "minijvm_rt")
            build_java(javac, jar, (ROOT / "java").rglob("*.java"), staging, "gownx")

            if windows:
                run(bash, "./build-windows.sh", staging.name, env=env)
            if switch:
                run(bash, "./build-switch.sh", staging.name, env=env)
            if windows:
                install(staging, "windows", ROOT / "dist/windows", WINDOWS_FILES)
            if switch:
                install(staging, "switch", ROOT / "dist/switch-sd/switch/gow_nx", SWITCH_FILES)
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Setup failed: {error}\n")


if __name__ == "__main__":
    main()
