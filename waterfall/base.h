/*
	Copyright 2013-2014 Erik Anderson

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
#include <blockImpl.h>
#include "device.h"

class CDirectxBase : public CThreadBlockBase
{
public:
	CDirectxBase(signals::IBlockDriver* driver);
	virtual ~CDirectxBase();

//	inline ID3D10Device1Ptr& device() { return m_pDevice; }

private:
	CDirectxBase(const CDirectxBase& other);
	CDirectxBase& operator=(const CDirectxBase& other);

public: // IBlock implementation
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual void Start() { m_incoming.AttachAttributes(); }

	struct
	{
		CAttributeBase* targetWindow;
	} attrs;

	void setTargetWindow(void* const & hWnd);

public:
	class CIncoming : public CSimpleIncomingChild, public signals::IAttributeObserver
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CDirectxBase* parent):CSimpleIncomingChild(signals::etypVecDouble, parent),
			m_lastWidthAttr(NULL),m_lastRateAttr(NULL),m_lastFreqAttr(NULL),m_isComplexAttr(NULL) { }
		virtual ~CIncoming();

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual void OnChanged(signals::IAttribute* attr, const void* value);
		virtual void OnDetached(signals::IAttribute* attr);
		void AttachAttributes();

	private:
		virtual void OnConnection(signals::IEPRecvFrom* recv);
		void AttachAttributes(signals::IEPRecvFrom *conn);
		void DetachAttributes();
		signals::IAttribute* m_lastWidthAttr;
		signals::IAttribute* m_lastRateAttr;
		signals::IAttribute* m_lastFreqAttr;
		signals::IAttribute* m_isComplexAttr;
	};

private:
	enum
	{
		IN_BUFFER_TIMEOUT = 1000
	};

	CIncoming m_incoming;

protected: // directx stuff
	Lock m_refLock;
	UINT m_frameWidth;
	UINT m_screenCliWidth;
	UINT m_screenCliHeight;
	volatile __int64 m_dataRate;
	volatile __int64 m_dataFrequency;
private: // directx stuff
	HWND m_hOutputWin;
	WNDPROC m_pOldWinProc;
	UINT m_screenWinWidth;
	UINT m_screenWinHeight;

protected:	// Direct3d references we use
	CVideoDevicePtr m_pDevice;
	ID3D10RenderTargetViewPtr m_pRenderTargetView;
private:	// Direct3d references we use
	IDXGISwapChainPtr m_pSwapChain;
	ID3D10DepthStencilViewPtr m_pDepthView;

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10DepthStencilStatePtr m_pDepthStencilState;

protected:
	inline float driverLevel() const { return m_pDevice ? m_pDevice->driverLevel() : 0; }

	virtual void setIsComplexInput(bool bComplex) {}
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture() PURE;
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture() PURE;
	virtual void clearFrame(ID3D10Device1* pDevice);
	virtual HRESULT drawFrameContents(ID3D10Device1* pDevice) PURE;
	HRESULT drawFrame();
	virtual HRESULT setContext(ID3D10Device1* pDevice);
	virtual HRESULT preDrawFrame() { return S_OK; }
	virtual void onReceivedFrame(double* frame, unsigned size) PURE;
private:
	HRESULT createDevice();
	HRESULT initDevice();

	virtual void thread_run();
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT WindowProcCatcher(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
