#pragma once
#include "base.h"

enum DXGI_FORMAT;
struct ID3D10Buffer;
struct ID3D10ShaderResourceView;
struct ID3D10Texture1D;

typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;
typedef unk_ref_t<ID3D10Texture1D> ID3D10Texture1DPtr;

class CDirectxWaterfall : public CDirectxBase
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
	void setIsComplexInput(const unsigned char& bComplex);

	struct
	{
		CAttributeBase* height;
		CAttributeBase* minRange;
		CAttributeBase* maxRange;
		CAttributeBase* isComplexInput;
	} attrs;

private:
	static const char* NAME;

private: // directx stuff
	DXGI_FORMAT m_texFormat;
	bool m_bUsingDX9Shader;
	UINT m_dataTexWidth, m_dataTexHeight;
	void *m_dataTexData;
	size_t m_dataTexElemSize;
	float* m_floatStaging;
	bool m_bIsComplexInput;

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
	ID3D10VertexShaderPtr m_pVS;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;
	static const unsigned short VERTEX_INDICES[4];

	// shader resource references
	ID3D10ShaderResourceViewPtr m_waterfallView;
	ID3D10ShaderResourceViewPtr m_dataView;
	ID3D10BufferPtr m_pVSGlobals;
	ID3D10BufferPtr m_pPSGlobals;

protected:
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture();
	virtual HRESULT drawFrameContents();
	virtual void onReceivedFrame(double* frame, unsigned size);
private:
	static HRESULT buildWaterfallTexture(ID3D10Device1Ptr pDevice, ID3D10Texture2DPtr& waterfallTex);
};
