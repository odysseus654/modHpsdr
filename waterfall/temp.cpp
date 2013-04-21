#include "stdafx.h"
#include "unkref.h"

#include <d3d10.h>
#include <stddef.h>
#pragma comment(lib, "d3d10.lib")

typedef unk_ref_t<ID3D10Device> ID3D10DevicePtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;

HRESULT doTest(HWND hOutputWin)
{
	RECT rect;
	if(!GetClientRect(hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());

	HDC hDC = GetDC(hOutputWin);
	if(hDC == NULL) return HRESULT_FROM_WIN32(GetLastError());
	int iRefresh = GetDeviceCaps(hDC, VREFRESH);
	ReleaseDC(hOutputWin, hDC);

	DXGI_SWAP_CHAIN_DESC sd;
	memset(&sd, 0, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = rect.right - rect.left;
	sd.BufferDesc.Height = rect.bottom - rect.top;
	sd.BufferDesc.RefreshRate.Numerator = iRefresh;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.Flags = 0;
	sd.OutputWindow = hOutputWin;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;

#ifdef _DEBUG
	enum { DX_FLAGS = D3D10_CREATE_DEVICE_DEBUG };
#else
	enum { DX_FLAGS = 0 };
#endif
	ID3D10DevicePtr pDevice;
	IDXGISwapChainPtr pSwapChain;
	HRESULT hR = D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE,
		NULL, DX_FLAGS, D3D10_SDK_VERSION, &sd, &pSwapChain, &pDevice);
	if(FAILED(hR)) return hR;

	ID3D10RenderTargetViewPtr mRenderTargetView;	// hold until end
	ID3D10Texture2DPtr backBuffer;
	hR = pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
	if(FAILED(hR)) return hR;
	hR = pDevice->CreateRenderTargetView(backBuffer, NULL, &mRenderTargetView);
	if(FAILED(hR)) return hR;

	D3D10_TEXTURE2D_DESC depthBufferDesc;
	memset(&depthBufferDesc, 0, sizeof(depthBufferDesc));
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.Height = rect.bottom - rect.top;
	depthBufferDesc.MipLevels = 1;
//	depthBufferDesc.MiscFlags = 
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	depthBufferDesc.Width = rect.right - rect.left;

	ID3D10Texture2DPtr depthBuffer;
	ID3D10DepthStencilViewPtr pDepthView;	// hold until end
	hR = pDevice->CreateTexture2D(&depthBufferDesc, NULL, &depthBuffer);
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
	
	ID3D10DepthStencilStatePtr pDepthStencilState; // hold until end
	hR = pDevice->CreateDepthStencilState(&depthStencilDesc, &pDepthStencilState);
	if(FAILED(hR)) return hR;

	pDevice->OMSetDepthStencilState(pDepthStencilState, 1);

	hR = pDevice->CreateDepthStencilView(depthBuffer, NULL, &pDepthView);
	if(FAILED(hR)) return hR;

	pDevice->OMSetRenderTargets(1, &mRenderTargetView, pDepthView);

	struct TLVERTEX
	{
		float posX;
		float posY;
		float posZ;
		float texX;
		float texY;
	};

	D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TLVERTEX, posX), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TLVERTEX, texX), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	ID3D10InputLayoutPtr pInputLayout;
	hR = pDevice->CreateInputLayout(vdesc, 2, NULL, 0, &pInputLayout);
	if(FAILED(hR)) return hR;
	/*
	enum { D3DFVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 };

	//Custom vertex

	ID3D10VertexBuffer* vertexBuffer;
	*/
	return S_OK;
}
