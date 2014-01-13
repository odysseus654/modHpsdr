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

// mono_bitmap_ps.hlsl

// globals
Texture2D image : register(t2);
SamplerState ImageSampler : register(s2);

cbuffer globals : register(b2)
{
	float4 color;
}

// parameters
struct PixelInputType
{
	unorm float2 tex : TEXCOORD;
	float4 position : SV_POSITION;
};

float4 PS(float2 p : TEXCOORD0) : SV_Target
{
	float lum = image.Sample(ImageSampler, p).r;
	clip(lum - 0.1);
	return color * lum;
}
