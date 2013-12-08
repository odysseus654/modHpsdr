// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "scope.h"

#include <d3d10_1.h>
#include <d3dx10.h>

struct TL_FALL_VERTEX
{
	D3DXVECTOR3 pos;
	D3DXVECTOR2 tex;
};

const unsigned short CDirectxScope::VERTEX_INDICES[4] = { 2, 0, 3, 1 };

// ---------------------------------------------------------------------------- class CDirectxWaterfall

CDirectxScope::CDirectxScope(signals::IBlockDriver* driver, bool bBottomOrigin):CDirectxBase(driver),
	m_bIsComplexInput(true),m_bBottomOrigin(bBottomOrigin),
	m_majFont(20, 0, FW_NORMAL, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Courier New")),
	m_dotFont(15, 0, FW_NORMAL, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Courier New")),
	m_minFont(20, 0, FW_NORMAL, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Courier New"))
{
}

CDirectxScope::~CDirectxScope()
{
	Locker lock(m_refLock);
	releaseDevice();
}

void CDirectxScope::buildAttrs()
{
	CDirectxBase::buildAttrs();
	attrs.isComplexInput = addLocalAttr(true, new CAttr_callback<signals::etypBoolean,CDirectxScope>
		(*this, "isComplexInput", "Input is based on complex data", &CDirectxScope::setIsComplexInput, true));
}

void CDirectxScope::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxBase::releaseDevice();
	m_pVS.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_pVSGlobals.Release();
	m_pBlendState.Release();
	m_pBitmapPS.Release();
	m_pBitmapSampler.Release();
}

HRESULT CDirectxScope::createVertexRect(float top, float left, float bottom, float right, float z,
	bool bBottomOrigin, ID3D10BufferPtr& vertex) const
{
	float texTop = bBottomOrigin ? 1.0f : 0.0f;
	float texBottom = bBottomOrigin ? 0.0f : 1.0f;

	TL_FALL_VERTEX vertices[4];
	vertices[0].pos = D3DXVECTOR3(left, top, z);
	vertices[0].tex = D3DXVECTOR2(0.0f, texTop);
	vertices[1].pos = D3DXVECTOR3(right, top, z);
	vertices[1].tex = D3DXVECTOR2(1.0f, texTop);
	vertices[2].pos = D3DXVECTOR3(left, bottom, z);
	vertices[2].tex = D3DXVECTOR2(0.0f, texBottom);
	vertices[3].pos = D3DXVECTOR3(right, bottom, z);
	vertices[3].tex = D3DXVECTOR2(1.0f, texBottom);

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

	return m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, vertex.inref());
}

HRESULT CDirectxScope::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER

	const static D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TL_FALL_VERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TL_FALL_VERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	// Load the shader in from the file.
	HRESULT hR = createVertexShaderFromResource(_T("ortho_vs.cso"), vdesc, 2, m_pVS, m_pInputLayout);
	if(FAILED(hR)) return hR;

	hR = createPixelShaderFromResource(_T("mono_bitmap_ps.cso"), m_pBitmapPS);
	if(FAILED(hR)) return hR;

	// Now create the vertex buffer.
	hR = createVertexRect(0, 0, (float)m_screenCliHeight, (float)m_screenCliWidth, 0.75f, m_bBottomOrigin, m_pVertexBuffer);

	// Now create the index buffer.
	{
		D3D10_BUFFER_DESC indexBufferDesc;
		memset(&indexBufferDesc, 0, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(VERTEX_INDICES);
		indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
	//	indexBufferDesc.CPUAccessFlags = 0;
	//	indexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA indexData;
		memset(&indexData, 0, sizeof(indexData));
		indexData.pSysMem = VERTEX_INDICES;

		hR = m_pDevice->CreateBuffer(&indexBufferDesc, &indexData, m_pVertexIndexBuffer.inref());
		if(FAILED(hR)) return hR;
	}

	{
		D3D10_BLEND_DESC BlendState;
		memset(&BlendState, 0, sizeof(BlendState));
		BlendState.BlendEnable[0] = TRUE;
		BlendState.SrcBlend = D3D10_BLEND_SRC_ALPHA;
		BlendState.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
		BlendState.BlendOp = D3D10_BLEND_OP_ADD;
		BlendState.SrcBlendAlpha = D3D10_BLEND_ZERO;
		BlendState.DestBlendAlpha = D3D10_BLEND_ZERO;
		BlendState.BlendOpAlpha = D3D10_BLEND_OP_ADD;
		BlendState.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
 
		hR = m_pDevice->CreateBlendState(&BlendState, m_pBlendState.inref());
		if(FAILED(hR)) return hR;
	}

	{
		D3D10_SAMPLER_DESC samplerDesc;
		memset(&samplerDesc, 0, sizeof(samplerDesc));
		samplerDesc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D10_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D10_FLOAT32_MAX;

		hR = m_pDevice->CreateSamplerState(&samplerDesc, m_pBitmapSampler.inref());
		if(FAILED(hR)) return hR;
	}

	// Create an orthographic projection matrix for 2D rendering.
	D3DXMATRIX orthoMatrix(
		2.0f/m_screenCliWidth,	0,		0,					-1,
		0,				-2.0f/m_screenCliHeight,	0,		+1,
		0,				0,				1,					0,
		0,				0,				0,					1);

	{
		D3D10_BUFFER_DESC vsGlobalBufferDesc;
		memset(&vsGlobalBufferDesc, 0, sizeof(vsGlobalBufferDesc));
		vsGlobalBufferDesc.Usage = D3D10_USAGE_DEFAULT;
		vsGlobalBufferDesc.ByteWidth = sizeof(orthoMatrix);
		vsGlobalBufferDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	//	vsGlobalBufferDesc.CPUAccessFlags = 0;
	//	vsGlobalBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA vsGlobalData;
		memset(&vsGlobalData, 0, sizeof(vsGlobalData));
		vsGlobalData.pSysMem = orthoMatrix;

		hR = m_pDevice->CreateBuffer(&vsGlobalBufferDesc, &vsGlobalData, m_pVSGlobals.inref());
		if(FAILED(hR)) return hR;
	}

	return initDataTexture();
}

void CDirectxScope::setIsComplexInput(const unsigned char& bComplex)
{
	if(!!bComplex != m_bIsComplexInput)
	{
		Locker lock(m_refLock);
		if(!!bComplex != m_bIsComplexInput)
		{
			m_bIsComplexInput = !!bComplex;
			initDataTexture();
		}
	}
}

HRESULT CDirectxScope::resizeDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	HRESULT hR = CDirectxBase::resizeDevice();
	if(hR != S_OK) return hR;

	if(m_pVertexBuffer)
	{
		// calculate texture coordinates
		float texTop = m_bBottomOrigin ? 1.0f : 0.0f;
		float texBottom = m_bBottomOrigin ? 0.0f : 1.0f;

		// Now create the vertex buffer.
		TL_FALL_VERTEX vertices[4];
		vertices[0].pos = D3DXVECTOR3(0.0f, (float)m_screenCliHeight, 0.75f);
		vertices[0].tex = D3DXVECTOR2(0.0f, texTop);
		vertices[1].pos = D3DXVECTOR3((float)m_screenCliWidth, (float)m_screenCliHeight, 0.75f);
		vertices[1].tex = D3DXVECTOR2(1.0f, texTop);
		vertices[2].pos = D3DXVECTOR3(0.0f, 0.0f, 0.75f);
		vertices[2].tex = D3DXVECTOR2(0.0f, texBottom);
		vertices[3].pos = D3DXVECTOR3((float)m_screenCliWidth, 0.0f, 0.75f);
		vertices[3].tex = D3DXVECTOR2(1.0f, texBottom);

		m_pDevice->UpdateSubresource(m_pVertexBuffer, 0, NULL, vertices, sizeof(vertices), sizeof(vertices));
	}

	if(m_pDevice && m_pVSGlobals)
	{
		// Create an orthographic projection matrix for 2D rendering.
		D3DXMATRIX orthoMatrix(
			2.0f/m_screenCliWidth,	0,		0,					-1,
			0,				-2.0f/m_screenCliHeight,	0,		+1,
			0,				0,				1,					0,
			0,				0,				0,					1);
		m_pDevice->UpdateSubresource(m_pVSGlobals, 0, NULL, &orthoMatrix, sizeof(orthoMatrix), sizeof(orthoMatrix));
	}

	return S_OK;
}

HRESULT CDirectxScope::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_pVS)
	{
		return E_POINTER;
	}

	HRESULT hR = preDrawFrame();
	if(FAILED(hR)) return hR;

	// setup the input assembler.
	const static UINT stride = sizeof(TL_FALL_VERTEX); 
	const static UINT offset = 0;
	m_pDevice->IASetVertexBuffers(0, 1, m_pVertexBuffer.ref(), &stride, &offset);
	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pDevice->IASetInputLayout(m_pInputLayout);

	// Build our vertex shader
	m_pDevice->VSSetShader(m_pVS);
	m_pDevice->VSSetConstantBuffers(0, 1, m_pVSGlobals.ref());

	// render the frame
	ID3D10BlendState* pOrigBlendState;
	float origBlendFactor[4];
	UINT origBlendMask;
	m_pDevice->OMGetBlendState(&pOrigBlendState, origBlendFactor, &origBlendMask);
	m_pDevice->OMSetBlendState(m_pBlendState, NULL, MAXUINT16);

	m_bDrawingFonts = false;
	m_majFont.BeginFrame();
	m_dotFont.BeginFrame();
	m_minFont.BeginFrame();

	hR = drawRect();
	if(FAILED(hR)) return hR;

	hR = drawText();
	if(FAILED(hR)) return hR;

	m_pDevice->OMSetBlendState(pOrigBlendState, origBlendFactor, origBlendMask);
	m_majFont.EndFrame();
	m_dotFont.EndFrame();
	m_minFont.EndFrame();

	return S_OK;
}

HRESULT CDirectxScope::drawMonoBitmap(const ID3D10ShaderResourceViewPtr &image, const ID3D10BufferPtr& vertex,
	const ID3D10BufferPtr& color)
{
	// setup the input assembler.
	const static UINT stride = sizeof(TL_FALL_VERTEX); 
	const static UINT offset = 0;
	m_pDevice->IASetVertexBuffers(0, 1, vertex.ref(), &stride, &offset);

	if(!m_bDrawingFonts)
	{
	//	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	//	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//	m_pDevice->IASetInputLayout(m_pInputLayout);

		// Build our vertex shader
	//	m_pDevice->VSSetShader(m_pVS);
	//	m_pDevice->VSSetConstantBuffers(0, 1, m_pVSGlobals.ref());

		// Build the pixel shader
		m_pDevice->PSSetShader(m_pBitmapPS);
		m_pDevice->PSSetSamplers(2, 1, m_pBitmapSampler.ref());

		m_bDrawingFonts = true;
	}

	m_pDevice->PSSetShaderResources(2, 1, image.ref());
	m_pDevice->PSSetConstantBuffers(2, 1, color.ref());
	m_pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	return S_OK;
}

HRESULT CDirectxScope::drawRect()
{
	m_pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	return S_OK;
}

HRESULT CDirectxScope::drawText()
{
	if(!m_dataRate) return S_FALSE;
	__int64 minFreq = m_dataFrequency - m_dataRate;
	short majDig, numDig;
	long digMag;
	TCHAR majChar;
	if(minFreq >= 1000000000)
	{
		majDig = 9;
		majChar = _T('G');
	}
	else if(minFreq >= 1000000)
	{
		majDig = 6;
		majChar = _T('M');
	}
	else if(minFreq >= 1000)
	{
		majDig = 1000;
		majChar = _T('k');
	}
	else
	{
		majDig = 1;
		majChar = _T('.');
	}
	long minIncr = m_dataRate / 5;
	if(minIncr >= 1000000000)
	{
		digMag = 1000000000;
		numDig = 9;
	}
	else if(minIncr >= 100000000)
	{
		digMag = 100000000;
		numDig = 8;
	}
	else if(minIncr >= 10000000)
	{
		digMag = 10000000;
		numDig = 7;
	}
	else if(minIncr >= 1000000)
	{
		digMag = 1000000;
		numDig = 6;
	}
	else if(minIncr >= 100000)
	{
		digMag = 100000;
		numDig = 5;
	}
	else if(minIncr >= 10000)
	{
		digMag = 10000;
		numDig = 4;
	}
	else if(minIncr >= 1000)
	{
		digMag = 1000;
		numDig = 3;
	}
	else if(minIncr >= 100)
	{
		digMag = 100;
		numDig = 2;
	}
	else if(minIncr >= 10)
	{
		digMag = 10;
		numDig = 1;
	}
	else
	{
		digMag = 1;
		numDig = 0;
	}
	if(numDig > majDig) numDig = majDig;
	minIncr += minIncr % digMag;

	D3DXCOLOR minColor(0.7f,0.7f,0.7f,1.0f);
	D3DXCOLOR maxColor(1.0f,1.0f,1.0f,1.0f);
	D3DXCOLOR dotColor(1.0f,1.0f,1.0f,1.0f);

	__int64 maxFreq = m_dataFrequency + m_dataRate;
	TCHAR charBuf[11];
	for(__int64 thisFreq = minFreq + (minFreq % digMag); thisFreq <= maxFreq; thisFreq += minIncr)
	{
		_stprintf_s(charBuf, _countof(charBuf), _T("%I64d"), thisFreq);
		short freqLen = _tcslen(charBuf);
		short majLen = max(freqLen - majDig, 0);
		short minLen = max(min(majDig, freqLen) - numDig, 0);

		SIZE majSize, dotSize, minSize;
		memset(&majSize, 0, sizeof(majSize));
		memset(&dotSize, 0, sizeof(dotSize));
		memset(&minSize, 0, sizeof(minSize));

		HRESULT hR;
		if(majLen)
		{
			hR = m_majFont.CalcSize(charBuf, majLen, majSize);
		} else {
			hR = m_majFont.CalcSize(_T("0"), 1, majSize);
		}
		if(FAILED(hR)) return hR;

		hR = m_dotFont.CalcSize(&majChar, 1, dotSize);
		if(FAILED(hR)) return hR;

		if(minLen)
		{
			hR = m_minFont.CalcSize(charBuf + majLen, minLen, minSize);
			if(FAILED(hR)) return hR;
		}

		long clientCenter = long(m_screenCliWidth * (thisFreq - minFreq) / (maxFreq - minFreq));
		unsigned drawLeft = max(0, clientCenter - (dotSize.cx/2) - majSize.cx);
		if(drawLeft + dotSize.cx + majSize.cx + minSize.cx > m_screenCliWidth)
		{
			drawLeft = m_screenCliWidth - dotSize.cx - majSize.cx - minSize.cx;
		}

		RECT rect;
		memset(&rect, 0, sizeof(rect));

		rect.top = 0;
		rect.bottom = majSize.cy;
		rect.left = drawLeft;
		rect.right = majSize.cx + drawLeft;
		if(majLen)
		{
			hR = m_majFont.DrawText(*this, charBuf, majLen, rect, maxColor);
		} else {
			hR = m_majFont.DrawText(*this, _T("0"), 1, rect, maxColor);
		}
		if(FAILED(hR)) return hR;

		rect.top = majSize.cy - dotSize.cy;
		rect.bottom = majSize.cy;
		rect.left = rect.right;
		rect.right = rect.left + dotSize.cx;
		hR = m_dotFont.DrawText(*this, &majChar, 1, rect, dotColor);
		if(FAILED(hR)) return hR;

		if(minLen)
		{
			rect.top = 0;
			rect.bottom = minSize.cy;
			rect.left = rect.right;
			rect.right = rect.left + minSize.cx;
			hR = m_minFont.DrawText(*this, charBuf + majLen, minLen, rect, minColor);
			if(FAILED(hR)) return hR;
		}
	}

//	hR = sprite->End();
//	if(FAILED(hR)) return hR;

	return S_OK;
}
