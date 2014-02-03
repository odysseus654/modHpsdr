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
#pragma once
#include "base.h"
#include "font.h"

class CDirectxScope : public CDirectxBase
{
public:
	CDirectxScope(signals::IBlockDriver* driver, bool bBottomOrigin);
	virtual ~CDirectxScope();

	HRESULT createVertexRect(float top, float left, float bottom, float right, float z,
		bool bBottomOrigin, ID3D10BufferPtr& vertex) const;
	HRESULT drawMonoBitmap(ID3D10Device1* pDevice, const ID3D10ShaderResourceViewPtr &image,
		const ID3D10BufferPtr& vertex, const ID3D10BufferPtr& color);

private:
	CDirectxScope(const CDirectxScope& other);
	CDirectxScope& operator=(const CDirectxScope& other);

public: // IBlock implementation
	virtual void setIsComplexInput(bool bComplex);

protected: // directx stuff
	bool m_bIsComplexInput;

private: // directx stuff
	// Direct3d references we use
	bool m_bBottomOrigin;
	bool m_bDrawingFonts;
	bool m_bBackfaceWritten;
	ID3D10VertexShaderPtr m_pVS;
	ID3D10PixelShaderPtr m_pBitmapPS;
	ID3D10SamplerStatePtr m_pBitmapSampler;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;
	ID3D10BlendStatePtr m_pBlendState;
	static const unsigned short VERTEX_INDICES[4];

	// shader resource references
	ID3D10BufferPtr m_pVSGlobals;

	CFont m_majFont, m_dotFont, m_minFont;

protected:
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
//	virtual HRESULT initDataTexture() PURE;
	virtual HRESULT drawFrameContents(ID3D10Device1* pDevice);
	void BackfaceWritten(ID3D10Device1* pDevice);
	virtual HRESULT drawRect(ID3D10Device1* pDevice);
//	virtual void onReceivedFrame(double* frame, unsigned size);
	HRESULT drawText(ID3D10Device1* pDevice);
};
