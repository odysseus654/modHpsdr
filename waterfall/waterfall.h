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
#include "scope.h"

class CDirectxWaterfall : public CDirectxScope
{
public:
	CDirectxWaterfall(signals::IBlockDriver* driver);
	virtual ~CDirectxWaterfall();

private:
	CDirectxWaterfall(const CDirectxWaterfall& other);
	CDirectxWaterfall& operator=(const CDirectxWaterfall& other);

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	void setHeight(const short& height);
	void setMinRange(const float& newMin);
	void setMaxRange(const float& newMax);

	struct
	{
		CAttributeBase* height;
		CAttributeBase* minRange;
		CAttributeBase* maxRange;
	} attrs;

private:
	static const char* NAME;

private: // directx stuff
	bool m_bUsingDX9Shader;
	DXGI_FORMAT m_texFormat;
	UINT m_dataTexWidth, m_dataTexHeight;
	void *m_dataTexData;
	size_t m_dataTexElemSize;
	float* m_floatStaging;

#pragma pack(push, 4)
	struct
	{
		float minRange;
		float maxRange;
		float unused1, unused2;
	} m_psRange;
#pragma pack(pop)

	// Direct3d references we use
	ID3D10Texture2DPtr m_dataTex;
	ID3D10PixelShaderPtr m_pPS;
	ID3D10SamplerStatePtr m_pColorSampler;
	ID3D10SamplerStatePtr m_pValSampler;

	// shader resource references
	ID3D10ShaderResourceViewPtr m_waterfallView;
	ID3D10ShaderResourceViewPtr m_dataView;
	ID3D10BufferPtr m_pPSGlobals;

protected:
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT initDataTexture();
	virtual HRESULT preDrawFrame();
	virtual HRESULT drawRect(ID3D10Device1* pDevice);
	virtual void onReceivedFrame(double* frame, unsigned size);
private:
	static HRESULT buildWaterfallTexture(const CVideoDevicePtr& pDevice, ID3D10Texture2DPtr& waterfallTex);
};
