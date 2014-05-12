using Babe.Lua.DataModel;
using Microsoft.VisualStudio.Text.Editor;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Babe.Lua.Package;

namespace Babe.Lua.Editor
{
    /// <summary>
    /// OutlineMarginControl.xaml 的交互逻辑
	/// 目前OutlineMargin还承担了显示编码、监测搜索触发等功能，后续考虑分离出去
    /// </summary>
    public partial class OutlineMarginControl : UserControl
    {
        IWpfTextViewHost TextViewHost;

        public OutlineMarginControl(IWpfTextViewHost TextViewHost)
        {
            InitializeComponent();
            
            this.TextViewHost = TextViewHost;

            this.Loaded += OutlineMarginControl_Loaded;
            this.Unloaded += OutlineMarginControl_Unloaded;
            this.KeyDown += OutlineMarginControl_KeyDown;
        }

        void OutlineMarginControl_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                TextViewHost.TextView.VisualElement.Focus();
            }
        }

        public void Refresh()
        {
            var File = FileManager.Instance.CurrentFile;

            if (File == null)
            {
                ComboBox_Table.ItemsSource = null;
                ComboBox_Member.ItemsSource = null;
            }
            else if (ComboBox_Table.ItemsSource != File.Members)
            {
                ComboBox_Table.ItemsSource = File.Members;

                var list = new List<LuaMember>();
                foreach (var member in File.Members)
                {
                    if (member is LuaTable)
                    {
                        list.AddRange((member as LuaTable).Members);
                    }
                    else
                    {
                        list.Add(member);
                    }
                }
                ComboBox_Member.ItemsSource = list;
            }
        }

        void OutlineMarginControl_Unloaded(object sender, RoutedEventArgs e)
        {
            TextViewHost.HostControl.MouseDoubleClick -= HostControl_MouseDoubleClick;
        }

        void OutlineMarginControl_Loaded(object sender, RoutedEventArgs e)
        {
			TextViewHost.HostControl.MouseDoubleClick += HostControl_MouseDoubleClick;

            //Refresh();
        }

        private void HostControl_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (BabePackage.Setting.IsFirstInstall)
            {
                BabePackage.Setting.IsFirstInstall = false;
                MessageBox.Show("press <Ctrl> key to search words in current file\r\npress <Alt> key to search in all files", "Tips");
                //DTEHelper.Current.FindSelectTokenRef(false);
                //DTEHelper.Current.OpenDocument(DTEHelper.Current.DTE.ActiveDocument.FullName);
            }
            else if (Keyboard.IsKeyDown(Key.LeftCtrl) || Keyboard.IsKeyDown(Key.RightCtrl))
            {
				EditorManager.FindSelectTokenReferences(false);
                //DTEHelper.Current.OpenDocument(DTEHelper.Current.DTE.ActiveDocument.FullName);
            }
            else if (Keyboard.IsKeyDown(Key.LeftAlt) || Keyboard.IsKeyDown(Key.RightAlt))
            {
				EditorManager.FindSelectTokenReferences(true);
                //DTEHelper.Current.OpenDocument(DTEHelper.Current.DTE.ActiveDocument.FullName);
            }
        }

        private void Combo_Table_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ComboBox_Table.SelectedItem != null)
            {
                var tb = ComboBox_Table.SelectedItem as LuaMember;

                if(tb is LuaTable) ComboBox_Member.ItemsSource = (tb as LuaTable).Members;

				EditorManager.GoTo(null, tb.Line);
            }
        }

        private void Combo_Member_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ComboBox_Member.SelectedItem != null)
            {
                var member = ComboBox_Member.SelectedItem as LuaMember;

				EditorManager.GoTo(null, member.Line);
            }
        }
    }
}
