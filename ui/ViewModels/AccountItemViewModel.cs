using System;
using System.Text;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Lumi.Core;
using Lumi.Services;

namespace Lumi.ViewModels
{
    /// <summary>
    /// ViewModel for a single account card — manages live TOTP code,
    /// countdown timer, copy-to-clipboard, and inline name editing.
    /// </summary>
    public partial class AccountItemViewModel : BaseViewModel
    {
        private readonly DispatcherTimer _timer;
        private LumiAccountNative _native;
        private DateTime? _copyFeedbackTime;

        [ObservableProperty]
        private string _name = string.Empty;

        [ObservableProperty]
        private string _issuer = string.Empty;

        [ObservableProperty]
        private string _code = "------";

        [ObservableProperty]
        private int _secondsLeft;

        [ObservableProperty]
        private bool _isEditing;

        [ObservableProperty]
        private bool _showCopied;

        public string Id { get; }

        public string FormattedCode => Code.Length == 8
            ? $"{Code[..4]} {Code[4..]}"
            : $"{Code[..3]} {Code[3..]}";

        public string IconText
        {
            get
            {
                string text = !string.IsNullOrWhiteSpace(Issuer) ? Issuer.Trim() :
                              !string.IsNullOrWhiteSpace(Name) ? Name.Trim() : "?";
                if (string.IsNullOrEmpty(text)) return "?";
                if (text.Length >= 2 && char.IsLetterOrDigit(text[0]) && char.IsLetterOrDigit(text[1]))
                {
                    return text.Substring(0, 1).ToUpperInvariant();
                }
                return text.Substring(0, 1).ToUpperInvariant();
            }
        }

        public bool IsExpiringSoon => SecondsLeft <= 5;

        public IRelayCommand CopyCommand { get; }
        public IRelayCommand StartEditCommand { get; }
        public IRelayCommand CommitEditCommand { get; }
        public IRelayCommand DeleteCommand { get; }

        public event Action? AccountDeleted;

        [ObservableProperty]
        private double _progress;

        public AccountItemViewModel(LumiAccountNative native)
        {
            _native = native;
            Id = native.Id ?? string.Empty;
            _issuer = native.Issuer ?? string.Empty;
            _name = native.Name ?? string.Empty;

            CopyCommand = new RelayCommand(CopyToClipboard);
            StartEditCommand = new RelayCommand(() => IsEditing = true);
            CommitEditCommand = new RelayCommand(CommitEdit);
            DeleteCommand = new RelayCommand(Delete);

            _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(30) };
            _timer.Tick += OnTimerTick;
            _timer.Start();

            Refresh();
        }

        private void OnTimerTick(object? sender, EventArgs e)
        {
            Refresh();

            // Auto-hide copy feedback after 2 seconds
            if (_copyFeedbackTime.HasValue &&
                DateTime.UtcNow - _copyFeedbackTime.Value > TimeSpan.FromSeconds(2))
            {
                ShowCopied = false;
                _copyFeedbackTime = null;
            }
        }

        private void Refresh()
        {
            var sb = new StringBuilder(9);
            int result = NativeInterop.lumi_get_totp(ref _native, sb, out int secsLeft);

            if (result == 0)
            {
                string newCode = sb.ToString();
                if (newCode != Code)
                {
                    Code = newCode;
                    OnPropertyChanged(nameof(FormattedCode));
                }
            }

            SecondsLeft = secsLeft;

            // Calculate precise smooth progress
            int period = _native.Period > 0 ? _native.Period : 30;
            long ms = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
            double fraction = 1.0 - ((ms % 1000) / 1000.0);
            if (secsLeft == period && fraction < 0.1) fraction = 1.0; // avoid flicker
            double preciseSeconds = Math.Max(0, secsLeft - 1 + fraction);
            
            Progress = preciseSeconds / period;

            OnPropertyChanged(nameof(IsExpiringSoon));
        }

        private void CopyToClipboard()
        {
            try
            {
                Clipboard.SetText(Code);
                ShowCopied = true;
                _copyFeedbackTime = DateTime.UtcNow;
            }
            catch
            {
                // Clipboard can throw if locked by another process
            }
        }

        private void CommitEdit()
        {
            IsEditing = false;
            if (!string.IsNullOrWhiteSpace(Name))
            {
                // Update native struct
                _native.Name = Name;
                // Persist to vault
                VaultManager.Instance.RenameAccount(Id, Name);
            }
        }

        private void Delete()
        {
            if (VaultManager.Instance.DeleteAccount(Id))
            {
                _timer.Stop();
                AccountDeleted?.Invoke();
            }
        }

        public void StopTimer() => _timer.Stop();
    }
}
