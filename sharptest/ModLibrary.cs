using System;
using System.Collections.Generic;
using System.Text;

namespace sharptest
{
    class ModLibrary
    {
        private Dictionary<ModuleKey, List<signals.IBlockDriver> > blocks;
        private Dictionary<string, List<signals.IFunctionSpec> > funcs;

        private class ModuleKey : IEquatable<ModuleKey>
        {
            public string name;
            public int numIn;
            public int numOut;

            public ModuleKey(signals.IBlockDriver blk)
            {
                name = blk.Name;
                signals.Fingerprint fgr = blk.Fingerprint;
                if (fgr == null)
                {
                    numIn = numOut = -1;
                }
                else
                {
                    numIn = fgr.inputs.Length;
                    numOut = fgr.outputs.Length;
                }
            }

            public override int GetHashCode()
            {
                return (17 * numIn + numOut).GetHashCode() ^ name.GetHashCode();
            }
            public override bool Equals(object obj)
            {
                return Equals(obj as ModuleKey);
            }
            public bool Equals(ModuleKey obj)
            {
                return numIn == obj.numIn && numOut == obj.numOut && String.Equals(name, obj.name, StringComparison.CurrentCultureIgnoreCase);
            }
            public override string ToString()
            {
                return String.Format("{0} [{1}->{2}]", name, numIn < 0 ? "?" : numIn.ToString(), numOut < 0 ? "?" : numOut.ToString());
            }
        }

        public ModLibrary()
        {
            blocks = new Dictionary<ModuleKey, List<signals.IBlockDriver>>();
            funcs = new Dictionary<string, List<signals.IFunctionSpec>>();
        }

        public void add(signals.IBlockDriver block)
        {
            ModuleKey key = new ModuleKey(block);
            List<signals.IBlockDriver> list;
            if (!blocks.TryGetValue(key, out list))
            {
                list = new List<signals.IBlockDriver>();
                blocks.Add(key, list);
            }
            list.Add(block);
        }

        public void add(signals.IFunctionSpec func)
        {
            string key = func.Name;
            List<signals.IFunctionSpec> list;
            if (!funcs.TryGetValue(key, out list))
            {
                list = new List<signals.IFunctionSpec>();
                funcs.Add(key, list);
            }
            list.Add(func);
        }

        public List<List<signals.IBlockDriver>> block(string name)
        {
            List<List<signals.IBlockDriver>> results = new List<List<signals.IBlockDriver>>();
            foreach (ModuleKey key in blocks.Keys)
            {
                if (key.name.Equals(name))
                {
                    results.Add(blocks[key]);
                }
            }
            return results.Count > 0 ? results : null;
        }

        public List<signals.IFunctionSpec> func(string name)
        {
            List<signals.IFunctionSpec> results;
            return funcs.TryGetValue(name, out results) ? results : null;
        }
    }
}
