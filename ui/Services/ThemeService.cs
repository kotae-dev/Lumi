using System;
using System.Windows;
using Wpf.Ui.Appearance;

namespace Lumi.Services
{
    /// <summary>
    /// Manages dark/light theme switching. Follows Windows system theme by default.
    /// </summary>
    public class ThemeService
    {
        private static readonly Lazy<ThemeService> _lazy = new(() => new ThemeService());
        public static ThemeService Instance => _lazy.Value;

        private ThemeService() { }

        public void Initialize()
        {
            // Apply theme based on system setting
            ApplicationThemeManager.Apply(GetSystemTheme());

            // Watch for system theme changes
            SystemThemeWatcher.Watch(Application.Current.MainWindow);
        }

        public void SetTheme(string theme)
        {
            switch (theme.ToLowerInvariant())
            {
                case "dark":
                    ApplicationThemeManager.Apply(ApplicationTheme.Dark);
                    break;
                case "light":
                    ApplicationThemeManager.Apply(ApplicationTheme.Light);
                    break;
                default: // "system"
                    ApplicationThemeManager.Apply(GetSystemTheme());
                    break;
            }
        }

        private static ApplicationTheme GetSystemTheme()
        {
            return ApplicationThemeManager.GetSystemTheme() == SystemTheme.Dark
                ? ApplicationTheme.Dark
                : ApplicationTheme.Light;
        }
    }
}
