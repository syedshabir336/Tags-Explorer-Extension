using System.Globalization;
using System.IO;
using System.Windows.Data;
using Binding = System.Windows.Data.Binding;
using System.Windows.Media.Imaging;
namespace TagExplorer;
public sealed class ThumbnailConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is not string path) return Binding.DoNothing;
        if (!File.Exists(path)) return Binding.DoNothing;
        try { var bitmap = new BitmapImage(); bitmap.BeginInit(); bitmap.CacheOption = BitmapCacheOption.OnLoad; bitmap.DecodePixelWidth = 260; bitmap.UriSource = new Uri(path); bitmap.EndInit(); bitmap.Freeze(); return bitmap; }
        catch { return Binding.DoNothing; }
    }
    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) => throw new NotSupportedException();
}