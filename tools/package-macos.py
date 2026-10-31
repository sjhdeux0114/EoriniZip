"""Build a self-contained, ad-hoc signed macOS ZIP and drag-install DMG."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def dependency_notices(destination, architecture):
    packages = {}
    for codec in ("unalz", "unegg", "easycrab"):
        result = run("cargo", "metadata", "--locked", "--format-version", "1",
                     "--filter-platform", f"{architecture}-apple-darwin",
                     "--manifest-path", ROOT / "third_party" / codec / "Cargo.toml",
                     capture_output=True, text=True)
        for package in json.loads(result.stdout)["packages"]:
            if package["source"]:
                packages[f'{package["name"]}-{package["version"]}'] = package
    for name, package in sorted(packages.items()):
        source = Path(package["manifest_path"]).parent
        files = [p for p in source.iterdir() if p.is_file()
                 and p.name.upper().startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE"))]
        if not files and (source / "LICENSES").is_dir():
            files = [p for p in (source / "LICENSES").iterdir() if p.is_file()]
        fallback = ROOT / "third_party" / "notices" / name
        if not files and fallback.is_dir():
            files = [p for p in fallback.iterdir() if p.is_file()]
        if not files:
            raise RuntimeError(f"Missing dependency license: {name}")
        target = destination / name
        target.mkdir(parents=True)
        for file in files:
            shutil.copy2(file, target / file.name)
        (target / "package.txt").write_text(
            f'{name}\nLicense: {package["license"]}\n{package["repository"]}\n', encoding="utf-8")
    print(f"Collected {len(packages)} dependency notices", flush=True)


def package(build, engine, destination, arch):
    if platform.system() != "Darwin":
        raise RuntimeError("Run packaging on macOS, not a cross-build host")
    version = "0.5.1"
    destination.mkdir(parents=True, exist_ok=True)
    stem = f"EoriniZip-{version}-macos-{arch}"
    # Private staging avoids carrying old package files into a new release.
    with tempfile.TemporaryDirectory(prefix="EoriniZip-package-") as scratch:
        stage = Path(scratch) / "image"
        stage.mkdir()
        app = stage / "EoriniZip.app"
        shutil.copytree(build / "EoriniZip.app", app, symlinks=True)
        binaries = app / "Contents" / "MacOS"
        shutil.copy2(engine / "7zz", binaries / "7zz")
        for codec in ("unalz", "unegg", "easycrab", "zpaq"):
            name = f"eorinizip-{codec}"
            # CMake places these beside the bundle executable.
            if not (binaries / name).is_file():
                raise RuntimeError(f"Missing bundled codec: {name}")
        licenses = app / "Contents" / "Resources" / "licenses"
        licenses.mkdir(parents=True)
        shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.txt", licenses)
        for codec in ("unalz", "unegg", "easycrab", "zpaq"):
            target = licenses / codec
            target.mkdir()
            for name in ("LICENSE", "COPYING", "UPSTREAM_COMMIT"):
                source = ROOT / "third_party" / codec / name
                if source.is_file():
                    shutil.copy2(source, target / name)
        shutil.copy2(ROOT / "third_party" / "zpaq" / "libzpaq.cpp", licenses / "zpaq")
        sevenzip_license = licenses / "7zip"
        sevenzip_license.mkdir()
        for name in ("License.txt", "readme.txt", "7z2605-src.tar.xz"):
            shutil.copy2(engine / name, sevenzip_license / name)
        dependency_notices(licenses / "rust", "aarch64" if arch == "arm64" else "x86_64")
        with (app / "Contents" / "Info.plist").open("rb") as file:
            info = plistlib.load(file)
        if info["CFBundleShortVersionString"] != version or info["LSMinimumSystemVersion"] != "13.0":
            raise RuntimeError("Unexpected bundle version or minimum macOS version")
        for binary in sorted(binaries.iterdir()):
            if not binary.is_file():
                raise RuntimeError(f"Unexpected bundle executable entry: {binary}")
            binary.chmod(binary.stat().st_mode | 0o111)
            run("lipo", binary, "-verify_arch", arch)
            linked = run("otool", "-L", binary, capture_output=True, text=True).stdout
            if any(path in linked for path in ("/opt/homebrew/", "/usr/local/", "/Users/runner/")):
                raise RuntimeError(f"Nonportable dynamic library reference: {binary.name}")
            run("codesign", "--force", "--sign", "-", binary)
        run("codesign", "--force", "--sign", "-", app)
        run("codesign", "--verify", "--deep", "--strict", "--verbose=2", app)
        # Test the actual packaged engine/codecs from a disposable bundle copy.
        validation = Path(scratch) / "validation.app"
        shutil.copytree(app, validation, symlinks=True)
        test_binary = validation / "Contents" / "MacOS" / "core_tests"
        shutil.copy2(build / "core_tests", test_binary)
        run(test_binary, validation / "Contents" / "MacOS" / "7zz", ROOT / "third_party")
        readme = ("어린이zip 0.5.1 — macOS 13 이상\n\n"
                  "EoriniZip.app을 Applications로 드래그해 설치하세요.\n"
                  "7-Zip과 코덱이 포함되어 별도 엔진 설치가 필요하지 않습니다.\n"
                  "이 개발 빌드는 ad-hoc 서명이며 Apple Developer ID 서명/공증은 없습니다.\n"
                  "Windows 전용 기능 전체가 Mac UI에 구현된 버전은 아닙니다.\n"
                  "프로젝트: https://github.com/sjhdeux0114/EoriniZip\n")
        (stage / "설치 안내.txt").write_text(readme, encoding="utf-8")
        os.symlink("/Applications", stage / "Applications")
        zip_path = destination / f"{stem}.zip"
        dmg_path = destination / f"{stem}.dmg"
        run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, zip_path)
        run("hdiutil", "create", "-volname", "EoriniZip 0.5.1", "-srcfolder", stage,
            "-format", "UDZO", "-ov", dmg_path)
        run("hdiutil", "verify", dmg_path)
        sums = []
        for file in (zip_path, dmg_path):
            sums.append(f"{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.name}\n")
        (destination / f"SHA256SUMS-{version}-macos-{arch}.txt").write_text("".join(sums), encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--engine", type=Path, default=ROOT / "vendor" / "7zip")
    parser.add_argument("--destination", type=Path, default=ROOT / "dist")
    parser.add_argument("--arch", choices=("arm64", "x86_64"), required=True)
    args = parser.parse_args()
    package(args.build.resolve(), args.engine.resolve(), args.destination.resolve(), args.arch)
