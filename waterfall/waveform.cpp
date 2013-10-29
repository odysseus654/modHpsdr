// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waveform.h"

#include <d3d10_1.h>
#include <d3dx10async.h>

char const * CDirectxWaveform::NAME = "DirectX waveform display";

// ---------------------------------------------------------------------------- class CDirectxWaveform

CDirectxWaveform::CDirectxWaveform(signals::IBlockDriver* driver):CDirectxBase(driver),
	m_dataTexData(NULL), m_texFormat(DXGI_FORMAT_UNKNOWN), m_bUsingDX9Shader(false),
	m_dataTexElemSize(0)
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

struct TL_FORM_VERTEX
{
	D3DXVECTOR3 pos;
	float mag;
	float tex;
};

void CDirectxWaveform::buildAttrs()
{
	CDirectxBase::buildAttrs();
	attrs.minRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaveform>
		(*this, "minRange", "Weakest signal to display", &CDirectxWaveform::setMinRange, -200.0f));
	attrs.maxRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaveform>
		(*this, "maxRange", "Strongest signal to display", &CDirectxWaveform::setMaxRange, -150.0f));
}

void CDirectxWaveform::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxBase::releaseDevice();
	m_dataTex.Release();
	m_pVSOrthoGlobals.Release();
	m_pVSRangeGlobals.Release();
	m_pPSColorGlobals.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_dataView.Release();
}

HRESULT CDirectxWaveform::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TL_FORM_VERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"RANGE", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(TL_FORM_VERTEX, mag), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(TL_FORM_VERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	// check for texture support
	enum { REQUIRED_SUPPORT = D3D10_FORMAT_SUPPORT_TEXTURE1D|D3D10_FORMAT_SUPPORT_SHADER_SAMPLE };
	UINT texSupport;
	if(m_driverLevel >= 10.0f && SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT, &texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = false;
		m_texFormat = DXGI_FORMAT_R32_FLOAT;
	}
	else if(SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R16_UNORM, &texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = true;
		m_texFormat = DXGI_FORMAT_R16_UNORM;
	}
	else
	{
		ASSERT(SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R8_UNORM, &texSupport))
			&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT);
		m_bUsingDX9Shader = true;
		m_texFormat = DXGI_FORMAT_R8_UNORM;
	}

	// Load the shader in from the file.
	HRESULT hR = createPixelShaderFromResource(_T("waveform_ps.cso"), m_pPS);
	if(FAILED(hR)) return hR;

	hR = createVertexShaderFromResource(m_bUsingDX9Shader ? _T("wavform_vs9.cso") : _T("wavform_vs.cso"), vdesc, 2, m_pVS, m_pInputLayout);
	if(FAILED(hR)) return hR;

	// calculate texture coordinates
	float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
	float mag = float(m_screenCliHeight) / 2.0f;

	TL_FORM_VERTEX* vertices = new TL_FORM_VERTEX[m_screenCliWidth];
	unsigned short* indices = new unsigned short[m_screenCliWidth];

	for(unsigned idx=0; idx < m_screenCliWidth; idx++)
	{
		vertices[idx].pos = D3DXVECTOR3(idx - texLeft, 0.0f, 0.0f);
		vertices[idx].tex = idx / float(m_screenCliWidth);
		vertices[idx].mag = mag;
		indices[idx] = idx;
	}

	{
		D3D10_BUFFER_DESC vertexBufferDesc;
		memset(&vertexBufferDesc, 0, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = sizeof(TL_FORM_VERTEX) * m_screenCliWidth;
		vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	//	vertexBufferDesc.CPUAccessFlags = 0;
	//	vertexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA vertexData;
		memset(&vertexData, 0, sizeof(vertexData));
		vertexData.pSysMem = vertices;

		hR = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, m_pVertexBuffer.inref());
	}
	delete [] vertices;
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
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_screenCliWidth;
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
	D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

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

	return initDataTexture();
}

HRESULT CDirectxWaveform::initDataTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_dataTexWidth || !m_pDevice)
	{
		return S_FALSE;
	}
	m_dataView.Release();
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;
	m_dataTexElemSize = 0;

	switch(m_texFormat)
	{
	case DXGI_FORMAT_R32_FLOAT:
		m_dataTexData = new float[m_dataTexWidth];
		m_dataTexElemSize = sizeof(float);
		break;
	case DXGI_FORMAT_R16_UNORM:
		m_dataTexData = new short[m_dataTexWidth];
		m_dataTexElemSize = sizeof(short);
		break;
	default:
		m_dataTexData = new char[m_dataTexWidth];
		m_dataTexElemSize = sizeof(char);
		break;
	}
	memset(m_dataTexData, 0, m_dataTexElemSize*m_dataTexWidth);

	HRESULT hR;
	{
		D3D10_TEXTURE1D_DESC dataDesc;
		memset(&dataDesc, 0, sizeof(dataDesc));
		dataDesc.Width = m_dataTexWidth;
		dataDesc.MipLevels = 1;
		dataDesc.ArraySize = 1;
		dataDesc.Format = DXGI_FORMAT_R16_FLOAT;
		dataDesc.Usage = D3D10_USAGE_DYNAMIC;
		dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		hR = m_pDevice->CreateTexture1D(&dataDesc, NULL, m_dataTex.inref());
		if(FAILED(hR)) return hR;
	}

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView.inref());
	if(FAILED(hR)) return hR;

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

	if(!m_bUsingDX9Shader)
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

HRESULT CDirectxWaveform::resizeDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	HRESULT hR = CDirectxBase::resizeDevice();
	if(hR != S_OK) return hR;

	if(m_pVertexBuffer)
	{
		// calculate texture coordinates
		float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
		float mag = float(m_screenCliHeight) / 2.0f;

		TL_FORM_VERTEX* vertices = new TL_FORM_VERTEX[m_screenCliWidth];
		unsigned long* indices = new unsigned long[m_screenCliWidth];

		for(unsigned idx=0; idx < m_screenCliWidth; idx++)
		{
			vertices[idx].pos = D3DXVECTOR3(idx - texLeft, 0.0f, 0.0f);
			vertices[idx].tex = idx / float(m_screenCliWidth);
			vertices[idx].mag = mag;
			indices[idx] = idx;
		}

		D3D10_BUFFER_DESC vertexBufferDesc;
		memset(&vertexBufferDesc, 0, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = sizeof(TL_FORM_VERTEX) * m_screenCliWidth;
		vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	//	vertexBufferDesc.CPUAccessFlags = 0;
	//	vertexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA vertexData;
		memset(&vertexData, 0, sizeof(vertexData));
		vertexData.pSysMem = vertices;

		m_pVertexBuffer.Release();
		m_pVertexIndexBuffer.Release();
		hR = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, m_pVertexBuffer.inref());
		delete [] vertices;
		if(FAILED(hR))
		{
			delete [] indices;
			return hR;
		}

		// Now create the index buffer.
		D3D10_BUFFER_DESC indexBufferDesc;
		memset(&indexBufferDesc, 0, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_screenCliWidth;
		indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
	//	indexBufferDesc.CPUAccessFlags = 0;
	//	indexBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA indexData;
		memset(&indexData, 0, sizeof(indexData));
		vertexData.pSysMem = indices;

		hR = m_pDevice->CreateBuffer(&indexBufferDesc, &vertexData, m_pVertexIndexBuffer.inref());
		delete [] indices;
		if(FAILED(hR)) return hR;
	}

	if(m_pDevice && m_pVSOrthoGlobals)
	{
		// Create an orthographic projection matrix for 2D rendering.
		D3DXMATRIX orthoMatrix;
		D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

		m_pDevice->UpdateSubresource(m_pVSOrthoGlobals, 0, NULL, &orthoMatrix, sizeof(orthoMatrix), sizeof(orthoMatrix));
	}

	return S_OK;
}

HRESULT CDirectxWaveform::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pVSOrthoGlobals || !m_pPSColorGlobals || !m_dataTexElemSize)
	{
		return E_POINTER;
	}

	void* mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex, m_dataTexData, m_dataTexElemSize*m_dataTexWidth);
	m_dataTex->Unmap(subr);

	// setup the input assembler.
	UINT stride = sizeof(TL_FORM_VERTEX); 
	UINT offset = 0;
	ID3D10Buffer* buf = m_pVertexBuffer;
	m_pDevice->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
	m_pDevice->IASetInputLayout(m_pInputLayout);

	// Build our vertex shader
	m_pDevice->VSSetShader(m_pVS);
	m_pDevice->VSSetShaderResources(0, 1, m_dataView.ref());
	if(!m_bUsingDX9Shader)
	{
		ID3D10Buffer* vsBuf[] = { m_pVSOrthoGlobals, m_pVSRangeGlobals };
		m_pDevice->VSSetConstantBuffers(0, 2, vsBuf);
	} else {
		m_pDevice->VSSetConstantBuffers(0, 1, m_pVSOrthoGlobals.ref());
	}

	// Build our piel shader
	m_pDevice->PSSetShader(m_pPS);
	m_pDevice->PSSetConstantBuffers(0, 1, m_pPSColorGlobals.ref());

	// render the frame
	m_pDevice->DrawIndexed(m_screenCliWidth, 0, 0);

	return S_OK;
}

void CDirectxWaveform::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_dataTexWidth)
	{
		if(size && size != m_dataTexWidth)
		{
			m_dataTexWidth = size;
			initDataTexture();
		}
	}

	double* src = frame;
	switch(m_texFormat)
	{
	case DXGI_FORMAT_R32_FLOAT:
		{
			// convert to float
			float* newLine = (float*)m_dataTexData;
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				float* dest = newLine + halfWidth;
				float* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = float(*src++);

				dest = newLine;
				destEnd = newLine + halfWidth;
				while(dest < destEnd) *dest++ = float(*src++);
			} else {
				float* dest = newLine;
				float* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = float(*src++);
			}
		}
		break;
	case DXGI_FORMAT_R16_UNORM:
		{
			// convert to normalised short
			unsigned short* newLine = (unsigned short*)m_dataTexData;
			double range = m_psRange.maxRange - m_psRange.minRange;
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				unsigned short* dest = newLine + halfWidth;
				unsigned short* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned short)(MAXUINT16 * (*src++ - m_psRange.minRange) / range);

				dest = newLine;
				destEnd = newLine + halfWidth;
				while(dest < destEnd) *dest++ = (unsigned short)(MAXUINT16 * (*src++ - m_psRange.minRange) / range);
			} else {
				unsigned short* dest = newLine;
				unsigned short* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned short)(MAXUINT16 * (*src++ - m_psRange.minRange) / range);
			}
		}
		break;
	case DXGI_FORMAT_R8_UNORM:
		{
			// convert to normalised char
			unsigned char* newLine = (unsigned char*)m_dataTexData;
			double range = m_psRange.maxRange - m_psRange.minRange;
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				unsigned char* dest = newLine + halfWidth;
				unsigned char* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned char)(MAXUINT8 * (*src++ - m_psRange.minRange) / range);

				dest = newLine;
				destEnd = newLine + halfWidth;
				while(dest < destEnd) *dest++ = (unsigned char)(MAXUINT8 * (*src++ - m_psRange.minRange) / range);
			} else {
				unsigned char* dest = newLine;
				unsigned char* destEnd = newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned char)(MAXUINT8 * (*src++ - m_psRange.minRange) / range);
			}
		}
		break;
	}

	VERIFY(SUCCEEDED(drawFrame()));
}
