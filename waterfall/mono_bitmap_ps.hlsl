// mono_bitmap_ps.hlsl

// globals
Texture2D image : register(t2);
SamplerState ImageSampler : register(s2);

cbuffer globals : register(b2)
{
	float4 color;
}

// parameters
struct PixelInputType
{
	unorm float2 tex : TEXCOORD;
	float4 position : SV_POSITION;
};

float4 PS(float2 p : TEXCOORD0) : SV_Target
{
	float lum = image.Sample(ImageSampler, p).r;
	clip(lum - 0.1);
	return color * lum;
}
