// waterfall.fx

// globals
matrix orthoMatrix;
Texture2D waterfallValues; // DXGI_FORMAT_R32_FLOAT, 0 >= x => 1
Texture1D waterfallColors; // DXGI_FORMAT_R32G32B32_FLOAT

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
struct VertexInputType
{
	float4 position : POSITION;
	float2 tex : TEXCOORD;
};
struct PixelInputType
{
	float2 tex : TEXCOORD;
	float4 position : SV_POSITION;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	
//	// Change the position vector to be 4 units for proper matrix calculations.
//	input.position.w = 1.0f;
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(input.position, orthoMatrix);
	
	// Store the input color for the pixel shader to use.
	output.tex = input.tex;
	
	return output;
}

float4 PS(PixelInputType input) : SV_Target
{
	float val = waterfallValues.Sample(ValueSampleType, input.tex);
	float4 texColor = waterfallColors.Sample(ColorSampleType, val);
	return texColor;
}

technique10 WaterfallTechnique
{
	pass pass0
	{
		SetVertexShader(CompileShader(vs_4_0, VS()));
		SetPixelShader(CompileShader(ps_4_0, PS()));
		SetGeometryShader(NULL);
	}
}