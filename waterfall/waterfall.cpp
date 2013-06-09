// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waterfall.h"

#include <d3d10.h>
#include <d3dx10async.h>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

typedef unk_ref_t<ID3D10Blob> ID3D10BlobPtr;

extern HMODULE gl_DllModule;

char const * CDirectxWaterfall::NAME = "DirectX waterfall display";
const unsigned long CDirectxWaterfall::VERTEX_INDICES[4] = { 2, 0, 3, 1 };

static const float PI = std::atan(1.0f)*4;

// ---------------------------------------------------------------------------- class CAttr_height

class CAttr_height : public CRWAttribute<signals::etypShort>
{
private:
	typedef CRWAttribute<signals::etypShort> base;
public:
	inline CAttr_height(CDirectxWaterfall& parent, const char* name, const char* descr, store_type deflt)
		:base(name, descr, deflt), m_parent(parent)
	{
		m_parent.setHeight(this->nativeGetValue());
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

CDirectxWaterfall::CDirectxWaterfall(signals::IBlockDriver* driver):CDirectxBase(driver),
	m_dataTexHeight(0),m_dataTexData(NULL),m_pTechnique(NULL),m_floatStaging(NULL)
{
	buildAttrs();
}

CDirectxWaterfall::~CDirectxWaterfall()
{
	Locker lock(m_refLock);
	releaseDevice();
	if(m_dataTexData)
	{
		delete [] m_dataTexData;
		m_dataTexData = NULL;
	}
	if(m_floatStaging)
	{
		delete [] m_floatStaging;
		m_floatStaging = NULL;
	}
}

struct TL_FALL_VERTEX
{
	D3DXVECTOR3 pos;
	D3DXVECTOR2 tex;
};

void CDirectxWaterfall::buildAttrs()
{
	CDirectxBase::buildAttrs();
	attrs.height = addLocalAttr(true, new CAttr_height(*this, "height", "Rows of history to display", 512));
}

void CDirectxWaterfall::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxBase::releaseDevice();
	m_dataTex.Release();
	m_pEffect.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_dataView.Release();
	m_waterfallView.Release();
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
	// ASSUMES m_refLock IS HELD BY CALLER
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
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TL_FALL_VERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TL_FALL_VERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	hR = m_pDevice->CreateInputLayout(vdesc, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, m_pInputLayout.inref());
	if(FAILED(hR)) return hR;

	// calculate texture coordinates
	float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
	float texRight = texLeft + (float)m_screenCliWidth;
	float texTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
	float texBottom = texTop - (float)m_screenCliHeight;

	// Now create the vertex buffer.
	TL_FALL_VERTEX vertices[4];
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
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_dataTexWidth || !m_dataTexHeight || !m_pDevice)
	{
		return S_FALSE;
	}
	m_dataView.Release();
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;
	if(m_floatStaging) delete [] m_floatStaging;

//	m_dataTexWidth = 1024;
//	m_dataTexHeight = 512;
	m_dataTexData = new dataTex_t[m_dataTexWidth*m_dataTexHeight];
	m_floatStaging = new float[m_dataTexWidth];
	memset(m_dataTexData, 0, sizeof(dataTex_t)*m_dataTexWidth*m_dataTexHeight);

	D3D10_TEXTURE2D_DESC dataDesc;
	memset(&dataDesc, 0, sizeof(dataDesc));
	dataDesc.Width = m_dataTexWidth;
	dataDesc.Height = m_dataTexHeight;
	dataDesc.MipLevels = 1;
	dataDesc.ArraySize = 1;
	dataDesc.Format = DXGI_FORMAT_R16_FLOAT;
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
	// ASSUMES m_refLock IS HELD BY CALLER
	HRESULT hR = CDirectxBase::resizeDevice();
	if(hR != S_OK) return hR;

	if(m_pVertexBuffer)
	{
		// calculate texture coordinates
		float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
		float texRight = texLeft + (float)m_screenCliWidth;
		float texTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
		float texBottom = texTop - (float)m_screenCliHeight;

		// Now create the vertex buffer.
		TL_FALL_VERTEX vertices[4];
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

HRESULT CDirectxWaterfall::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pTechnique)
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

	// setup the input assembler.
	UINT stride = sizeof(TL_FALL_VERTEX); 
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

	return S_OK;
}

void CDirectxWaterfall::setHeight(short newHeight)
{
	if(newHeight == m_dataTexHeight || newHeight <= 0) return;
	Locker lock(m_refLock);
	m_dataTexHeight = newHeight;
	initDataTexture();
}

void CDirectxWaterfall::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_dataTexWidth)
	{
		if(size && size != m_dataTexWidth)
		{
			m_dataTexWidth = size;
			initDataTexture();
		}
	}

	// shift the data up one row
	memmove(m_dataTexData, m_dataTexData+m_dataTexWidth, sizeof(dataTex_t)*m_dataTexWidth*(m_dataTexHeight-1));

	// convert to float
	for(unsigned i = 0; i < m_dataTexWidth; i++) m_floatStaging[i] = float(frame[i]);

	// write a new bottom line
	dataTex_t* newLine = m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
	D3DXFloat32To16Array((D3DXFLOAT16*)(newLine), m_floatStaging, m_dataTexWidth);
	VERIFY(SUCCEEDED(drawFrame()));
}
