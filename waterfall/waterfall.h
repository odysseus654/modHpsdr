#pragma once
#include "scope.h"

struct ID3D10ShaderResourceView;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;

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
	virtual HRESULT setupPixelShader();
	virtual void onReceivedFrame(double* frame, unsigned size);
private:
	static HRESULT buildWaterfallTexture(ID3D10Device1Ptr pDevice, ID3D10Texture2DPtr& waterfallTex);
};
