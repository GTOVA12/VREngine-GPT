#include "ACES.hlsli"

struct ToneConstants
{
	float Exposure;
	uint Filmic;
	float2 Padding;
};
[[vk::push_constant]] ConstantBuffer<ToneConstants> g_Tone : register(b0);
Texture2D<float4> t_Hdr : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
	float3 color = max(0.0, t_Hdr.Load(int3(uint2(position.xy), 0)).rgb) * g_Tone.Exposure;
	if (g_Tone.Filmic != 0)
		color = ACESFitted(color);
	else
	{
		// Legacy fixed-luminance operator for visual comparison.
		float luminance = dot(color, float3(.2126, .7152, .0722));
		float scaled = luminance * 2.0;
		color *= 2.0 * (1.0 + scaled / 9.0) / (1.0 + scaled);
	}
	// The SRGBA8 target encodes linear display color exactly once.
	return float4(saturate(color), 1.0);
}