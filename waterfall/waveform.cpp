/*
	Copyright 2013 Erik Anderson

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
#include "waveform.h"

#include <d3d10_1.h>
#include <d3dx10.h>

char const * CDirectxWaveform::NAME = "DirectX waveform display";

// ---------------------------------------------------------------------------- class CDirectxWaveform

CDirectxWaveform::CDirectxWaveform(signals::IBlockDriver* driver):CDirectxScope(driver, true),
	m_dataTexWidth(0), m_dataTexData(NULL), m_dataTexElemSize(0), m_texFormat(DXGI_FORMAT_UNKNOWN),
	m_bUsingDX9Shader(false)
{
	m_psRange.minRange = 0.0f;
	m_psRange.maxRange = 0.0f;
	m_psGlobals.m_waveformColor[0] = 1.0f;
	m_psGlobals.m_waveformColor[1] = 0.0f;
	m_psGlobals.m_waveformColor[2] = 0.0f;
	m_psGlobals.m_waveformColor[3] = 1.0f;
	m_psGlobals.m_shadowColor[0] = 0.75f;
	m_psGlobals.m_shadowColor[1] = 0.0f;
	m_psGlobals.m_shadowColor[2] = 0.0f;
	m_psGlobals.m_shadowColor[3] = 0.75f;
	m_psGlobals.line_width = 1.0f;
	buildAttrs();
}

CDirectxWaveform::~CDirectxWaveform()
{
	stopThread();
	Locker lock(m_refLock);
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
		(*this, "minRange", "Weakest signal to display", &CDirectxWaveform::setMinRange, -300.0f));
	attrs.maxRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaveform>
		(*this, "maxRange", "Strongest signal to display", &CDirectxWaveform::setMaxRange, -100.0f));
}

void CDirectxWaveform::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxScope::releaseDevice();
	m_pPS.Release();
	m_pShadowPS.Release();
	m_dataTex.Release();
	m_pPSRangeGlobals.Release();
	m_pPSGlobals.Release();
}

HRESULT CDirectxWaveform::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	// check for texture support
	enum { REQUIRED_SUPPORT = D3D10_FORMAT_SUPPORT_TEXTURE2D };
	UINT texSupport;
	if(driverLevel() >= 10.0f && SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT, texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = false;
		m_texFormat = DXGI_FORMAT_R32_FLOAT;
	}
	else if(SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R16_UNORM, texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = true;
		m_texFormat = DXGI_FORMAT_R16_UNORM;
	}
	else
	{
		ASSERT(SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R8_UNORM, texSupport))
			&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT);
		m_bUsingDX9Shader = true;
		m_texFormat = DXGI_FORMAT_R8_UNORM;
	}

	// Load the shader in from the file.
	HRESULT hR = m_pDevice->createPixelShaderFromResource(m_bUsingDX9Shader ? _T("waveform_ps9.cso") : _T("waveform_ps.cso"), m_pPS);
	if(FAILED(hR)) return hR;

	hR = m_pDevice->createPixelShaderFromResource(m_bUsingDX9Shader ? _T("waveform_shadow_ps9.cso") : _T("waveform_shadow_ps.cso"), m_pShadowPS);
	if(FAILED(hR)) return hR;

	{
		D3D10_SAMPLER_DESC samplerDesc;
		memset(&samplerDesc, 0, sizeof(samplerDesc));
		samplerDesc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D10_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D10_FLOAT32_MAX;

		hR = m_pDevice->CreateSamplerState(samplerDesc, m_pPSSampler);
		if(FAILED(hR)) return hR;
	}

	return CDirectxScope::initTexture();
}

HRESULT CDirectxWaveform::initDataTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_frameWidth || !m_pDevice)
	{
		return S_FALSE;
	}
	m_dataTexWidth = m_bIsComplexInput ? m_frameWidth : m_frameWidth / 2;
	m_dataView.Release();
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;
	m_dataTexElemSize = 0;
	m_psGlobals.texture_width = (float)m_dataTexWidth;

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

	m_psGlobals.viewWidth = m_screenCliWidth / 2.0f;
	m_psGlobals.viewHeight = m_screenCliHeight / 2.0f;

	HRESULT hR;
	{
		D3D10_TEXTURE2D_DESC dataDesc;
		memset(&dataDesc, 0, sizeof(dataDesc));
		dataDesc.Width = m_dataTexWidth;
		dataDesc.Height = 1;
		dataDesc.MipLevels = 1;
		dataDesc.ArraySize = 1;
		dataDesc.Format = m_texFormat;
		dataDesc.SampleDesc.Count = 1;
		dataDesc.SampleDesc.Quality = 0;
		dataDesc.Usage = D3D10_USAGE_DYNAMIC;
		dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		hR = m_pDevice->CreateTexture2D(dataDesc, NULL, m_dataTex);
		if(FAILED(hR)) return hR;
	}

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView);
	if(FAILED(hR)) return hR;

	{
		D3D10_BUFFER_DESC psGlobalBufferDesc;
		memset(&psGlobalBufferDesc, 0, sizeof(psGlobalBufferDesc));
		psGlobalBufferDesc.Usage = D3D10_USAGE_DEFAULT;
		psGlobalBufferDesc.ByteWidth = sizeof(m_psGlobals);
		psGlobalBufferDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	//	psGlobalBufferDesc.CPUAccessFlags = 0;
	//	psGlobalBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA psGlobalData;
		memset(&psGlobalData, 0, sizeof(psGlobalData));
		psGlobalData.pSysMem = &m_psGlobals;

		hR = m_pDevice->CreateBuffer(psGlobalBufferDesc, &psGlobalData, m_pPSGlobals);
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

		hR = m_pDevice->CreateBuffer(psGlobalBufferDesc, &psGlobalData, m_pPSRangeGlobals);
		if(FAILED(hR)) return hR;
	}

	return S_OK;
}

void CDirectxWaveform::setMinRange(const float& newMin)
{
	if(newMin != m_psRange.minRange)
	{
		m_psRange.minRange = newMin;

		if(m_pDevice && m_pPSRangeGlobals)
		{
			m_pDevice->UpdateSubresource(m_pPSRangeGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

void CDirectxWaveform::setMaxRange(const float& newMax)
{
	if(newMax != m_psRange.maxRange)
	{
		m_psRange.maxRange = newMax;

		if(m_pDevice && m_pPSRangeGlobals)
		{
			m_pDevice->UpdateSubresource(m_pPSRangeGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

HRESULT CDirectxWaveform::resizeDevice()
{
	HRESULT hR = CDirectxScope::resizeDevice();
	if(SUCCEEDED(hR))
	{
		m_psGlobals.viewWidth = m_screenCliWidth / 2.0f;
		m_psGlobals.viewHeight = m_screenCliHeight / 2.0f;
		if(m_pDevice && m_pPSGlobals)
		{
			m_pDevice->UpdateSubresource(m_pPSGlobals, 0, NULL, &m_psGlobals, sizeof(m_psGlobals), sizeof(m_psGlobals));
		}
	}
	return hR;
}

void CDirectxWaveform::clearFrame(ID3D10Device1* pDevice)
{
	static const float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pDevice->ClearRenderTargetView(m_pRenderTargetView, color);
	BackfaceWritten(pDevice);

	CDirectxScope::clearFrame(pDevice);
}

HRESULT CDirectxWaveform::preDrawFrame()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_dataTex || !m_dataTexData || !m_pPSGlobals)
	{
		return E_POINTER;
	}

	// push the radio input to the video buffers
	D3D10_MAPPED_TEXTURE2D mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	if(m_bIsComplexInput)
	{
		UINT halfWidth = m_dataTexWidth / 2;
		memcpy(((char*)mappedDataTex.pData) + halfWidth*m_dataTexElemSize, m_dataTexData, m_dataTexElemSize*halfWidth);
		memcpy(mappedDataTex.pData, ((char*)m_dataTexData) + halfWidth*m_dataTexElemSize, m_dataTexElemSize*halfWidth);
	} else {
		memcpy(mappedDataTex.pData, m_dataTexData, m_dataTexElemSize*m_dataTexWidth);
	}
	m_dataTex->Unmap(subr);

	return S_OK;
}

HRESULT CDirectxWaveform::drawRect(ID3D10Device1* pDevice)
{
	// Build our pixel shader
	pDevice->PSSetShader(m_pShadowPS);
	if(!m_bUsingDX9Shader)
	{
		ID3D10Buffer* psResr[] = { m_pPSGlobals, m_pPSRangeGlobals };
		pDevice->PSSetConstantBuffers(0, 2, psResr);
	} else {
		pDevice->PSSetConstantBuffers(0, 1, m_pPSGlobals.ref());
	}
	pDevice->PSSetShaderResources(0, 1, m_dataView.ref());
	pDevice->PSSetSamplers(0, 1, m_pPSSampler.ref());

	HRESULT hR = CDirectxScope::drawRect(pDevice);
	if(FAILED(hR)) return hR;

	pDevice->PSSetShader(m_pPS);
	return CDirectxScope::drawRect(pDevice);
}

static inline double scale(double val, double high)
{
	return val <= 0.0 ? 0.0 : val >= 1.0 ? high : val * high + 0.5;
}

void CDirectxWaveform::onReceivedFrame(double* frame, unsigned size)
{
	if(!m_pDevice) return;

	if(size && size != m_frameWidth)
	{
		m_frameWidth = size;
		initDataTexture();
	}

	double* src = frame;
	switch(m_texFormat)
	{
	case DXGI_FORMAT_R32_FLOAT:
		{
			// convert to float
			float* newLine = (float*)m_dataTexData;
			float* dest = newLine;
			float* destEnd = newLine + m_dataTexWidth;
			while(dest < destEnd) *dest++ = float(*src++);
		}
		break;
	case DXGI_FORMAT_R16_UNORM:
		{
			// convert to normalised short
			double range = m_psRange.maxRange - m_psRange.minRange;
			unsigned short* newLine = (unsigned short*)m_dataTexData;
			unsigned short* dest = newLine;
			unsigned short* destEnd = newLine + m_dataTexWidth;
			while(dest < destEnd) *dest++ = (unsigned short)scale((*src++ - m_psRange.minRange) / range, MAXUINT16);
		}
		break;
	case DXGI_FORMAT_R8_UNORM:
		{
			// convert to normalised char
			double range = m_psRange.maxRange - m_psRange.minRange;
			unsigned char* newLine = (unsigned char*)m_dataTexData;
			unsigned char* dest = newLine;
			unsigned char* destEnd = newLine + m_dataTexWidth;
			while(dest < destEnd) *dest++ = (unsigned char)scale((*src++ - m_psRange.minRange) / range, MAXUINT8);
		}
		break;
	}

	VERIFY(SUCCEEDED(drawFrame()));
}
