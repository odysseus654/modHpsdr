#include "stdafx.h"
#include "temp.h"

#include <d3d10.h>
#include <d3dx10async.h>
#include <stddef.h>
#include <string>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;
typedef unk_ref_t<ID3D10Texture1D> ID3D10Texture1DPtr;

struct TLVERTEX
{
	D3DXVECTOR3 pos;
	D3DXVECTOR2 tex;
};

const unsigned long D3Dtest::VERTEX_INDICES[4] = { 2, 0, 3, 1 };

D3Dtest::~D3Dtest()
{
	if(m_pDevice)
	{
		m_pDevice->ClearState();
//		m_pDevice->OMSetDepthStencilState(NULL, 1);
//		m_pDevice->OMSetRenderTargets(0, NULL, NULL);
//		m_pDevice->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
//		m_pDevice->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
//		m_pDevice->IASetInputLayout(NULL);
	}
	if(m_dataTexData)
	{
		delete [] m_dataTexData;
		m_dataTexData = NULL;
	}
}

static D3DXVECTOR3 hsv2rgb(const D3DVECTOR& hsv)
{
	if(hsv.y == 0.0f) return D3DXVECTOR3(hsv.z, hsv.z, hsv.z);
	int sect = int(hsv.x) % 6;
	if(sect < 0) sect += 6;
	float fact = hsv.x - floor(hsv.x);

	float p = hsv.z * ( 1.0f - hsv.y );
	float q = hsv.z * ( 1.0f - hsv.y * fact );
	float t = hsv.z * ( 1.0f - hsv.y * ( 1.0f - fact ) );

	switch(int(hsv.x) % 6)
	{
	case 0:
		return D3DXVECTOR3(hsv.z, t, p);
	case 1:
		return D3DXVECTOR3(q, hsv.z, p);
	case 2:
		return D3DXVECTOR3(p, hsv.z, t);
	case 3:
		return D3DXVECTOR3(p, q, hsv.z);
	case 4:
		return D3DXVECTOR3(t, p, hsv.z);
	default: // case 5:
		return D3DXVECTOR3(hsv.z, p, q);
	}
}

HRESULT D3Dtest::initDevice()
{
	RECT rect;
	if(!GetClientRect(m_hOutputWin, &rect)) return HRESULT_FROM_WIN32(GetLastError());
	m_screenHeight = rect.bottom - rect.top;
	m_screenWidth = rect.right - rect.left;

	HDC hDC = GetDC(m_hOutputWin);
	if(hDC == NULL) return HRESULT_FROM_WIN32(GetLastError());
	int iRefresh = GetDeviceCaps(hDC, VREFRESH);
	ReleaseDC(m_hOutputWin, hDC);

	DXGI_SWAP_CHAIN_DESC sd;
	memset(&sd, 0, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_screenWidth;
	sd.BufferDesc.Height = m_screenHeight;
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
	depthBufferDesc.Height = m_screenHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	depthBufferDesc.Width = m_screenWidth;

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
	viewport.Width = m_screenWidth;
	viewport.Height = m_screenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_pDevice->RSSetViewports(1, &viewport);

	return S_OK;
}

HRESULT D3Dtest::initTexture()
{
	static const D3DVECTOR WATERFALL_POINTS[] = {
		{ 0.0f,		0.0f,	0.0f },
		{ 4.0f,		1.0f,	0.533f },
		{ 3.904f,	1.0f,	0.776f },
		{ 3.866f,	1.0f,	0.937f },
		{ 0.925f,	0.390f,	0.937f },
		{ 1.027f,	0.753f,	0.776f },
		{ 1.025f,	0.531f,	0.894f },
		{ 1.0f,		1.0f,	1.0f },
		{ 0.203f,	1.0f,	0.984f },
	};

	// Load the shader in from the file.
	static LPCTSTR filename = _T("waterfall.fx");
	ID3D10BlobPtr errorMessage;
#ifdef _DEBUG
	enum { DX_FLAGS = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG };
#else
	enum { DX_FLAGS = D3D10_SHADER_ENABLE_STRICTNESS };
#endif
	HRESULT hR = D3DX10CreateEffectFromResource(m_hModule, filename, filename, NULL, NULL,
		"fx_4_0", DX_FLAGS, 0, m_pDevice, NULL, NULL, m_pEffect.inref(), errorMessage.inref(), NULL);
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

	m_pTechnique = m_pEffect->GetTechniqueByName("WaterfallTechnique");
	D3D10_PASS_DESC PassDesc;
	m_pTechnique->GetPassByIndex(0)->GetDesc(&PassDesc);

	D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TLVERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TLVERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	hR = m_pDevice->CreateInputLayout(vdesc, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, m_pInputLayout.inref());
	if(FAILED(hR)) return hR;

	// calculate texture coordinates
	float texLeft = (float)(int(m_screenWidth / 2) * -1);// + (float)positionX;
	float texRight = texLeft + (float)m_screenWidth;
	float texTop = (float)(m_screenHeight / 2);// - (float)positionY;
	float texBottom = texTop - (float)m_screenHeight;

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

	hR = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, m_pVertexBuffer.inref());
	if(FAILED(hR)) return hR;

	// Now create the index buffer.
	D3D10_BUFFER_DESC indexBufferDesc;
	memset(&indexBufferDesc, 0, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = sizeof(VERTEX_INDICES);
	indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
//	indexBufferDesc.CPUAccessFlags = 0;
//	indexBufferDesc.MiscFlags = 0;

	D3D10_SUBRESOURCE_DATA indexData;
	memset(&indexData, 0, sizeof(indexData));
	vertexData.pSysMem = VERTEX_INDICES;

	hR = m_pDevice->CreateBuffer(&indexBufferDesc, &vertexData, m_pVertexIndexBuffer.inref());
	if(FAILED(hR)) return hR;

	D3D10_TEXTURE1D_DESC waterfallDesc;
	memset(&waterfallDesc, 0, sizeof(waterfallDesc));
	waterfallDesc.Width = 256;
	waterfallDesc.MipLevels = 1;
	waterfallDesc.ArraySize = 1;
	waterfallDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	waterfallDesc.Usage = D3D10_USAGE_IMMUTABLE;
	waterfallDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	waterfallDesc.CPUAccessFlags = 0;

	D3DXVECTOR3 waterfallColors[256];
	for(int i=0; i < 256; i++)
	{
		int slice = i >> 5;
		float distto = (i % 32) / 32.0f;
		float distfrom = 1.0f - distto;
		const D3DVECTOR& from = WATERFALL_POINTS[slice];
		const D3DVECTOR& to = WATERFALL_POINTS[slice=1];
		if(from.x == 0.0f && distto != 0.0f)
		{
			waterfallColors[i] = hsv2rgb(D3DXVECTOR3(to.x, from.y*distfrom + to.y*distto, from.z*distfrom + to.z*distto));
		}
		else if(to.x == 0.0f && distfrom != 0.0f)
		{
			waterfallColors[i] = hsv2rgb(D3DXVECTOR3(from.x, from.y*distfrom + to.y*distto, from.z*distfrom + to.z*distto));
		}
		else
		{
			waterfallColors[i] = hsv2rgb(D3DXVECTOR3(from.x*distfrom + to.x*distto, from.y*distfrom + to.y*distto, from.z*distfrom + to.z*distto));
		}
	}
	waterfallColors[255] = D3DXVECTOR3(1, 0, 1);
	waterfallColors[0] = D3DXVECTOR3(0, 1, 0);

	D3D10_SUBRESOURCE_DATA waterfallData;
	memset(&waterfallColors, 0, sizeof(waterfallColors));
	waterfallData.pSysMem = waterfallColors;

	ID3D10Texture1DPtr waterfallTex;
	hR = m_pDevice->CreateTexture1D(&waterfallDesc, &waterfallData, waterfallTex.inref());
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(waterfallTex, NULL, m_waterfallView.inref());
	if(FAILED(hR)) return hR;

	ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("waterfallColors");
	if(!pVar) return E_FAIL;
	ID3D10EffectShaderResourceVariable* pVarRes = pVar->AsShaderResource();
	if(!pVarRes || !pVarRes->IsValid()) return E_FAIL;
	hR = pVarRes->SetResource(m_waterfallView);
	if(FAILED(hR)) return hR;

	m_dataTexWidth = 1024;
	m_dataTexHeight = 512;
	m_dataTexData = new float[m_dataTexWidth*m_dataTexHeight];
	memset(m_dataTexData, 0, sizeof(float)*m_dataTexWidth*m_dataTexHeight);

	D3D10_TEXTURE2D_DESC dataDesc;
	memset(&dataDesc, 0, sizeof(dataDesc));
	dataDesc.Width = m_dataTexWidth;
	dataDesc.Height = m_dataTexHeight;
	dataDesc.MipLevels = 1;
	dataDesc.ArraySize = 1;
	dataDesc.Format = DXGI_FORMAT_R32_FLOAT;
	dataDesc.SampleDesc.Count = 1;
	dataDesc.SampleDesc.Quality = 0;
	dataDesc.Usage = D3D10_USAGE_DYNAMIC;
	dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

	hR = m_pDevice->CreateTexture2D(&dataDesc, NULL, m_dataTex.inref());
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView.inref());
	if(FAILED(hR)) return hR;

	pVar = m_pEffect->GetVariableByName("waterfallValues");
	if(!pVar) return E_FAIL;
	pVarRes = pVar->AsShaderResource();
	if(!pVarRes || !pVarRes->IsValid()) return E_FAIL;
	hR = pVarRes->SetResource(m_dataView);
	if(FAILED(hR)) return hR;

	// Create an orthographic projection matrix for 2D rendering.
	D3DXMATRIX orthoMatrix;
	D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenWidth, (float)m_screenHeight, 0.0f, 1.0f);

	pVar = m_pEffect->GetVariableByName("orthoMatrix");
	if(!pVar) return E_FAIL;
	ID3D10EffectMatrixVariable* pVarMat = pVar->AsMatrix();
	if(!pVarMat || !pVarMat->IsValid()) return E_FAIL;
	hR = pVarMat->SetMatrix(orthoMatrix);
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT D3Dtest::init(HMODULE hModule, HWND hOutputWin)
{
	m_hModule = hModule;
	m_hOutputWin = hOutputWin;

	HRESULT hR = initDevice();
	if(FAILED(hR)) return hR;

	hR = initTexture();
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT D3Dtest::bumpTex()
{
	// shift the data up one row
	memmove(m_dataTexData, m_dataTexData+m_dataTexWidth, sizeof(float)*m_dataTexWidth*(m_dataTexHeight-1));

	// write a new bottom line
	float* newLine = m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
	for(unsigned i = 0; i < m_dataTexWidth; i++)
	{
		newLine[i] = float(i) / m_dataTexWidth;
	}
	return S_OK;
}

HRESULT D3Dtest::renderCycle()
{
	// bump the texture
	HRESULT hR = bumpTex();
	if(FAILED(hR)) return hR;

	D3D10_MAPPED_TEXTURE2D mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex.pData, m_dataTexData, sizeof(float)*m_dataTexWidth*m_dataTexHeight);
	m_dataTex->Unmap(subr);

	// setup the input assembler.
	UINT stride = sizeof(TLVERTEX); 
	UINT offset = 0;
	ID3D10Buffer* buf = m_pVertexBuffer;
	m_pDevice->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pDevice->IASetInputLayout(m_pInputLayout);

	// Get the description structure of the technique from inside the shader so it can be used for rendering.
	D3D10_TECHNIQUE_DESC techniqueDesc;
	memset(&techniqueDesc, 0, sizeof(techniqueDesc));
	hR = m_pTechnique->GetDesc(&techniqueDesc);
	if(FAILED(hR)) return hR;

	// Go through each pass in the technique (should be just one currently) and render the triangles.
	for(UINT i=0; i<techniqueDesc.Passes; ++i)
	{
		hR = m_pTechnique->GetPassByIndex(i)->Apply(0);
		if(FAILED(hR)) return hR;

		m_pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	}

	hR = m_pSwapChain->Present(m_bEnableVsync ? 1 : 0, 0);
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
*/