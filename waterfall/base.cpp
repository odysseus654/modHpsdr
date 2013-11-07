// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "base.h"

#include <d3d10_1.h>
#include <d3dx10async.h>
#pragma comment(lib, "d3d10_1.lib")
#pragma comment(lib, "d3dx10.lib")

extern HMODULE gl_DllModule;

typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;
typedef unk_ref_t<IDXGIFactory> IDXGIFactoryPtr;
typedef unk_ref_t<IDXGIAdapter> IDXGIAdapterPtr;

const char* CDirectxBase::CIncoming::EP_NAME = "in";
const char* CDirectxBase::CIncoming::EP_DESCR = "Display incoming endpoint";

// ---------------------------------------------------------------------------- class CDirectxBase

#pragma warning(push)
#pragma warning(disable: 4355)
CDirectxBase::CDirectxBase(signals::IBlockDriver* driver):CThreadBlockBase(driver),m_incoming(this),
	 m_hOutputWin(NULL),m_screenCliWidth(0),m_screenCliHeight(0),m_screenWinWidth(0),m_driverLevel(0),
	 m_screenWinHeight(0),m_pOldWinProc(NULL),m_frameWidth(0),m_dataRate(0),m_dataFrequency(0)
{
	startThread();
}
#pragma warning(pop)

CDirectxBase::~CDirectxBase()
{
	Locker lock(m_refLock);
	releaseDevice();
}

void CDirectxBase::thread_run()
{
	ThreadBase::SetThreadName("Waterfall Display Thread");

	std::vector<double> buffer;
	while(threadRunning())
	{
		{
			Locker lock(m_refLock);
			if(m_frameWidth > buffer.capacity()) buffer.resize(m_frameWidth);
		}
		if(!buffer.capacity()) buffer.resize(DEFAULT_CAPACITY);
		unsigned recvCount = m_incoming.Read(signals::etypVecDouble, buffer.data(),
			buffer.capacity(), FALSE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			Locker lock(m_refLock);
			unsigned bufSize = m_frameWidth;
			if(!bufSize) bufSize = recvCount;

			double* buffPtr = buffer.data();
			while(recvCount)
			{
				onReceivedFrame(buffer.data(), bufSize);
				buffPtr += bufSize;
				recvCount -= min(recvCount, bufSize);
			}
		}
	}
}

void CDirectxBase::buildAttrs()
{
	attrs.targetWindow = addLocalAttr(true, new CAttr_callback<signals::etypWinHdl,CDirectxBase>
		(*this, "targetWindow", "Window to display waterfall on", &CDirectxBase::setTargetWindow, NULL));
	attrs.enableVsync = addLocalAttr(true, new CRWAttribute<signals::etypBoolean>
		("enableVsync", "Use monitor vsync on display", false));
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

void CDirectxBase::setTargetWindow(void* const & newTarg)
{
	HWND hWnd = HWND(newTarg);
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

HRESULT CDirectxBase::createPixelShaderFromResource(LPCTSTR fileName, ID3D10PixelShaderPtr& pShader)
{
	HRSRC hResource = FindResource(gl_DllModule, fileName, RT_RCDATA);
	if(!hResource) return HRESULT_FROM_WIN32(GetLastError());

	HGLOBAL hLoadedResource = LoadResource(gl_DllModule, hResource);
	if(!hLoadedResource) return HRESULT_FROM_WIN32(GetLastError());

	DWORD dwSize = SizeofResource(gl_DllModule, hResource);
	if(!dwSize) return HRESULT_FROM_WIN32(GetLastError());

	LPVOID pLockedResource = LockResource(hLoadedResource);
	if(!pLockedResource) return E_UNEXPECTED;

	return m_pDevice->CreatePixelShader(pLockedResource, dwSize, pShader.inref());
}

HRESULT CDirectxBase::createVertexShaderFromResource(LPCTSTR fileName, const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
		UINT NumElements, ID3D10VertexShaderPtr& pShader, ID3D10InputLayoutPtr& pInputLayout)
{
	HRSRC hResource = FindResource(gl_DllModule, fileName, RT_RCDATA);
	if(!hResource) return HRESULT_FROM_WIN32(GetLastError());

	HGLOBAL hLoadedResource = LoadResource(gl_DllModule, hResource);
	if(!hLoadedResource) return HRESULT_FROM_WIN32(GetLastError());

	DWORD dwSize = SizeofResource(gl_DllModule, hResource);
	if(!dwSize) return HRESULT_FROM_WIN32(GetLastError());

	LPVOID pLockedResource = LockResource(hLoadedResource);
	if(!pLockedResource) return E_UNEXPECTED;

	HRESULT hR = m_pDevice->CreateVertexShader(pLockedResource, dwSize, pShader.inref());
	if(FAILED(hR)) return hR;

	return m_pDevice->CreateInputLayout(pInputElementDescs, NumElements, pLockedResource, dwSize, pInputLayout.inref());
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

	// pull the current refresh rate from the current display DC
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
	UINT DX_FLAGS = D3D10_CREATE_DEVICE_DEBUG;
#else
	UINT DX_FLAGS = 0;
#endif

	HRESULT hR;
	for(;;)
	{
		HRESULT hR = D3D10CreateDeviceAndSwapChain1(NULL, D3D10_DRIVER_TYPE_HARDWARE,
			NULL, DX_FLAGS, D3D10_FEATURE_LEVEL_9_1, D3D10_1_SDK_VERSION, &sd, m_pSwapChain.inref(), m_pDevice.inref());
		if(SUCCEEDED(hR)) break;

#ifdef _DEBUG
		if((hR == E_FAIL || hR == DXGI_ERROR_UNSUPPORTED) && (DX_FLAGS & D3D10_CREATE_DEVICE_DEBUG))
		{
			// no debug layer available?
			DX_FLAGS &= ~D3D10_CREATE_DEVICE_DEBUG;
			continue;
		}
#endif
		if(hR == DXGI_ERROR_UNSUPPORTED && sd.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			// try an alternate screen format
			sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			continue;
		}

		// request failed
		return hR;
	}

	switch(m_pDevice->GetFeatureLevel())
	{
	case D3D10_FEATURE_LEVEL_9_1:
		m_driverLevel = 9.1f;
		break;
	case D3D10_FEATURE_LEVEL_9_2:
		m_driverLevel = 9.2f;
		break;
	case D3D10_FEATURE_LEVEL_9_3:
		m_driverLevel = 9.3f;
		break;
	case D3D10_FEATURE_LEVEL_10_0:
		m_driverLevel = 10.0f;
		break;
	case D3D10_FEATURE_LEVEL_10_1:
		m_driverLevel = 10.1f;
		break;
	}

	ID3D10Texture2DPtr backBuffer;
	hR = m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(backBuffer.inref()));
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
	hR = m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(backBuffer.inref()));
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
	DetachAttributes();
}

void CDirectxBase::CIncoming::OnConnection(signals::IEPRecvFrom *conn)
{
	DetachAttributes();
	if(conn) AttachAttributes(conn);
}

void CDirectxBase::CIncoming::AttachAttributes()
{
	signals::IEPRecvFrom *conn = this->CurrentEndpoint();
	if(conn) AttachAttributes(conn);
}

void CDirectxBase::CIncoming::DetachAttributes()
{
	if(m_lastWidthAttr)
	{
		m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
	if(m_lastRateAttr)
	{
		m_lastRateAttr->Unobserve(this);
		m_lastRateAttr = NULL;
	}
	if(m_lastFreqAttr)
	{
		m_lastFreqAttr->Unobserve(this);
		m_lastFreqAttr = NULL;
	}
}

void CDirectxBase::CIncoming::AttachAttributes(signals::IEPRecvFrom *conn)
{
	if(!conn) return;
	signals::IAttributes* attrs = conn->OutputAttributes();
	if(!attrs) return;
	if(!m_lastWidthAttr)
	{
		signals::IAttribute* attr = attrs->GetByName("blockSize");
		if(!attr ||  (attr->Type() != signals::etypShort && attr->Type() != signals::etypLong)) return;
		m_lastWidthAttr = attr;
		m_lastWidthAttr->Observe(this);
		OnChanged(attr, attr->getValue());
	}
	if(!m_lastRateAttr)
	{
		signals::IAttribute* attr = attrs->GetByName("rate");
		if(!attr || attr->Type() != signals::etypLong) return;
		m_lastRateAttr = attr;
		m_lastRateAttr->Observe(this);
		OnChanged(attr, attr->getValue());
	}
	if(!m_lastFreqAttr)
	{
		signals::IAttribute* attr = attrs->GetByName("freq");
		if(!attr || (attr->Type() != signals::etypLong && attr->Type() != signals::etypInt64)) return;
		m_lastFreqAttr = attr;
		m_lastFreqAttr->Observe(this);
		OnChanged(attr, attr->getValue());
	}
}

void CDirectxBase::CIncoming::OnChanged(signals::IAttribute* attr, const void* value)
{
	if(attr == m_lastWidthAttr)
	{
		CDirectxBase* base = static_cast<CDirectxBase*>(m_parent);
		long width;
		if(attr->Type() == signals::etypShort)
		{
			width = *(short*)value;
		} else {
			width = *(long*)value;
		}
		if(width != base->m_frameWidth && width > 0)
		{
			Locker lock(base->m_refLock);
			if(width != base->m_frameWidth && width > 0)
			{
				base->m_frameWidth = width;
				base->initDataTexture();
			}
		}
	}
	else if(attr == m_lastRateAttr && attr->Type() == signals::etypLong)
	{
		CDirectxBase* base = static_cast<CDirectxBase*>(m_parent);
		long rate = *(long*)value;
		if(rate > 0)
		{
			InterlockedExchange(&base->m_dataRate, rate);
		}
	}
	else if(attr == m_lastFreqAttr)
	{
		CDirectxBase* base = static_cast<CDirectxBase*>(m_parent);
		__int64 freq;
		signals::EType type = attr->Type();
		if(type == signals::etypLong)
		{
			freq = *(long*)value;
		}
		else if(type == signals::etypInt64)
		{
			freq = *(__int64*)value;
		}
		if(freq > 0)
		{
			InterlockedExchange64(&base->m_dataFrequency, freq);
		}
	}
}

void CDirectxBase::CIncoming::OnDetached(signals::IAttribute* attr)
{
	if(attr == m_lastWidthAttr)
	{
		m_lastWidthAttr = NULL;
	}
	else if(attr == m_lastRateAttr)
	{
		m_lastRateAttr = NULL;
	}
	else if(attr == m_lastFreqAttr)
	{
		m_lastFreqAttr = NULL;
	}
	else ASSERT(FALSE);
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
