// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waterfall.h"

#include <d3d10.h>
#include <d3dx10async.h>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;

HMODULE gl_DllModule;

const char* CDirectxWaterfallDriver::NAME = "waterfall";
const char* CDirectxWaterfallDriver::DESCR = "DirectX waterfall display";
const unsigned char CDirectxWaterfallDriver::FINGERPRINT[] = { 1, signals::etypVecDouble, 0 };
char const * CDirectxWaterfall::NAME = "DirectX waterfall display";
const char* CDirectxWaterfall::CIncoming::EP_NAME = "in";
const char* CDirectxWaterfall::CIncoming::EP_DESCR = "Waterfall incoming endpoint";
const unsigned long CDirectxWaterfall::VERTEX_INDICES[4] = { 2, 0, 3, 1 };
static const float PI = std::atan(1.0f)*4;

struct TLVERTEX
{
	D3DXVECTOR3 pos;
	D3DXVECTOR2 tex;
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
		gl_DllModule = hModule;
		break;
//	case DLL_THREAD_DETACH:
//	case DLL_PROCESS_DETACH:
//		break;
	}
	return TRUE;
}

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	static CDirectxWaterfallDriver DRIVER_waterfall;
	if(drivers && availDrivers)
	{
		drivers[0] = &DRIVER_waterfall;
	}
	return 1;
}

// ---------------------------------------------------------------------------- class CDirectxWaterfallDriver

signals::IBlock* CDirectxWaterfallDriver::Create()
{
	signals::IBlock* blk = new CDirectxWaterfall(this);
	blk->AddRef();
	return blk;
}

// ---------------------------------------------------------------------------- class CAttr_target_hwnd

class CAttr_target_hwnd : public CRWAttribute<signals::etypWinHdl>
{
private:
	typedef CRWAttribute<signals::etypWinHdl> base;
public:
	inline CAttr_target_hwnd(CDirectxWaterfall& parent, const char* name, const char* descr)
		:base(name, descr, NULL), m_parent(parent)
	{
	}

protected:
	CDirectxWaterfall& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setTargetWindow((HWND)newVal);
		base::onSetValue(newVal);
	}
};

class CAttr_height : public CRWAttribute<signals::etypShort>
{
private:
	typedef CRWAttribute<signals::etypShort> base;
public:
	inline CAttr_height(CDirectxWaterfall& parent, const char* name, const char* descr, store_type deflt)
		:base(name, descr, deflt), m_parent(parent)
	{
	}

protected:
	CDirectxWaterfall& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setHeight(newVal);
		base::onSetValue(newVal);
	}
};

// ---------------------------------------------------------------------------- DirectX utilities

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

static std::pair<float,float> polar2rect(float t, float r)
{
	return std::pair<float,float>(r*cos(t*PI/3.0f), r*sin(t*PI/3.0f));
}

static std::pair<float,float> rect2polar(float x, float y)
{
	if(x != 0.0f)
	{
		std::pair<float,float> ret(atan2(y, x) * 3.0f/PI, sqrt(x*x+y*y));
		if(ret.first < 0) ret.first += 6.0f;
		return ret;
	} else {
		return std::pair<float,float>(y == 0.0f ? 0.0f : y > 0 ? 1.5f : 4.5f, abs(y));
	}
}

// ---------------------------------------------------------------------------- class CDirectxWaterfall

#pragma warning(push)
#pragma warning(disable: 4355)
CDirectxWaterfall::CDirectxWaterfall(signals::IBlockDriver* driver):m_driver(driver),m_incoming(this),m_bDataThreadEnabled(true),
	 m_dataThread(Thread<CDirectxWaterfall*>::delegate_type(&CDirectxWaterfall::process_thread)),m_bufSize(0),
	 m_hOutputWin(NULL),m_screenCliWidth(0),m_screenCliHeight(0),m_screenWinWidth(0),
	 m_screenWinHeight(0),m_dataTexWidth(0),m_dataTexHeight(0),m_dataTexData(NULL),m_pTechnique(NULL),m_pOldWinProc(NULL),
	 m_pWinprocDelgate(this, &CDirectxWaterfall::WindowProc)
{
	buildAttrs();
	m_dataThread.launch(this, THREAD_PRIORITY_NORMAL);
}
#pragma warning(pop)

CDirectxWaterfall::~CDirectxWaterfall()
{
	releaseDevice();
	if(m_dataTexData)
	{
		delete [] m_dataTexData;
		m_dataTexData = NULL;
	}
}

unsigned CDirectxWaterfall::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_incoming;
	}
	return 1;
}

void CDirectxWaterfall::process_thread(CDirectxWaterfall* owner)
{
	ThreadBase::SetThreadName("Waterfall Display Thread");

	std::vector<double> buffer;
	while(owner->m_bDataThreadEnabled)
	{
		{
			Locker lock(owner->m_buffLock);
			if(owner->m_bufSize > buffer.capacity()) buffer.resize(owner->m_bufSize);
		}
		if(!buffer.capacity()) buffer.resize(DEFAULT_CAPACITY);
		unsigned recvCount = owner->m_incoming.Read(signals::etypCmplDbl, &buffer,
			buffer.capacity(), FALSE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			Locker lock(owner->m_buffLock);
			unsigned bufSize = owner->m_bufSize;
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

void CDirectxWaterfall::buildAttrs()
{
	attrs.targetWindow = addLocalAttr(true, new CAttr_target_hwnd(*this, "targetWindow", "Window to display waterfall on"));
	attrs.enableVsync = addLocalAttr(true, new CRWAttribute<signals::etypBoolean>("enableVsync", "Use monitor vsync on display", false));
	attrs.height = addLocalAttr(true, new CAttr_height(*this, "height", "Rows of history to display", 512));
}

void CDirectxWaterfall::releaseDevice()
{
	if(m_hOutputWin != NULL)
	{
		LONG_PTR pCurWnd = GetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC);
		if((void*)pCurWnd == &m_pWinprocDelgate)
		{
			SetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC, (LONG)m_pOldWinProc);
		}
		m_hOutputWin = NULL;
	}
	if(m_pDevice) m_pDevice->ClearState();
	m_pSwapChain.Release();
	m_dataTex.Release();
	m_pEffect.Release();
	m_pDepthView.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_pDepthStencilState.Release();
	m_pRenderTargetView.Release();
	m_waterfallView.Release();
	m_dataView.Release();
	m_pDevice.Release();
}

void CDirectxWaterfall::setTargetWindow(HWND hWnd)
{
	releaseDevice();
	m_hOutputWin = hWnd;
	if(hWnd != NULL && hWnd != INVALID_HANDLE_VALUE)
	{
		m_pOldWinProc = (WNDPROC)GetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC);
		if(!m_pOldWinProc) ThrowLastError(GetLastError());
		SetWindowLongPtr(m_hOutputWin, GWLP_WNDPROC, (LONG)&m_pWinprocDelgate);

		HRESULT hR = initDevice();
		if(SUCCEEDED(hR)) hR = initTexture();
		if(FAILED(hR)) ThrowLastError(hR);
	}
}

HRESULT CDirectxWaterfall::initDevice()
{
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

HRESULT CDirectxWaterfall::buildWaterfallTexture(ID3D10DevicePtr pDevice, ID3D10Texture1DPtr& waterfallTex)
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

	D3D10_TEXTURE1D_DESC waterfallDesc;
	memset(&waterfallDesc, 0, sizeof(waterfallDesc));
	waterfallDesc.Width = 256;
	waterfallDesc.MipLevels = 1;
	waterfallDesc.ArraySize = 1;
	waterfallDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	waterfallDesc.Usage = D3D10_USAGE_IMMUTABLE;
	waterfallDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	waterfallDesc.CPUAccessFlags = 0;

#pragma pack(push,1)
	struct colors_t {
		byte x;
		byte y;
		byte z;
		byte a;
		inline colors_t() {}
		inline colors_t(byte pX, byte pY, byte pZ):x(pX),y(pY),z(pZ),a(0) {}
	};
	colors_t waterfallColors[256];
#pragma pack(pop)

	for(int slice = 0; slice < 8; slice++)
	{
		const D3DVECTOR& from = WATERFALL_POINTS[slice];
		const D3DVECTOR& to = WATERFALL_POINTS[slice+1];

		std::pair<float,float> fromRect = polar2rect(from.x, from.y);
		std::pair<float,float> toRect = polar2rect(to.x, to.y);
		D3DXVECTOR3 diffRect(toRect.first - fromRect.first, toRect.second - fromRect.second, to.z - from.z);

		for(int off=0; off < 32; off++)
		{
			int i = slice * 32 + off;
			float distto = off / 32.0f;

			D3DXVECTOR3 newRect(fromRect.first + diffRect.x * distto,
				fromRect.second + diffRect.y * distto,
				from.z + diffRect.z * distto);
			std::pair<float,float> newPolar = rect2polar(newRect.x, newRect.y);

			D3DXVECTOR3 newRGB = hsv2rgb(D3DXVECTOR3(newPolar.first, newPolar.second, newRect.z));
			waterfallColors[i] = colors_t(byte(newRGB.x * 255), byte(newRGB.y * 255), byte(newRGB.z * 255));
		}
	}
	waterfallColors[255] = colors_t(255, 0, 255);

	D3D10_SUBRESOURCE_DATA waterfallData;
	memset(&waterfallData, 0, sizeof(waterfallData));
	waterfallData.pSysMem = waterfallColors;

	HRESULT hR = pDevice->CreateTexture1D(&waterfallDesc, &waterfallData, waterfallTex.inref());
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT CDirectxWaterfall::initTexture()
{
	// Load the shader in from the file.
	static LPCTSTR filename = _T("waterfall.fx");
	ID3D10BlobPtr errorMessage;
#ifdef _DEBUG
	enum { DX_FLAGS = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG };
#else
	enum { DX_FLAGS = D3D10_SHADER_ENABLE_STRICTNESS };
#endif
	HRESULT hR = D3DX10CreateEffectFromResource(gl_DllModule, filename, filename, NULL, NULL,
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
	float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
	float texRight = texLeft + (float)m_screenCliWidth;
	float texTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
	float texBottom = texTop - (float)m_screenCliHeight;

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
	vertexBufferDesc.Usage = D3D10_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(vertices);
	vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
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

	ID3D10Texture1DPtr waterfallTex;
	hR = buildWaterfallTexture(m_pDevice, waterfallTex);
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(waterfallTex, NULL, m_waterfallView.inref());
	if(FAILED(hR)) return hR;

	ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("waterfallColors");
	if(!pVar) return E_FAIL;
	ID3D10EffectShaderResourceVariable* pVarRes = pVar->AsShaderResource();
	if(!pVarRes || !pVarRes->IsValid()) return E_FAIL;
	hR = pVarRes->SetResource(m_waterfallView);
	if(FAILED(hR)) return hR;

	// Create an orthographic projection matrix for 2D rendering.
	D3DXMATRIX orthoMatrix;
	D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

	pVar = m_pEffect->GetVariableByName("orthoMatrix");
	if(!pVar) return E_FAIL;
	ID3D10EffectMatrixVariable* pVarMat = pVar->AsMatrix();
	if(!pVarMat || !pVarMat->IsValid()) return E_FAIL;
	hR = pVarMat->SetMatrix(orthoMatrix);
	if(FAILED(hR)) return hR;

	return initDataTexture();
}

HRESULT CDirectxWaterfall::initDataTexture()
{
	if(!m_dataTexWidth || !m_dataTexHeight)
	{
		return S_FALSE;
	}
	m_dataView.Release();
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;

//	m_dataTexWidth = 1024;
//	m_dataTexHeight = 512;
	m_dataTexData = new dataTex_t[m_dataTexWidth*m_dataTexHeight];
	memset(m_dataTexData, 0, sizeof(dataTex_t)*m_dataTexWidth*m_dataTexHeight);

	D3D10_TEXTURE2D_DESC dataDesc;
	memset(&dataDesc, 0, sizeof(dataDesc));
	dataDesc.Width = m_dataTexWidth;
	dataDesc.Height = m_dataTexHeight;
	dataDesc.MipLevels = 1;
	dataDesc.ArraySize = 1;
	dataDesc.Format = DXGI_FORMAT_R16_SNORM;
	dataDesc.SampleDesc.Count = 1;
	dataDesc.SampleDesc.Quality = 0;
	dataDesc.Usage = D3D10_USAGE_DYNAMIC;
	dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

	HRESULT hR = m_pDevice->CreateTexture2D(&dataDesc, NULL, m_dataTex.inref());
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView.inref());
	if(FAILED(hR)) return hR;

	ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("waterfallValues");
	if(!pVar) return E_FAIL;
	ID3D10EffectShaderResourceVariable* pVarRes = pVar->AsShaderResource();
	if(!pVarRes || !pVarRes->IsValid()) return E_FAIL;
	hR = pVarRes->SetResource(m_dataView);
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT CDirectxWaterfall::resizeDevice()
{
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

	if(m_pVertexBuffer)
	{
		// calculate texture coordinates
		float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
		float texRight = texLeft + (float)m_screenCliWidth;
		float texTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
		float texBottom = texTop - (float)m_screenCliHeight;

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

		m_pDevice->UpdateSubresource(m_pVertexBuffer, 0, NULL, vertices, sizeof(vertices), sizeof(vertices));
	}

	if(m_pEffect)
	{
		// Create an orthographic projection matrix for 2D rendering.
		D3DXMATRIX orthoMatrix;
		D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

		ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("orthoMatrix");
		if(!pVar) return E_FAIL;
		ID3D10EffectMatrixVariable* pVarMat = pVar->AsMatrix();
		if(!pVarMat || !pVarMat->IsValid()) return E_FAIL;
		hR = pVarMat->SetMatrix(orthoMatrix);
		if(FAILED(hR)) return hR;
	}

	return S_OK;
}

HRESULT CDirectxWaterfall::drawFrame()
{
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pTechnique || !m_pSwapChain)
	{
		return E_POINTER;
	}

	D3D10_MAPPED_TEXTURE2D mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex.pData, m_dataTexData, sizeof(dataTex_t)*m_dataTexWidth*m_dataTexHeight);
	m_dataTex->Unmap(subr);

	m_pDevice->ClearDepthStencilView(m_pDepthView, D3D10_CLEAR_DEPTH, 1.0f, 0);

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

	hR = m_pSwapChain->Present(attrs.enableVsync && attrs.enableVsync->nativeGetValue() ? 1 : 0, 0);
	if(FAILED(hR)) return hR;

	return S_OK;
}

LRESULT CDirectxWaterfall::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
					m_screenWinWidth = LOWORD(lParam);
					m_screenWinHeight = HIWORD(lParam);
					VERIFY(SUCCEEDED(resizeDevice()));
				}
				break;
			}
			break;
		case WM_WINDOWPOSCHANGED:
			{
				LPWINDOWPOS winPos = (LPWINDOWPOS)lParam;
				if((winPos->flags & SWP_NOSIZE) == 0 && (winPos->cx != m_screenWinWidth || winPos->cy != m_screenWinHeight))
				{
					m_screenWinWidth = winPos->cx;
					m_screenWinHeight = winPos->cy;
					VERIFY(SUCCEEDED(resizeDevice()));
				}
				break;
			}
		}
	}
	return CallWindowProc(m_pOldWinProc, hWnd, uMsg, wParam, lParam);
}

void CDirectxWaterfall::setHeight(short newHeight)
{
	if(newHeight == m_dataTexHeight || newHeight <= 0) return;
	m_dataTexHeight = newHeight;
	initDataTexture();
}

CDirectxWaterfall::CIncoming::~CIncoming()
{
	if(m_lastWidthAttr)
	{
		m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
}

void CDirectxWaterfall::CIncoming::OnConnection(signals::IEPRecvFrom *conn)
{
	if(m_lastWidthAttr)
	{
		m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
	if(!conn) return;
	signals::IAttributes* attrs = conn->InputAttributes();
	if(!attrs) return;
	signals::IAttribute* attr = attrs->GetByName("blockSize");
	if(!attr || attr->Type() != signals::etypShort) return;
	m_lastWidthAttr = attr;
	m_lastWidthAttr->Observe(this);
	OnChanged(attr->Name(), attr->Type(), attr->getValue());
}

void CDirectxWaterfall::CIncoming::OnChanged(const char* name, signals::EType type, const void* value)
{
	if(_stricmp(name, "blockSize") == 0 && type == signals::etypShort)
	{
		short width = *(short*)value;
		if(width != m_parent->m_dataTexWidth && width > 0)
		{
			m_parent->m_dataTexWidth = width;
			m_parent->initDataTexture();
		}
	}
}

void CDirectxWaterfall::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_dataTexWidth)
	{
		m_dataTexWidth = size;
		initDataTexture();
	}

	// shift the data up one row
	memmove(m_dataTexData, m_dataTexData+m_dataTexWidth, sizeof(dataTex_t)*m_dataTexWidth*(m_dataTexHeight-1));

	// write a new bottom line
	dataTex_t* newLine = m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
	for(unsigned i = 0; i < m_dataTexWidth; i++)
	{
		newLine[i] = dataTex_t(SHRT_MAX * frame[i]);
	}

	VERIFY(drawFrame());
}
