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
#pragma once
#include "scope.h"

class CDirectxWaveform : public CDirectxScope
{
public:
	CDirectxWaveform(signals::IBlockDriver* driver);
	virtual ~CDirectxWaveform();

private:
	CDirectxWaveform(const CDirectxWaveform& other);
	CDirectxWaveform& operator=(const CDirectxWaveform& other);

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	void setMinRange(const float& newMin);
	void setMaxRange(const float& newMax);

	struct
	{
		CAttributeBase* minRange;
		CAttributeBase* maxRange;
	} attrs;

private:
	static const char* NAME;

private: // directx stuff
	bool m_bUsingDX9Shader;
	DXGI_FORMAT m_texFormat;
	UINT m_dataTexWidth;
	void *m_dataTexData;
	size_t m_dataTexElemSize;

#pragma pack(push, 4)
	struct
	{
		float minRange;
		float maxRange;
		float unused1, unused2;
	} m_psRange;

	struct
	{
		float m_waveformColor[4];
		float m_shadowColor[4];
		float viewWidth;
		float viewHeight;
		float texture_width;
		float line_width;
	} m_psGlobals;
#pragma pack(pop)

	// Direct3d references we use
	ID3D10Texture2DPtr m_dataTex;
	ID3D10PixelShaderPtr m_pPS;
	ID3D10PixelShaderPtr m_pShadowPS;

	// shader resource references
	ID3D10ShaderResourceViewPtr m_dataView;
	ID3D10BufferPtr m_pPSRangeGlobals;
	ID3D10BufferPtr m_pPSGlobals;
	ID3D10SamplerStatePtr m_pPSSampler;

protected:
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT resizeDevice();
	virtual HRESULT initTexture();
	virtual HRESULT initDataTexture();
	virtual void clearFrame();
	virtual HRESULT preDrawFrame();
	virtual HRESULT drawRect();
	virtual void onReceivedFrame(double* frame, unsigned size);
};
