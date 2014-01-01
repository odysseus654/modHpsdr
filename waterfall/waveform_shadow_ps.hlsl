// waveform_shadow_ps.hlsl

// globals
Texture2D texValues : register(t0);
SamplerState ValueSampleType : register(s0);

cbuffer globals : register(b0)
{
	float4 waveformColor;
	float4 shadowColor;
	float width;
	float height;
	float texture_width;
	float line_width;
}

cbuffer rangeGlobals : register(b1)
{
	float minRange;
	float maxRange;
}

float4 PS(unorm float2 p : TEXCOORD) : SV_TARGET
{
	float delta = 1.0 / texture_width;
	float tc = floor(p.x * texture_width) / texture_width;
	float offset = (p.x - tc) / delta;

	float2 c;
	c[0] = texValues.Sample(ValueSampleType, float2(tc, 0)).x;
	c[1] = texValues.Sample(ValueSampleType, float2(tc + delta, 0)).x;
	
	float ourPos = (lerp(c[0], c[1], offset) - minRange) / (maxRange - minRange);
	clip(ourPos - p.y);
	
	return shadowColor;
}
