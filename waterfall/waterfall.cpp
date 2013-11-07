// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waterfall.h"

#include <d3d10_1.h>
#include <d3dx10async.h>

char const * CDirectxWaterfall::NAME = "DirectX waterfall display";
const unsigned short CDirectxWaterfall::VERTEX_INDICES[4] = { 2, 0, 3, 1 };

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

CDirectxWaterfall::CDirectxWaterfall(signals::IBlockDriver* driver):CDirectxBase(driver),
	m_dataTexWidth(0), m_dataTexHeight(0), m_dataTexData(NULL), m_floatStaging(NULL), m_bIsComplexInput(true),
	m_texFormat(DXGI_FORMAT_UNKNOWN), m_bUsingDX9Shader(false), m_dataTexElemSize(0)
{
	m_psRange.minRange = 0.0f;
	m_psRange.maxRange = 0.0f;
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
	attrs.height = addLocalAttr(true, new CAttr_callback<signals::etypShort,CDirectxWaterfall>
		(*this, "height", "Rows of history to display", &CDirectxWaterfall::setHeight, 512));
	attrs.minRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaterfall>
		(*this, "minRange", "Weakest signal to display", &CDirectxWaterfall::setMinRange, -200.0f));
	attrs.maxRange = addLocalAttr(true, new CAttr_callback<signals::etypSingle,CDirectxWaterfall>
		(*this, "maxRange", "Strongest signal to display", &CDirectxWaterfall::setMaxRange, -100.0f));
	attrs.isComplexInput = addLocalAttr(true, new CAttr_callback<signals::etypBoolean,CDirectxWaterfall>
		(*this, "isComplexInput", "Input is based on complex data", &CDirectxWaterfall::setIsComplexInput, true));
}

void CDirectxWaterfall::releaseDevice()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	CDirectxBase::releaseDevice();
	m_dataTex.Release();
	m_pPS.Release();
	m_pVS.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_dataView.Release();
	m_waterfallView.Release();
	m_pVSGlobals.Release();
	m_pPSGlobals.Release();
}

HRESULT CDirectxWaterfall::buildWaterfallTexture(ID3D10Device1Ptr pDevice, ID3D10Texture2DPtr& waterfallTex)
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

	HRESULT hR = pDevice->CreateTexture2D(&waterfallDesc, &waterfallData, waterfallTex.inref());
	if(FAILED(hR)) return hR;

	return S_OK;
}

HRESULT CDirectxWaterfall::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER

	const static D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TL_FALL_VERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TL_FALL_VERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	// check for texture support
	enum { REQUIRED_SUPPORT = D3D10_FORMAT_SUPPORT_TEXTURE2D|D3D10_FORMAT_SUPPORT_SHADER_SAMPLE };
	UINT texSupport;
	if(m_driverLevel >= 10.0f && SUCCEEDED(m_pDevice->CheckFormatSupport(DXGI_FORMAT_R16_FLOAT, &texSupport))
		&& (texSupport & REQUIRED_SUPPORT) == REQUIRED_SUPPORT)
	{
		m_bUsingDX9Shader = false;
		m_texFormat = DXGI_FORMAT_R16_FLOAT;
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
	HRESULT hR = createPixelShaderFromResource(m_bUsingDX9Shader ? _T("waterfall_ps9.cso") : _T("waterfall_ps.cso"), m_pPS);
	if(FAILED(hR)) return hR;

	hR = createVertexShaderFromResource(_T("waterfall_vs.cso"), vdesc, 2, m_pVS, m_pInputLayout);
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

	{
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
	}

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

	ID3D10Texture2DPtr waterfallTex;
	hR = buildWaterfallTexture(m_pDevice, waterfallTex);
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(waterfallTex, NULL, m_waterfallView.inref());
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

		hR = m_pDevice->CreateBuffer(&vsGlobalBufferDesc, &vsGlobalData, m_pVSGlobals.inref());
		if(FAILED(hR)) return hR;
	}

	return initDataTexture();
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

		hR = m_pDevice->CreateTexture2D(&dataDesc, NULL, m_dataTex.inref());
		if(FAILED(hR)) return hR;
	}

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView.inref());
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

		hR = m_pDevice->CreateBuffer(&psGlobalBufferDesc, &psGlobalData, m_pPSGlobals.inref());
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

void CDirectxWaterfall::setIsComplexInput(const unsigned char& bComplex)
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

	if(m_pDevice && m_pVSGlobals)
	{
		// Create an orthographic projection matrix for 2D rendering.
		D3DXMATRIX orthoMatrix;
		D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

		m_pDevice->UpdateSubresource(m_pVSGlobals, 0, NULL, &orthoMatrix, sizeof(orthoMatrix), sizeof(orthoMatrix));
	}

	return S_OK;
}

HRESULT CDirectxWaterfall::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pPS || !m_pVS || !m_dataTexElemSize)
	{
		return E_POINTER;
	}

	D3D10_MAPPED_TEXTURE2D mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex.pData, m_dataTexData, m_dataTexElemSize*m_dataTexWidth*m_dataTexHeight);
	m_dataTex->Unmap(subr);

	// setup the input assembler.
	UINT stride = sizeof(TL_FALL_VERTEX); 
	UINT offset = 0;
	ID3D10Buffer* buf = m_pVertexBuffer;
	m_pDevice->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pDevice->IASetInputLayout(m_pInputLayout);

	// Build our vertex shader
	m_pDevice->VSSetShader(m_pVS);
	m_pDevice->VSSetConstantBuffers(0, 1, m_pVSGlobals.ref());

	// Build our piel shader
	ID3D10ShaderResourceView* psResr[] = { m_dataView, m_waterfallView };
	m_pDevice->PSSetShader(m_pPS);
	if(!m_bUsingDX9Shader) m_pDevice->PSSetConstantBuffers(0, 1, m_pPSGlobals.ref());
	m_pDevice->PSSetShaderResources(0, 2, psResr);

	// render the frame
	m_pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);
	hR = drawText();

	return S_OK;
}

void CDirectxWaterfall::setHeight(const short& newHeight)
{
	if(newHeight == m_dataTexHeight || newHeight <= 0) return;
	Locker lock(m_refLock);
	m_dataTexHeight = newHeight;
	initDataTexture();
}

void CDirectxWaterfall::onReceivedFrame(double* frame, unsigned size)
{
	if(size && size != m_frameWidth)
	{
		if(size && size != m_frameWidth)
		{
			m_frameWidth = size;
			initDataTexture();
		}
	}

	// shift the data up one row
	memmove(m_dataTexData, ((char*)m_dataTexData)+m_dataTexWidth*m_dataTexElemSize,
		m_dataTexElemSize*m_dataTexWidth*(m_dataTexHeight-1));

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
			D3DXFLOAT16* newLine = (D3DXFLOAT16*)m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
			D3DXFloat32To16Array(newLine, m_floatStaging, m_dataTexWidth);
		}
		break;
	case DXGI_FORMAT_R16_UNORM:
		{
			// convert to normalised short
			unsigned short* newLine = (unsigned short*)m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
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
			unsigned char* newLine = (unsigned char*)m_dataTexData + m_dataTexWidth*(m_dataTexHeight-1);
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
