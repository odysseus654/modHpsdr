/*
	Copyright 2012-2013 Erik Anderson

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
#pragma once
namespace signals
{
	__interface IAttribute;
	__interface IAttributes;
	__interface IAttributeObserver;
	__interface IBlock;
	__interface IBlockDriver;
	__interface IEPBuffer;
	__interface IEPRecvFrom;
	__interface IEPSendTo;
	__interface IFunction;
	__interface IFunctionSpec;
	__interface IInEndpoint;
	__interface IInputFunction;
	__interface IOutEndpoint;
	__interface IOutputFunction;

	enum EType
	{
		etypNone	= 0x00,
		etypEvent	= 0x01,
		etypString	= 0x02,
		etypWinHdl	= 0x03,

		etypBoolean	= 0x10,
		etypByte	= 0x11,
		etypShort	= 0x12,
		etypLong	= 0x13,
		etypInt64	= 0x14,
		etypSingle	= 0x23,
		etypDouble	= 0x24,
		etypComplex	= 0x34,
		etypCmplDbl = 0x35,
		etypLRSingle = 0x44,

		etypVecBoolean	= 0x18,
		etypVecByte		= 0x19,
		etypVecShort	= 0x1A,
		etypVecLong		= 0x1B,
		etypVecInt64	= 0x1C,
		etypVecSingle	= 0x2B,
		etypVecDouble	= 0x2C,
		etypVecComplex	= 0x3C,
		etypVecCmplDbl	= 0x3D,
		etypVecLRSingle	= 0x4C
	};

	__interface IBlockDriver
	{
		const char* Name();
		const char* Description();
		BOOL canCreate();
		BOOL canDiscover();
		unsigned Discover(IBlock** blocks, unsigned availBlocks);
		IBlock* Create();
		const unsigned char* Fingerprint();
	};

	__interface IBlock
	{
		unsigned AddRef();
		unsigned Release();
		const char* Name();
		unsigned NodeId(char* buff, unsigned availChar);
		IBlockDriver* Driver();
		IBlock* Parent();
		unsigned Children(IBlock** blocks, unsigned availBlocks);
		unsigned Incoming(IInEndpoint** ep, unsigned availEP);
		unsigned Outgoing(IOutEndpoint** ep, unsigned availEP);
		IAttributes* Attributes();
		void Start();
		void Stop();
	};

	__interface IEPSendTo
	{
		unsigned Write(EType type, const void* buffer, unsigned numElem, unsigned msTimeout);
		BOOL WriteOne(EType type, const void* buffer, unsigned msTimeout);
		unsigned AddRef(IOutEndpoint* src);
		unsigned Release(IOutEndpoint* src);
		IAttributes* InputAttributes();
	};

	__interface IEPRecvFrom
	{
		unsigned Read(EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout);
		BOOL ReadOne(EType type, void* buffer, unsigned msTimeout);
		void onSinkConnected(IInEndpoint* src);
		void onSinkDisconnected(IInEndpoint* src);
		IAttributes* OutputAttributes();
		IEPBuffer* CreateBuffer();
	};

	__interface IEPBuffer : public IEPSendTo, public IEPRecvFrom
	{
		EType Type();
		unsigned Capacity();
		unsigned Used();
	};

	__interface IVector
	{
		EType Type();
		unsigned Size();
		unsigned AddRef();
		unsigned Release();
		const void* Data();
	};

	__interface IInEndpoint
	{
		unsigned AddRef();
		unsigned Release();
		const char* EPName();
		const char* EPDescr();
		EType Type();
		IAttributes* Attributes();
		BOOL Connect(IEPRecvFrom* recv);
		BOOL isConnected();
		BOOL Disconnect();
	};

	__interface IOutEndpoint
	{
		const char* EPName();
		const char* EPDescr();
		EType Type();
		IAttributes* Attributes();
		BOOL Connect(IEPSendTo* send);
		BOOL isConnected();
		BOOL Disconnect();
		IEPBuffer* CreateBuffer();
	};

	enum EAttrEnumFlags
	{
		flgLocalOnly = 1,
		flgIncludeHidden = 2
	};

	__interface IAttributes
	{
		unsigned Itemize(IAttribute** attrs, unsigned availElem, unsigned flags);
		IAttribute* GetByName(const char* name);
		void Observe(IAttributeObserver* obs);
		void Unobserve(IAttributeObserver* obs);
	};

	__interface IAttribute
	{
		const char* Name();
		const char* Description();
		EType Type();
		void Observe(IAttributeObserver* obs);
		void Unobserve(IAttributeObserver* obs);
		BOOL isReadOnly() const;
		const void* getValue();
		BOOL setValue(const void* newVal);
		unsigned options(const void* values, const char** opts, unsigned availElem);
	};

	__interface IAttributeObserver
	{
		void OnChanged(IAttribute* attr, const void* value);
		void OnDetached(IAttribute* attr);
	};

	__interface IFunctionSpec
	{
		const char* Name();
		const char* Description();
		IFunction* Create();
		const unsigned char* Fingerprint();
	};

	__interface IFunction
	{
		IFunctionSpec* Spec();
		unsigned AddRef();
		unsigned Release();
		IInputFunction* Input();
		IOutputFunction* Output();
	};

	__interface IInputFunction : public IInEndpoint, public IEPRecvFrom
	{
	};

	__interface IOutputFunction : public IOutEndpoint, public IEPSendTo
	{
	};
}