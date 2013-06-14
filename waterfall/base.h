#pragma once
#include <blockImpl.h>
#include "unkref.h"
#include <Unknwn.h>

struct ID3D10DepthStencilState;
struct ID3D10DepthStencilView;
struct ID3D10Device;
struct ID3D10Effect;
struct ID3D10RenderTargetView;
struct ID3D10Texture2D;
struct IDXGISwapChain;

typedef unk_ref_t<ID3D10DepthStencilState> ID3D10DepthStencilStatePtr;
typedef unk_ref_t<ID3D10DepthStencilView> ID3D10DepthStencilViewPtr;
typedef unk_ref_t<ID3D10Device> ID3D10DevicePtr;
typedef unk_ref_t<ID3D10Effect> ID3D10EffectPtr;
typedef unk_ref_t<ID3D10RenderTargetView> ID3D10RenderTargetViewPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;

class CDirectxBase : public CBlockBase
{
public:
	CDirectxBase(signals::IBlockDriver* driver);
	virtual ~CDirectxBase();

private:
	CDirectxBase(const CDirectxBase& other);
	CDirectxBase& operator=(const CDirectxBase& other);

public: // IBlock implementation
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }

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
		inline CIncoming(CDirectxBase* parent):CSimpleIncomingChild(parent),m_lastWidthAttr(NULL),m_bAttached(false) { }
		virtual ~CIncoming();

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual void OnChanged(const char* name, signals::EType type, const void* value);
		virtual void OnDetached(const char* name);

	private:
		virtual void OnConnection(signals::IEPRecvFrom* recv);
		signals::IAttribute* m_lastWidthAttr;
		bool m_bAttached;
	};

private:
	enum
	{
		DEFAULT_CAPACITY = 2048,
		IN_BUFFER_TIMEOUT = 1000
	};

	CIncoming m_incoming;
	Thread<CDirectxBase*> m_dataThread;
	bool m_bDataThreadEnabled;

protected: // directx stuff
	Lock m_refLock;
	UINT m_dataTexWidth;
	UINT m_screenCliWidth;
	UINT m_screenCliHeight;
private: // directx stuff
	HWND m_hOutputWin;
	WNDPROC m_pOldWinProc;
	UINT m_screenWinWidth;
	UINT m_screenWinHeight;

protected:	// Direct3d references we use
	ID3D10DevicePtr m_pDevice;
private:	// Direct3d references we use
	IDXGISwapChainPtr m_pSwapChain;
	ID3D10DepthStencilViewPtr m_pDepthView;

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10DepthStencilStatePtr m_pDepthStencilState;
	ID3D10RenderTargetViewPtr m_pRenderTargetView;

protected:
	HRESULT compileResource(LPCTSTR fileName, ID3D10EffectPtr& pEffect, std::string& errors);
	virtual void buildAttrs();
	virtual void releaseDevice();
	virtual HRESULT initTexture() PURE;
	virtual HRESULT resizeDevice();
	virtual HRESULT initDataTexture() PURE;
	virtual HRESULT drawFrameContents() PURE;
	HRESULT drawFrame();
	virtual void onReceivedFrame(double* frame, unsigned size) PURE;
private:
	HRESULT initDevice();

	static void process_thread(CDirectxBase* owner);
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT WindowProcCatcher(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
