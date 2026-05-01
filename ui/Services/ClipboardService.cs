using System;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

namespace Lumi.Services
{
    /// <summary>
    /// Auto-clears clipboard after a configurable delay.
    /// </summary>
    public class ClipboardService
    {
        private static CancellationTokenSource? _clearCts;

        /// <summary>
        /// Sets clipboard text and schedules auto-clear after delay.
        /// </summary>
        public static void SetTextWithAutoClear(string text, int clearAfterSeconds = 30)
        {
            try
            {
                Clipboard.SetText(text);
            }
            catch
            {
                return; // Clipboard locked
            }

            // Cancel any previous clear timer
            _clearCts?.Cancel();
            _clearCts = new CancellationTokenSource();
            var token = _clearCts.Token;

            Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(TimeSpan.FromSeconds(clearAfterSeconds), token);
                    if (!token.IsCancellationRequested)
                    {
                        Application.Current?.Dispatcher.Invoke(() =>
                        {
                            try
                            {
                                if (Clipboard.GetText() == text)
                                {
                                    Clipboard.Clear();
                                }
                            }
                            catch { }
                        });
                    }
                }
                catch (TaskCanceledException) { }
            });
        }
    }
}
