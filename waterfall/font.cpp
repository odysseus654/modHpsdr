#include "stdafx.h"
#include "font.h"

#include "scope.h"

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

	for(unsigned clr=0; clr < 256; clr++)
	{
		m_bmiColors[clr].rgbBlue = m_bmiColors[clr].rgbGreen = m_bmiColors[clr].rgbRed = clr;
	}
}

CFont::~CFont()
{
	if(m_hFont) DeleteObject(m_hFont);
}

HRESULT CFont::CalcSize(LPCTSTR szText, UINT uiLen, SIZE& size)
{
	std::basic_string<TCHAR> text(szText, uiLen);
	TSizeMap::const_iterator lookup = m_sizeMap.find(text);
	if(lookup != m_sizeMap.end())
	{
		size.cx = lookup->second.cx;
		size.cy = lookup->second.cy;
		return S_OK;
	}

	if(!m_hFont)
	{
		m_hFont = CreateFontIndirect(&m_lf);
		if(!m_hFont) return HRESULT_FROM_WIN32(GetLastError());
	}

	HDC hDC = GetDC(NULL);
	if(!hDC) return HRESULT_FROM_WIN32(GetLastError());

	RECT rect;
	memset(&rect, 0, sizeof(rect));

	HGDIOBJ hOldFont = SelectObject(hDC, m_hFont);

	if(!::DrawText(hDC, szText, uiLen, &rect, DT_CALCRECT | DT_LEFT | DT_SINGLELINE | DT_TOP))
	{
		HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
		SelectObject(hDC, hOldFont);
		ReleaseDC(NULL, hDC);
		return hR;
	}

	SelectObject(hDC, hOldFont);
	ReleaseDC(NULL, hDC);
	size.cx = rect.right;
	size.cy = rect.bottom;
	m_sizeMap.insert(TSizeMap::value_type(text, size));
	return S_OK;
}

HRESULT CFont::DrawText(CDirectxScope& pDevice, LPCTSTR szText, UINT uiLen, const RECT& rect, const D3DXCOLOR& color)
{
	SIZE size;
	size.cx = rect.right - rect.left;
	size.cy = rect.bottom - rect.top;

	const ID3D10ShaderResourceViewPtr* bitmapView = NULL;
	HRESULT hR = retrieveBitmap(szText, uiLen, size, pDevice.device(), bitmapView);
	if(FAILED(hR)) return hR;
	ASSERT(bitmapView);

	const ID3D10BufferPtr* pVertex = NULL;
	hR = retrieveVertex(rect, pDevice, pVertex);
	if(FAILED(hR)) return hR;
	ASSERT(pVertex);

	const ID3D10BufferPtr* pColor = NULL;
	hR = retrieveColor(color, pDevice.device(), pColor);
	if(FAILED(hR)) return hR;
	ASSERT(pColor);

	return pDevice.drawMonoBitmap(*bitmapView, *pVertex, *pColor);
}

HRESULT CFont::retrieveBitmap(LPCTSTR szText, UINT uiLen, const SIZE& size, ID3D10Device1Ptr& pDevice,
	const ID3D10ShaderResourceViewPtr*& pView)
{
	std::basic_string<TCHAR> text(szText, uiLen);
	TBitmapMap::iterator lookup = m_bitmapMap.find(text);
	if(lookup == m_bitmapMap.end())
	{
		if(!m_hFont)
		{
			m_hFont = CreateFontIndirect(&m_lf);
			if(!m_hFont) return HRESULT_FROM_WIN32(GetLastError());
		}

		HDC hDC = GetDC(NULL);
		if(!hDC) return HRESULT_FROM_WIN32(GetLastError());

		COLORREF oldBk = SetBkColor(hDC, RGB(0,0,0));
		COLORREF oldFg = SetTextColor(hDC, RGB(255,255,255));

		struct
		{
			BITMAPINFOHEADER bmiHeader;
			RGBQUAD          bmiColors[256];
		} bmi;

		memset(&bmi, 0, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = size.cx;
		bmi.bmiHeader.biHeight = -size.cy;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 8;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biClrUsed = 256;
		memcpy(bmi.bmiColors, m_bmiColors, sizeof(bmi.bmiColors));

		void* pBits;
		HBITMAP hBMP = CreateDIBSection(hDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
		if(!hBMP)
		{
			HRESULT hR = HRESULT_FROM_WIN32(GetLastError());
			SetBkColor(hDC, oldBk);
			SetTextColor(hDC, oldFg);
			ReleaseDC(NULL, hDC);
			return hR;
		}

		UINT pitch = size.cx;
		if(pitch%4) pitch += 4-pitch%4;

		SetBkColor(hDC, oldBk);
		SetTextColor(hDC, oldFg);

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
		oldBk = SetBkColor(hMemDC, RGB(0,0,0));
		oldFg = SetTextColor(hMemDC, RGB(255,255,255));

		RECT gdiRect;
		memset(&gdiRect, 0, sizeof(gdiRect));
		gdiRect.right = size.cx;
		gdiRect.bottom = size.cy;
		if(!::DrawText(hMemDC, szText, uiLen, &gdiRect, DT_LEFT | DT_SINGLELINE | DT_TOP))
		{
			HRESULT hR = HRESULT_FROM_WIN32(GetLastError());

			SelectObject(hMemDC, hOldBMP);
			SelectObject(hMemDC, hOldFont);
			SetBkColor(hMemDC, oldBk);
			SetTextColor(hMemDC, oldFg);

			DeleteDC(hMemDC);
			ReleaseDC(NULL, hDC);
			DeleteObject(hBMP);
			return hR;
		}

		SelectObject(hMemDC, hOldBMP);
		SelectObject(hMemDC, hOldFont);
		SetBkColor(hMemDC, oldBk);
		SetTextColor(hMemDC, oldFg);

		DeleteDC(hMemDC);
		ReleaseDC(NULL, hDC);

		lookup = m_bitmapMap.insert(TBitmapMap::value_type(text, TBitmapStore())).first;

		{
			D3D10_TEXTURE2D_DESC dataDesc;
			memset(&dataDesc, 0, sizeof(dataDesc));
			dataDesc.Width = size.cx;
			dataDesc.Height = size.cy;
			dataDesc.MipLevels = 1;
			dataDesc.ArraySize = 1;
			dataDesc.Format = DXGI_FORMAT_R8_UNORM;
			dataDesc.SampleDesc.Count = 1;
			dataDesc.SampleDesc.Quality = 0;
			dataDesc.Usage = D3D10_USAGE_IMMUTABLE;
			dataDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

			D3D10_SUBRESOURCE_DATA fontData;
			memset(&fontData, 0, sizeof(fontData));
			fontData.pSysMem = pBits;
			fontData.SysMemPitch = pitch;

			HRESULT hR = pDevice->CreateTexture2D(&dataDesc, &fontData, lookup->second.dataTex.inref());
			if(FAILED(hR))
			{
				DeleteObject(hBMP);
				m_bitmapMap.erase(lookup);
				return hR;
			}
		}
		DeleteObject(hBMP);

		HRESULT hR = pDevice->CreateShaderResourceView(lookup->second.dataTex, NULL, lookup->second.bitmapView.inref());
		if(FAILED(hR))
		{
			m_bitmapMap.erase(lookup);
			return hR;
		}
	}

	lookup->second.bTouched = true;
	pView = &lookup->second.bitmapView;
	return S_OK;
}

HRESULT CFont::retrieveColor(const D3DXCOLOR& color, ID3D10Device1Ptr& pDevice, const ID3D10BufferPtr*& pBuffer)
{
	float4 fColor = { color.r, color.g, color.b, color.a };
	TColorMap::iterator lookup = m_colorMap.find(fColor);
	if(lookup == m_colorMap.end())
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

		lookup = m_colorMap.insert(TColorMap::value_type(fColor, TBufferStore())).first;
		HRESULT hR = pDevice->CreateBuffer(&vsColorBufferDesc, &vsGlobalData, lookup->second.pBuffer.inref());
		if(FAILED(hR))
		{
			m_colorMap.erase(lookup);
			return hR;
		}
	}
	lookup->second.bTouched = true;
	pBuffer = &lookup->second.pBuffer;
	return S_OK;
}

HRESULT CFont::retrieveVertex(const RECT& rect, CDirectxScope& pDevice, const ID3D10BufferPtr*& pVertex)
{
	float4 fColor = { (float)rect.top, (float)rect.left, (float)rect.bottom, (float)rect.right };
	TColorMap::iterator lookup = m_vertexMap.find(fColor);
	if(lookup == m_vertexMap.end())
	{
		lookup = m_vertexMap.insert(TColorMap::value_type(fColor, TBufferStore())).first;
		HRESULT hR = pDevice.createVertexRect(fColor.f1, fColor.f2, fColor.f3, fColor.f4, 0.25f, false, lookup->second.pBuffer);
		if(FAILED(hR))
		{
			m_vertexMap.erase(lookup);
			return hR;
		}
	}
	lookup->second.bTouched = true;
	pVertex = &lookup->second.pBuffer;
	return S_OK;
}

void CFont::BeginFrame()
{
	for(TColorMap::iterator trans = m_colorMap.begin(); trans != m_colorMap.end(); trans++)
	{
		trans->second.bTouched = false;
	}

	for(TColorMap::iterator trans = m_vertexMap.begin(); trans != m_vertexMap.end(); trans++)
	{
		trans->second.bTouched = false;
	}

	for(TBitmapMap::iterator trans = m_bitmapMap.begin(); trans != m_bitmapMap.end(); trans++)
	{
		trans->second.bTouched = false;
	}
}

void CFont::EndFrame()
{
	{
		TColorMap::iterator trans = m_colorMap.begin();
		while(trans != m_colorMap.end())
		{
			TColorMap::iterator here = trans++;
			if(!here->second.bTouched) m_colorMap.erase(here);
		}
	}

	{
		TColorMap::iterator trans = m_vertexMap.begin();
		while(trans != m_vertexMap.end())
		{
			TColorMap::iterator here = trans++;
			if(!here->second.bTouched) m_vertexMap.erase(here);
		}
	}

	{
		TBitmapMap::iterator trans = m_bitmapMap.begin();
		while(trans != m_bitmapMap.end())
		{
			TBitmapMap::iterator here = trans++;
			if(!here->second.bTouched) m_bitmapMap.erase(here);
		}
	}
}
