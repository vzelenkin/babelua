using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Babe.Lua.Package;

namespace Babe.Lua.DataModel
{
    class OuterCompletionData : LuaFile
    {
        static OuterCompletionData instance;
        public static OuterCompletionData Instance
        {
            get
            {
                if (instance == null) instance = new OuterCompletionData();
                return instance;
            }
        }

        private OuterCompletionData()
            : base(null, null)
        {
            var path = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), SettingConstants.SettingFolder, SettingConstants.CompletionFolder);
            if (!System.IO.Directory.Exists(path)) return;
            var files = System.IO.Directory.EnumerateFiles(path, "*.lua", System.IO.SearchOption.AllDirectories);
            var parser = new TreeParser();
            foreach(var file in files)
            {
                var Lf = parser.ParseFile(file);
                if (Lf != null)
                {
                    this.Members.AddRange(Lf.Members);
                }
            }
        }
    }
}
