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
	m_bIsComplexInput(true),m_bBottomOrigin(bBottomOrigin)
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

	// calculate texture coordinates
	float scrLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
	float scrRight = scrLeft + (float)m_screenCliWidth;
	float scrTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
	float scrBottom = scrTop - (float)m_screenCliHeight;
	float texTop = m_bBottomOrigin ? 1.0f : 0.0f;
	float texBottom = m_bBottomOrigin ? 0.0f : 1.0f;

	// Now create the vertex buffer.
	TL_FALL_VERTEX vertices[4];
	vertices[0].pos = D3DXVECTOR3(scrLeft, scrTop, 0.0f);
	vertices[0].tex = D3DXVECTOR2(0.0f, texTop);
	vertices[1].pos = D3DXVECTOR3(scrRight, scrTop, 0.0f);
	vertices[1].tex = D3DXVECTOR2(1.0f, texTop);
	vertices[2].pos = D3DXVECTOR3(scrLeft, scrBottom, 0.0f);
	vertices[2].tex = D3DXVECTOR2(0.0f, texBottom);
	vertices[3].pos = D3DXVECTOR3(scrRight, scrBottom, 0.0f);
	vertices[3].tex = D3DXVECTOR2(1.0f, texBottom);

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
		float texLeft = (float)(int(m_screenCliWidth / 2) * -1);// + (float)positionX;
		float texRight = texLeft + (float)m_screenCliWidth;
		float texTop = (float)(m_screenCliHeight / 2);// - (float)positionY;
		float texBottom = texTop - (float)m_screenCliHeight;

		// Now create the vertex buffer.
		TL_FALL_VERTEX vertices[4];
		vertices[0].pos = D3DXVECTOR3(texLeft, texTop, 0.5f);
		vertices[0].tex = D3DXVECTOR2(0.0f, 0.0f);
		vertices[1].pos = D3DXVECTOR3(texRight, texTop, 0.5f);
		vertices[1].tex = D3DXVECTOR2(1.0f, 0.0f);
		vertices[2].pos = D3DXVECTOR3(texLeft, texBottom, 0.5f);
		vertices[2].tex = D3DXVECTOR2(0.0f, 1.0f);
		vertices[3].pos = D3DXVECTOR3(texRight, texBottom, 0.5f);
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

	hR = setupPixelShader();
	if(FAILED(hR)) return hR;

	// render the frame
	ID3D10BlendState* pOrigBlendState;
	float origBlendFactor[4];
	UINT origBlendMask;
	m_pDevice->OMGetBlendState(&pOrigBlendState, origBlendFactor, &origBlendMask);
	m_pDevice->OMSetBlendState(m_pBlendState, NULL, MAXUINT16);

	m_pDevice->DrawIndexed(_countof(VERTEX_INDICES), 0, 0);

	m_pDevice->OMSetBlendState(pOrigBlendState, origBlendFactor, origBlendMask);

	return S_OK;
}
