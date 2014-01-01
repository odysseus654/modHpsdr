/*
	Copyright 2013 Erik Anderson

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

// waveform_ps9.hlsl
// inspired/adapted from http://lolengine.net/browser/trunk/tutorial/04_texture.lolfx?rev=2628

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

float segdist(float2 p1, float2 p2, float2 a)
{
	float len2 = max(1e-10, dot(p2 - p1, p2 - p1));
	float t = clamp(dot(a - p1, p2 - p1) / len2, 0.0, 1.0);
	return distance(a, lerp(p1, p2, t));
}

float4 PS(unorm float2 p : TEXCOORD) : SV_TARGET
{
	float delta = 1.0 / texture_width;
	float tc = floor(p.x * texture_width) / texture_width;

	float2 c;
	c[0] = texValues.Sample(ValueSampleType, float2(tc, 0)).x;
	c[1] = texValues.Sample(ValueSampleType, float2(tc + delta, 0)).x;
	
	/* Find the 4 closest points in screen space */
	float2 p1 = float2((tc        ) * width, c[0] * height);
	float2 p2 = float2((tc + delta) * width, c[1] * height);
	float2 a = float2(p.x * width, p.y * height);
	
	/* Compute distance to segments */
	float d = segdist(p1, p2, a);
	
	/* Add line width */
	float lum = clamp(line_width - d, 0.0, 1.0);
	clip(lum - 0.1f);
	
	/* Compensate for sRGB */
	lum = pow(lum, 1.0 / 2.4);
	
	/* Choose some funny colours */
	return float4(waveformColor.rgb, lum);
}
