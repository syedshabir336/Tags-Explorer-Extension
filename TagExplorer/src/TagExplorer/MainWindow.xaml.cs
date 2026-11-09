using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Win32;
using TagExplorer.Models;
using TagExplorer.Services;

namespace TagExplorer;

public partial class MainWindow : Window
{
    private readonly TagDatabase _database = new();
    private readonly ObservableCollection<TagInfo> _tags = new();
    private readonly ObservableCollection<PhotoItem> _photos = new();
    private string? _selectedTag;

    public MainWindow()
    {
        InitializeComponent();
        TagList.ItemsSource = _tags;
        PhotoList.ItemsSource = _photos;
        Loaded += (_, _) =>
        {
            RefreshTags();
            var args = Environment.GetCommandLineArgs();
            var quickTagIndex = Array.FindIndex(args, x => string.Equals(x, "--quick-tag", StringComparison.OrdinalIgnoreCase));
            if (quickTagIndex >= 0 && quickTagIndex + 1 < args.Length)
                QuickTag(args[quickTagIndex + 1]);
            RefreshPhotos();
        };
        PreviewKeyDown += MainWindow_PreviewKeyDown;
    }

    private void AddFiles_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Add images to Tag Explorer",
            Filter = "Image files|*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic|All files|*.*",
            Multiselect = true
        };
        if (dialog.ShowDialog(this) == true)
        {
            _database.RegisterImages(dialog.FileNames);
            RefreshPhotos();
            StatusText.Text = $"Added {dialog.FileNames.Length} image(s).";
        }
    }

    private void AddFolder_Click(object sender, RoutedEventArgs e)
    {
        using var dialog = new System.Windows.Forms.FolderBrowserDialog { Description = "Choose a folder containing images" };
        if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
        {
            var extensions = new HashSet<string>(new[] { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tif", ".tiff", ".webp", ".heic" }, StringComparer.OrdinalIgnoreCase);
            var files = Directory.EnumerateFiles(dialog.SelectedPath, "*", SearchOption.AllDirectories)
                .Where(p => extensions.Contains(System.IO.Path.GetExtension(p))).ToArray();
            _database.RegisterImages(files);
            RefreshPhotos();
            StatusText.Text = $"Added {files.Length} image(s) from folder.";
        }
    }

    private void InstallShell_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            ShellIntegration.Install();
            StatusText.Text = "File Explorer context menu installed for this Windows user.";
        }
        catch (Exception ex) { MessageBox.Show(this, ex.Message, "Explorer integration", MessageBoxButton.OK, MessageBoxImage.Error); }
    }

    private void CreateTag_Click(object sender, RoutedEventArgs e)
    {
        var name = Prompt("Enter a name for the new tag:", "Create tag");
        if (string.IsNullOrWhiteSpace(name)) return;
        _database.EnsureTag(name);
        RefreshTags();
        StatusText.Text = $"Tag '{name.Trim()}' is ready.";
    }

    private void TagList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        _selectedTag = (TagList.SelectedItem as TagInfo)?.Name;
        RefreshPhotos();
    }

    private void SearchBox_TextChanged(object sender, TextChangedEventArgs e) => RefreshPhotos();

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
        SearchBox.Clear();
        TagList.SelectedItem = null;
        _selectedTag = null;
        RefreshPhotos();
    }

    private void ApplyTags_Click(object sender, RoutedEventArgs e)
    {
        var selected = PhotoList.SelectedItems.Cast<PhotoItem>().Select(p => p.Path).ToArray();
        if (selected.Length == 0)
        {
            StatusText.Text = "Select one or more images first.";
            return;
        }
        var input = Prompt("Enter tags separated by commas:", "Apply tags");
        if (string.IsNullOrWhiteSpace(input)) return;
        var names = input.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        _database.ApplyTags(selected, names);
        RefreshTags();
        RefreshPhotos();
        StatusText.Text = $"Applied {names.Length} tag(s) to {selected.Length} image(s).";
    }

    private void Refresh_Click(object sender, RoutedEventArgs e)
    {
        RefreshTags();
        RefreshPhotos();
        StatusText.Text = "Catalog refreshed.";
    }

    private void RefreshTags()
    {
        var selectedName = _selectedTag;
        _tags.Clear();
        foreach (var tag in _database.GetTags()) _tags.Add(tag);
        TagList.SelectedItem = _tags.FirstOrDefault(t => t.Name == selectedName);
    }

    private void RefreshPhotos()
    {
        _photos.Clear();
        foreach (var photo in _database.Search(SearchBox?.Text, _selectedTag)) _photos.Add(photo);
        if (StatusText is not null) StatusText.Text = $"{_photos.Count} image(s) in view.";
    }

    private void QuickTag(string path)
    {
        if (!File.Exists(path)) return;
        _database.RegisterImages(new[] { path });
        var tags = Prompt($"Add comma-separated tags to:\n{System.IO.Path.GetFileName(path)}", "Quick tag");
        if (!string.IsNullOrWhiteSpace(tags))
            _database.ApplyTags(new[] { path }, tags.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries));
    }

    private void MainWindow_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.T && Keyboard.Modifiers == (ModifierKeys.Control | ModifierKeys.Alt))
        {
            ApplyTags_Click(this, new RoutedEventArgs());
            e.Handled = true;
        }
    }

    private string? Prompt(string message, string title)
    {
        var dialog = new Window
        {
            Owner = this, Title = title, Width = 430, Height = 190,
            WindowStartupLocation = WindowStartupLocation.CenterOwner, ResizeMode = ResizeMode.NoResize
        };
        var panel = new DockPanel { Margin = new Thickness(14) };
        var label = new TextBlock { Text = message, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 10) };
        DockPanel.SetDock(label, Dock.Top);
        panel.Children.Add(label);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        var ok = new Button { Content = "OK", IsDefault = true, MinWidth = 76, Margin = new Thickness(5) };
        var cancel = new Button { Content = "Cancel", IsCancel = true, MinWidth = 76, Margin = new Thickness(5) };
        buttons.Children.Add(ok);
        buttons.Children.Add(cancel);
        DockPanel.SetDock(buttons, Dock.Bottom);
        panel.Children.Add(buttons);
        var input = new TextBox { Margin = new Thickness(0, 0, 0, 10), MinWidth = 360 };
        panel.Children.Add(input);
        ok.Click += (_, _) => dialog.DialogResult = true;
        dialog.Content = panel;
        return dialog.ShowDialog() == true ? input.Text : null;
    }
}
