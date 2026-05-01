using System;
using System.IO;
using System.Windows;
using Lumi.Services;

namespace Lumi
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Initialize theme
            ThemeService.Instance.Initialize();

            // Initialize vault
            string vaultDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "Lumi");
            Directory.CreateDirectory(vaultDir);

            string vaultPath = Path.Combine(vaultDir, "vault.lumi");
            VaultManager.Instance.Initialize(vaultPath);
        }

        protected override void OnExit(ExitEventArgs e)
        {
            VaultManager.Instance.Close();
            base.OnExit(e);
        }
    }
}
