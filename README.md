# Tag Explorer for Windows

A local photo gallery and tag manager. The repository currently contains two implementations:

- **Native C edition** in `TagExplorer-native/`: Win32 + WIC + SQLite, with no .NET runtime or browser engine. This is the lightweight implementation under active development.
- **WPF edition** in `TagExplorer/`: the earlier .NET 8 implementation, retained as a fallback while the native edition is validated.

See [TagExplorer-native/README.md](TagExplorer-native/README.md) for native build instructions and current feature status.

Both editions keep photo files in their original locations and store catalog/tag data locally in `%LOCALAPPDATA%\TagExplorer\catalog.db`. Photo contents are not uploaded.
