/*
	Copyright 2013-2014 Erik Anderson

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

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
