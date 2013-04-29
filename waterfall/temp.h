#pragma once
#include "unkref.h"
#include <Unknwn.h>

struct ID3D10Buffer;
struct ID3D10DepthStencilState;
struct ID3D10DepthStencilView;
struct ID3D10Device;
struct ID3D10Effect;
struct ID3D10EffectTechnique;
struct ID3D10InputLayout;
struct ID3D10RenderTargetView;
struct ID3D10ShaderResourceView;
struct ID3D10Texture2D;
struct IDXGISwapChain;

typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10Device> ID3D10DevicePtr;
typedef unk_ref_t<ID3D10Effect> ID3D10EffectPtr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;

class D3Dtest
{
public:
	inline D3Dtest(HMODULE hModule, HWND hOutputWin)
		:m_hModule(hModule),m_hOutputWin(hOutputWin),m_bEnableVsync(false),
		 m_screenWidth(0),m_screenHeight(0),m_pTechnique(NULL) {}
	~D3Dtest();
	HRESULT init();

protected:
	HRESULT initDevice();
	HRESULT initTexture();

	HMODULE m_hModule;
	HWND m_hOutputWin;
	bool m_bEnableVsync;
	UINT m_screenWidth;
	UINT m_screenHeight;

	// Direct3d references we use
	ID3D10DevicePtr m_pDevice;
	IDXGISwapChainPtr m_pSwapChain;
	ID3D10Texture2DPtr m_dataTex;
	ID3D10EffectPtr m_pEffect;
	ID3D10EffectTechnique* m_pTechnique;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;
	static const unsigned long VERTEX_INDICES[4];

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10DepthStencilStatePtr m_pDepthStencilState;
	ID3D10DepthStencilViewPtr m_pDepthView;
	ID3D10RenderTargetViewPtr m_pRenderTargetView;
	ID3D10ShaderResourceViewPtr m_waterfallView;
	ID3D10ShaderResourceViewPtr m_dataView;
};

