#include "stdafx.h"
#include "unkref.h"

#include <d3d10.h>
#include <d3dx10async.h>
#include <stddef.h>
#include <string>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

typedef unk_ref_t<ID3D10Device> ID3D10DevicePtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;
typedef unk_ref_t<ID3D10Effect> ID3D10EffectPtr;
typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;
typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;

HRESULT doTest(HMODULE hModule, HWND hOutputWin)
{
	RECT rect;
	if(!GetClientRect(hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());
	UINT screenHeight = rect.bottom - rect.top;
	UINT screenWidth = rect.right - rect.left;

	HDC hDC = GetDC(hOutputWin);
	if(hDC == NULL) return HRESULT_FROM_WIN32(GetLastError());
	int iRefresh = GetDeviceCaps(hDC, VREFRESH);
	ReleaseDC(hOutputWin, hDC);

	DXGI_SWAP_CHAIN_DESC sd;
	memset(&sd, 0, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = screenWidth;
	sd.BufferDesc.Height = screenHeight;
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
	depthBufferDesc.Height = screenHeight;
	depthBufferDesc.MipLevels = 1;
//	depthBufferDesc.MiscFlags = 
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	depthBufferDesc.Width = screenWidth;

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

	// Setup the viewport for rendering.
	D3D10_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(viewport));
	viewport.Width = screenWidth;
	viewport.Height = screenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	pDevice->RSSetViewports(1, &viewport);

//	// Create an orthographic projection matrix for 2D rendering.
//	D3DXMatrixOrthoLH(&m_orthoMatrix, (float)screenWidth, (float)screenHeight, screenNear, screenDepth);

	// Load the shader in from the file.
	static LPCTSTR filename = _T("waterfall.fx");
	ID3D10EffectPtr pEffect;
	ID3D10BlobPtr errorMessage;
	hR = D3DX10CreateEffectFromResource(hModule, filename, filename, NULL, NULL,
		"fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, pDevice, NULL, NULL, &pEffect, &errorMessage, NULL);
	if(FAILED(hR))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if(errorMessage)
		{
			std::string compileErrors(LPCSTR(errorMessage->GetBufferPointer()), errorMessage->GetBufferSize());
//			MessageBox(hOutputWin, filename, L"Missing Shader File", MB_OK);
			int _i = 0;
		}
		return hR;
	}

	ID3D10EffectTechnique* pTechnique = pEffect->GetTechniqueByName("WaterfallTechnique");
	D3D10_PASS_DESC PassDesc;
	pTechnique->GetPassByIndex(0)->GetDesc(&PassDesc);

	struct TLVERTEX
	{
		D3DXVECTOR3 pos;
		D3DXVECTOR2 tex;
	};

	D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TLVERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TLVERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	ID3D10InputLayoutPtr pInputLayout;
	hR = pDevice->CreateInputLayout(vdesc, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &pInputLayout);
	if(FAILED(hR)) return hR;

	// calculate texture coordinates
	float texLeft = (float)(int(screenWidth / 2) * -1);// + (float)positionX;
	float texRight = texLeft + (float)screenWidth;
	float texTop = (float)(screenHeight / 2);// - (float)positionY;
	float texBottom = texTop - (float)screenHeight;

	// Now create the vertex buffer.
	TLVERTEX vertices[4];
	vertices[0].pos = D3DXVECTOR3(texLeft, texTop, 0.0f);
	vertices[0].tex = D3DXVECTOR2(0.0f, 0.0f);
	vertices[1].pos = D3DXVECTOR3(texRight, texTop, 0.0f);
	vertices[1].tex = D3DXVECTOR2(1.0f, 0.0f);
	vertices[2].pos = D3DXVECTOR3(texLeft, texBottom, 0.0f);
	vertices[2].tex = D3DXVECTOR2(0.0f, 1.0f);
	vertices[3].pos = D3DXVECTOR3(texRight, texBottom, 0.0f);
	vertices[3].tex = D3DXVECTOR2(1.0f, 1.0f);

	D3D10_BUFFER_DESC vertexBufferDesc;
	memset(&vertexBufferDesc, 0, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D10_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = sizeof(TLVERTEX) * 4;
	vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
//	vertexBufferDesc.MiscFlags = 0;

	D3D10_SUBRESOURCE_DATA vertexData;
	memset(&vertexData, 0, sizeof(vertexData));
	vertexData.pSysMem = vertices;

	ID3D10BufferPtr pVertexBuffer;
	hR = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &pVertexBuffer);
	if(FAILED(hR)) return hR;

	// Now create the index buffer.
	static const unsigned long vertexIndices[4] = { 2, 0, 3, 1 };
	D3D10_BUFFER_DESC indexBufferDesc;
	memset(&indexBufferDesc, 0, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * 4;
	indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
//	indexBufferDesc.CPUAccessFlags = 0;
//	indexBufferDesc.MiscFlags = 0;

	D3D10_SUBRESOURCE_DATA indexData;
	memset(&indexData, 0, sizeof(indexData));
	vertexData.pSysMem = vertexIndices;

	ID3D10BufferPtr pVertexIndexBuffer;
	hR = pDevice->CreateBuffer(&indexBufferDesc, &vertexData, &pVertexIndexBuffer);
	if(FAILED(hR)) return hR;

	return S_OK;
}
/*
void D3DClass::BeginScene(float red, float green, float blue, float alpha)
{
	float color[4];

	// Setup the color to clear the buffer to.
	color[0] = red;
	color[1] = green;
	color[2] = blue;
	color[3] = alpha;

	// Clear the back buffer.
	m_device->ClearRenderTargetView(m_renderTargetView, color);
    
	// Clear the depth buffer.
	m_device->ClearDepthStencilView(m_depthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0);
}

void D3DClass::EndScene()
{
	// Present the back buffer to the screen since rendering is complete.
	if(m_vsync_enabled)
	{
		// Lock to screen refresh rate.
		m_swapChain->Present(1, 0);
	}
	else
	{
		// Present as fast as possible.
		m_swapChain->Present(0, 0);
	}

	return;
}
*/