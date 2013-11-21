#pragma once
typedef long HRESULT;

#include <Windows.h>

struct D3DXCOLOR;
class CDirectxScope;

class CFont
{
public:
	CFont(INT Height, UINT Width, UINT Weight, UINT Style, UINT Charset, UINT OutputPrecision,
		UINT Quality, UINT PitchAndFamily, LPCTSTR pFaceName);
	~CFont();

	HRESULT CalcRect(LPCTSTR szText, UINT uiLen, RECT* rect);
	HRESULT DrawText(CDirectxScope& pDevice, LPCTSTR szText, UINT uiLen, RECT* rect, const D3DXCOLOR& color);

	enum { stItalic = 1, stUnderline = 2, stStrikeout = 4 };

private:
	LOGFONT m_lf;
	HFONT m_hFont;

};