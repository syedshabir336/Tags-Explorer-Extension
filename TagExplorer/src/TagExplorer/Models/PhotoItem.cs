namespace TagExplorer.Models;
public sealed class PhotoItem
{
    public string Path { get; set; } = "";
    public string Name => System.IO.Path.GetFileName(Path);
    public string Tags { get; set; } = "";
    public string ThumbnailPath => Path;
}
public sealed record TagInfo(long Id, string Name, string Color);