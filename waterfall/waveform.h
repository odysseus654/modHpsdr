#pragma once
#include "scope.h"

struct ID3D10ShaderResourceView;
struct ID3D10SamplerState;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;
typedef unk_ref_t<ID3D10SamplerState> ID3D10SamplerStatePtr;

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
		float viewWidth;
		float viewHeight;
		float texture_width;
		float line_width;
	} m_psGlobals;
#pragma pack(pop)

	// Direct3d references we use
	ID3D10Texture2DPtr m_dataTex;
	ID3D10PixelShaderPtr m_pPS;

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
	virtual HRESULT setupPixelShader();
	virtual void onReceivedFrame(double* frame, unsigned size);
};
