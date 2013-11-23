// waterfall_ps.hlsl

// globals
Texture2D waterfallValues : register(t0); // DXGI_FORMAT_R16_FLOAT
Texture2D waterfallColors : register(t1); // 1D texture, DXGI_FORMAT_R32G32B32_FLOAT
SamplerState ValueSampleType : register(s0);
SamplerState ColorSampleType : register(s1);

float4 PS(unorm float2 tex : TEXCOORD) : SV_TARGET
{
	unorm float val = waterfallValues.Sample(ValueSampleType, tex).r;
	return waterfallColors.Sample(ColorSampleType, float2(val, 0.0));
}
