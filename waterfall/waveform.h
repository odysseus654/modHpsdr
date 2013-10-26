#pragma once
#include "base.h"

struct ID3D10Buffer;
struct ID3D10ShaderResourceView;
struct ID3D10Texture1D;

typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;
typedef unk_ref_t<ID3D10Texture1D> ID3D10Texture1DPtr;

class CDirectxWaveform : public CDirectxBase
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
	typedef unsigned short dataTex_t;
	dataTex_t *m_dataTexData;
	float m_waveformColor[4];
	float* m_floatStaging;

#pragma pack(push, 4)
	struct
	{
		float minRange;
		float maxRange;
	} m_psRange;
#pragma pack(pop)

	// Direct3d references we use
	ID3D10Texture1DPtr m_dataTex;
	ID3D10PixelShaderPtr m_pPS;
	ID3D10VertexShaderPtr m_pVS;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;

	// shader resource references
	ID3D10ShaderResourceViewPtr m_dataView;
	ID3D10BufferPtr m_pVSOrthoGlobals;
	ID3D10BufferPtr m_pVSRangeGlobals;
	ID3D10BufferPtr m_pPSColorGlobals;

protected:
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture();
	virtual HRESULT drawFrameContents();
	virtual void onReceivedFrame(double* frame, unsigned size);
};
