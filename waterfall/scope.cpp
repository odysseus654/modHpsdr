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
	m_majFont(15, 0, FW_NORMAL, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Tahoma")),
	m_dotFont(10, 0, FW_NORMAL, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Tahoma"))
{
}

CDirectxScope::~CDirectxScope()
{
	Locker lock(m_refLock);
	releaseDevice();
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

	return m_pDevice->CreateBuffer(vertexBufferDesc, &vertexData, vertex);
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
	HRESULT hR = m_pDevice->createVertexShaderFromResource(_T("ortho_vs.cso"), vdesc, 2, m_pVS, m_pInputLayout);
	if(FAILED(hR)) return hR;

	hR = m_pDevice->createPixelShaderFromResource(_T("mono_bitmap_ps.cso"), m_pBitmapPS);
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

		hR = m_pDevice->CreateBuffer(indexBufferDesc, &indexData, m_pVertexIndexBuffer);
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
		if(driverLevel() < 10)
		{
			BlendState.BlendEnable[3] = BlendState.BlendEnable[2] = BlendState.BlendEnable[1] = TRUE;
		}
 
		hR = m_pDevice->CreateBlendState(BlendState, m_pBlendState);
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

		hR = m_pDevice->CreateSamplerState(samplerDesc, m_pBitmapSampler);
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

		hR = m_pDevice->CreateBuffer(vsGlobalBufferDesc, &vsGlobalData, m_pVSGlobals);
		if(FAILED(hR)) return hR;
	}

	return initDataTexture();
}

void CDirectxScope::setIsComplexInput(bool bComplex)
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

HRESULT CDirectxScope::drawFrameContents(ID3D10Device1* pDevice)
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_pVS || !pDevice)
	{
		return E_POINTER;
	}

	ID3D10BlendState* pOrigBlendState;
	float origBlendFactor[4];
	UINT origBlendMask;
	pDevice->OMGetBlendState(&pOrigBlendState, origBlendFactor, &origBlendMask);
	m_bBackfaceWritten = false;

	clearFrame(pDevice);

	// setup the input assembler.
	const static UINT stride = sizeof(TL_FALL_VERTEX); 
	const static UINT offset = 0;
	pDevice->IASetVertexBuffers(0, 1, m_pVertexBuffer.ref(), &stride, &offset);
	pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pDevice->IASetInputLayout(m_pInputLayout);

	// Build our vertex shader
	pDevice->VSSetShader(m_pVS);
	pDevice->VSSetConstantBuffers(0, 1, m_pVSGlobals.ref());

	// render the frame
	m_bDrawingFonts = false;
	m_majFont.BeginFrame();
	m_dotFont.BeginFrame();

	HRESULT hR = drawRect(pDevice);
	if(FAILED(hR)) return hR;
	BackfaceWritten(pDevice);

	hR = drawText(pDevice);
	if(FAILED(hR)) return hR;

	pDevice->OMSetBlendState(pOrigBlendState, origBlendFactor, origBlendMask);
	m_majFont.EndFrame();
	m_dotFont.EndFrame();

	return S_OK;
}

void CDirectxScope::BackfaceWritten(ID3D10Device1* pDevice)
{
	if(!m_bBackfaceWritten)
	{
		pDevice->OMSetBlendState(m_pBlendState, NULL, MAXUINT16);
		m_bBackfaceWritten = true;
	}
}

HRESULT CDirectxScope::drawMonoBitmap(ID3D10Device1* pDevice, const ID3D10ShaderResourceViewPtr &image,
	const ID3D10BufferPtr& vertex, const ID3D10BufferPtr& color)
{
	// setup the input assembler.
	const static UINT stride = sizeof(TL_FALL_VERTEX); 
	const static UINT offset = 0;
	pDevice->IASetVertexBuffers(0, 1, vertex.ref(), &stride, &offset);

	if(!m_bDrawingFonts)
	{
	//	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	//	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//	m_pDevice->IASetInputLayout(m_pInputLayout);

		// Build our vertex shader
	//	m_pDevice->VSSetShader(m_pVS);
	//	m_pDevice->VSSetConstantBuffers(0, 1, m_pVSGlobals.ref());

		// Build the pixel shader
		pDevice->PSSetShader(m_pBitmapPS);
		pDevice->PSSetSamplers(2, 1, m_pBitmapSampler.ref());

		m_bDrawingFonts = true;
	}

	pDevice->PSSetShaderResources(2, 1, image.ref());
	pDevice->PSSetConstantBuffers(2, 1, color.ref());
	pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	return S_OK;
}

HRESULT CDirectxScope::drawRect(ID3D10Device1* pDevice)
{
	pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	return S_OK;
}

HRESULT CDirectxScope::drawText(ID3D10Device1* pDevice)
{
	enum { EST_LABEL_COUNT = 5 };

	struct TMajorDigitDef {
		__int64 freq;
		short dig;
		TCHAR ch;
	};

	struct TMinorDigitDef {
		__int64 mag;
		short dig;
	};

	const static TMajorDigitDef MAJOR_DIG[] = {
		{ 1000000000, 9, _T('G') },
		{    1000000, 6, _T('M') },
		{       1000, 3, _T('k') },
		{          0, 1, _T('.') }
	};

	const static TMinorDigitDef MINOR_DIG[] = {
		{ 1000000000, 9 },
		{  100000000, 8 },
		{   10000000, 7 },
		{    1000000, 6 },
		{     100000, 5 },
		{      10000, 4 },
		{       1000, 3 },
		{        100, 2 },
		{         10, 1 },
		{          0, 0 },
	};

	if(m_dataRate <= 0) return S_FALSE;
	__int64 minFreq = m_dataFrequency;
	__int64 dataWidth = m_dataRate / 2;
	__int64 maxFreq = m_dataFrequency + dataWidth;
	if(m_bIsComplexInput)
	{
		minFreq -= dataWidth;
		dataWidth *= 2;
	}

	const TMajorDigitDef* major = MAJOR_DIG;
	while(max(minFreq,dataWidth) < major->freq) major++;

	__int64 minIncr = dataWidth / EST_LABEL_COUNT;
	int minIdx=0;
	while(minIncr*2 < MINOR_DIG[minIdx].mag) minIdx++;
	minIncr = MINOR_DIG[minIdx].mag;
	short minorDig = MINOR_DIG[minIdx].dig;
	if(dataWidth / minIncr > EST_LABEL_COUNT*2)
	{
		minIncr *= 2;
	}
	else if(2* dataWidth / minIncr < EST_LABEL_COUNT)
	{
		minIncr /= 2;
		minorDig++;
	}
	minorDig = min(major->dig, minorDig);

	D3DXCOLOR minColor(0.7f,0.7f,0.7f,1.0f);
	D3DXCOLOR maxColor(1.0f,1.0f,1.0f,1.0f);
	D3DXCOLOR dotColor(1.0f,1.0f,1.0f,1.0f);

	TCHAR charBuf[11];
	__int64 thisFreq = minFreq;
	if(thisFreq % minIncr) thisFreq += minIncr - (thisFreq % minIncr);
	for(; thisFreq <= maxFreq; thisFreq += minIncr)
	{
		_stprintf_s(charBuf, _countof(charBuf), _T("%I64d"), thisFreq);
		short freqLen = _tcslen(charBuf);
		short majLen = max(freqLen - major->dig, 0);
		short minLen = max(min(major->dig, freqLen) - minorDig, 0);

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

		hR = m_dotFont.CalcSize(&major->ch, 1, dotSize);
		if(FAILED(hR)) return hR;

		if(minLen)
		{
			hR = m_majFont.CalcSize(charBuf + majLen, minLen, minSize);
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
			hR = m_majFont.DrawText(*this, pDevice, charBuf, majLen, rect, maxColor);
		} else {
			hR = m_majFont.DrawText(*this, pDevice, _T("0"), 1, rect, maxColor);
		}
		if(FAILED(hR)) return hR;

		rect.top = majSize.cy - dotSize.cy / 2;
		rect.bottom = rect.top + dotSize.cy;
		rect.left = rect.right;
		rect.right = rect.left + dotSize.cx;
		hR = m_dotFont.DrawText(*this, pDevice, &major->ch, 1, rect, dotColor);
		if(FAILED(hR)) return hR;

		if(minLen)
		{
			rect.top = 0;
			rect.bottom = minSize.cy;
			rect.left = rect.right;
			rect.right = rect.left + minSize.cx;
			hR = m_majFont.DrawText(*this, pDevice, charBuf + majLen, minLen, rect, minColor);
			if(FAILED(hR)) return hR;
		}
	}

//	hR = sprite->End();
//	if(FAILED(hR)) return hR;

	return S_OK;
}
