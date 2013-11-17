#pragma once
#include "base.h"

enum DXGI_FORMAT;
struct ID3D10BlendState;
struct ID3D10Buffer;
typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10BlendState> ID3D10BlendStatePtr;

class CDirectxScope : public CDirectxBase
{
public:
	CDirectxScope(signals::IBlockDriver* driver, bool bBottomOrigin);
	virtual ~CDirectxScope();

private:
	CDirectxScope(const CDirectxScope& other);
	CDirectxScope& operator=(const CDirectxScope& other);

public: // IBlock implementation
	void setIsComplexInput(const unsigned char& bComplex);

	struct
	{
		CAttributeBase* isComplexInput;
	} attrs;

protected: // directx stuff
	bool m_bIsComplexInput;

private: // directx stuff
	// Direct3d references we use
	bool m_bBottomOrigin;
	ID3D10VertexShaderPtr m_pVS;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;
	ID3D10BlendStatePtr m_pBlendState;
	static const unsigned short VERTEX_INDICES[4];

	// shader resource references
	ID3D10BufferPtr m_pVSGlobals;

protected:
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
//	virtual HRESULT initDataTexture() PURE;
	virtual HRESULT drawFrameContents();
	virtual HRESULT preDrawFrame() PURE;
	virtual HRESULT setupPixelShader() PURE;
//	virtual void onReceivedFrame(double* frame, unsigned size);
};
