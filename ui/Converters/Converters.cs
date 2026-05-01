using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace Lumi.Converters
{
    /// <summary>
    /// Converts a progress value (0.0–1.0) to a circular arc PathGeometry
    /// for the countdown ring.
    /// </summary>
    public class TimeToArcConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            double progress = (double)value; // 0.0 to 1.0 (SecondsLeft / Period)
            double angle = (1.0 - progress) * 360.0; // Depletes clockwise

            if (angle <= 0) angle = 0.01;
            if (angle >= 360) angle = 359.99;

            double cx = 22, cy = 22, r = 20;
            double radStart = -90 * Math.PI / 180.0; // Start at top
            double radEnd = (angle - 90) * Math.PI / 180.0;

            double startX = cx + r * Math.Cos(radStart);
            double startY = cy + r * Math.Sin(radStart);
            double endX = cx + r * Math.Cos(radEnd);
            double endY = cy + r * Math.Sin(radEnd);

            bool largeArc = angle > 180;

            string pathData = $"M {cx.ToString("F2", CultureInfo.InvariantCulture)},{cy.ToString("F2", CultureInfo.InvariantCulture)} " +
                              $"L {startX.ToString("F2", CultureInfo.InvariantCulture)},{startY.ToString("F2", CultureInfo.InvariantCulture)} " +
                              $"A {r},{r} 0 {(largeArc ? 1 : 0)},1 " +
                              $"{endX.ToString("F2", CultureInfo.InvariantCulture)},{endY.ToString("F2", CultureInfo.InvariantCulture)} Z";

            return Geometry.Parse(pathData);
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Formats a code string: "123456" → "123 456", "12345678" → "1234 5678"
    /// </summary>
    public class CodeFormatterConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is string code && code.Length >= 6)
            {
                return code.Length == 8
                    ? $"{code[..4]} {code[4..]}"
                    : $"{code[..3]} {code[3..]}";
            }
            return value?.ToString() ?? "";
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Returns danger color brush when IsExpiringSoon is true.
    /// </summary>
    public class ExpiryColorConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool isExpiring && isExpiring)
            {
                return Application.Current.FindResource("DangerBrush") as Brush
                       ?? Brushes.Red;
            }
            return Application.Current.FindResource("TextPrimaryBrush") as Brush
                   ?? Brushes.White;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Boolean to Visibility (inverse supported via parameter "Inverse").
    /// </summary>
    public class BoolToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            bool val = value is bool b && b;
            bool inverse = parameter is string s && s == "Inverse";
            if (inverse) val = !val;
            return val ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
