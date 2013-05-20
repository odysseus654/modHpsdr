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

        public static string TypeName(signals.EType typ)
        {
            switch (typ)
            {
                case signals.EType.None:
                    return "none";
                case signals.EType.Event:
                    return "event";
                case signals.EType.String:
                    return "string";
                case signals.EType.WinHdl:
                    return "HWND";
                case signals.EType.Boolean:
                    return "bool";
                case signals.EType.Byte:
                    return "byte";
                case signals.EType.Short:
                    return "short";
                case signals.EType.Long:
                    return "int";
                case signals.EType.Int64:
                    return "long";
                case signals.EType.Single:
                    return "float";
                case signals.EType.Double:
                    return "double";
                case signals.EType.Complex:
                    return "complex(float)";
                case signals.EType.CmplDbl:
                    return "complex(double)";
                case signals.EType.LRSingle:
                    return "left-right(float)";
                case signals.EType.VecBoolean:
                    return "array(bool)";
                case signals.EType.VecByte:
                    return "array(byte)";
                case signals.EType.VecShort:
                    return "array(short)";
                case signals.EType.VecLong:
                    return "array(int)";
                case signals.EType.VecInt64:
                    return "array(long)";
                case signals.EType.VecSingle:
                    return "array(float)";
                case signals.EType.VecDouble:
                    return "array(double)";
                case signals.EType.VecComplex:
                    return "array(complex(float))";
                case signals.EType.VecCmplDbl:
                    return "array(complex(double))";
                case signals.EType.VecLRSingle:
                    return "array(left-right(float))";
                default:
                    return String.Format("unknown({0})", typ);
            }
        }

        public static string ElementTypeName(ElementType typ)
        {
            switch (typ)
            {
                case ElementType.Module:
                    return "module";
                case ElementType.Function:
                    return "function";
                case ElementType.FunctionOnOut:
                    return "function(out)";
                case ElementType.FunctionOnIn:
                    return "function(in)";
                default:
                    return String.Format("unknown({0})", typ);
            }
        }

        public class Element : ICloneable
        {
            public readonly string name;
            public string nodeId;
            public int circuitId;
            public ElementType type;
            public bool explicitAvail;
            public List<signals.ICircuitConnectible> availObjects;

            public Element(signals.IBlock realBlock)
            {
                if (realBlock == null) throw new ArgumentNullException("realBlock");
                type = ElementType.Module;
                name = realBlock.Name;
                nodeId = realBlock.NodeId;
                circuitId = 0;
                availObjects = new List<signals.ICircuitConnectible>();
                availObjects.Add(realBlock);
                explicitAvail = true;
            }

            public Element(signals.IBlockDriver realDriver)
            {
                if (realDriver == null) throw new ArgumentNullException("realDriver");
                type = ElementType.Module;
                name = realDriver.Name;
                nodeId = null;
                circuitId = 0;
                availObjects = new List<signals.ICircuitConnectible>();
                availObjects.Add(realDriver);
                explicitAvail = true;
            }

            public Element(signals.IFunctionSpec realSpec)
            {
                if (realSpec == null) throw new ArgumentNullException("realSpec");
                type = ElementType.Function;
                name = realSpec.Name;
                nodeId = null;
                circuitId = 0;
                availObjects = new List<signals.ICircuitConnectible>();
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
                circuitId = 0;
                availObjects = new List<signals.ICircuitConnectible>();
                availObjects.Add(spec);
                explicitAvail = true;
            }

            public Element(ElementType type, string name)
            {
                if (name == null) throw new ArgumentNullException("name");
                this.type = type;
                this.name = name;
                this.nodeId = null;
                this.circuitId = 0;
                this.availObjects = null;
                this.explicitAvail = false;
            }

            public override string ToString()
            {
                string result = String.Format("{0} {1}", ElementTypeName(this.type), this.name);
                if (this.nodeId != null) result += String.Format("[{0}]", this.nodeId);
                return result;
            }

            public void populateAvail(ModLibrary library)
            {
                if (library == null) throw new ArgumentNullException("library");
                if (this.explicitAvail) return;
                availObjects = new List<signals.ICircuitConnectible>();

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
                newOb.circuitId = this.circuitId;
                newOb.explicitAvail = this.explicitAvail;
                if (this.availObjects != null)
                {
                    newOb.availObjects = new List<signals.ICircuitConnectible>();
                    foreach (signals.ICircuitConnectible ob in this.availObjects)
                    {
                        newOb.availObjects.Add(ob);
                    }
                }
                return newOb;
            }
        }

        public class ElemKey : IEquatable<ElemKey>
        {
            public readonly ElementType type;
            public readonly string name;
            public readonly string nodeId;

            public ElemKey(ElementType type, string name, string nodeId)
            {
                if (name == null) throw new ArgumentNullException("name");
                this.type = type;
                this.name = name;
                this.nodeId = nodeId;
            }

            public ElemKey(ElementType type, string name)
            {
                if (name == null) throw new ArgumentNullException("name");
                this.type = type;
                this.name = name;
                this.nodeId = null;
            }

            public ElemKey(Element elem)
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
                return Equals(obj as ElemKey);
            }
            public bool Equals(ElemKey obj)
            {
                return type == obj.type && String.Equals(name, obj.name, StringComparison.CurrentCultureIgnoreCase)
                    && ((nodeId == null && obj.nodeId == null) || nodeId.Equals(obj.nodeId));
            }
            public override string ToString()
            {
                string result = String.Format("{0} {1}", ElementTypeName(this.type), this.name);
                if (this.nodeId != null) result += String.Format("[{0}]", this.nodeId);
                return result;
            }
        }
        
        public class EndpointKey : IEquatable<EndpointKey>
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

            public signals.EType InputType(signals.ICircuitConnectible conn)
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

            public signals.EType OutputType(signals.ICircuitConnectible conn)
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

        public abstract class ResolveFailureReason : ApplicationException, IEquatable<ResolveFailureReason>
        {
            public override bool Equals(object obj)
            {
                return Equals(obj as ResolveFailureReason);
            }

            public abstract bool Equals(ResolveFailureReason obj);
            public abstract override int GetHashCode();
        }

        public class LinkTypeFailure : ResolveFailureReason
        {
            public readonly EndpointKey fromEP;
            public readonly signals.EType fromType;
            public readonly EndpointKey toEP;
            public readonly signals.EType toType;

            public LinkTypeFailure(EndpointKey fromEP, signals.EType fromType, EndpointKey toEP, signals.EType toType)
            {
                this.fromEP = fromEP;
                this.fromType = fromType;
                this.toEP = toEP;
                this.toType = toType;
                this.HResult = unchecked((int)0x80020005);
            }

            public override bool Equals(ResolveFailureReason obj)
            {
                LinkTypeFailure other = obj as LinkTypeFailure;
                if (other == null) return false;
                return this.fromEP.Equals(other.fromEP) && this.fromType == other.fromType &&
                    this.toEP.Equals(other.toEP) && this.toType == other.toType;
            }

            public override int GetHashCode()
            {
                return this.fromEP.GetHashCode() ^ this.toEP.GetHashCode() ^ (int)this.fromType ^ (int)this.toType;
            }

            public override string Message
            {
                get
                {
                    return String.Format("Cannot connect {0} (type {1}) to {2} (type {3})",
                        fromEP.ToString(), TypeName(fromType), toEP.ToString(), TypeName(toType));
                }
            }
        }

        public class CannotResolveElement : ResolveFailureReason
        {
            public readonly Element elm;

            public CannotResolveElement(Element elm)
            {
                this.elm = elm;
                this.HResult = unchecked((int)0x80070490);
            }

            public override bool Equals(ResolveFailureReason obj)
            {
                CannotResolveElement other = obj as CannotResolveElement;
                if (other == null) return false;
                return this.elm.Equals(other.elm);
            }

            public override int GetHashCode()
            {
                return this.elm.GetHashCode();
            }

            public override string Message
            {
                get
                {
                    return String.Format("No implementations for {0} are available", elm.ToString());
                }
            }
        }

        public class ResolveFailure : ApplicationException
        {
            public Dictionary<ResolveFailureReason,bool> reasons;

            public ResolveFailure()
            {
                reasons = new Dictionary<ResolveFailureReason, bool>();
            }

            public void Add(ResolveFailureReason rsn)
            {
                if(!reasons.ContainsKey(rsn)) reasons.Add(rsn, true);
            }

            public void Add(ResolveFailure fail)
            {
                foreach(KeyValuePair<ResolveFailureReason,bool> entry in fail.reasons)
                {
                    Add(entry.Key);
                }
            }

            public override string Message
            {
                get
                {
                    string msgs = null;
                    foreach (KeyValuePair<ResolveFailureReason, bool> entry in this.reasons)
                    {
                        if (msgs != null)
                        {
                            msgs += ", ";
                        }
                        else
                        {
                            msgs = "";
                        }
                        msgs += entry.Key.Message;
                    }
                    return String.Format("Cannot resolve circuit: {0}", msgs);
                }
            }
        }

        private Dictionary<EndpointKey, EndpointKey> connections;
        private Dictionary<int, Element> contents;
        private Dictionary<ElemKey, Element> uniqueElem;
        private int genericID;

        public Schematic()
        {
            this.contents = new Dictionary<int, Element>();
            this.uniqueElem = new Dictionary<ElemKey, Element>();
            this.connections = new Dictionary<EndpointKey, EndpointKey>();
            this.genericID = 0;
        }

        public void add(signals.IBlockDriver realDriver)
        {
            add(new Element(realDriver));
        }

        public void add(signals.IFunctionSpec realSpec)
        {
            add(new Element(realSpec));
        }

        public void add(signals.IFunction realFunc)
        {
            add(new Element(realFunc));
        }

        public void add(Element elem)
        {
            if (elem == null) throw new ArgumentNullException("elem");

            if (elem.circuitId == 0)
            {
                elem.circuitId = ++this.genericID;
            }
            else if (contents.ContainsKey(elem.circuitId))
            {
                throw new ArgumentException("An element with this circuit ID has already been added", "elem");
            }

            if (elem.nodeId != null)
            {
                ElemKey key = new ElemKey(elem);
                if (uniqueElem.ContainsKey(key))
                {
                    throw new ArgumentException("A different element with this same unique ID is already in the schema", "elem");
                }
                uniqueElem.Add(key, elem);
            }

            contents.Add(elem.circuitId, elem);
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
            newOb.genericID = this.genericID;
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
            Dictionary<int, Element>.Enumerator firstEnum = this.contents.GetEnumerator();
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
            Dictionary<int, Element>.Enumerator firstEnum = all.contents.GetEnumerator();
            firstEnum.MoveNext();
            Element first = firstEnum.Current.Value;
            Dictionary<int, bool> seen = new Dictionary<int, bool>();
            return resolveImpl(library, all, seen, first.circuitId);
        }

        private static List<Schematic> resolveImpl(ModLibrary library, Schematic here, Dictionary<int, bool> seen, int elmKey)
        {
            if (seen.ContainsKey(elmKey)) return new List<Schematic> { here };
            if(!here.contents.ContainsKey(elmKey)) throw new ApplicationException("couldn't kind elmKey in contents");
            Element elm = here.contents[elmKey];
            seen.Add(elmKey, true);
            try
            {
                List<Schematic> answers = new List<Schematic>();
                ResolveFailure failure = new ResolveFailure();
                foreach (signals.ICircuitConnectible avail in elm.availObjects)
                {
                    Schematic newSchem = here.Clone();
                    Element newElem = newSchem.contents[elmKey];
                    newElem.availObjects.Clear();
                    newElem.availObjects.Add(avail);

                    try
                    {
                        newSchem.resolveNeighbors(library, newElem);
                    }
                    catch (ResolveFailureReason fault)
                    {
                        failure.Add(fault);
                        continue;
                    }

                    Dictionary<int, bool> recurseList = new Dictionary<int, bool>();
                    foreach (KeyValuePair<EndpointKey, EndpointKey> entry in newSchem.connections)
                    {
                        EndpointKey key = entry.Key;
                        EndpointKey value = entry.Value;
                        if (key.elem == newElem)
                        {
                            if (!seen.ContainsKey(value.elem.circuitId)) recurseList.Add(value.elem.circuitId, true);
                        }
                        else if (value.elem == newElem)
                        {
                            if (!seen.ContainsKey(key.elem.circuitId)) recurseList.Add(key.elem.circuitId, true);
                        }
                    }
                    List<Schematic> possibles = new List<Schematic>();
                    possibles.Add(newSchem);
                    foreach (KeyValuePair<int, bool> entry in recurseList)
                    {
                        List<Schematic> prevPass = possibles;
                        possibles = new List<Schematic>();
                        foreach (Schematic possible in prevPass)
                        {
                            try
                            {
                                possibles.AddRange(resolveImpl(library, possible, seen, entry.Key));
                            }
                            catch (ResolveFailure fail)
                            {
                                failure.Add(fail);
                            }
                        }
                    }
                    answers.AddRange(possibles);
                }
                if (answers.Count == 0 && failure.reasons.Count > 0)
                {
                    throw failure;
                }
                return answers;
            }
            finally
            {
                seen.Remove(elmKey);
            }
        }

        private void resolveNeighbors(ModLibrary library, Element elm)
        {
            if (elm.availObjects.Count != 1) throw new ApplicationException("elm should contain a single availObject by this point");
            signals.ICircuitConnectible avail = elm.availObjects[0];
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
                        List<signals.ICircuitConnectible> newAvail = new List<signals.ICircuitConnectible>();
                        foreach (signals.ICircuitConnectible otherAvail in otherElm.availObjects)
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
                        signals.ICircuitConnectible otherAvail = otherElm.availObjects[0];
                        signals.EType otherType = value.InputType(otherAvail);
                        if (ourType != otherType)
                        {
                            signals.IFunctionSpec func = findImplicitConversion(library, ourType, otherType);
                            if (func == null)
                            {
                                // only option isn't type-compatible? Looks like we broke a connection
                                throw new LinkTypeFailure(entry.Key, ourType, entry.Value, otherType);
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
                        throw new CannotResolveElement(otherElm);
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
                        List<signals.ICircuitConnectible> newAvail = new List<signals.ICircuitConnectible>();
                        foreach (signals.ICircuitConnectible otherAvail in otherElm.availObjects)
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
                        signals.ICircuitConnectible otherAvail = otherElm.availObjects[0];
                        signals.EType otherType = key.OutputType(otherAvail);
                        if (ourType != otherType)
                        {
                            signals.IFunctionSpec func = findImplicitConversion(library, otherType, ourType);
                            if (func == null)
                            {
                                // only option isn't type-compatible? Looks like we broke a connection
                                throw new LinkTypeFailure(entry.Key, otherType, entry.Value, ourType);
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
                        throw new CannotResolveElement(otherElm);
                    }
                    if (changed) recurseList.Add(value.elem, true);
                }
            }
            foreach (KeyValuePair<Element, bool> entry in recurseList)
            {
                resolveNeighbors(library, entry.Key);
            }
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
            add(newElm);
            this.connections[fromEP] = new EndpointKey(newElm, 0);
            this.connections[new EndpointKey(newElm, 0)] = toEP;
            return true;
        }

        public Circuit construct()
        {
            Dictionary<int, Circuit.Element> result = new Dictionary<int, Circuit.Element>();
            Dictionary<Element, object> elmResult = new Dictionary<Element, object>();
            foreach (KeyValuePair<int, Element> here in contents)
            {
                Element elm = here.Value;
                if (elm.availObjects.Count != 1) throw new ApplicationException("expecting all elements to have exactly one solution");
                signals.ICircuitConnectible avail = elm.availObjects[0];
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
                result.Add(elm.circuitId, new Circuit.Element(new ElemKey(elm), elm.circuitId, newOb));
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
            return new Circuit(result);
        }
    }

    public class Circuit
    {
        public class Element
        {
            public readonly Schematic.ElemKey descr;
            public readonly int circuitId;
            public readonly object obj; // either IBlock or IFunction

            public Element(Schematic.ElemKey d, int c, object o)
            {
                descr = d;
                circuitId = c;
                obj = o;
            }
        }

        protected Dictionary<int, Element> radio;

        public Circuit(Dictionary<int, Element> r)
        {
            radio = r;
        }

        public object Entry(Schematic.Element elem)
        {
            if(elem == null) throw new ArgumentNullException("elem");
            if(elem.circuitId == 0) throw new ArgumentException("Element has no circuit ID", "elem");
            return Entry(elem.circuitId);
        }

        public object Entry(int circuitId)
        {
            Element val;
            if (radio.TryGetValue(circuitId, out val))
            {
                return val.obj;
            }
            else
            {
                return null;
            }
        }

        public List<Element> Find(Schematic.ElemKey key)
        {
            List<Element> results = new List<Element>();
            foreach (KeyValuePair<int, Element> entry in radio)
            {
                if (entry.Value.descr.Equals(key))
                {
                    results.Add(entry.Value);
                }
            }
            return results;
        }

        public void Start()
        {
            foreach(KeyValuePair<int, Element> entry in radio)
            {
                signals.IBlock blk = entry.Value.obj as signals.IBlock;
                if(blk != null)
                {
                    blk.Start();
                }
            }
        }

        public void Stop()
        {
            foreach(KeyValuePair<int, Element> entry in radio)
            {
                signals.IBlock blk = entry.Value.obj as signals.IBlock;
                if(blk != null)
                {
                    blk.Stop();
                }
            }
        }
    }
}
