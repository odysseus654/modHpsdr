// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waveform.h"

#include <d3d10_1.h>
#include <d3dx10async.h>

char const * CDirectxWaveform::NAME = "DirectX waveform display";

// ---------------------------------------------------------------------------- class CDirectxWaveform

CDirectxWaveform::CDirectxWaveform(signals::IBlockDriver* driver):CDirectxBase(driver),
	m_dataTexWidth(0), m_dataTexData(NULL), m_bIsComplexInput(true)
{
	m_psRange.minRange = 0.0f;
	m_psRange.maxRange = 0.0f;
	m_waveformColor[0] = 1.0f;
	m_waveformColor[1] = 0.0f;
	m_waveformColor[2] = 0.0f;
	m_waveformColor[3] = 1.0f;
	buildAttrs();
}

CDirectxWaveform::~CDirectxWaveform()
{
	Locker lock(m_refLock);
	releaseDevice();
	if(m_dataTexData)
	{
		delete [] m_dataTexData;
		m_dataTexData = NULL;
	}
}

void CDirectxWaveform::buildAttrs()
{
	CDirectxBase::buildAttrs();
	attrs.minRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaveform>
		(*this, "minRange", "Weakest signal to display", &CDirectxWaveform::setMinRange, -200.0f));
	attrs.maxRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaveform>
		(*this, "maxRange", "Strongest signal to display", &CDirectxWaveform::setMaxRange, -150.0f));
	attrs.isComplexInput = addLocalAttr(true, new CAttr_callback<signals::etypBoolean,CDirectxWaveform>
		(*this, "isComplexInput", "Input is based on complex data", &CDirectxWaveform::setIsComplexInput, true));
}

void CDirectxWaveform::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxBase::releaseDevice();
	m_pPS.Release();
	m_pVS.Release();
	m_dataTex.Release();
	m_pVSOrthoGlobals.Release();
	m_pVSRangeGlobals.Release();
	m_pPSColorGlobals.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
}

HRESULT CDirectxWaveform::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	const static D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"MAG", 0, DXGI_FORMAT_R32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	// Load the shader in from the file.
	HRESULT hR = createPixelShaderFromResource(_T("waveform_ps.cso"), m_pPS);
	if(FAILED(hR)) return hR;

	hR = createVertexShaderFromResource(_T("waveform_vs.cso"), vdesc, 2, m_pVS, m_pInputLayout);
	if(FAILED(hR)) return hR;

	return initDataTexture();
}

HRESULT CDirectxWaveform::initDataTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_frameWidth || !m_pDevice)
	{
		return S_FALSE;
	}
	m_dataTexWidth = m_bIsComplexInput ? m_frameWidth : m_frameWidth / 2;
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;
	m_dataTexData = new float[m_dataTexWidth];
	memset(m_dataTexData, 0, sizeof(float)*m_dataTexWidth);

	float* texcoord = new float[m_dataTexWidth];
	unsigned short* indices = new unsigned short[m_dataTexWidth];

	for(unsigned idx=0; idx < m_dataTexWidth; idx++)
	{
		texcoord[idx] = (float)idx;
		indices[idx] = idx;
	}

	HRESULT hR;
	{
		D3D10_BUFFER_DESC vertexBufferDesc;
		memset(&vertexBufferDesc, 0, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = sizeof(*texcoord) * m_dataTexWidth;
		vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	//	vertexBufferDesc.CPUAccessFlags = 0;
	//	vertexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA vertexData;
		memset(&vertexData, 0, sizeof(vertexData));
		vertexData.pSysMem = texcoord;

		hR = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, m_pVertexBuffer.inref());
	}
	delete [] texcoord;
	if(FAILED(hR))
	{
		delete [] indices;
		return hR;
	}

	// Now create the index buffer.
	{
		D3D10_BUFFER_DESC indexBufferDesc;
		memset(&indexBufferDesc, 0, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(*indices) * m_dataTexWidth;
		indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
	//	indexBufferDesc.CPUAccessFlags = 0;
	//	indexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA indexData;
		memset(&indexData, 0, sizeof(indexData));
		indexData.pSysMem = indices;

		hR = m_pDevice->CreateBuffer(&indexBufferDesc, &indexData, m_pVertexIndexBuffer.inref());
	}
	delete [] indices;
	if(FAILED(hR)) return hR;

	// Create an orthographic projection matrix for 2D rendering.
	D3DXMATRIX orthoMatrix;
	D3DXMatrixOrthoOffCenterLH(&orthoMatrix, 0, (float)m_dataTexWidth, 0.0f, 1.0f, 0.0f, 1.0f);

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

		hR = m_pDevice->CreateBuffer(&vsGlobalBufferDesc, &vsGlobalData, m_pVSOrthoGlobals.inref());
		if(FAILED(hR)) return hR;
	}

	{
		D3D10_BUFFER_DESC dataDesc;
		memset(&dataDesc, 0, sizeof(dataDesc));
		dataDesc.Usage = D3D10_USAGE_DYNAMIC;
		dataDesc.ByteWidth = sizeof(float) * m_dataTexWidth;
		dataDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	//	dataDesc.MiscFlags = 0;

		hR = m_pDevice->CreateBuffer(&dataDesc, NULL, m_dataTex.inref());
		if(FAILED(hR)) return hR;
	}

	{
		D3D10_BUFFER_DESC psGlobalBufferDesc;
		memset(&psGlobalBufferDesc, 0, sizeof(psGlobalBufferDesc));
		psGlobalBufferDesc.Usage = D3D10_USAGE_DEFAULT;
		psGlobalBufferDesc.ByteWidth = sizeof(m_waveformColor);
		psGlobalBufferDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	//	psGlobalBufferDesc.CPUAccessFlags = 0;
	//	psGlobalBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA psGlobalData;
		memset(&psGlobalData, 0, sizeof(psGlobalData));
		psGlobalData.pSysMem = &m_waveformColor;

		hR = m_pDevice->CreateBuffer(&psGlobalBufferDesc, &psGlobalData, m_pPSColorGlobals.inref());
		if(FAILED(hR)) return hR;
	}

	{
		D3D10_BUFFER_DESC psGlobalBufferDesc;
		memset(&psGlobalBufferDesc, 0, sizeof(psGlobalBufferDesc));
		psGlobalBufferDesc.Usage = D3D10_USAGE_DEFAULT;
		psGlobalBufferDesc.ByteWidth = sizeof(m_psRange);
		psGlobalBufferDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	//	psGlobalBufferDesc.CPUAccessFlags = 0;
	//	psGlobalBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA psGlobalData;
		memset(&psGlobalData, 0, sizeof(psGlobalData));
		psGlobalData.pSysMem = &m_psRange;

		hR = m_pDevice->CreateBuffer(&psGlobalBufferDesc, &psGlobalData, m_pVSRangeGlobals.inref());
		if(FAILED(hR)) return hR;
	}

	return S_OK;
}

void CDirectxWaveform::setMinRange(const float& newMin)
{
	if(newMin != m_psRange.minRange)
	{
		m_psRange.minRange = newMin;

		if(m_pDevice && m_pVSRangeGlobals)
		{
			m_pDevice->UpdateSubresource(m_pVSRangeGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

void CDirectxWaveform::setMaxRange(const float& newMax)
{
	if(newMax != m_psRange.maxRange)
	{
		m_psRange.maxRange = newMax;

		if(m_pDevice && m_pVSRangeGlobals)
		{
			m_pDevice->UpdateSubresource(m_pVSRangeGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

HRESULT CDirectxWaveform::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pVSOrthoGlobals || !m_pPSColorGlobals)
	{
		return E_POINTER;
	}

	{
		// push the radio input to the video buffers
		void* mappedDataTex;
		HRESULT hR = m_dataTex->Map(D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
		if(FAILED(hR)) return hR;
		memcpy(mappedDataTex, m_dataTexData, sizeof(float)*m_dataTexWidth);
		m_dataTex->Unmap();
	}

	{
		// setup the input assembler.
		static const UINT stride[] = { sizeof(float), sizeof(float) }; 
		static const UINT offset[] = { 0, 0 };
		ID3D10Buffer* buf[] = { m_pVertexBuffer, m_dataTex };
		m_pDevice->IASetVertexBuffers(0, 2, buf, stride, offset);
		m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
		m_pDevice->IASetInputLayout(m_pInputLayout);
	}

	{
		// Build our vertex shader
		m_pDevice->VSSetShader(m_pVS);
		ID3D10Buffer* vsBuf[] = { m_pVSOrthoGlobals, m_pVSRangeGlobals };
		m_pDevice->VSSetConstantBuffers(0, 2, vsBuf);
	}

	{
		// Build our pixel shader
		m_pDevice->PSSetShader(m_pPS);
		m_pDevice->PSSetConstantBuffers(0, 1, m_pPSColorGlobals.ref());
	}

	// render the frame
	m_pDevice->DrawIndexed(m_dataTexWidth, 0, 0);

	return S_OK;
}

void CDirectxWaveform::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_frameWidth)
	{
		if(size && size != m_frameWidth)
		{
			m_frameWidth = size;
			initDataTexture();
		}
	}

	// convert to float
	double* src = frame;
	if(m_bIsComplexInput)
	{
		UINT halfWidth = m_dataTexWidth / 2;
		float* dest = m_dataTexData + halfWidth;
		float* destEnd = m_dataTexData + m_dataTexWidth;
		while(dest < destEnd) *dest++ = float(*src++);

		dest = m_dataTexData;
		destEnd = m_dataTexData + halfWidth;
		while(dest < destEnd) *dest++ = float(*src++);
	} else {
		float* dest = m_dataTexData;
		float* destEnd = m_dataTexData + m_dataTexWidth;
		while(dest < destEnd) *dest++ = float(*src++);
	}

	VERIFY(SUCCEEDED(drawFrame()));
}
