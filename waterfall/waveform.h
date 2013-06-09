#pragma once
#include "base.h"

struct ID3D10Buffer;
struct ID3D10Effect;
struct ID3D10EffectTechnique;
struct ID3D10InputLayout;
struct ID3D10ShaderResourceView;
struct ID3D10Texture1D;

typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10Effect> ID3D10EffectPtr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
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

private:
	static const char* NAME;

private: // directx stuff
	typedef unsigned short dataTex_t;
	dataTex_t *m_dataTexData;
	float m_waveformColor[4];

	// Direct3d references we use
	ID3D10Texture1DPtr m_dataTex;
	ID3D10EffectPtr m_pEffect;
	ID3D10EffectTechnique* m_pTechnique;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10ShaderResourceViewPtr m_dataView;

protected:
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture();
	virtual HRESULT drawFrameContents();
	virtual void onReceivedFrame(double* frame, unsigned size);
};
