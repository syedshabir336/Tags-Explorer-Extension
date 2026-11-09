using Microsoft.Win32;
namespace TagExplorer.Services;

public static class ShellIntegration
{
    private static readonly string[] Extensions = { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tif", ".tiff", ".webp", ".heic" };

    public static void Install()
    {
        var executable = Environment.ProcessPath ?? throw new InvalidOperationException("Cannot locate executable");
        foreach (var extension in Extensions)
        {
            using var key = Registry.CurrentUser.CreateSubKey($@"Software\Classes\SystemFileAssociations\{extension}\shell\TagExplorer");
            key.SetValue("", "Tag with Tag Explorer");
            key.SetValue("Icon", $"\"{executable}\",0");
            using var command = key.CreateSubKey("command");
            command.SetValue("", $"\"{executable}\" --quick-tag \"%1\"");
        }
    }
}