#pragma once
#include <blockImpl.h>
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
struct ID3D10Texture1D;
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
typedef unk_ref_t<ID3D10Texture1D> ID3D10Texture1DPtr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<IDXGISwapChain> IDXGISwapChainPtr;

class CDirectxWaterfallDriver : public signals::IBlockDriver
{
public:
	inline CDirectxWaterfallDriver() {}
	virtual ~CDirectxWaterfallDriver() {}

private:
	CDirectxWaterfallDriver(const CDirectxWaterfallDriver& other);
	CDirectxWaterfallDriver operator=(const CDirectxWaterfallDriver& other);

public:
	virtual const char* Name()			{ return NAME; }
	virtual const char* Description()	{ return DESCR; }
	virtual BOOL canCreate()			{ return true; }
	virtual BOOL canDiscover()			{ return false; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual signals::IBlock* Create();
	virtual const unsigned char* Fingerprint() { return FINGERPRINT; }

protected:
	static const char* NAME;
	static const char* DESCR;
	static const unsigned char FINGERPRINT[];
};

class CDirectxWaterfall : public signals::IBlock, public CAttributesBase, protected CRefcountObject
{
public:
	CDirectxWaterfall(signals::IBlockDriver* driver);
	virtual ~CDirectxWaterfall();

private:
	CDirectxWaterfall(const CDirectxWaterfall& other);
	CDirectxWaterfall& operator=(const CDirectxWaterfall& other);

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
	virtual const char* Name()				{ return NAME; }
	virtual unsigned NodeId(char* /* buff */ , unsigned /* availChar */) { return 0; }
	virtual signals::IBlockDriver* Driver()	{ return m_driver; }
	virtual signals::IBlock* Parent()		{ return NULL; }
	virtual signals::IAttributes* Attributes() { return this; }
	virtual unsigned Children(signals::IBlock** /* blocks */, unsigned /* availBlocks */) { return 0; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP);
	virtual unsigned Outgoing(signals::IOutEndpoint** /* ep */ , unsigned /* availEP */) { return 0; }
	virtual void Start()					{ }
	virtual void Stop()						{ }

	struct
	{
		CAttributeBase* targetWindow;
		CRWAttribute<signals::etypBoolean>* enableVsync;
		CAttributeBase* height;
	} attrs;

	void setTargetWindow(HWND hWnd);
	void setHeight(short height);

public:
	class CIncoming : public CInEndpointBase, public CAttributesBase, public signals::IAttributeObserver
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CDirectxWaterfall* parent):m_parent(parent),m_lastWidthAttr(NULL),m_bAttached(false) { }
		virtual ~CIncoming();

	protected:
		enum { DEFAULT_BUFSIZE = 4096 };
		CDirectxWaterfall* m_parent;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::EType Type()				{ return signals::etypVecDouble; }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual void OnChanged(const char* name, signals::EType type, const void* value);
		virtual void OnDetached(const char* name);

		virtual signals::IEPBuffer* CreateBuffer()
		{
			signals::IEPBuffer* buff = new CEPBuffer<signals::etypVecDouble>(DEFAULT_BUFSIZE);
			buff->AddRef(NULL);
			return buff;
		}

	protected:
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

	static const char* NAME;

	signals::IBlockDriver* m_driver;
	CIncoming m_incoming;
	Thread<CDirectxWaterfall*> m_dataThread;
	bool m_bDataThreadEnabled;
	Lock m_buffLock;
	unsigned m_bufSize;

private: // directx stuff
	Lock m_refLock;
	HWND m_hOutputWin;
	WNDPROC m_pOldWinProc;
	UINT m_screenWinWidth;
	UINT m_screenWinHeight;
	UINT m_screenCliWidth;
	UINT m_screenCliHeight;

	typedef unsigned short dataTex_t;
	UINT m_dataTexWidth;
	UINT m_dataTexHeight;
	dataTex_t *m_dataTexData;

	// Direct3d references we use
	ID3D10DevicePtr m_pDevice;
	IDXGISwapChainPtr m_pSwapChain;
	ID3D10Texture2DPtr m_dataTex;
	ID3D10EffectPtr m_pEffect;
	ID3D10DepthStencilViewPtr m_pDepthView;
	ID3D10EffectTechnique* m_pTechnique;

	// vertex stuff
	ID3D10BufferPtr m_pVertexBuffer;
	ID3D10BufferPtr m_pVertexIndexBuffer;
	ID3D10InputLayoutPtr m_pInputLayout;
	static const unsigned long VERTEX_INDICES[4];

	// these are mapped resources, we don't reference them other than managing their lifetime
	ID3D10DepthStencilStatePtr m_pDepthStencilState;
	ID3D10RenderTargetViewPtr m_pRenderTargetView;
	ID3D10ShaderResourceViewPtr m_waterfallView;
	ID3D10ShaderResourceViewPtr m_dataView;

protected:
	typedef fastdelegate::FastDelegate4<HWND,UINT,WPARAM,LPARAM,LRESULT> windowproc_delegate_type;
	const windowproc_delegate_type m_pWinprocDelgate;

	void buildAttrs();
	void releaseDevice();
	HRESULT initDevice();
	static HRESULT buildWaterfallTexture(ID3D10DevicePtr pDevice, ID3D10Texture1DPtr& waterfallTex);
	HRESULT initTexture();
	HRESULT initDataTexture();
	HRESULT resizeDevice();
	HRESULT drawFrame();

	void onReceivedFrame(double* frame, unsigned size);
	static void process_thread(CDirectxWaterfall* owner);
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
