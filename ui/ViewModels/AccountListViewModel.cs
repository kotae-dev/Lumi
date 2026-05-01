using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Windows.Data;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Lumi.Core;
using Lumi.Services;

namespace Lumi.ViewModels
{
    /// <summary>
    /// Main ViewModel — manages the account list, search filtering,
    /// and account add/import actions.
    /// </summary>
    public partial class AccountListViewModel : BaseViewModel
    {
        private readonly ObservableCollection<AccountItemViewModel> _accounts = new();
        private ICollectionView? _filteredView;

        [ObservableProperty]
        private string _searchQuery = string.Empty;

        [ObservableProperty]
        private bool _isAddPanelOpen;

        [ObservableProperty]
        private bool _hasAccounts;

        public ICollectionView FilteredAccounts
        {
            get
            {
                if (_filteredView == null)
                {
                    _filteredView = CollectionViewSource.GetDefaultView(_accounts);
                    _filteredView.Filter = FilterAccount;
                }
                return _filteredView;
            }
        }

        public IRelayCommand AddQrCommand { get; }
        public IRelayCommand AddManualCommand { get; }
        public IRelayCommand AddUriCommand { get; }
        public IRelayCommand SettingsCommand { get; }
        public IRelayCommand RefreshCommand { get; }

        public AccountListViewModel()
        {
            AddQrCommand = new RelayCommand(OpenQrImport);
            AddManualCommand = new RelayCommand(OpenManualAdd);
            AddUriCommand = new RelayCommand(OpenUriImport);
            SettingsCommand = new RelayCommand(OpenSettings);
            RefreshCommand = new RelayCommand(LoadAccounts);

            LoadAccounts();
        }

        partial void OnSearchQueryChanged(string value)
        {
            _filteredView?.Refresh();
        }

        private bool FilterAccount(object obj)
        {
            if (string.IsNullOrWhiteSpace(SearchQuery)) return true;

            if (obj is AccountItemViewModel vm)
            {
                string query = SearchQuery.ToLowerInvariant();
                return (vm.Name?.ToLowerInvariant().Contains(query) ?? false) ||
                       (vm.Issuer?.ToLowerInvariant().Contains(query) ?? false);
            }

            return false;
        }

        public void LoadAccounts()
        {
            // Stop existing timers
            foreach (var vm in _accounts) vm.StopTimer();
            _accounts.Clear();

            var accounts = VaultManager.Instance.GetAllAccounts();
            foreach (var account in accounts)
            {
                var native = account.ToNative();
                var vm = new AccountItemViewModel(native);
                vm.AccountDeleted += () =>
                {
                    _accounts.Remove(vm);
                    HasAccounts = _accounts.Any();
                };
                _accounts.Add(vm);
            }

            HasAccounts = _accounts.Any();
        }

        public void ImportAccounts(LumiAccountNative[] accounts)
        {
            foreach (var native in accounts)
            {
                var account = TotpAccount.FromNative(native);
                VaultManager.Instance.SaveAccount(account);
            }
            LoadAccounts();
        }

        private void OpenQrImport()
        {
            var dialog = new Microsoft.Win32.OpenFileDialog
            {
                Title = "Import QR Code Image",
                Filter = "Image Files|*.png;*.jpg;*.jpeg;*.bmp;*.gif|All Files|*.*",
                Multiselect = false
            };

            if (dialog.ShowDialog() == true)
            {
                var imported = VaultManager.Instance.ImportFromQrFile(dialog.FileName);
                if (imported.Any())
                {
                    foreach (var account in imported)
                    {
                        VaultManager.Instance.SaveAccount(account);
                    }
                    LoadAccounts();
                }
            }
        }

        private void OpenManualAdd()
        {
            var dialog = new Views.AddAccountDialog();
            dialog.Owner = System.Windows.Application.Current.MainWindow;
            dialog.ShowDialog();
            if (dialog.AccountAdded)
            {
                LoadAccounts();
            }
        }

        private void OpenUriImport()
        {
            try
            {
                // Check if clipboard has image (QR code)
                if (System.Windows.Clipboard.ContainsImage())
                {
                    var imageSource = System.Windows.Clipboard.GetImage();
                    if (imageSource != null)
                    {
                        string tempFile = System.IO.Path.GetTempFileName() + ".png";
                        using (var fileStream = new System.IO.FileStream(tempFile, System.IO.FileMode.Create))
                        {
                            var encoder = new System.Windows.Media.Imaging.PngBitmapEncoder();
                            encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(imageSource));
                            encoder.Save(fileStream);
                        }

                        var imported = VaultManager.Instance.ImportFromQrFile(tempFile);
                        System.IO.File.Delete(tempFile);

                        if (imported.Any())
                        {
                            foreach (var account in imported)
                                VaultManager.Instance.SaveAccount(account);
                            LoadAccounts();
                            return;
                        }
                    }
                }

                // Fallback to text URI
                if (System.Windows.Clipboard.ContainsText())
                {
                    string uri = System.Windows.Clipboard.GetText().Trim();
                    if (uri.StartsWith("otpauth://") || uri.StartsWith("otpauth-migration://"))
                    {
                        var account = VaultManager.Instance.ImportFromUri(uri);
                        if (account != null)
                        {
                            VaultManager.Instance.SaveAccount(account);
                            LoadAccounts();
                        }
                    }
                }
            }
            catch
            {
                // Ignore clipboard errors
            }
        }

        private void OpenSettings()
        {
            var settings = new Views.SettingsWindow();
            settings.Owner = System.Windows.Application.Current.MainWindow;
            settings.ShowDialog();
        }
    }
}
