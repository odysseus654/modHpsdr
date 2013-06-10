// waveform.fx

// globals
matrix orthoMatrix;
Texture1D waveformValues; // DXGI_FORMAT_R16_FLOAT
float4 waveformColor;
float minRange;
float maxRange;

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

	float4 newPos = input.position;
	float val = waveformValues.Sample(WaveSampleType, input.tex);
	float newVal = (val - minRange) / (maxRange - minRange);
	newPos.y = newVal * input.mag;
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(newPos, orthoMatrix);
	
	return output;
}

float4 PS(PixelInputType input) : SV_Target
{
	return waveformColor;
}

technique10 WaveformTechnique
{
	pass pass0
	{
		SetVertexShader(CompileShader(vs_4_0, VS()));
		SetPixelShader(CompileShader(ps_4_0, PS()));
		SetGeometryShader(NULL);
	}
}