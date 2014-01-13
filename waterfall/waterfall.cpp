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
#include "waterfall.h"

#include <d3d10_1.h>
#include <d3dx10.h>

#pragma comment(lib, "d3dx10.lib")

char const * CDirectxWaterfall::NAME = "DirectX waterfall display";

static const float PI = std::atan(1.0f)*4;

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

CDirectxWaterfall::CDirectxWaterfall(signals::IBlockDriver* driver):CDirectxScope(driver, false),
	m_dataTexWidth(0), m_dataTexHeight(0), m_dataTexData(NULL), m_floatStaging(NULL), m_dataTexElemSize(0),
	m_texFormat(DXGI_FORMAT_UNKNOWN), m_bUsingDX9Shader(false)
{
	m_psRange.minRange = 0.0f;
	m_psRange.maxRange = 0.0f;
	buildAttrs();
}

CDirectxWaterfall::~CDirectxWaterfall()
{
	stopThread();
	Locker lock(m_refLock);
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

void CDirectxWaterfall::buildAttrs()
{
	CDirectxScope::buildAttrs();
	attrs.height = addLocalAttr(true, new CAttr_callback<signals::etypShort,CDirectxWaterfall>
		(*this, "height", "Rows of history to display", &CDirectxWaterfall::setHeight, 512));
	attrs.minRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaterfall>
		(*this, "minRange", "Weakest signal to display", &CDirectxWaterfall::setMinRange, -200.0f));
	attrs.maxRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaterfall>
		(*this, "maxRange", "Strongest signal to display", &CDirectxWaterfall::setMaxRange, -100.0f));
}

void CDirectxWaterfall::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxScope::releaseDevice();
	m_dataTex.Release();
	m_pPS.Release();
	m_waterfallView.Release();
	m_pPSGlobals.Release();
	m_pColorSampler.Release();
	m_pValSampler.Release();
}

HRESULT CDirectxWaterfall::buildWaterfallTexture(const CVideoDevicePtr& pDevice, ID3D10Texture2DPtr& waterfallTex)
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

	D3D10_TEXTURE2D_DESC waterfallDesc;
	memset(&waterfallDesc, 0, sizeof(waterfallDesc));
	waterfallDesc.Width = 256;
	waterfallDesc.Height = 1;
	waterfallDesc.MipLevels = 1;
	waterfallDesc.ArraySize = 1;
	waterfallDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	waterfallDesc.Usage = D3D10_USAGE_IMMUTABLE;
	waterfallDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	waterfallDesc.CPUAccessFlags = 0;
	waterfallDesc.SampleDesc.Count = 1;
	waterfallDesc.SampleDesc.Quality = 0;

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
	waterfallData.SysMemPitch = sizeof(waterfallColors);

	HRESULT hR = pDevice->CreateTexture2D(waterfallDesc, &waterfallData, waterfallTex);
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT CDirectxWaterfall::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	// check for texture support
	enum { REQUIRED_SUPPORT = D3D10_FORMAT_SUPPORT_TEXTURE2D|D3D10_FORMAT_SUPPORT_SHADER_SAMPLE };
	UINT texSupport;
	if(driverLevel() >= 10.0f && SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R16_FLOAT, texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = false;
		m_texFormat = DXGI_FORMAT_R16_FLOAT;
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
	HRESULT hR = m_pDevice->createPixelShaderFromResource(m_bUsingDX9Shader ? _T("waterfall_ps9.cso") : _T("waterfall_ps.cso"), m_pPS);
	if(FAILED(hR)) return hR;

	ID3D10Texture2DPtr waterfallTex;
	hR = buildWaterfallTexture(m_pDevice, waterfallTex);
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(waterfallTex, NULL, m_waterfallView);
	if(FAILED(hR)) return hR;

	{
		D3D10_SAMPLER_DESC samplerDesc;
		memset(&samplerDesc, 0, sizeof(samplerDesc));
		samplerDesc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		if(driverLevel() >= 10.0f)
		{
			samplerDesc.AddressU = D3D10_TEXTURE_ADDRESS_BORDER;
			samplerDesc.AddressV = D3D10_TEXTURE_ADDRESS_BORDER;
			samplerDesc.AddressW = D3D10_TEXTURE_ADDRESS_BORDER;
		} else {
			samplerDesc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		}
		samplerDesc.ComparisonFunc = D3D10_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D10_FLOAT32_MAX;
		//samplerDesc.BorderColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		hR = m_pDevice->CreateSamplerState(samplerDesc, m_pValSampler);
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

		hR = m_pDevice->CreateSamplerState(samplerDesc, m_pColorSampler);
		if(FAILED(hR)) return hR;
	}

	return CDirectxScope::initTexture();
}

HRESULT CDirectxWaterfall::initDataTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_frameWidth || !m_dataTexHeight || !m_pDevice)
	{
		return S_FALSE;
	}
	m_dataTexWidth = m_bIsComplexInput ? m_frameWidth : m_frameWidth / 2;
	m_dataView.Release();
	m_dataTex.Release();
	if(m_dataTexData) delete [] m_dataTexData;
	if(m_floatStaging)
	{
		delete [] m_floatStaging;
		m_floatStaging = NULL;
	}
	m_dataTexElemSize = 0;

//	m_dataTexHeight = 512;
//	m_floatStaging = new float[m_dataTexWidth];

	switch(m_texFormat)
	{
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UNORM:
		m_dataTexData = new short[m_dataTexWidth*m_dataTexHeight];
		m_dataTexElemSize = sizeof(short);
		break;
	default:
		m_dataTexData = new char[m_dataTexWidth*m_dataTexHeight];
		m_dataTexElemSize = sizeof(char);
		break;
	}
	memset(m_dataTexData, 0, m_dataTexElemSize*m_dataTexWidth*m_dataTexHeight);

	HRESULT hR;
	{
		D3D10_TEXTURE2D_DESC dataDesc;
		memset(&dataDesc, 0, sizeof(dataDesc));
		dataDesc.Width = m_dataTexWidth;
		dataDesc.Height = m_dataTexHeight;
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

		hR = m_pDevice->CreateBuffer(psGlobalBufferDesc, &psGlobalData, m_pPSGlobals);
		if(FAILED(hR)) return hR;
	}

	return S_OK;
}

void CDirectxWaterfall::setMinRange(const float& newMin)
{
	if(newMin != m_psRange.minRange)
	{
		m_psRange.minRange = newMin;

		if(m_pDevice && m_pPSGlobals)
		{
			m_pDevice->UpdateSubresource(m_pPSGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

void CDirectxWaterfall::setMaxRange(const float& newMax)
{
	if(newMax != m_psRange.maxRange)
	{
		m_psRange.maxRange = newMax;

		if(m_pDevice && m_pPSGlobals)
		{
			m_pDevice->UpdateSubresource(m_pPSGlobals, 0, NULL, &m_psRange, sizeof(m_psRange), sizeof(m_psRange));
		}
	}
}

HRESULT CDirectxWaterfall::preDrawFrame()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_dataTex || !m_dataTexData || !m_pPS || !m_dataTexElemSize)
	{
		return E_POINTER;
	}

	// push the radio input to the video buffers
	D3D10_MAPPED_TEXTURE2D mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex.pData, m_dataTexData, m_dataTexElemSize*m_dataTexWidth*m_dataTexHeight);
	m_dataTex->Unmap(subr);

	return S_OK;
}

HRESULT CDirectxWaterfall::drawRect(ID3D10Device1* pDevice)
{
	// Build our pixel shader
	ID3D10ShaderResourceView* psResr[] = { m_dataView, m_waterfallView };
	ID3D10SamplerState* psSamp[] = { m_pValSampler, m_pColorSampler };
	pDevice->PSSetShader(m_pPS);
	if(!m_bUsingDX9Shader) pDevice->PSSetConstantBuffers(0, 1, m_pPSGlobals.ref());
	pDevice->PSSetShaderResources(0, 2, psResr);
	pDevice->PSSetSamplers(0, 2, psSamp);

	return CDirectxScope::drawRect(pDevice);
}

void CDirectxWaterfall::setHeight(const short& newHeight)
{
	if(newHeight == m_dataTexHeight || newHeight <= 0) return;
	Locker lock(m_refLock);
	m_dataTexHeight = newHeight;
	initDataTexture();
}

static inline double scale(double val, double high)
{
	return val <= 0.0 ? 0.0 : val >= 1.0 ? high : val * high + 0.5;
}

void CDirectxWaterfall::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_frameWidth)
	{
		m_frameWidth = size;
		initDataTexture();
	}

	// shift the data up one row
	unsigned byteWidth = m_dataTexWidth*m_dataTexElemSize;
	memmove(m_dataTexData, ((char*)m_dataTexData)+byteWidth, byteWidth*(m_dataTexHeight-1));
	void* newLine = (char*)m_dataTexData + byteWidth*(m_dataTexHeight-1);

	double* src = frame;
	switch(m_texFormat)
	{
	case DXGI_FORMAT_R16_FLOAT:
		{
			// convert to float
			if(!m_floatStaging) m_floatStaging = new float[m_dataTexWidth];
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				float* dest = m_floatStaging + halfWidth;
				float* destEnd = m_floatStaging + m_dataTexWidth;
				while(dest < destEnd) *dest++ = float(*src++);

				dest = m_floatStaging;
				destEnd = m_floatStaging + halfWidth;
				while(dest < destEnd) *dest++ = float(*src++);
			} else {
				float* dest = m_floatStaging;
				float* destEnd = m_floatStaging + m_dataTexWidth;
				while(dest < destEnd) *dest++ = float(*src++);
			}

			// write a new bottom line
			D3DXFloat32To16Array((D3DXFLOAT16*)newLine, m_floatStaging, m_dataTexWidth);
		}
		break;
	case DXGI_FORMAT_R16_UNORM:
		{
			// convert to normalised short
			double range = m_psRange.maxRange - m_psRange.minRange;
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				unsigned short* dest = (unsigned short*)newLine + halfWidth;
				unsigned short* destEnd = (unsigned short*)newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned short)scale((*src++ - m_psRange.minRange) / range, MAXUINT16);

				dest = (unsigned short*)newLine;
				destEnd = (unsigned short*)newLine + halfWidth;
				while(dest < destEnd) *dest++ = (unsigned short)scale((*src++ - m_psRange.minRange) / range, MAXUINT16);
			} else {
				unsigned short* dest = (unsigned short*)newLine;
				unsigned short* destEnd = (unsigned short*)newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned short)scale((*src++ - m_psRange.minRange) / range, MAXUINT16);
			}
		}
		break;
	case DXGI_FORMAT_R8_UNORM:
		{
			// convert to normalised char
			double range = m_psRange.maxRange - m_psRange.minRange;
			if(m_bIsComplexInput)
			{
				UINT halfWidth = m_dataTexWidth / 2;
				unsigned char* dest = (unsigned char*)newLine + halfWidth;
				unsigned char* destEnd = (unsigned char*)newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned char)scale((*src++ - m_psRange.minRange) / range, MAXUINT8);

				dest = (unsigned char*)newLine;
				destEnd = (unsigned char*)newLine + halfWidth;
				while(dest < destEnd) *dest++ = (unsigned char)scale((*src++ - m_psRange.minRange) / range, MAXUINT8);
			} else {
				unsigned char* dest = (unsigned char*)newLine;
				unsigned char* destEnd = (unsigned char*)newLine + m_dataTexWidth;
				while(dest < destEnd) *dest++ = (unsigned char)scale((*src++ - m_psRange.minRange) / range, MAXUINT8);
			}
		}
		break;
	}

	VERIFY(SUCCEEDED(drawFrame()));
}
