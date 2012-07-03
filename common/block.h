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
		etypEvent	= 0x01,
		etypBoolean	= 0x08,
		etypByte	= 0x0B,
		etypShort	= 0x0C,
		etypLong	= 0x0D,
		etypSingle	= 0x15,
		etypDouble	= 0x16,
		etypComplex	= 0x1D,
		etypCmplDbl = 0x1E,
		etypString	= 0x23,
		etypLRSingle = 0x2D
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
		unsigned AddRef();
		unsigned Release();
	};

	__interface IEPReceiver
	{
		unsigned Read(EType type, void* buffer, unsigned numAvail, unsigned msTimeout);
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
		IBlock* Block();
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
		IBlock* Block();
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
		IBlock* Block();
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