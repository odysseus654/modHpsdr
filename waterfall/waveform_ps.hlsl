// waveform_ps.hlsl

// globals
cbuffer globals : register(b0)
{
	float4 waveformColor;
}

float4 PS() : SV_Target
{
	return waveformColor;
}
