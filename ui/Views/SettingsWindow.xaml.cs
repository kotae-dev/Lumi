using System.Windows;
using System.Windows.Controls;
using Lumi.Services;
using Wpf.Ui.Controls;

namespace Lumi.Views
{
    public partial class SettingsWindow : FluentWindow
    {
        private bool _initializing = true;
        public SettingsWindow()
        {
            InitializeComponent();
            
            // Sync current theme to combo box
            var currentTheme = Wpf.Ui.Appearance.ApplicationThemeManager.GetAppTheme();
            
            // Explicitly apply theme to this window to ensure background matches
            Wpf.Ui.Appearance.ApplicationThemeManager.Apply(this);

            ThemeCombo.SelectedIndex = currentTheme switch
            {
                Wpf.Ui.Appearance.ApplicationTheme.Dark => 1,
                Wpf.Ui.Appearance.ApplicationTheme.Light => 2,
                _ => 0
            };
            
            _initializing = false;
        }

        private void OnThemeChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_initializing) return;

            if (ThemeCombo.SelectedItem is ComboBoxItem item &&
                item.Tag is string theme)
            {
                ThemeService.Instance.SetTheme(theme);
                
                // Immediately apply to this window too
                var themeEnum = theme switch {
                    "dark" => Wpf.Ui.Appearance.ApplicationTheme.Dark,
                    "light" => Wpf.Ui.Appearance.ApplicationTheme.Light,
                    _ => Wpf.Ui.Appearance.ApplicationThemeManager.GetSystemTheme() == Wpf.Ui.Appearance.SystemTheme.Dark ? Wpf.Ui.Appearance.ApplicationTheme.Dark : Wpf.Ui.Appearance.ApplicationTheme.Light
                };
                Wpf.Ui.Appearance.ApplicationThemeManager.Apply(this);
            }
        }
    }
}
