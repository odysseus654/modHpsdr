// waveform_ps.hlsl

// globals
cbuffer globals : register(b0)
{
	float4 waveformColor;
}

// parameters
struct PixelInputType
{
	float4 position : SV_POSITION;
};

float4 PS(PixelInputType input) : SV_Target
{
	return waveformColor;
}
