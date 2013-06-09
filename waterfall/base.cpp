// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "base.h"

#include <d3d10.h>
#include <d3dx10async.h>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;

const char* CDirectxBase::CIncoming::EP_NAME = "in";
const char* CDirectxBase::CIncoming::EP_DESCR = "Display incoming endpoint";

// ---------------------------------------------------------------------------- class CAttr_target_hwnd

class CAttr_target_hwnd : public CRWAttribute<signals::etypWinHdl>
{
private:
	typedef CRWAttribute<signals::etypWinHdl> base;
public:
	inline CAttr_target_hwnd(CDirectxBase& parent, const char* name, const char* descr)
		:base(name, descr, NULL), m_parent(parent)
	{
	}

protected:
	CDirectxBase& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setTargetWindow((HWND)newVal);
		base::onSetValue(newVal);
	}
};

// ---------------------------------------------------------------------------- class CDirectxBase

#pragma warning(push)
#pragma warning(disable: 4355)
CDirectxBase::CDirectxBase(signals::IBlockDriver* driver):m_driver(driver),m_incoming(this),m_bDataThreadEnabled(true),
	 m_dataThread(Thread<CDirectxBase*>::delegate_type(&CDirectxBase::process_thread)),
	 m_hOutputWin(NULL),m_screenCliWidth(0),m_screenCliHeight(0),m_screenWinWidth(0),
	 m_screenWinHeight(0),m_pOldWinProc(NULL),m_dataTexWidth(0)
{
	m_dataThread.launch(this, THREAD_PRIORITY_NORMAL);
}
#pragma warning(pop)

CDirectxBase::~CDirectxBase()
{
	Locker lock(m_refLock);
	releaseDevice();
}

unsigned CDirectxBase::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_incoming;
	}
	return 1;
}

void CDirectxBase::process_thread(CDirectxBase* owner)
{
	ThreadBase::SetThreadName("Waterfall Display Thread");

	std::vector<double> buffer;
	while(owner->m_bDataThreadEnabled)
	{
		{
			Locker lock(owner->m_refLock);
			if(owner->m_dataTexWidth > buffer.capacity()) buffer.resize(owner->m_dataTexWidth);
		}
		if(!buffer.capacity()) buffer.resize(DEFAULT_CAPACITY);
		unsigned recvCount = owner->m_incoming.Read(signals::etypVecDouble, buffer.data(),
			buffer.capacity(), FALSE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			Locker lock(owner->m_refLock);
			unsigned bufSize = owner->m_dataTexWidth;
			if(!bufSize) bufSize = recvCount;

			double* buffPtr = buffer.data();
			while(recvCount)
			{
				owner->onReceivedFrame(buffer.data(), bufSize);
				buffPtr += bufSize;
				recvCount -= min(recvCount, bufSize);
			}
		}
	}
}

void CDirectxBase::buildAttrs()
{
	attrs.targetWindow = addLocalAttr(true, new CAttr_target_hwnd(*this, "targetWindow", "Window to display waterfall on"));
	attrs.enableVsync = addLocalAttr(true, new CRWAttribute<signals::etypBoolean>("enableVsync", "Use monitor vsync on display", false));
}

void CDirectxBase::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(m_hOutputWin != NULL)
	{
		LONG_PTR pCurWnd = GetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC);
		if((void*)pCurWnd == &WindowProcCatcher)
		{
			SetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC, (LONG)m_pOldWinProc);
		}
		m_hOutputWin = NULL;
	}
	if(m_pDevice) m_pDevice->ClearState();
	m_pSwapChain.Release();
	m_pDepthView.Release();
	m_pDepthStencilState.Release();
	m_pRenderTargetView.Release();
	m_pDevice.Release();
}

void CDirectxBase::setTargetWindow(HWND hWnd)
{
	Locker lock(m_refLock);
	releaseDevice();
	m_hOutputWin = hWnd;
	if(hWnd != NULL && hWnd != INVALID_HANDLE_VALUE)
	{
		m_pOldWinProc = (WNDPROC)GetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC);
		if(!m_pOldWinProc) ThrowLastError(GetLastError());
		SetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC, (LONG)&WindowProcCatcher);
		SetWindowLongPtr(m_hOutputWin, GWL_USERDATA, (LONG)this);

		HRESULT hR = initDevice();
		if(SUCCEEDED(hR)) hR = initTexture();
		if(FAILED(hR)) ThrowLastError(hR);
	}
}

HRESULT CDirectxBase::initDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	{
		RECT rect;
		if(!GetClientRect(m_hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());
		m_screenCliHeight = rect.bottom - rect.top;
		m_screenCliWidth = rect.right - rect.left;
		if(!GetWindowRect(m_hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());
		m_screenWinHeight = rect.bottom - rect.top;
		m_screenWinWidth = rect.right - rect.left;
	}

	HDC hDC = GetDC(m_hOutputWin);
	if(hDC == NULL) return HRESULT_FROM_WIN32(GetLastError());
	int iRefresh = GetDeviceCaps(hDC, VREFRESH);
	ReleaseDC(m_hOutputWin, hDC);

	DXGI_SWAP_CHAIN_DESC sd;
	memset(&sd, 0, sizeof(sd));
	sd.BufferCount = 1;
//	sd.BufferDesc.Width = m_screenWidth;
//	sd.BufferDesc.Height = m_screenHeight;
	if(iRefresh > 1)
	{
		sd.BufferDesc.RefreshRate.Numerator = iRefresh;
		sd.BufferDesc.RefreshRate.Denominator = 1;
	} else {
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 0;
	}
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.Flags = 0;
	sd.OutputWindow = m_hOutputWin;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;

#ifdef _DEBUG
	enum { DX_FLAGS = D3D10_CREATE_DEVICE_DEBUG };
#else
	enum { DX_FLAGS = 0 };
#endif
	HRESULT hR = D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE,
		NULL, DX_FLAGS, D3D10_SDK_VERSION, &sd, m_pSwapChain.inref(), m_pDevice.inref());
	if(FAILED(hR)) return hR;

	ID3D10Texture2DPtr backBuffer;
	hR = m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
	if(FAILED(hR)) return hR;
	hR = m_pDevice->CreateRenderTargetView(backBuffer, NULL, m_pRenderTargetView.inref());
	if(FAILED(hR)) return hR;

	D3D10_TEXTURE2D_DESC depthBufferDesc;
	memset(&depthBufferDesc, 0, sizeof(depthBufferDesc));
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.Height = m_screenCliHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	depthBufferDesc.Width = m_screenCliWidth;

	ID3D10Texture2DPtr depthBuffer;
	hR = m_pDevice->CreateTexture2D(&depthBufferDesc, NULL, depthBuffer.inref());
	if(FAILED(hR)) return hR;

	D3D10_DEPTH_STENCIL_DESC depthStencilDesc;
	memset(&depthStencilDesc, 0, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D10_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_KEEP; // Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D10_STENCIL_OP_KEEP; // Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
	
	hR = m_pDevice->CreateDepthStencilState(&depthStencilDesc, m_pDepthStencilState.inref());
	if(FAILED(hR)) return hR;

	m_pDevice->OMSetDepthStencilState(m_pDepthStencilState, 1);

	hR = m_pDevice->CreateDepthStencilView(depthBuffer, NULL, m_pDepthView.inref());
	if(FAILED(hR)) return hR;

	ID3D10RenderTargetView* targs = m_pRenderTargetView;
	m_pDevice->OMSetRenderTargets(1, &targs, m_pDepthView);

	// Setup the viewport for rendering.
	D3D10_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(viewport));
	viewport.Width = m_screenCliWidth;
	viewport.Height = m_screenCliHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_pDevice->RSSetViewports(1, &viewport);

	return S_OK;
}

HRESULT CDirectxBase::resizeDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_pSwapChain || !m_hOutputWin)
	{
		return S_FALSE;
	}

	{
		RECT rect;
		if(!GetClientRect(m_hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());
		m_screenCliHeight = rect.bottom - rect.top;
		m_screenCliWidth = rect.right - rect.left;
	}

	// Release all outstanding references to the swap chain's buffers.
	m_pDevice->OMSetRenderTargets(0, NULL, NULL);
	m_pRenderTargetView.Release();
	m_pDepthView.Release();

	// Preserve the existing buffer count and format.
	// Automatically choose the width and height to match the client rect for HWNDs.
	HRESULT hR = m_pSwapChain->ResizeBuffers(0, m_screenCliWidth, m_screenCliHeight, DXGI_FORMAT_UNKNOWN, 0);
	if(FAILED(hR)) return hR;

	// Get buffer and create a render-target-view.
	ID3D10Texture2DPtr backBuffer;
	hR = m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateRenderTargetView(backBuffer, NULL, m_pRenderTargetView.inref());
	if(FAILED(hR)) return hR;

	D3D10_TEXTURE2D_DESC depthBufferDesc;
	memset(&depthBufferDesc, 0, sizeof(depthBufferDesc));
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.Height = m_screenCliHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	depthBufferDesc.Width = m_screenCliWidth;

	ID3D10Texture2DPtr depthBuffer;
	hR = m_pDevice->CreateTexture2D(&depthBufferDesc, NULL, depthBuffer.inref());
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateDepthStencilView(depthBuffer, NULL, m_pDepthView.inref());
	if(FAILED(hR)) return hR;

	ID3D10RenderTargetView* targs = m_pRenderTargetView;
	m_pDevice->OMSetRenderTargets(1, &targs, m_pDepthView);

	// Setup the viewport for rendering.
	D3D10_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(viewport));
	viewport.Width = m_screenCliWidth;
	viewport.Height = m_screenCliHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
//	viewport.TopLeftX = 0;
//	viewport.TopLeftY = 0;
	m_pDevice->RSSetViewports(1, &viewport);

	return S_OK;
}

CDirectxBase::CIncoming::~CIncoming()
{
	if(m_lastWidthAttr)
	{
		if(m_bAttached) m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
}

void CDirectxBase::CIncoming::OnConnection(signals::IEPRecvFrom *conn)
{
	if(m_lastWidthAttr)
	{
		if(m_bAttached) m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
	if(!conn) return;
	signals::IAttributes* attrs = conn->OutputAttributes();
	if(!attrs) return;
	signals::IAttribute* attr = attrs->GetByName("blockSize");
	if(!attr || attr->Type() != signals::etypShort) return;
	m_lastWidthAttr = attr;
	m_lastWidthAttr->Observe(this);
	m_bAttached = true;
	OnChanged(attr->Name(), attr->Type(), attr->getValue());
}

void CDirectxBase::CIncoming::OnChanged(const char* name, signals::EType type, const void* value)
{
	if(_stricmp(name, "blockSize") == 0 && type == signals::etypShort)
	{
		short width = *(short*)value;
		if(width != m_parent->m_dataTexWidth && width > 0)
		{
			Locker lock(m_parent->m_refLock);
			if(width != m_parent->m_dataTexWidth && width > 0)
			{
				m_parent->m_dataTexWidth = width;
				m_parent->initDataTexture();
			}
		}
	}
}

void CDirectxBase::CIncoming::OnDetached(const char* name)
{
	if(_stricmp(name, "blockSize") == 0)
	{
		m_bAttached = false;
	}
}

LRESULT CDirectxBase::WindowProcCatcher(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CDirectxBase* waterfall = (CDirectxBase*)::GetWindowLongPtr(hWnd, GWL_USERDATA);
	ASSERT(waterfall);
	return waterfall->WindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CDirectxBase::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(hWnd == m_hOutputWin)
	{
		switch(uMsg)
		{
		case WM_SIZE:
			switch(wParam)
			{
			case SIZE_MAXIMIZED:
			case SIZE_RESTORED:
				if(m_screenWinWidth != LOWORD(lParam) || m_screenWinHeight != HIWORD(lParam))
				{
					Locker lock(m_refLock);
					if(m_screenWinWidth != LOWORD(lParam) || m_screenWinHeight != HIWORD(lParam))
					{
						m_screenWinWidth = LOWORD(lParam);
						m_screenWinHeight = HIWORD(lParam);
						VERIFY(SUCCEEDED(resizeDevice()));
					}
				}
				break;
			}
			break;
		case WM_WINDOWPOSCHANGED:
			{
				LPWINDOWPOS winPos = (LPWINDOWPOS)lParam;
				if((winPos->flags & SWP_NOSIZE) == 0 && (winPos->cx != m_screenWinWidth || winPos->cy != m_screenWinHeight))
				{
					Locker lock(m_refLock);
					if((winPos->flags & SWP_NOSIZE) == 0 && (winPos->cx != m_screenWinWidth || winPos->cy != m_screenWinHeight))
					{
						m_screenWinWidth = winPos->cx;
						m_screenWinHeight = winPos->cy;
						VERIFY(SUCCEEDED(resizeDevice()));
					}
				}
				break;
			}
		}
	}
	return CallWindowProc(m_pOldWinProc, hWnd, uMsg, wParam, lParam);
}

HRESULT CDirectxBase::drawFrame()
{
	Locker lock(m_refLock);
	if(!m_pDevice || !m_pSwapChain)
	{
		return E_POINTER;
	}

	m_pDevice->ClearDepthStencilView(m_pDepthView, D3D10_CLEAR_DEPTH, 1.0f, 0);

	HRESULT hR = drawFrameContents();
	if(FAILED(hR)) return hR;

	hR = m_pSwapChain->Present(attrs.enableVsync && attrs.enableVsync->nativeGetValue() ? 1 : 0, 0);
	if(FAILED(hR)) return hR;

	return S_OK;
}