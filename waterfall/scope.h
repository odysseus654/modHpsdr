#pragma once
#include "base.h"
#include "font.h"

enum DXGI_FORMAT;
struct ID3D10BlendState;
struct ID3D10Buffer;
struct ID3D10SamplerState;
struct ID3D10ShaderResourceView;

typedef unk_ref_t<ID3D10BlendState> ID3D10BlendStatePtr;
typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10SamplerState> ID3D10SamplerStatePtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;

class CDirectxScope : public CDirectxBase
{
public:
	CDirectxScope(signals::IBlockDriver* driver, bool bBottomOrigin);
	virtual ~CDirectxScope();

	HRESULT createVertexRect(float top, float left, float bottom, float right, float z,
		bool bBottomOrigin, ID3D10BufferPtr& vertex) const;
	HRESULT drawMonoBitmap(const ID3D10ShaderResourceViewPtr &image, const ID3D10BufferPtr& vertex,
		const ID3D10BufferPtr& color);

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
	bool m_bDrawingFonts;
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
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture();
	virtual HRESULT resizeDevice();
//	virtual HRESULT initDataTexture() PURE;
	virtual HRESULT drawFrameContents();
	virtual HRESULT preDrawFrame() PURE;
	virtual HRESULT drawRect();
//	virtual void onReceivedFrame(double* frame, unsigned size);
	HRESULT drawText();
};
