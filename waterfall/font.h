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

#include <map>
#include "unkref.h"
#include <Unknwn.h>

#include <Windows.h>

struct D3DXCOLOR;
struct ID3D10Buffer;
struct ID3D10Device1;
struct ID3D10Texture2D;
struct ID3D10ShaderResourceView;
class CDirectxScope;

typedef unk_ref_t<ID3D10Buffer> ID3D10BufferPtr;
typedef unk_ref_t<ID3D10Device1> ID3D10Device1Ptr;
typedef unk_ref_t<ID3D10Texture2D> ID3D10Texture2DPtr;
typedef unk_ref_t<ID3D10ShaderResourceView> ID3D10ShaderResourceViewPtr;

class CFont
{
public:
	CFont(INT Height, UINT Width, UINT Weight, UINT Style, UINT Charset, UINT OutputPrecision,
		UINT Quality, UINT PitchAndFamily, LPCTSTR pFaceName);
	~CFont();

	void BeginFrame();
	void EndFrame();
	HRESULT CalcSize(LPCTSTR szText, UINT uiLen, SIZE& size);
	HRESULT DrawText(CDirectxScope& pDevice, ID3D10Device1* pD3Device, LPCTSTR szText, UINT uiLen, const RECT& rect, const D3DXCOLOR& color);

	enum { stItalic = 1, stUnderline = 2, stStrikeout = 4 };

private:
	LOGFONT m_lf;
	HFONT m_hFont;
	RGBQUAD m_bmiColors[256];

	struct float4
	{
		float f1, f2, f3, f4;
		inline bool operator<(const float4& other) const;
	};

	struct TBufferStore
	{
		bool bTouched;
		ID3D10BufferPtr pBuffer;
	};

	struct TBitmapStore
	{
		bool bTouched;
		ID3D10Texture2DPtr dataTex;
		ID3D10ShaderResourceViewPtr bitmapView;
	};

	HRESULT retrieveColor(const D3DXCOLOR& color, ID3D10Device1* pDevice, const ID3D10BufferPtr*& pBuffer);
	HRESULT retrieveVertex(const RECT& rect, CDirectxScope& pDevice, const ID3D10BufferPtr*& pVertex);
	HRESULT retrieveBitmap(LPCTSTR szText, UINT uiLen, const SIZE& size,
		ID3D10Device1* pDevice, const ID3D10ShaderResourceViewPtr*& pView);

	typedef std::map<float4, TBufferStore> TColorMap;
	typedef std::map<std::basic_string<TCHAR>, TBitmapStore> TBitmapMap;
	typedef std::map<std::basic_string<TCHAR>, SIZE> TSizeMap;
	TColorMap m_colorMap;
	TColorMap m_vertexMap;
	TBitmapMap m_bitmapMap;
	TSizeMap m_sizeMap;
};

bool CFont::float4::operator<(const float4& other) const
{
	return (f1 < other.f1) ? true : (f1 > other.f1) ? false :
		(f2 < other.f2) ? true : (f2 > other.f2) ? false :
		(f3 < other.f3) ? true : (f3 > other.f3) ? false :
		(f4 < other.f4) ? true : (f4 > other.f4) ? false :
		false;
}
