// waterfall_ps.hlsl

// globals
Texture2D waterfallValues : register(t0); // DXGI_FORMAT_R16_FLOAT
Texture2D waterfallColors : register(t1); // 1D texture, DXGI_FORMAT_R32G32B32_FLOAT

cbuffer globals : register(b0)
{
	float minRange;
	float maxRange;
}

SamplerState ValueSampleType
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Border;
	AddressV = Border;
};

SamplerState ColorSampleType
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

// parameters
struct PixelInputType
{
	unorm float2 tex : TEXCOORD;
	float4 position : SV_POSITION;
};

float4 PS(PixelInputType input) : SV_Target
{
	float val = waterfallValues.Sample(ValueSampleType, input.tex).r;
	float newVal = (val - minRange) / (maxRange - minRange);
	return waterfallColors.Sample(ColorSampleType, float2(newVal, 0.0));
}
