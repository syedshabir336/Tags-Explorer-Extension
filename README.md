# Tag Explorer for Windows 11

A WPF desktop companion for organizing image files with local tags.

## Features in this milestone
- .NET 8 WPF application with a local SQLite catalog.
- Import individual images or recursively import an image folder.
- Multiple tags per image and bulk tag assignment.
- Search by file path/name and tag; filter by a selected tag.
- Ctrl+Alt+T applies tags to selected images while the app is focused.
- Per-user File Explorer context-menu integration for common image formats.
- Windows GitHub Actions build and self-contained x64 publish artifact.

## Build
On Windows with the .NET 8 SDK:
```powershell
dotnet restore TagExplorer.sln
dotnet build TagExplorer.sln -c Release
dotnet run --project src/TagExplorer/TagExplorer.csproj
```

Use Explorer menu in the app, then right-click a supported image in File Explorer and select Tag with Tag Explorer.

## Integration boundary
This milestone stores tags in %LOCALAPPDATA%\\TagExplorer\\catalog.db. It does not write Windows System.Keywords metadata, so tags are not yet automatically visible in the built-in Explorer Tags column or Windows Photos. Windows Photos does not expose a general third-party plug-in API.

## Privacy
The catalog stores local file paths and tag associations. Image contents are not uploaded or modified.
