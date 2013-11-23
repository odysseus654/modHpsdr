// waterfall_ps.hlsl

// globals
Texture2D waterfallValues : register(t0); // DXGI_FORMAT_R16_FLOAT
Texture2D waterfallColors : register(t1); // 1D texture, DXGI_FORMAT_R32G32B32_FLOAT
SamplerState ValueSampleType : register(s0);
SamplerState ColorSampleType : register(s1);

cbuffer globals : register(b0)
{
	float minRange;
	float maxRange;
}

float4 PS(unorm float2 tex : TEXCOORD) : SV_TARGET
{
	float val = waterfallValues.Sample(ValueSampleType, tex).r;
	float newVal = (val - minRange) / (maxRange - minRange);
	return waterfallColors.Sample(ColorSampleType, float2(newVal, 0.0));
}
