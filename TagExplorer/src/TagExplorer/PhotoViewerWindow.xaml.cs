using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Imaging;
namespace TagExplorer;
public partial class PhotoViewerWindow : Window
{
    private readonly IReadOnlyList<string> _paths;
    private int _index;
    private double _angle;
    public PhotoViewerWindow(IReadOnlyList<string> paths, int startIndex)
    {
        InitializeComponent(); _paths = paths; _index = Math.Clamp(startIndex, 0, Math.Max(0, paths.Count - 1)); ShowCurrent();
    }
    private void ShowCurrent()
    {
        if (_paths.Count == 0) return;
        var path = _paths[_index];
        try {
            var bitmap = new BitmapImage(); bitmap.BeginInit(); bitmap.CacheOption = BitmapCacheOption.OnLoad; bitmap.UriSource = new Uri(path); bitmap.EndInit(); bitmap.Freeze();
            FullImage.Source = bitmap; _angle = 0; ImageRotation.Angle = 0; PhotoTitle.Text = System.IO.Path.GetFileName(path);
            PhotoCounter.Text = $"{_index + 1} of {_paths.Count} • Rotation is for viewing only; original file is unchanged";
        } catch (Exception ex) { FullImage.Source = null; PhotoTitle.Text = System.IO.Path.GetFileName(path); PhotoCounter.Text = $"Unable to display image: {ex.Message}"; }
    }
    private void RotateLeft_Click(object sender, RoutedEventArgs e) => Rotate(-90);
    private void RotateRight_Click(object sender, RoutedEventArgs e) => Rotate(90);
    private void Rotate(double amount) { _angle = (_angle + amount + 360) % 360; ImageRotation.Angle = _angle; }
    private void Previous_Click(object sender, RoutedEventArgs e) => Move(-1);
    private void Next_Click(object sender, RoutedEventArgs e) => Move(1);
    private void Move(int amount) { if (_paths.Count == 0) return; _index = (_index + amount + _paths.Count) % _paths.Count; ShowCurrent(); }
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
    private void Window_KeyDown(object sender, KeyEventArgs e) {
        switch (e.Key) { case Key.Left: Move(-1); e.Handled = true; break; case Key.Right: Move(1); e.Handled = true; break; case Key.R: Rotate(90); e.Handled = true; break; case Key.Escape: Close(); e.Handled = true; break; }
    }
}