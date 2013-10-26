// waveform_vs.hlsl

// globals
Texture1D waveformValues : register(t0); // DXGI_FORMAT_R16_FLOAT

cbuffer orthoglobals : register(b0)
{
	matrix orthoMatrix;
}

cbuffer rangeglobals : register(b1)
{
	float minRange;
	float maxRange;
}

SamplerState WaveSampleType
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

// parameters
struct VertexInputType
{
	float4 position : POSITION;
	float mag : RANGE;
	unorm float tex : TEXCOORD;
};
struct PixelInputType
{
	float4 position : SV_POSITION;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	
//	// Change the position vector to be 4 units for proper matrix calculations.
//	input.position.w = 1.0f;

	float val = waveformValues.Sample(WaveSampleType, input.tex).x;
	float newVal = (val - minRange) / (maxRange - minRange);

	float4 newPos = input.position;
	newPos.y = newVal * input.mag;
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(input.position, orthoMatrix);
	
	return output;
}
