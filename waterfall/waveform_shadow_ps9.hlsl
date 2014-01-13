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

// waveform_shadow_ps9.hlsl

// globals
Texture2D texValues : register(t0);
SamplerState ValueSampleType : register(s0);

cbuffer globals : register(b0)
{
	float4 waveformColor;
	float4 shadowColor;
	float width;
	float height;
	float texture_width;
	float line_width;
}

float4 PS(unorm float2 p : TEXCOORD) : SV_TARGET
{
	float delta = 1.0 / texture_width;
	float tc = floor(p.x * texture_width) / texture_width;
	float offset = (p.x - tc) / delta;

	float2 c;
	c[0] = texValues.Sample(ValueSampleType, float2(tc, 0)).x;
	c[1] = texValues.Sample(ValueSampleType, float2(tc + delta, 0)).x;
	
	float ourPos = lerp(c[0], c[1], offset);
	clip(ourPos - p.y);
	
	return shadowColor;
}
