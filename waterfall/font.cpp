#include "stdafx.h"
#include "font.h"

#include "scope.h"
#include "unkref.h"
#include <Unknwn.h>

#include <d3d10_1.h>
#include <d3dx10.h>

typedef unk_ref_t<ID3D10Device1> ID3D10Device1Ptr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;

CFont::CFont(INT Height, UINT Width, UINT Weight, UINT Style, UINT Charset, UINT OutputPrecision,
	UINT Quality, UINT PitchAndFamily, LPCTSTR pFaceName)
	:m_hFont(NULL)
{
	memset(&m_lf, 0, sizeof(m_lf));
	m_lf.lfHeight = Height;
	m_lf.lfWidth = Width;
	m_lf.lfWeight = Weight;
	m_lf.lfItalic = !!(Style & stItalic);
	m_lf.lfUnderline = !!(Style & stUnderline);
	m_lf.lfStrikeOut = !!(Style & stStrikeout);
	m_lf.lfCharSet = Charset;
	m_lf.lfOutPrecision = OutputPrecision;
	m_lf.lfQuality = Quality;
	m_lf.lfPitchAndFamily = PitchAndFamily;
	_tcsncpy_s(m_lf.lfFaceName, pFaceName, LF_FACESIZE-1);
}

CFont::~CFont()
{
	if(m_hFont) DeleteObject(m_hFont);
}

HRESULT CFont::CalcRect(LPCTSTR szText, UINT uiLen, RECT* rect)
{
	if(!m_hFont)
	{
		m_hFont = CreateFontIndirect(&m_lf);
		if(!m_hFont) return HRESULT_FROM_WIN32(GetLastError());
	}

	HDC hDC = GetDC(NULL);
	if(!hDC) return HRESULT_FROM_WIN32(GetLastError());

	RECT textRect;
	memset(&textRect, 0, sizeof(textRect));

	if(!::DrawText(hDC, szText, uiLen, rect, DT_CALCRECT | DT_LEFT | DT_SINGLELINE | DT_TOP))
	{
		HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
		ReleaseDC(NULL, hDC);
		return hR;
	}

	ReleaseDC(NULL, hDC);
	return S_OK;
}

HRESULT CFont::DrawText(CDirectxScope& pDevice, LPCTSTR szText, UINT uiLen, RECT* rect, const D3DXCOLOR& color)
{
	if(!m_hFont)
	{
		m_hFont = CreateFontIndirect(&m_lf);
		if(!m_hFont) return HRESULT_FROM_WIN32(GetLastError());
	}

	HDC hDC = GetDC(NULL);
	if(!hDC) return HRESULT_FROM_WIN32(GetLastError());

	struct
	{
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[256];
	} bmi;

	int width = rect->right - rect->left + 1;
	int height = rect->bottom - rect->top + 1;

	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 8;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biClrUsed = 256;

	for(unsigned clr=0; clr < 256; clr++)
	{
		bmi.bmiColors[clr].rgbBlue = bmi.bmiColors[clr].rgbGreen = bmi.bmiColors[clr].rgbRed = clr;
	}

	void* pBits;
	HBITMAP hBMP = CreateDIBSection(hDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
	if(!hBMP)
	{
		HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
		ReleaseDC(NULL, hDC);
		return hR;
	}

	HDC hMemDC = CreateCompatibleDC(hDC);
	if(!hMemDC)
	{
		HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
		ReleaseDC(NULL, hDC);
		DeleteObject(hBMP);
		return hR;
	}

	HGDIOBJ hOldBMP = SelectObject(hMemDC, hBMP);
	HGDIOBJ hOldFont = SelectObject(hMemDC, m_hFont);

	RECT gdiRect;
	memset(&gdiRect, 0, sizeof(gdiRect));
	gdiRect.right = width;
	gdiRect.bottom = height;
	if(!::DrawText(hMemDC, szText, uiLen, &gdiRect, DT_LEFT | DT_SINGLELINE | DT_TOP))
	{
		HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
		DeleteDC(hMemDC);
		ReleaseDC(NULL, hDC);
		DeleteObject(hBMP);
		return hR;
	}

	SelectObject(hMemDC, hOldBMP);
	SelectObject(hMemDC, hOldFont);

	DeleteDC(hMemDC);
	ReleaseDC(NULL, hDC);

	ID3D10Texture2DPtr dataTex;
	{
		D3D10_TEXTURE2D_DESC dataDesc;
		memset(&dataDesc, 0, sizeof(dataDesc));
		dataDesc.Width = width;
		dataDesc.Height = height;
		dataDesc.MipLevels = 1;
		dataDesc.ArraySize = 1;
		dataDesc.Format = DXGI_FORMAT_R8_UNORM;
		dataDesc.SampleDesc.Count = 1;
		dataDesc.SampleDesc.Quality = 0;
		dataDesc.Usage = D3D10_USAGE_IMMUTABLE;
		dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

		UINT pitch = width;
		if(pitch%4) pitch += 4-pitch%4;
		D3D10_SUBRESOURCE_DATA fontData;
		memset(&fontData, 0, sizeof(fontData));
		fontData.pSysMem = pBits;
		fontData.SysMemPitch = pitch;

		HRESULT hR = pDevice.device()->CreateTexture2D(&dataDesc, &fontData, dataTex.inref());
		if(FAILED(hR)) return hR;
	}
	DeleteObject(hBMP);

	ID3D10BufferPtr pVertex;
	HRESULT hR = pDevice.createVertexRect((float)rect->top, (float)rect->left, (float)rect->bottom, (float)rect->right,
		0.25f, false, pVertex);
	if(FAILED(hR)) return hR;

	ID3D10ShaderResourceViewPtr bitmapView;
	hR = pDevice.device()->CreateShaderResourceView(dataTex, NULL, bitmapView.inref());
	if(FAILED(hR)) return hR;

	ID3D10BufferPtr pColor;
	{
		D3D10_BUFFER_DESC vsColorBufferDesc;
		memset(&vsColorBufferDesc, 0, sizeof(vsColorBufferDesc));
		vsColorBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		vsColorBufferDesc.ByteWidth = sizeof(color);
		vsColorBufferDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	//	vsColorBufferDesc.CPUAccessFlags = 0;
	//	vsColorBufferDesc.MiscFlags = 0;

		D3D10_SUBRESOURCE_DATA vsGlobalData;
		memset(&vsGlobalData, 0, sizeof(vsGlobalData));
		vsGlobalData.pSysMem = &color;

		hR = pDevice.device()->CreateBuffer(&vsColorBufferDesc, &vsGlobalData, pColor.inref());
		if(FAILED(hR)) return hR;
	}

	return pDevice.drawMonoBitmap(bitmapView, pVertex, pColor);
}
