using System.Windows;
using System.Windows.Controls;
using Lumi.Services;
using Wpf.Ui.Controls;

namespace Lumi.Views
{
    public partial class SettingsWindow : FluentWindow
    {
        public SettingsWindow()
        {
            InitializeComponent();
        }

        private void OnThemeChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ThemeCombo.SelectedItem is ComboBoxItem item &&
                item.Tag is string theme)
            {
                ThemeService.Instance.SetTheme(theme);
            }
        }
    }
}
