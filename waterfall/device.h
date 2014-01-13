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
#include <mt.h>
#include "unkref.h"
#include <Unknwn.h>

struct ID3D10BlendState;
struct ID3D10Buffer;
struct ID3D10DepthStencilState;
struct ID3D10DepthStencilView;
struct ID3D10Device1;
struct ID3D10InputLayout;
struct ID3D10PixelShader;
struct ID3D10RenderTargetView;
struct ID3D10Resource;
struct ID3D10SamplerState;
struct ID3D10ShaderResourceView;
struct ID3D10Texture2D;
struct ID3D10VertexShader;
//struct IDXGIDevice1;
struct IDXGIFactory;
struct IDXGISwapChain;

typedef unk_ref_t<ID3D10BlendState> ID3D10BlendStatePtr;
typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10Device1> ID3D10Device1Ptr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
typedef unk_ref_t<ID3D10PixelShader> ID3D10PixelShaderPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10SamplerState> ID3D10SamplerStatePtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10VertexShader> ID3D10VertexShaderPtr;
//typedef unk_ref_t<IDXGIDevice1> IDXGIDevice1Ptr;
typedef unk_ref_t<IDXGIFactory> IDXGIFactoryPtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;

struct D3D10_BOX;
struct D3D10_BLEND_DESC;
struct D3D10_BUFFER_DESC;
struct D3D10_DEPTH_STENCIL_DESC;
struct D3D10_DEPTH_STENCIL_VIEW_DESC;
struct D3D10_INPUT_ELEMENT_DESC;
struct D3D10_RENDER_TARGET_VIEW_DESC;
struct D3D10_SAMPLER_DESC;
struct D3D10_SHADER_RESOURCE_VIEW_DESC;
struct D3D10_SUBRESOURCE_DATA;
struct D3D10_TEXTURE2D_DESC;
struct DXGI_SWAP_CHAIN_DESC;
enum DXGI_FORMAT;

class CDirectxBase;
class CVideoDevice;
typedef unk_ref_t<CVideoDevice,CVideoDevice> CVideoDevicePtr;

HRESULT GetVideoDevice(CVideoDevicePtr& pDevice);

class CVideoDevice
{
public:
	inline CVideoDevice():m_driverLevel(0),m_currentFocus(NULL),m_refCount(0) {}
	~CVideoDevice();
	inline float driverLevel() const { return m_driverLevel; }

	bool BeginDraw(CDirectxBase* newFocus, ID3D10Device1*& pDevice, Locker& devLock);
	bool ClearFocus(CDirectxBase* oldFocus);

	void AddRef();
	void Release();

	HRESULT createPixelShaderFromResource(LPCTSTR fileName, ID3D10PixelShaderPtr& pShader);
	HRESULT createVertexShaderFromResource(LPCTSTR fileName, const D3D10_INPUT_ELEMENT_DESC* pInputElementDescs,
		UINT NumElements, ID3D10VertexShaderPtr& pShader, ID3D10InputLayoutPtr& pInputLayout);

	HRESULT CheckFormatSupport(DXGI_FORMAT Format, UINT& pFormatSupport);
	HRESULT CreateBlendState(const D3D10_BLEND_DESC& pBlendStateDesc, ID3D10BlendStatePtr& ppBlendState);
	HRESULT CreateBuffer(const D3D10_BUFFER_DESC& pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData,
		ID3D10BufferPtr& ppBuffer);
	HRESULT CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC& pDepthStencilDesc,
		ID3D10DepthStencilStatePtr& ppDepthStencilState);
	HRESULT CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
		ID3D10DepthStencilViewPtr& ppDepthStencilView);
	HRESULT CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
		ID3D10RenderTargetViewPtr& ppRTView);
	HRESULT CreateSamplerState(const D3D10_SAMPLER_DESC& pSamplerDesc, ID3D10SamplerStatePtr& ppSamplerState);
	HRESULT CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
		ID3D10ShaderResourceViewPtr& ppSRView);
	HRESULT CreateSwapChain(const DXGI_SWAP_CHAIN_DESC& pDesc, IDXGISwapChainPtr& ppSwapChain);
	HRESULT CreateTexture2D(const D3D10_TEXTURE2D_DESC &pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData,
		ID3D10Texture2DPtr& ppTexture2D);
	void UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox,
		const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

private:
	ID3D10Device1Ptr m_pDevice;
//	IDXGIDevice1Ptr m_pDxgiDevice;
	IDXGIFactoryPtr m_pFactory;
	float m_driverLevel;
	Lock m_drawLock;
	CDirectxBase* m_currentFocus;
	volatile unsigned int m_refCount;

	HRESULT createDevice();

	friend HRESULT GetVideoDevice(CVideoDevicePtr& pDevice);
};
