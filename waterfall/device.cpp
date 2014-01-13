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
#include "stdafx.h"
#include "device.h"

#include <d3d10_1.h>
#pragma comment(lib, "d3d10_1.lib")
#pragma comment(lib, "dxgi.lib")

extern HMODULE gl_DllModule;
static Lock gl_deviceLock;
static CVideoDevice* gl_device = NULL;

struct IDXGIAdapter;
typedef unk_ref_t<IDXGIAdapter> IDXGIAdapterPtr;

HRESULT GetVideoDevice(CVideoDevicePtr& pDevice)
{
	Locker lock(gl_deviceLock);
	if(gl_device)
	{
		pDevice = CVideoDevicePtr(gl_device);
		return S_OK;
	}

	CVideoDevice* dev = new CVideoDevice;
	HRESULT hR = dev->createDevice();
	if(FAILED(hR))
	{
		delete dev;
	}
	else
	{
		gl_device = dev;
		pDevice = CVideoDevicePtr(dev);
	}
	return hR;
}

void CVideoDevice::AddRef()
{
	InterlockedIncrement(&m_refCount);
}

void CVideoDevice::Release()
{
	{
		Locker lock(gl_deviceLock);
		if(InterlockedDecrement(&m_refCount)) return;
		if(gl_device == this) gl_device = NULL;
	}
	delete this;
}

CVideoDevice::~CVideoDevice()
{
	if(m_pDevice) m_pDevice->ClearState();
	m_pDevice.Release();
}

HRESULT CVideoDevice::createDevice()
{
	HRESULT hR = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)m_pFactory.inref());
	if(FAILED(hR)) return hR;

	IDXGIAdapterPtr pAdapter;
	hR = m_pFactory->EnumAdapters(0, pAdapter.inref());
	if(FAILED(hR)) return hR;

//	DXGI_ADAPTER_DESC descr;
//	memset(&descr, 0, sizeof(descr));
//	hR = pAdapter->GetDesc(&descr);

#ifdef _DEBUG
	UINT DX_FLAGS = D3D10_CREATE_DEVICE_DEBUG;
#else
	UINT DX_FLAGS = 0;
#endif
	static const D3D10_FEATURE_LEVEL1 LEVELS[] = 
		{
			D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0,
			D3D10_FEATURE_LEVEL_9_3, D3D10_FEATURE_LEVEL_9_2, D3D10_FEATURE_LEVEL_9_1
		};
	int featureIdx = 0;
	D3D10_FEATURE_LEVEL1 level = LEVELS[0];

	for(;;)
	{
		HRESULT hR = D3D10CreateDevice1(pAdapter, D3D10_DRIVER_TYPE_HARDWARE,
			NULL, DX_FLAGS, level, D3D10_1_SDK_VERSION, m_pDevice.inref());
		if(SUCCEEDED(hR)) break;

#ifdef _DEBUG
		if((hR == E_FAIL || hR == DXGI_ERROR_UNSUPPORTED) && (DX_FLAGS & D3D10_CREATE_DEVICE_DEBUG))
		{
			// no debug layer available?
			DX_FLAGS &= ~D3D10_CREATE_DEVICE_DEBUG;
			continue;
		}
		DX_FLAGS |= D3D10_CREATE_DEVICE_DEBUG;
#endif
		if(featureIdx < _countof(LEVELS))
		{
			level = LEVELS[++featureIdx];
			continue;
		}

		// request failed
		return hR;
	}

//	HRESULT hR = m_pDevice->QueryInterface(__uuidof(IDXGIDevice1), (void **)m_pDxgiDevice.inref());

	switch(level)
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

	return S_OK;
}

HRESULT CVideoDevice::createPixelShaderFromResource(LPCTSTR fileName, ID3D10PixelShaderPtr& pShader)
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

HRESULT CVideoDevice::createVertexShaderFromResource(LPCTSTR fileName, const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
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

HRESULT CVideoDevice::CheckFormatSupport(DXGI_FORMAT Format, UINT& pFormatSupport)
{
	return m_pDevice->CheckFormatSupport(Format, &pFormatSupport);
}

HRESULT CVideoDevice::CreateBlendState(const D3D10_BLEND_DESC& pBlendStateDesc, ID3D10BlendStatePtr& ppBlendState)
{
	return m_pDevice->CreateBlendState(&pBlendStateDesc, ppBlendState.inref());
}

HRESULT CVideoDevice::CreateBuffer(const D3D10_BUFFER_DESC& pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData,
	ID3D10BufferPtr& ppBuffer)
{
	return m_pDevice->CreateBuffer(&pDesc, pInitialData, ppBuffer.inref());
}

HRESULT CVideoDevice::CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC& pDepthStencilDesc,
	ID3D10DepthStencilStatePtr& ppDepthStencilState)
{
	return m_pDevice->CreateDepthStencilState(&pDepthStencilDesc, ppDepthStencilState.inref());
}

HRESULT CVideoDevice::CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
	ID3D10DepthStencilViewPtr& ppDepthStencilView)
{
	return m_pDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView.inref());
}

HRESULT CVideoDevice::CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D10RenderTargetViewPtr& ppRTView)
{
	return m_pDevice->CreateRenderTargetView(pResource, pDesc, ppRTView.inref());
}

HRESULT CVideoDevice::CreateSamplerState(const D3D10_SAMPLER_DESC& pSamplerDesc, ID3D10SamplerStatePtr& ppSamplerState)
{
	return m_pDevice->CreateSamplerState(&pSamplerDesc, ppSamplerState.inref());
}

HRESULT CVideoDevice::CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
	ID3D10ShaderResourceViewPtr& ppSRView)
{
	return m_pDevice->CreateShaderResourceView(pResource, pDesc, ppSRView.inref());
}

HRESULT CVideoDevice::CreateTexture2D(const D3D10_TEXTURE2D_DESC &pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData,
	ID3D10Texture2DPtr& ppTexture2D)
{
	return m_pDevice->CreateTexture2D(&pDesc, pInitialData, ppTexture2D.inref());
}

void CVideoDevice::UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox,
	const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	return m_pDevice->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}

HRESULT CVideoDevice::CreateSwapChain(const DXGI_SWAP_CHAIN_DESC& pDesc, IDXGISwapChainPtr& ppSwapChain)
{
	return m_pFactory->CreateSwapChain(m_pDevice, const_cast<DXGI_SWAP_CHAIN_DESC*>(&pDesc), ppSwapChain.inref());
}

bool CVideoDevice::BeginDraw(CDirectxBase* newFocus, ID3D10Device1*& pDevice, Locker& devLock)
{
	devLock.set(m_drawLock);
	devLock.lock();
	pDevice = m_pDevice;
	bool bFocusChange = m_currentFocus != newFocus;
	m_currentFocus = newFocus;
	return bFocusChange;
}

bool CVideoDevice::ClearFocus(CDirectxBase* oldFocus)
{
	Locker lock(m_drawLock);
	if(m_currentFocus == oldFocus)
	{
		m_pDevice->ClearState();
		m_currentFocus = NULL;
		return true;
	}
	return false;
}
