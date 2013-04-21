// waterfall.fx

// globals
matrix orthoMatrix;

// parameters
struct VertexInputType
{
	float4 position : POSITION;
	float intensity : VALUE;
};
struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 intensity : VALUE;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	
	// Change the position vector to be 4 units for proper matrix calculations.
	input.position.w = 1.0f;
	
	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(input.position, orthoMatrix);
	
	// Store the input color for the pixel shader to use.
	output.intensity = input.intensity;
	
	return output;
}

float4 PS(PixelInputType input) : SV_Target
{
	return input.color;
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