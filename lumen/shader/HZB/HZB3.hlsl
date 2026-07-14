#line 1 "MoveShaderParametersToRootConstantBuffer"
cbuffer _RootShaderParameters
{
float4 DispatchThreadIdToBufferUV : packoffset(c0);
uint4 PixelViewPortMinMax : packoffset(c1);
float2 InputViewportMaxBound : packoffset(c2);
float2 InvSize : packoffset(c2.z);
}

// DebugHash_ac047e137ba13f57811e0d014d8d12441e2068475dfde5bbfb575e0b0c8d765f
#line 1 "__UE_FILENAME_SENTINEL"
float min3(float a, float b, float c)
{
	return min(a, min(b, c));
}
int min3(int a, int b, int c)
{
	return min(a, min(b, c));
}
uint min3(uint a, uint b, uint c)
{
	return min(a, min(b, c));
}
float2 min3 ( float2 a, float2 b, float2 c) { return float2( min3 (a.x , b.x , c.x ), min3 (a.y , b.y , c.y )); }
float3 min3 ( float3 a, float3 b, float3 c) { return float3( min3 (a.xy, b.xy, c.xy), min3 (a.z , b.z , c.z )); }
float4 min3 ( float4 a, float4 b, float4 c) { return float4( min3 (a.xy, b.xy, c.xy), min3 (a.zw, b.zw, c.zw)); }
int2 min3 ( int2 a, int2 b, int2 c) { return int2( min3 (a.x , b.x , c.x ), min3 (a.y , b.y , c.y )); }
int3 min3 ( int3 a, int3 b, int3 c) { return int3( min3 (a.xy, b.xy, c.xy), min3 (a.z , b.z , c.z )); }
int4 min3 ( int4 a, int4 b, int4 c) { return int4( min3 (a.xy, b.xy, c.xy), min3 (a.zw, b.zw, c.zw)); }
uint2 min3 ( uint2 a, uint2 b, uint2 c) { return uint2( min3 (a.x , b.x , c.x ), min3 (a.y , b.y , c.y )); }
uint3 min3 ( uint3 a, uint3 b, uint3 c) { return uint3( min3 (a.xy, b.xy, c.xy), min3 (a.z , b.z , c.z )); }
uint4 min3 ( uint4 a, uint4 b, uint4 c) { return uint4( min3 (a.xy, b.xy, c.xy), min3 (a.zw, b.zw, c.zw)); }
float max3(float a, float b, float c)
{
	return max(a, max(b, c));
}
int max3(int a, int b, int c)
{
	return max(a, max(b, c));
}
uint max3(uint a, uint b, uint c)
{
	return max(a, max(b, c));
}
float2 max3 ( float2 a, float2 b, float2 c) { return float2( max3 (a.x , b.x , c.x ), max3 (a.y , b.y , c.y )); }
float3 max3 ( float3 a, float3 b, float3 c) { return float3( max3 (a.xy, b.xy, c.xy), max3 (a.z , b.z , c.z )); }
float4 max3 ( float4 a, float4 b, float4 c) { return float4( max3 (a.xy, b.xy, c.xy), max3 (a.zw, b.zw, c.zw)); }
int2 max3 ( int2 a, int2 b, int2 c) { return int2( max3 (a.x , b.x , c.x ), max3 (a.y , b.y , c.y )); }
int3 max3 ( int3 a, int3 b, int3 c) { return int3( max3 (a.xy, b.xy, c.xy), max3 (a.z , b.z , c.z )); }
int4 max3 ( int4 a, int4 b, int4 c) { return int4( max3 (a.xy, b.xy, c.xy), max3 (a.zw, b.zw, c.zw)); }
uint2 max3 ( uint2 a, uint2 b, uint2 c) { return uint2( max3 (a.x , b.x , c.x ), max3 (a.y , b.y , c.y )); }
uint3 max3 ( uint3 a, uint3 b, uint3 c) { return uint3( max3 (a.xy, b.xy, c.xy), max3 (a.z , b.z , c.z )); }
uint4 max3 ( uint4 a, uint4 b, uint4 c) { return uint4( max3 (a.xy, b.xy, c.xy), max3 (a.zw, b.zw, c.zw)); }

Texture2D		ParentTextureMip;
SamplerState	ParentTextureMipSampler;

float4 Gather4(Texture2D Texture, SamplerState TextureSampler, float2 BufferUV, uint Level)
{
	float2 UV = min(BufferUV + float2(-0.25f, -0.25f) * InvSize, InputViewportMaxBound - InvSize);
	return Texture.GatherRed(TextureSampler, UV, Level);
}
float4 Gather4(Texture2D Texture, SamplerState TextureSampler, float2 BufferUV)
{
	return Gather4(Texture, TextureSampler, BufferUV, 0);
}
RWTexture2D<float> FurthestHZBOutput_0;

[numthreads(8, 8, 1)]
void CS(
	uint2 GroupId : SV_GroupID,
	uint GroupThreadIndex : SV_GroupIndex)
{
		uint2 GroupThreadId = uint2(GroupThreadIndex % 8, GroupThreadIndex / 8);
	uint2 GroupOffset = 8 * GroupId;
	uint2 DispatchThreadId = GroupOffset + GroupThreadId;
	uint2 MinPixelCoord = (GroupOffset << 1) + PixelViewPortMinMax.xy;
	bool bValidGroup = all(MinPixelCoord < PixelViewPortMinMax.zw);
	float2 BufferUV = (DispatchThreadId + 0.5) * DispatchThreadIdToBufferUV.xy + DispatchThreadIdToBufferUV.zw;
	float4 DeviceZ = Gather4(ParentTextureMip, ParentTextureMipSampler, BufferUV);
	uint2 SrcPixelCoord = (DispatchThreadId << 1) + PixelViewPortMinMax.xy;
	float MinDeviceZ = min(min3(DeviceZ.x, DeviceZ.y, DeviceZ.z), DeviceZ.w);
	float MaxDeviceZ = max(max3(DeviceZ.x, DeviceZ.y, DeviceZ.z), DeviceZ.w);
	uint2 OutputPixelPos = DispatchThreadId;
		FurthestHZBOutput_0[OutputPixelPos] = MinDeviceZ;
	{
	}
}
