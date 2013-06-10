// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waveform.h"

#include <d3d10.h>
#include <d3dx10async.h>
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3dx10.lib")

char const * CDirectxWaveform::NAME = "DirectX waveform display";

// ---------------------------------------------------------------------------- class CDirectxWaveform

CDirectxWaveform::CDirectxWaveform(signals::IBlockDriver* driver):CDirectxBase(driver),
	m_dataTexData(NULL),m_pTechnique(NULL),m_minRange(0.0f),m_maxRange(0.0f)
{
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
	if(m_floatStaging)
	{
		delete [] m_floatStaging;
		m_floatStaging = NULL;
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
	m_pEffect.Release();
	m_pVertexBuffer.Release();
	m_pVertexIndexBuffer.Release();
	m_pInputLayout.Release();
	m_dataView.Release();
}

HRESULT CDirectxWaveform::initTexture()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	// Load the shader in from the file.
	static LPCTSTR filename = _T("waveform.fx");
	std::string errors;
	HRESULT hR = compileResource(filename, m_pEffect, errors);
	if(FAILED(hR)) return hR;

	m_pTechnique = m_pEffect->GetTechniqueByName("WaveformTechnique");
	D3D10_PASS_DESC PassDesc;
	m_pTechnique->GetPassByIndex(0)->GetDesc(&PassDesc);

	D3D10_INPUT_ELEMENT_DESC vdesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(TL_FORM_VERTEX, pos), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"RANGE", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(TL_FORM_VERTEX, mag), D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(TL_FORM_VERTEX, tex), D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	hR = m_pDevice->CreateInputLayout(vdesc, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, m_pInputLayout.inref());
	if(FAILED(hR)) return hR;

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

	// Create an orthographic projection matrix for 2D rendering.
	D3DXMATRIX orthoMatrix;
	D3DXMatrixOrthoLH(&orthoMatrix, (float)m_screenCliWidth, (float)m_screenCliHeight, 0.0f, 1.0f);

	ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("orthoMatrix");
	if(!pVar) return E_FAIL;
	ID3D10EffectMatrixVariable* pVarMat = pVar->AsMatrix();
	if(!pVarMat || !pVarMat->IsValid()) return E_FAIL;
	hR = pVarMat->SetMatrix(orthoMatrix);
	if(FAILED(hR)) return hR;

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
	if(m_floatStaging) delete [] m_floatStaging;

//	m_dataTexWidth = 1024;
	m_dataTexData = new dataTex_t[m_dataTexWidth];
	m_floatStaging = new float[m_dataTexWidth];
	memset(m_dataTexData, 0, sizeof(dataTex_t)*m_dataTexWidth);

	D3D10_TEXTURE1D_DESC dataDesc;
	memset(&dataDesc, 0, sizeof(dataDesc));
	dataDesc.Width = m_dataTexWidth;
	dataDesc.MipLevels = 1;
	dataDesc.ArraySize = 1;
	dataDesc.Format = DXGI_FORMAT_R16_FLOAT;
	dataDesc.Usage = D3D10_USAGE_DYNAMIC;
	dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	dataDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

	HRESULT hR = m_pDevice->CreateTexture1D(&dataDesc, NULL, m_dataTex.inref());
	if(FAILED(hR)) return hR;

	hR = m_pDevice->CreateShaderResourceView(m_dataTex, NULL, m_dataView.inref());
	if(FAILED(hR)) return hR;

	ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("waveformValues");
	if(!pVar) return E_FAIL;
	ID3D10EffectShaderResourceVariable* pVarRes = pVar->AsShaderResource();
	if(!pVarRes || !pVarRes->IsValid()) return E_FAIL;
	hR = pVarRes->SetResource(m_dataView);
	if(FAILED(hR)) return hR;

	pVar = m_pEffect->GetVariableByName("waveformColor");
	if(!pVar) return E_FAIL;
	ID3D10EffectScalarVariable* pVarScal = pVar->AsScalar();
	if(!pVarScal || !pVarScal->IsValid()) return E_FAIL;
	hR = pVarScal->SetFloatArray(m_waveformColor, 0, _countof(m_waveformColor));
	if(FAILED(hR)) return hR;

	pVar = m_pEffect->GetVariableByName("minRange");
	if(!pVar) return E_FAIL;
	pVarScal = pVar->AsScalar();
	if(!pVarScal || !pVarScal->IsValid()) return E_FAIL;
	hR = pVarScal->SetFloat(m_minRange);
	if(FAILED(hR)) return hR;

	pVar = m_pEffect->GetVariableByName("maxRange");
	if(!pVar) return E_FAIL;
	pVarScal = pVar->AsScalar();
	if(!pVarScal || !pVarScal->IsValid()) return E_FAIL;
	hR = pVarScal->SetFloat(m_maxRange);
	if(FAILED(hR)) return hR;

	return S_OK;
}

void CDirectxWaveform::setMinRange(const float& newMin)
{
	if(newMin != m_minRange)
	{
		m_minRange = newMin;
		if(!m_pEffect) return;
		ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("minRange");
		if(!pVar) return;
		ID3D10EffectScalarVariable* pVarScal = pVar->AsScalar();
		if(!pVarScal || !pVarScal->IsValid()) return;
		pVarScal->SetFloat(m_minRange);
	}
}

void CDirectxWaveform::setMaxRange(const float& newMax)
{
	if(newMax != m_maxRange)
	{
		m_maxRange = newMax;
		if(!m_pEffect) return;
		ID3D10EffectVariable* pVar = m_pEffect->GetVariableByName("maxRange");
		if(!pVar) return;
		ID3D10EffectScalarVariable* pVarScal = pVar->AsScalar();
		if(!pVarScal || !pVarScal->IsValid()) return;
		pVarScal->SetFloat(m_maxRange);
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

HRESULT CDirectxWaveform::drawFrameContents()
{
	// ASSUMES m_refLock IS HELD BY CALLER
	if(!m_pDevice || !m_dataTex || !m_dataTexData || !m_pTechnique)
	{
		return E_POINTER;
	}

	void* mappedDataTex;
	memset(&mappedDataTex, 0, sizeof(mappedDataTex));
	UINT subr = D3D10CalcSubresource(0, 0, 0);
	HRESULT hR = m_dataTex->Map(subr, D3D10_MAP_WRITE_DISCARD, 0, &mappedDataTex);
	if(FAILED(hR)) return hR;
	memcpy(mappedDataTex, m_dataTexData, sizeof(dataTex_t)*m_dataTexWidth);
	m_dataTex->Unmap(subr);

	// setup the input assembler.
	UINT stride = sizeof(TL_FORM_VERTEX); 
	UINT offset = 0;
	ID3D10Buffer* buf = m_pVertexBuffer;
	m_pDevice->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
	m_pDevice->IASetIndexBuffer(m_pVertexIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	m_pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
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

		m_pDevice->DrawIndexed(m_screenCliWidth, 0, 0);
	}

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

	// convert to float
	for(unsigned i = 0; i < m_dataTexWidth; i++) m_floatStaging[i] = float(frame[i]);

	// write the new line out
	D3DXFloat32To16Array((D3DXFLOAT16*)(m_dataTexData), m_floatStaging, m_dataTexWidth);

	VERIFY(SUCCEEDED(drawFrame()));
}
