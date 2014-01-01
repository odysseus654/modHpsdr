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

// waterfall_ps.hlsl

// globals
Texture2D waterfallValues : register(t0); // DXGI_FORMAT_R16_FLOAT
Texture2D waterfallColors : register(t1); // 1D texture, DXGI_FORMAT_R32G32B32_FLOAT
SamplerState ValueSampleType : register(s0);
SamplerState ColorSampleType : register(s1);

cbuffer globals : register(b0)
{
	float minRange;
	float maxRange;
}

float4 PS(unorm float2 tex : TEXCOORD) : SV_TARGET
{
	float val = waterfallValues.Sample(ValueSampleType, tex).r;
	float newVal = (val - minRange) / (maxRange - minRange);
	return waterfallColors.Sample(ColorSampleType, float2(newVal, 0.0));
}
