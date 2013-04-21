using System;
using System.Collections.Generic;

namespace sharptest
{
    public class Schematic : ICloneable
    {
        public enum ElementType
        {
            Module,
            Function,
            FunctionOnOut,
            FunctionOnIn
        }

        public class Element : ICloneable
        {
            public readonly string name;
            public string nodeId;
            public ElementType type;
            public bool explicitAvail;
            public List<signals.IConnectible> availObjects;

            public Element(signals.IBlock realBlock)
            {
                if (realBlock == null) throw new ArgumentNullException("realBlock");
                type = ElementType.Module;
                name = realBlock.Name;
                nodeId = realBlock.NodeId;
                availObjects = new List<signals.IConnectible>();
                availObjects.Add(realBlock);
                explicitAvail = true;
            }

            public Element(signals.IBlockDriver realDriver)
            {
                if (realDriver == null) throw new ArgumentNullException("realDriver");
                type = ElementType.Module;
                name = realDriver.Name;
                nodeId = null;
                availObjects = new List<signals.IConnectible>();
                availObjects.Add(realDriver);
                explicitAvail = true;
            }

            public Element(signals.IFunctionSpec realSpec)
            {
                if (realSpec == null) throw new ArgumentNullException("realSpec");
                type = ElementType.Function;
                name = realSpec.Name;
                nodeId = null;
                availObjects = new List<signals.IConnectible>();
                availObjects.Add(realSpec);
                explicitAvail = true;
            }

            public Element(signals.IFunction realFunc)
            {
                if (realFunc == null) throw new ArgumentNullException("realFunc");
                signals.IFunctionSpec spec = realFunc.Spec;
                type = ElementType.Function;
                name = spec.Name;
                nodeId = null;
                availObjects = new List<signals.IConnectible>();
                availObjects.Add(spec);
                explicitAvail = true;
            }

            public Element(ElementType type, string name)
            {
                if (name == null) throw new ArgumentNullException("name");
                this.type = type;
                this.name = name;
                this.nodeId = null;
                this.availObjects = null;
                this.explicitAvail = false;
            }

            public void populateAvail(ModLibrary library)
            {
                if (library == null) throw new ArgumentNullException("library");
                if (this.explicitAvail) return;
                availObjects = new List<signals.IConnectible>();

                switch (this.type)
                {
                    case ElementType.Module:
                        List<List<signals.IBlockDriver>> blocks = library.block(this.name);
                        if (blocks != null)
                        {
                            foreach (List<signals.IBlockDriver> variant in blocks)
                            {
                                foreach (signals.IBlockDriver driver in variant)
                                {
                                    if (driver.canCreate)
                                    {
                                        this.availObjects.Add(driver);
                                    }
                                    if (driver.canDiscover)
                                    {
                                        signals.IBlock[] found = driver.Discover();
                                        foreach(signals.IBlock discBlk in found)
                                        {
                                            if (this.nodeId == null || String.Compare(discBlk.NodeId, this.nodeId, true) == 0)
                                            {
                                                this.availObjects.Add(discBlk);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        break;

                    case ElementType.Function:
                    case ElementType.FunctionOnIn:
                    case ElementType.FunctionOnOut:
                        List<signals.IFunctionSpec> funcs = library.func(this.name);
                        if (funcs != null)
                        {
                            foreach (signals.IFunctionSpec spec in funcs)
                            {
                                this.availObjects.Add(spec);
                            }
                        }
                        break;
                }
            }

            object ICloneable.Clone()
            {
                return this.Clone();
            }

            public Element Clone()
            {
                Element newOb = new Element(this.type, this.name);
                newOb.nodeId = this.nodeId;
                newOb.explicitAvail = this.explicitAvail;
                if (this.availObjects != null)
                {
                    newOb.availObjects = new List<signals.IConnectible>();
                    foreach (signals.IConnectible ob in this.availObjects)
                    {
                        newOb.availObjects.Add(ob);
                    }
                }
                return newOb;
            }
        }

        public class UniqueElemKey : IEquatable<UniqueElemKey>
        {
            public readonly ElementType type;
            public readonly string name;
            public readonly string nodeId;

            public UniqueElemKey(ElementType type, string name, string nodeId)
            {
                if (name == null) throw new ArgumentNullException("name");
                this.type = type;
                this.name = name;
                this.nodeId = nodeId;
            }

            public UniqueElemKey(Element elem)
            {
                if (elem == null) throw new ArgumentNullException("elem");
                this.type = elem.type;
                this.name = elem.name;
                this.nodeId = elem.nodeId;
            }

            public override int GetHashCode()
            {
                return type.GetHashCode() ^ name.GetHashCode() ^ (nodeId == null ? 0 : nodeId.GetHashCode());
            }
            public override bool Equals(object obj)
            {
                return Equals(obj as UniqueElemKey);
            }
            public bool Equals(UniqueElemKey obj)
            {
                return type == obj.type && String.Equals(name, obj.name, StringComparison.CurrentCultureIgnoreCase)
                    && ((nodeId == null && obj.nodeId == null) || nodeId.Equals(obj.nodeId));
            }
            public override string ToString()
            {
                return String.Format("{0}[{1}]", name, nodeId);
            }
        }

        private class EndpointKey : IEquatable<EndpointKey>
        {
            public readonly Element elem;
            public readonly string epString;
            public readonly int epIdx;

            public EndpointKey(Element elem, string ep)
            {
                if (elem == null) throw new ArgumentNullException("elem");
                if (ep == null) throw new ArgumentNullException("ep");
                this.elem = elem;
                this.epString = ep;
                this.epIdx = -1;
            }

            public EndpointKey(Element elem, int ep)
            {
                if (elem == null) throw new ArgumentNullException("elem");
                if (ep < 0) throw new ArgumentException("Cannot be less than zero", "ep");
                this.elem = elem;
                this.epString = null;
                this.epIdx = ep;
            }

            public override int GetHashCode()
            {
                return elem.GetHashCode() ^ ((this.epString == null) ? epIdx.GetHashCode() : epString.GetHashCode());
            }
            public override bool Equals(object obj)
            {
                return Equals(obj as EndpointKey);
            }
            public bool Equals(EndpointKey obj)
            {
                if (this.epString == null)
                {
                    return elem.Equals(obj.elem) && obj.epString == null && obj.epIdx == epIdx;
                }
                else
                {
                    return elem.Equals(obj.elem) && obj.epString != null && obj.epIdx == -1
                        && String.Equals(epString, obj.epString, StringComparison.CurrentCultureIgnoreCase);
                }
            }
            public override string ToString()
            {
                return String.Format("{0}[{1}]", elem, epString == null ? (object)epIdx : epString);
            }

            public signals.EType InputType(signals.IConnectible conn)
            {
                return InputType(conn.Fingerprint);
            }

            public signals.EType InputType(signals.Fingerprint print)
            {
                if (this.epString == null)
                {
                    if (epIdx < print.inputs.Length)
                    {
                        return print.inputs[epIdx];
                    }
                }
                else
                {
                    if (print.inputNames != null)
                    {
                        for (int idx = 0; idx < print.inputs.Length; idx++)
                        {
                            if (print.inputNames[idx] != null && String.Compare(print.inputNames[idx], epString, true) == 0)
                            {
                                return print.inputs[idx];
                            }
                        }
                    }
                }
                throw new IndexOutOfRangeException();
            }

            public signals.IInEndpoint InputEP(signals.IBlock blk)
            {
                signals.Fingerprint print = blk.Fingerprint;
                if (this.epString == null)
                {
                    if (epIdx < print.inputs.Length)
                    {
                        return blk.Incoming[epIdx];
                    }
                }
                else
                {
                    if (print.inputNames != null)
                    {
                        for (int idx = 0; idx < print.inputs.Length; idx++)
                        {
                            if (print.inputNames[idx] != null && String.Compare(print.inputNames[idx], epString, true) == 0)
                            {
                                return blk.Incoming[idx];
                            }
                        }
                    }
                }
                throw new IndexOutOfRangeException();
            }

            public signals.EType OutputType(signals.IConnectible conn)
            {
                return OutputType(conn.Fingerprint);
            }

            public signals.EType OutputType(signals.Fingerprint print)
            {
                if (this.epString == null)
                {
                    if (epIdx < print.outputs.Length)
                    {
                        return print.outputs[epIdx];
                    }
                }
                else
                {
                    if (print.outputNames != null)
                    {
                        for (int idx = 0; idx < print.outputs.Length; idx++)
                        {
                            if (print.outputNames[idx] != null && String.Compare(print.outputNames[idx], epString, true) == 0)
                            {
                                return print.outputs[idx];
                            }
                        }
                    }
                }
                throw new IndexOutOfRangeException();
            }

            public signals.IOutEndpoint OutputEP(signals.IBlock blk)
            {
                signals.Fingerprint print = blk.Fingerprint;
                if (this.epString == null)
                {
                    if (epIdx < print.outputs.Length)
                    {
                        return blk.Outgoing[epIdx];
                    }
                }
                else
                {
                    if (print.inputNames != null)
                    {
                        for (int idx = 0; idx < print.outputs.Length; idx++)
                        {
                            if (print.outputNames[idx] != null && String.Compare(print.outputNames[idx], epString, true) == 0)
                            {
                                return blk.Outgoing[idx];
                            }
                        }
                    }
                }
                throw new IndexOutOfRangeException();
            }

        }

        private Dictionary<EndpointKey, EndpointKey> connections;
        private Dictionary<UniqueElemKey, Element> contents;
        private int genericID;

        public Schematic()
        {
            this.contents = new Dictionary<UniqueElemKey, Element>();
            this.connections = new Dictionary<EndpointKey, EndpointKey>();
            this.genericID = 0;
        }

        public void addGeneric(signals.IBlockDriver realDriver)
        {
            addGeneric(new Element(realDriver));
        }

        public void addGeneric(signals.IFunctionSpec realSpec)
        {
            addGeneric(new Element(realSpec));
        }

        public void addGeneric(signals.IFunction realFunc)
        {
            addGeneric(new Element(realFunc));
        }

        public void addGeneric(Element elem)
        {
            if (elem == null) throw new ArgumentNullException("elem");
            if (elem.nodeId != null) throw new ArgumentException("Element should not have a unique ID", "elem");
            elem.nodeId = String.Format("generic_{0}", ++this.genericID);
            add(elem);
        }

        public void add(Element elem)
        {
            if (elem == null) throw new ArgumentNullException("elem");

            UniqueElemKey key = new UniqueElemKey(elem);
            if(contents.ContainsKey(key))
            {
                Element storedElem = contents[key];
                if (storedElem != elem)
                {
                    throw new ArgumentException("A different element with this same unique ID is already in the schema", "elem");
                }
                return;
            }
            contents.Add(key, elem);
        }

        public void connect(Element from, string ep1, Element to, string ep2)
        {
            connect(new EndpointKey(from, ep1), new EndpointKey(to, ep2));
        }

        public void connect(Element from, int ep1, Element to, int ep2)
        {
            connect(new EndpointKey(from, ep1), new EndpointKey(to, ep2));
        }

        public void connect(Element from, int ep1, Element to, string ep2)
        {
            connect(new EndpointKey(from, ep1), new EndpointKey(to, ep2));
        }

        public void connect(Element from, string ep1, Element to, int ep2)
        {
            connect(new EndpointKey(from, ep1), new EndpointKey(to, ep2));
        }

        private void connect(EndpointKey fromEp, EndpointKey toEp)
        {
            if (connections.ContainsKey(fromEp))
            {
                throw new ArgumentException("This connection source is already in use", "fromEp");
            }
            if (connections.ContainsValue(toEp))
            {
                throw new ArgumentException("This connection sink is already in use", "toEp");
            }
            connections.Add(fromEp, toEp);
        }

        object ICloneable.Clone()
        {
            return this.Clone();
        }

        public Schematic Clone()
        {
            Dictionary<Element, Element> elmMap = new Dictionary<Element, Element>();
            Schematic newOb = new Schematic();
            foreach (Element elm in this.contents.Values)
            {
                Element newElm = elm.Clone();
                newOb.add(newElm);
                elmMap.Add(elm, newElm);
            }
            foreach (KeyValuePair<EndpointKey, EndpointKey> entry in this.connections)
            {
                EndpointKey key = entry.Key;
                EndpointKey value = entry.Value;
                EndpointKey newKey = key.epString == null ?
                    new EndpointKey(elmMap[key.elem], key.epIdx) : new EndpointKey(elmMap[key.elem], key.epString);
                EndpointKey newValue = value.epString == null ?
                    new EndpointKey(elmMap[value.elem], value.epIdx) : new EndpointKey(elmMap[value.elem], value.epString);
                newOb.connections.Add(newKey, newValue);
            }
            return newOb;
        }

        private bool isFullyConnected()
        {
            if (this.contents.Count == 0 || this.connections.Count == 0) return false;
            Dictionary<Element, bool> seen = new Dictionary<Element, bool>();
            Dictionary<UniqueElemKey, Element>.Enumerator firstEnum = this.contents.GetEnumerator();
            firstEnum.MoveNext();
            Element first = firstEnum.Current.Value;
            isFullyConnectedImpl(seen, first);
            foreach (Element elm in this.contents.Values)
            {
                if (!seen.ContainsKey(elm)) return false;
            }
            return true;
        }

        private void isFullyConnectedImpl(Dictionary<Element, bool> seen, Element elm)
        {
            if (seen.ContainsKey(elm)) return;
            seen.Add(elm, true);
            foreach (KeyValuePair<EndpointKey, EndpointKey> entry in this.connections)
            {
                EndpointKey key = entry.Key;
                EndpointKey value = entry.Value;
                if (key.elem == elm)
                {
                    isFullyConnectedImpl(seen, value.elem);
                }
                else if (value.elem == elm)
                {
                    isFullyConnectedImpl(seen, key.elem);
                }
            }
        }

        public List<Schematic> resolve(ModLibrary library)
        {
            if (library == null) throw new ArgumentNullException("library");
            if (this.contents.Count == 0 || this.connections.Count == 0) return null;
            if (!isFullyConnected()) throw new ApplicationException("schematic is not fully connected");

            foreach (Element elm in this.contents.Values)
            {
                elm.populateAvail(library);
            }

            Schematic all = this.Clone();
            Dictionary<UniqueElemKey, Element>.Enumerator firstEnum = all.contents.GetEnumerator();
            firstEnum.MoveNext();
            Element first = firstEnum.Current.Value;
            Dictionary<UniqueElemKey, bool> seen = new Dictionary<UniqueElemKey, bool>();
            return resolveImpl(library, all, seen, new UniqueElemKey(first));
        }

        private static List<Schematic> resolveImpl(ModLibrary library, Schematic here, Dictionary<UniqueElemKey, bool> seen, UniqueElemKey elmKey)
        {
            if (seen.ContainsKey(elmKey)) return new List<Schematic> { here };
            if(!here.contents.ContainsKey(elmKey)) throw new ApplicationException("couldn't kind elmKey in contents");
            Element elm = here.contents[elmKey];
            seen.Add(elmKey, true);
            try
            {
                List<Schematic> answers = new List<Schematic>();
                foreach (signals.IConnectible avail in elm.availObjects)
                {
                    Schematic newSchem = here.Clone();
                    Element newElem = newSchem.contents[elmKey];
                    newElem.availObjects.Clear();
                    newElem.availObjects.Add(avail);

                    if (!newSchem.resolveNeighbors(library, newElem)) continue;

                    Dictionary<UniqueElemKey, bool> recurseList = new Dictionary<UniqueElemKey, bool>();
                    foreach (KeyValuePair<EndpointKey, EndpointKey> entry in newSchem.connections)
                    {
                        EndpointKey key = entry.Key;
                        EndpointKey value = entry.Value;
                        if (key.elem == newElem)
                        {
                            UniqueElemKey newKey = new UniqueElemKey(value.elem);
                            if (!seen.ContainsKey(newKey)) recurseList.Add(newKey, true);
                        }
                        else if (value.elem == newElem)
                        {
                            UniqueElemKey newKey = new UniqueElemKey(key.elem);
                            if (!seen.ContainsKey(newKey)) recurseList.Add(newKey, true);
                        }
                    }
                    List<Schematic> possibles = new List<Schematic>();
                    possibles.Add(newSchem);
                    foreach (KeyValuePair<UniqueElemKey, bool> entry in recurseList)
                    {
                        List<Schematic> prevPass = possibles;
                        possibles = new List<Schematic>();
                        foreach (Schematic possible in prevPass)
                        {
                            possibles.AddRange(resolveImpl(library, possible, seen, entry.Key));
                        }
                    }
                    answers.AddRange(possibles);
                }
                return answers;
            }
            finally
            {
                seen.Remove(elmKey);
            }
        }

        private bool resolveNeighbors(ModLibrary library, Element elm)
        {
            if (elm.availObjects.Count != 1) throw new ApplicationException("elm should contain a single availObject by this point");
            signals.IConnectible avail = elm.availObjects[0];
            Dictionary<Element, bool> recurseList = new Dictionary<Element, bool>();
            List<KeyValuePair<EndpointKey, EndpointKey>> tempCollect = new List<KeyValuePair<EndpointKey,EndpointKey>>();
            tempCollect.AddRange(connections);
            foreach (KeyValuePair<EndpointKey, EndpointKey> entry in tempCollect)
            {
                EndpointKey key = entry.Key;
                EndpointKey value = entry.Value;
                if (key.elem == elm)
                {
                    Element otherElm = value.elem;
                    signals.EType ourType = key.OutputType(avail);
                    bool changed = false;
                    if (otherElm.availObjects.Count > 1)
                    {
                        // if we have more than one option here, just look for possible compatibility
                        List<signals.IConnectible> newAvail = new List<signals.IConnectible>();
                        foreach (signals.IConnectible otherAvail in otherElm.availObjects)
                        {
                            signals.EType otherType = value.InputType(otherAvail);
                            if (ourType != otherType && findImplicitConversion(library, ourType, otherType) == null)
                            {
                                changed = true;
                            }
                            else
                            {
                                newAvail.Add(otherAvail);
                            }
                        }
                        otherElm.availObjects = newAvail;
                    }
                    if (otherElm.availObjects.Count == 1)
                    {
                        // if we're on one-to-one terms, we might add compat connections
                        signals.IConnectible otherAvail = otherElm.availObjects[0];
                        signals.EType otherType = value.InputType(otherAvail);
                        if (ourType != otherType)
                        {
                            signals.IFunctionSpec func = findImplicitConversion(library, ourType, otherType);
                            if (func == null)
                            {
                                // only option isn't type-compatible? Looks like we broke a connection
                                return false;
                            }
                            else
                            {
                                AddImplicitConversion(key, value, func);
                            }
                        }
                    }
                    else if (otherElm.availObjects.Count == 0)
                    {
                        // ran out of possible connections? Looks like we broke a connection
                        return false;
                    }
                    if(changed) recurseList.Add(value.elem, true);
                }
                if (value.elem == elm)
                {
                    Element otherElm = key.elem;
                    signals.EType ourType = value.InputType(avail);
                    bool changed = false;
                    if (otherElm.availObjects.Count > 1)
                    {
                        // if we have more than one option here, just look for possible compatibility
                        List<signals.IConnectible> newAvail = new List<signals.IConnectible>();
                        foreach (signals.IConnectible otherAvail in otherElm.availObjects)
                        {
                            signals.EType otherType = key.OutputType(otherAvail);
                            if (ourType != otherType && findImplicitConversion(library, otherType, ourType) == null)
                            {
                                changed = true;
                            }
                            else
                            {
                                newAvail.Add(otherAvail);
                            }
                        }
                        otherElm.availObjects = newAvail;
                    }
                    if (otherElm.availObjects.Count == 1)
                    {
                        // if we're on one-to-one terms, we might add compat connections
                        signals.IConnectible otherAvail = otherElm.availObjects[0];
                        signals.EType otherType = key.OutputType(otherAvail);
                        if (ourType != otherType)
                        {
                            signals.IFunctionSpec func = findImplicitConversion(library, otherType, ourType);
                            if (func == null)
                            {
                                // only option isn't type-compatible? Looks like we broke a connection
                                return false;
                            }
                            else
                            {
                                AddImplicitConversion(key, value, func);
                            }
                        }
                    }
                    else if (otherElm.availObjects.Count == 0)
                    {
                        // ran out of possible connections? Looks like we broke a connection
                        return false;
                    }
                    if (changed) recurseList.Add(value.elem, true);
                }
            }
            foreach (KeyValuePair<Element, bool> entry in recurseList)
            {
                if (!resolveNeighbors(library, entry.Key)) return false;
            }
            return true;
        }

        private static signals.IFunctionSpec findImplicitConversion(ModLibrary library, signals.EType inpType, signals.EType outType)
        {
            List<signals.IFunctionSpec> idents = library.func("=");
            if(idents == null) return null;
            foreach(signals.IFunctionSpec avail in idents)
            {
                signals.EType identInput = avail.Fingerprint.inputs[0];
                signals.EType identOutput = avail.Fingerprint.outputs[0];
                if (inpType == identInput && outType == identOutput)
                {
                    return avail;
                }
            }
            return null;
        }

        private bool AddImplicitConversion(EndpointKey fromEP, EndpointKey toEP, signals.IFunctionSpec convFunc)
        {
            Element newElm = new Element(convFunc);
            addGeneric(newElm);
            this.connections[fromEP] = new EndpointKey(newElm, 0);
            this.connections[new EndpointKey(newElm, 0)] = toEP;
            return true;
        }

        public Dictionary<UniqueElemKey, object> construct()
        {
            Dictionary<UniqueElemKey, object> result = new Dictionary<UniqueElemKey, object>();
            Dictionary<Element, object> elmResult = new Dictionary<Element, object>();
            foreach (KeyValuePair<UniqueElemKey, Element> here in contents)
            {
                Element elm = here.Value;
                if (elm.availObjects.Count != 1) throw new ApplicationException("expecting all elements to have exactly one solution");
                signals.IConnectible avail = elm.availObjects[0];
                object newOb = null;
                switch (elm.type)
                {
                    case ElementType.Module:
                        {
                            signals.IBlockDriver drv = avail as signals.IBlockDriver;
                            if(drv != null)
                            {
                                newOb = drv.Create();
                            } else {
                                newOb = (signals.IBlock)avail;
                            }
                            break;
                        }
                    case ElementType.Function:
                    case ElementType.FunctionOnIn:
                    case ElementType.FunctionOnOut:
                        newOb = ((signals.IFunctionSpec)avail).Create();
                        break;
                }
                result.Add(here.Key, newOb);
                elmResult.Add(elm, newOb);
            }

            foreach(KeyValuePair<EndpointKey,EndpointKey> conn in connections)
            {
                EndpointKey from = conn.Key;
                EndpointKey to = conn.Value;
                object fromObj = elmResult[from.elem];
                object toObj = elmResult[to.elem];
                switch (from.elem.type)
                {
                    case ElementType.Module:
                        {
                            signals.IOutEndpoint outEP = from.OutputEP((signals.IBlock)fromObj);
                            switch (to.elem.type)
                            {
                                case ElementType.Module:
                                    {
                                        signals.IEPBuffer buff = outEP.CreateBuffer();
                                        signals.IInEndpoint inEP = to.InputEP((signals.IBlock)toObj);
                                        outEP.Connect(buff);
                                        inEP.Connect(buff);
                                        break;
                                    }
                                case ElementType.Function:
                                case ElementType.FunctionOnIn:
                                    {
                                        signals.IEPBuffer buff = outEP.CreateBuffer();
                                        signals.IInEndpoint inEP = ((signals.IFunction)toObj).Input;
                                        outEP.Connect(buff);
                                        inEP.Connect(buff);
                                        break;
                                    }
                                case ElementType.FunctionOnOut:
                                    {
                                        signals.IEPSendTo inEP = ((signals.IFunction)toObj).Output;
                                        outEP.Connect(inEP);
                                        break;
                                    }
                            }
                            break;
                        }
                    case ElementType.Function:
                    case ElementType.FunctionOnIn:
                        {
                            signals.IInputFunction func = ((signals.IFunction)fromObj).Input;
                            switch (to.elem.type)
                            {
                                case ElementType.Module:
                                    {
                                        signals.IInEndpoint inEP = to.InputEP((signals.IBlock)toObj);
                                        inEP.Connect(func);
                                        break;
                                    }
                                case ElementType.Function:
                                case ElementType.FunctionOnIn:
                                    {
                                        signals.IInEndpoint inEP = ((signals.IFunction)toObj).Input;
                                        inEP.Connect(func);
                                        break;
                                    }
                                case ElementType.FunctionOnOut:
                                    throw new ApplicationException("incompatible connection types");
                            }
                            break;
                        }
                    case ElementType.FunctionOnOut:
                        {
                            signals.IOutputFunction func = ((signals.IFunction)fromObj).Output;
                            switch (to.elem.type)
                            {
                                case ElementType.Module:
                                    {
                                        signals.IEPBuffer buff = func.CreateBuffer();
                                        signals.IInEndpoint inEP = to.InputEP((signals.IBlock)toObj);
                                        func.Connect(buff);
                                        inEP.Connect(buff);
                                        break;
                                    }
                                case ElementType.Function:
                                case ElementType.FunctionOnIn:
                                    {
                                        signals.IEPBuffer buff = func.CreateBuffer();
                                        signals.IInEndpoint inEP = ((signals.IFunction)toObj).Input;
                                        func.Connect(buff);
                                        inEP.Connect(buff);
                                        break;
                                    }
                                case ElementType.FunctionOnOut:
                                    {
                                        signals.IOutEndpoint outEP = ((signals.IFunction)toObj).Output;
                                        outEP.Connect(func);
                                        break;
                                    }
                            }
                            break;
                        }
                }
            }
            return result;
        }
    }
}
