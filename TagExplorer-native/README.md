# Tag Explorer — Native C edition

A lightweight native Windows x64 photo catalog and tag manager. This implementation is separate from the existing WPF app while the native replacement is validated.

## Technology
- C11 and Win32 common controls (no .NET, Electron, browser runtime, or bundled UI framework).
- Windows Imaging Component (WIC) for image decoding and thumbnail pixels.
- SQLite amalgamation, fetched at configure time and linked statically.
- GDI for native rendering and owner-drawn icon buttons.\n- Tabler Icons (MIT) SVG assets, rasterized at startup with vendored NanoSVG; no browser or managed runtime.\n- Icons are shipped locally in `assets/icons`, so the UI works offline. See `assets/icons/THIRD_PARTY_NOTICES.txt` and `third_party/nanosvg/` for attribution.
- CMake + MSVC on Windows.

## Current native milestone
- Index individual image files or recursively index a folder.
- Persistent SQLite catalog in `%LOCALAPPDATA%\TagExplorer\catalog.db`, compatible with the existing catalog schema.
- Search by path/name/tag.
- Multi-tag assignment to a selected image.
- Thumbnail grid, selection preview, full-size viewer, keyboard previous/next, and rotate with R.
- No photo copying and no network services at runtime.

## Build (Visual Studio 2022 / Build Tools)
```powershell
cmake -S TagExplorer-native -B build-native -A x64
cmake --build build-native --config Release
```

The Windows GitHub Actions workflow publishes `TagExplorer.exe` as `TagExplorer-Native-win-x64`.

## Image formats
WIC uses codecs installed on Windows. JPEG, PNG, BMP, GIF, TIFF, and ICO are commonly available. HEIF/HEIC and WebP depend on Windows codec availability. This build intentionally does not bundle extra codecs yet.

## Notes
The native edition is an initial port, not yet a feature-complete replacement for the WPF edition. The old WPF project and workflow remain intact until native CI and functional testing pass.
