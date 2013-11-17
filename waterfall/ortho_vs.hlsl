// ortho_vs.hlsl

// globals
cbuffer globals : register(b0)
{
	matrix orthoMatrix;
}

// parameters
struct VertexInputType
{
	float4 position : POSITION;
	unorm float2 tex : TEXCOORD;
};
struct PixelInputType
{
	unorm float2 tex : TEXCOORD;
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
