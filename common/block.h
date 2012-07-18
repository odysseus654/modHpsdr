/*
	Copyright 2012 Erik Anderson

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
	__interface IEPReceiver;
	__interface IEPSender;
	__interface IInEndpoint;
	__interface IOutEndpoint;

	enum EType
	{
		etypNone	= 0x00,
		etypEvent	= 0x01, // 0 0000 001
		etypString	= 0x03, // 0 0000 011

		etypBoolean	= 0x08, // 0 0001 000
		etypByte	= 0x0B, // 0 0001 011
		etypShort	= 0x0C, // 0 0001 100
		etypLong	= 0x0D, // 0 0001 101
		etypSingle	= 0x15, // 0 0010 101
		etypDouble	= 0x16, // 0 0010 110
		etypComplex	= 0x1D, // 0 0011 101
		etypCmplDbl = 0x1E, // 0 0011 110
		etypLRSingle = 0x25, // 0 0100 101

		etypVecBoolean	= 0x88, // 1 0001 000
		etypVecByte		= 0x8B, // 1 0001 011
		etypVecShort	= 0x8C, // 1 0001 100
		etypVecLong		= 0x8D, // 1 0001 101
		etypVecSingle	= 0x95, // 1 0010 101
		etypVecDouble	= 0x96, // 1 0010 110
		etypVecComplex	= 0x9D, // 1 0011 101
		etypVecCmplDbl	= 0x9E, // 1 0011 110
		etypVecLRSingle	= 0xA5 // 1 0100 101
	};

	__interface IBlockDriver
	{
		const char* Name();
		BOOL canCreate();
		BOOL canDiscover();
		unsigned Discover(IBlock** blocks, unsigned availBlocks);
		IBlock* Create();
	};

	__interface IBlock
	{
		unsigned AddRef();
		unsigned Release();
		const char* Name();
		IBlockDriver* Driver();
		IBlock* Parent();
		unsigned Children(IBlock** blocks, unsigned availBlocks);
		unsigned Incoming(IInEndpoint** ep, unsigned availEP);
		unsigned Outgoing(IOutEndpoint** ep, unsigned availEP);
		IAttributes* Attributes();
		void Start();
		void Stop();
	};

	__interface IEPSender
	{
		unsigned Write(EType type, const void* buffer, unsigned numElem, unsigned msTimeout);
		unsigned AddRef(IOutEndpoint* src);
		unsigned Release(IOutEndpoint* src);
	};

	__interface IEPReceiver
	{
		unsigned Read(EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout);
		void onSinkConnected(IInEndpoint* src);
		void onSinkDisconnected(IInEndpoint* src);
	};

	__interface IEPBuffer : public IEPSender, public IEPReceiver
	{
		EType Type();
		unsigned Capacity();
		unsigned Used();
	};

	__interface IInEndpoint
	{
		unsigned AddRef();
		unsigned Release();
		const char* EPName();
		EType Type();
		IAttributes* Attributes();
		BOOL Connect(IEPReceiver* recv);
		BOOL isConnected();
		BOOL Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IOutEndpoint
	{
		const char* EPName();
		EType Type();
		IAttributes* Attributes();
		BOOL Connect(IEPSender* send);
		BOOL isConnected();
		BOOL Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IAttributes
	{
		unsigned Itemize(IAttribute** attrs, unsigned availElem);
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
		BOOL isReadOnly();
		const void* getValue();
		BOOL setValue(const void* newVal);
		unsigned options(const void* values, const char** opts, unsigned availElem);
	};

	__interface IAttributeObserver
	{
		void OnChanged(const char* name, EType type, const void* value);
	};
}