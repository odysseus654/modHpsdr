// waveform_vs.hlsl

// globals
cbuffer orthoglobals : register(b0)
{
	matrix orthoMatrix;
}

cbuffer rangeglobals : register(b1)
{
	float minRange;
	float maxRange;
}

float4 VS(float tex : TEXCOORD, float mag : MAG) : SV_POSITION
{
	float newVal = (mag - minRange) / (maxRange - minRange);

	float4 newPos = float4(tex, newVal, 1.0f, 1.0f);
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	return mul(newPos, orthoMatrix);
}
