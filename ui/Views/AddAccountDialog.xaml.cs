using System;
using System.Windows;
using Lumi.Core;
using Lumi.Services;
using Wpf.Ui.Controls;

namespace Lumi.Views
{
    public partial class AddAccountDialog : FluentWindow
    {
        public bool AccountAdded { get; private set; }

        public AddAccountDialog()
        {
            InitializeComponent();
        }

        private void OnAdd(object sender, RoutedEventArgs e)
        {
            string name = NameBox.Text.Trim();
            string issuer = IssuerBox.Text.Trim();
            string secret = SecretBox.Text.Trim().Replace(" ", "").ToUpperInvariant();

            if (string.IsNullOrWhiteSpace(secret))
            {
                System.Windows.MessageBox.Show(
                    "Please enter a secret key.",
                    "Validation Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Warning);
                return;
            }

            // Determine algorithm
            int algorithm = 0;
            if (AlgoSHA256.IsChecked == true) algorithm = 1;
            else if (AlgoSHA512.IsChecked == true) algorithm = 2;

            int digits = Digits8.IsChecked == true ? 8 : 6;
            int period = Period60.IsChecked == true ? 60 : 30;

            // Build otpauth URI and parse it
            string uri = $"otpauth://totp/{Uri.EscapeDataString(issuer)}:{Uri.EscapeDataString(name)}" +
                         $"?secret={secret}&issuer={Uri.EscapeDataString(issuer)}" +
                         $"&algorithm={(algorithm == 0 ? "SHA1" : algorithm == 1 ? "SHA256" : "SHA512")}" +
                         $"&digits={digits}&period={period}";

            var account = VaultManager.Instance.ImportFromUri(uri);
            if (account != null)
            {
                // Override name/issuer if provided
                if (!string.IsNullOrWhiteSpace(name)) account.Name = name;
                if (!string.IsNullOrWhiteSpace(issuer)) account.Issuer = issuer;

                VaultManager.Instance.SaveAccount(account);
                AccountAdded = true;
                Close();
            }
            else
            {
                System.Windows.MessageBox.Show(
                    "Invalid secret key. Please check and try again.",
                    "Import Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }
        }

        private void OnCancel(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
