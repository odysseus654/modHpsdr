#pragma once
#include <blockImpl.h>
#include "unkref.h"
#include <Unknwn.h>

struct ID3D10DepthStencilState;
struct ID3D10DepthStencilView;
struct ID3D10Device1;
struct ID3D10InputLayout;
struct ID3D10PixelShader;
struct ID3D10RenderTargetView;
struct ID3D10Texture2D;
struct ID3D10VertexShader;
struct IDXGISwapChain;
struct D3D10_INPUT_ELEMENT_DESC;

typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10Device1> ID3D10Device1Ptr;
typedef unk_ref_t<ID3D10InputLayout> ID3D10InputLayoutPtr;
typedef unk_ref_t<ID3D10PixelShader> ID3D10PixelShaderPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10VertexShader> ID3D10VertexShaderPtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;

class CDirectxBase : public CThreadBlockBase
{
public:
	CDirectxBase(signals::IBlockDriver* driver);
	virtual ~CDirectxBase();

private:
	CDirectxBase(const CDirectxBase& other);
	CDirectxBase& operator=(const CDirectxBase& other);

public: // IBlock implementation
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual void Start() { m_incoming.AttachAttributes(); }

	struct
	{
		CAttributeBase* targetWindow;
		CRWAttribute<signals::etypBoolean>* enableVsync;
	} attrs;

	void setTargetWindow(void* const & hWnd);

public:
	class CIncoming : public CSimpleIncomingChild<signals::etypVecDouble>, public signals::IAttributeObserver
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CDirectxBase* parent):CSimpleIncomingChild(parent),m_lastWidthAttr(NULL),m_lastRateAttr(NULL),
			m_lastFreqAttr(NULL) { }
		virtual ~CIncoming();

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual void OnChanged(signals::IAttribute* attr, const void* value);
		virtual void OnDetached(signals::IAttribute* attr);
		void AttachAttributes();

	private:
		virtual void OnConnection(signals::IEPRecvFrom* recv);
		void AttachAttributes(signals::IEPRecvFrom *conn);
		void DetachAttributes();
		signals::IAttribute* m_lastWidthAttr;
		signals::IAttribute* m_lastRateAttr;
		signals::IAttribute* m_lastFreqAttr;
	};

private:
	enum
	{
		DEFAULT_CAPACITY = 2048,
		IN_BUFFER_TIMEOUT = 1000
	};

	CIncoming m_incoming;

protected: // directx stuff
	Lock m_refLock;
	UINT m_frameWidth;
	UINT m_screenCliWidth;
	UINT m_screenCliHeight;
	float m_driverLevel;
	volatile long m_dataRate;
	volatile __int64 m_dataFrequency;
private: // directx stuff
	HWND m_hOutputWin;
	WNDPROC m_pOldWinProc;
	UINT m_screenWinWidth;
	UINT m_screenWinHeight;

protected:	// Direct3d references we use
	ID3D10Device1Ptr m_pDevice;
	ID3D10RenderTargetViewPtr m_pRenderTargetView;
private:	// Direct3d references we use
	IDXGISwapChainPtr m_pSwapChain;
	ID3D10DepthStencilViewPtr m_pDepthView;

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10DepthStencilStatePtr m_pDepthStencilState;

protected:
	HRESULT createPixelShaderFromResource(LPCTSTR fileName, ID3D10PixelShaderPtr& pShader);
	HRESULT createVertexShaderFromResource(LPCTSTR fileName, const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
		UINT NumElements, ID3D10VertexShaderPtr& pShader, ID3D10InputLayoutPtr& pInputLayout);
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture() PURE;
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture() PURE;
	virtual void clearFrame();
	virtual HRESULT drawFrameContents() PURE;
	HRESULT drawFrame();
	virtual void onReceivedFrame(double* frame, unsigned size) PURE;
private:
	HRESULT initDevice();

	virtual void thread_run();
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT WindowProcCatcher(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
