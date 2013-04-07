using System;
using System.Collections.Generic;
using System.Text;

namespace sharptest
{
    public class WeakValuedDictionary<TKey, TValue> : IDictionary<TKey, TValue>
    {
        // adapted from http://stackoverflow.com/questions/2784291/good-implementation-of-weak-dictionary-in-net
        // and http://blogs.msdn.com/b/nicholg/archive/2006/06/04/616787.aspx
        private IDictionary<TKey, WeakReference> _innerDictionary = new Dictionary<TKey, WeakReference>();
        private ICollection<TKey> keys = null;
        private ICollection<TValue> values = null;

        public TValue this[TKey key]
        {
            get
            {
                var reference = _innerDictionary[key];
                if (reference.IsAlive)
                    return (TValue)reference.Target;
                _innerDictionary.Remove(key);
                throw new KeyNotFoundException("Key found but was expired.");
            }
            set
            {
                _innerDictionary[key] = new WeakReference(value);
            }
        }

        public bool IsReadOnly
        {
            get
            {
                return false;
            }
        }

        public void Clear()
        {
            _innerDictionary.Clear();
        }

        public void Add(TKey key, TValue value)
        {
            _innerDictionary.Add(key, new WeakReference(value));
        }

        public void Add(KeyValuePair<TKey, TValue> pair)
        {
            _innerDictionary.Add(new KeyValuePair<TKey, WeakReference>(pair.Key, new WeakReference(pair.Value)));
        }

        public bool Remove(TKey key)
        {
            return _innerDictionary.Remove(key);
        }

        public bool Remove(KeyValuePair<TKey, TValue> pair)
        {
            return _innerDictionary.Remove(pair.Key);
        }

        public bool TryGetValue(TKey key, out TValue value)
        {
            WeakReference query;
            if (!_innerDictionary.TryGetValue(key, out query) || !query.IsAlive)
            {
                value = default(TValue);
                return false;
            }
            else
            {
                value = (TValue)query.Target;
                return true;
            }
        }

        public bool ContainsKey(TKey key)
        {
            WeakReference query;
            if (!_innerDictionary.TryGetValue(key, out query)) return false;
            if (query.IsAlive)
            {
                return true;
            }
            else
            {
                _innerDictionary.Remove(key);
                return false;
            }
        }

        public bool Contains(KeyValuePair<TKey, TValue> pair)
        {
            WeakReference query;
            if (!_innerDictionary.TryGetValue(pair.Key, out query)) return false;
            return query.IsAlive && EqualityComparer<TValue>.Default.Equals((TValue)query.Target, pair.Value);
        }

        public void CopyTo(KeyValuePair<TKey, TValue>[] dest, int outIdx)
        {
            int cnt = _innerDictionary.Count;
            KeyValuePair<TKey, WeakReference>[] innerDest = new KeyValuePair<TKey, WeakReference>[cnt];
            _innerDictionary.CopyTo(innerDest, 0);
            for (int inIdx = 0; inIdx < cnt; inIdx++)
            {
                KeyValuePair<TKey, WeakReference> thisPair = innerDest[inIdx];
                if (thisPair.Value.IsAlive)
                {
                    dest[outIdx++] = new KeyValuePair<TKey, TValue>(thisPair.Key, (TValue)thisPair.Value.Target);
                }
            }
        }

        public IEnumerator<KeyValuePair<TKey, TValue>> GetEnumerator()
        {
            foreach (KeyValuePair<TKey, WeakReference> pair in _innerDictionary)
            {
                if (pair.Value.IsAlive)
                {
                    yield return new KeyValuePair<TKey, TValue>(pair.Key, (TValue)pair.Value.Target);
                }
            }
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }

        public int Count
        {
            get
            {
                int cnt = 0;
                foreach (KeyValuePair<TKey, WeakReference> pair in _innerDictionary)
                {
                    if (pair.Value.IsAlive) cnt++;
                }
                return cnt;
            }
        }

        private abstract class Collection<T> : ICollection<T>
        {
            protected WeakValuedDictionary<TKey, TValue> _parent;
            protected readonly IDictionary<TKey, WeakReference> _innerDictionary;

            protected Collection(WeakValuedDictionary<TKey, TValue> parent, IDictionary<TKey, WeakReference> innerDictionary)
            {
                _parent = parent;
                _innerDictionary = innerDictionary;
            }

            public int Count
            {
                get { return _parent.Count; }
            }

            public bool IsReadOnly
            {
                get { return true; }
            }

            public void CopyTo(T[] array, int arrayIndex)
            {
                Copy(this, array, arrayIndex);
            }

            public bool Remove(T item)
            {
                throw new NotSupportedException("Collection is read-only.");
            }

            public void Add(T item)
            {
                throw new NotSupportedException("Collection is read-only.");
            }

            public void Clear()
            {
                throw new NotSupportedException("Collection is read-only.");
            }

            System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
            {
                return this.GetEnumerator();
            }

            public abstract bool Contains(T key);
            public abstract IEnumerator<T> GetEnumerator();
        }

        private class KeyCollection : Collection<TKey>
        {
            public KeyCollection(WeakValuedDictionary<TKey, TValue> parent, IDictionary<TKey, WeakReference> innerDictionary)
                : base(parent, innerDictionary)
            {
            }

            public override IEnumerator<TKey> GetEnumerator()
            {
                foreach (KeyValuePair<TKey, WeakReference> pair in _innerDictionary)
                {
                    if (pair.Value.IsAlive)
                    {
                        yield return pair.Key;
                    }
                }
            }

            public override bool Contains(TKey key)
            {
                WeakReference query;
                if (!_innerDictionary.TryGetValue(key, out query)) return false;
                return query.IsAlive;
            }
        }

        private class ValueCollection : Collection<TValue>
        {
            public ValueCollection(WeakValuedDictionary<TKey, TValue> parent, IDictionary<TKey, WeakReference> innerDictionary)
                : base(parent, innerDictionary)
            {
            }

            public override IEnumerator<TValue> GetEnumerator()
            {
                foreach (KeyValuePair<TKey, WeakReference> pair in _innerDictionary)
                {
                    if (pair.Value.IsAlive)
                    {
                        yield return (TValue)pair.Value.Target;
                    }
                }
            }

            public override bool Contains(TValue value)
            {
                foreach (KeyValuePair<TKey, WeakReference> pair in _innerDictionary)
                {
                    if (pair.Value.IsAlive && EqualityComparer<TValue>.Default.Equals((TValue)pair.Value.Target, value))
                    {
                        return true;
                    }
                }
                return false;
            }
        }

        private static void Copy<T>(ICollection<T> source, T[] array, int arrayIndex)
        {
            if (array == null)
            {
                throw new ArgumentNullException("array");
            }

            if (arrayIndex < 0 || arrayIndex > array.Length)
            {
                throw new ArgumentOutOfRangeException("arrayIndex");
            }

            if ((array.Length - arrayIndex) < source.Count)
            {
                throw new ArgumentException("Destination array is not large enough. Check array.Length and arrayIndex.");
            }

            foreach (T item in source)
            {
                array[arrayIndex++] = item;
            }
        }

        public ICollection<TKey> Keys
        {
            get
            {
                if (this.keys == null)
                {
                    this.keys = new KeyCollection(this, _innerDictionary);
                }
                return this.keys;
            }
        }

        public ICollection<TValue> Values
        {
            get
            {
                if (this.values == null)
                {
                    this.values = new ValueCollection(this, _innerDictionary);
                }
                return this.values;
            }
        }
    }
}
