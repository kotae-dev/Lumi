using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Lumi.ViewModels;

namespace Lumi.Views
{
    public partial class AccountCard : UserControl
    {
        public AccountCard()
        {
            InitializeComponent();
        }

        private void OnNameDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2 && DataContext is AccountItemViewModel vm)
            {
                vm.IsEditing = true;
                EditBox.Focus();
                EditBox.SelectAll();
            }
        }

        private void OnEditLostFocus(object sender, RoutedEventArgs e)
        {
            if (DataContext is AccountItemViewModel vm && vm.IsEditing)
            {
                vm.CommitEditCommand.Execute(null);
            }
        }

        private void OnEditKeyDown(object sender, KeyEventArgs e)
        {
            if (DataContext is AccountItemViewModel vm)
            {
                if (e.Key == Key.Enter)
                {
                    vm.CommitEditCommand.Execute(null);
                    e.Handled = true;
                }
                else if (e.Key == Key.Escape)
                {
                    vm.IsEditing = false;
                    e.Handled = true;
                }
            }
        }
    }
}
