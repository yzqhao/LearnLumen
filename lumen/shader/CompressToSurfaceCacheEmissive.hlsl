ByteAddressBuffer RectCoordBuffer : register(t0);
ByteAddressBuffer RectUVBuffer : register(t1);
int2 select_internal(bool2   c, int2 a, int2 b) { return int2(c.x ? a.x : b.x, c.y ? a.y : b.y); }
float2 select_internal(bool2   c, float2 a, float2 b) { return float2(c.x ? a.x : b.x, c.y ? a.y : b.y); }                       
bool2 VertMax(uint VertexId)
{
	bool2 bVertMax;
	bVertMax.x = VertexId == 1 || VertexId == 2 || VertexId == 4;
	bVertMax.y = VertexId == 2 || VertexId == 4 || VertexId == 5;
	return bVertMax;
}

uint4 Decode(ByteAddressBuffer buf, uint InstanceId)
{
    uint RectCoord0 = buf.Load(InstanceId * 16u);
    uint RectCoord1 = buf.Load(InstanceId * 16u + 4);
    uint RectCoord2 = buf.Load(InstanceId * 16u + 8);
    uint RectCoord3 = buf.Load(InstanceId * 16u + 12);
    uint4 RectCoord = uint4(RectCoord0, RectCoord1, RectCoord2, RectCoord3);
    return RectCoord;
}

void VS(
	in uint InstanceId : SV_InstanceID,
	in uint VertexId : SV_VertexID,
	out float4 OutPosition : SV_POSITION,
	out float2 OutUV : TEXCOORD0,
	out float2 OutRectUV : TEXCOORD1,
	out float OutRectIndex : RECT_INDEX)
{
	float DownsampleFactor=0.25f;
	float2 InvTextureSize=float2(0.0078125f,0.0078125f);
	float2 InvViewSize=float2(0.0009766f,0.0009766f);
    uint4 RectCoord = Decode(RectCoordBuffer, InstanceId) * DownsampleFactor;
	uint2 VertexCoord =  select_internal( VertMax(VertexId) , RectCoord.zw , RectCoord.xy );
	float4 RectUV = RectCoord * InvViewSize.xyxy;
	{
        RectUV = Decode(RectUVBuffer, InstanceId) * DownsampleFactor * InvTextureSize.xyxy;
    }
	float2 VertexUV =  select_internal( VertMax(VertexId) , RectUV.zw , RectUV.xy );
	OutPosition = float4(float2(VertexCoord) * InvViewSize * float2(2.0f, -2.0f) + float2(-1.0, 1.0f), 0.0f, 1.0f);
	OutUV = VertexUV;
	OutRectUV.x = VertMax(VertexId).x ? 1.0f : 0.0f;
	OutRectUV.y = VertMax(VertexId).y ? 1.0f : 0.0f;
	OutRectIndex = InstanceId;
}
//(0.0f,0.0f,1.0f) => (0.3,0.4,0.4) => 0.3*0.3+0.4*0.4+0.4*0.4 != 1.0
SamplerState D3DStaticPointClampedSampler : register(s0);
void Swap(inout uint A, inout uint B) { uint Temp = A; A = B; B = Temp; }
void Swap(inout uint2 A, inout uint2 B) { uint2 Temp = A; A = B; B = Temp; }
void Swap(inout uint3 A, inout uint3 B) { uint3 Temp = A; A = B; B = Temp; }
void Swap(inout uint4 A, inout uint4 B) { uint4 Temp = A; A = B; B = Temp; }
void Swap(inout float A, inout float B) { float Temp = A; A = B; B = Temp; }
void Swap(inout float2 A, inout float2 B) { float2 Temp = A; A = B; B = Temp; }
void Swap(inout float3 A, inout float3 B) { float3 Temp = A; A = B; B = Temp; }
void Swap(inout float4 A, inout float4 B) { float4 Temp = A; A = B; B = Temp; }
void ReadBlockRGB(Texture2D<float4> SourceTexture, SamplerState TextureSampler, float2 UV, float2 TexelUVSize, out float3 Block[16])
{
	{
		float4 Red = SourceTexture.GatherRed(TextureSampler, UV);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UV);
		float4 Blue = SourceTexture.GatherBlue(TextureSampler, UV);
		Block[0] = float3(Red[3], Green[3], Blue[3]);
		Block[1] = float3(Red[2], Green[2], Blue[2]);
		Block[4] = float3(Red[0], Green[0], Blue[0]);
		Block[5] = float3(Red[1], Green[1], Blue[1]);
	}
	{
		float2 UVOffset = UV + float2(2.f * TexelUVSize.x, 0);
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		float4 Blue = SourceTexture.GatherBlue(TextureSampler, UVOffset);
		Block[2] = float3(Red[3], Green[3], Blue[3]);
		Block[3] = float3(Red[2], Green[2], Blue[2]);
		Block[6] = float3(Red[0], Green[0], Blue[0]);
		Block[7] = float3(Red[1], Green[1], Blue[1]);
	}
	{
		float2 UVOffset = UV + float2(0, 2.f * TexelUVSize.y);
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		float4 Blue = SourceTexture.GatherBlue(TextureSampler, UVOffset);
		Block[8] = float3(Red[3], Green[3], Blue[3]);
		Block[9] = float3(Red[2], Green[2], Blue[2]);
		Block[12] = float3(Red[0], Green[0], Blue[0]);
		Block[13] = float3(Red[1], Green[1], Blue[1]);
	}
	{
		float2 UVOffset = UV + 2.f * TexelUVSize;
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		float4 Blue = SourceTexture.GatherBlue(TextureSampler, UVOffset);
		Block[10] = float3(Red[3], Green[3], Blue[3]);
		Block[11] = float3(Red[2], Green[2], Blue[2]);
		Block[14] = float3(Red[0], Green[0], Blue[0]);
		Block[15] = float3(Red[1], Green[1], Blue[1]);
	}
}
float3 Quantize10(float3 X)
{
	return (f32tof16(X) * 1024.0f) / (0x7bff + 1.0f);
}
uint ComputeIndexBC6HIndex(float3 Color, float3 BlockVector, float EndPoint0Pos, float EndPoint1Pos)
{
	float Pos = (float)f32tof16(dot(Color, BlockVector));
	float NormalizedPos = saturate((Pos - EndPoint0Pos) / (EndPoint1Pos - EndPoint0Pos));
	return (uint)clamp(NormalizedPos * 14.93333f + 0.03333f + 0.5f, 0.0f, 15.0f);
}
uint4 CompressBC6HBlock(in float3 BlockRGB[16])
{
	float3 BlockMin = BlockRGB[0];
	float3 BlockMax = BlockRGB[0];
	{
		for (uint TexelIndex = 1; TexelIndex < 16; ++TexelIndex)
		{
			BlockMin = min(BlockMin, BlockRGB[TexelIndex]);
			BlockMax = max(BlockMax, BlockRGB[TexelIndex]);
		}
	}
	float3 BlockVector = BlockMax - BlockMin;
	BlockVector = BlockVector / (BlockVector.x + BlockVector.y + BlockVector.z);
	float3 Endpoint0 = Quantize10(BlockMin);
	float3 Endpoint1 = Quantize10(BlockMax);
	float EndPoint0Pos = (float)f32tof16(dot(BlockMin, BlockVector));
	float EndPoint1Pos = (float)f32tof16(dot(BlockMax, BlockVector));
	uint FixupIndex = ComputeIndexBC6HIndex(BlockRGB[0], BlockVector, EndPoint0Pos, EndPoint1Pos);
	if (FixupIndex > 7)
	{
		Swap(EndPoint0Pos, EndPoint1Pos);
		Swap(Endpoint0, Endpoint1);
	}
	uint Indices[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (uint TexelIndex = 0; TexelIndex < 16; ++TexelIndex)
	{
		Indices[TexelIndex] = ComputeIndexBC6HIndex(BlockRGB[TexelIndex], BlockVector, EndPoint0Pos, EndPoint1Pos);
	}
	uint4 Block = 0;
	{
		Block.x = 0x03;
		Block.x |= (uint)Endpoint0.x << 5;
		Block.x |= (uint)Endpoint0.y << 15;
		Block.x |= (uint)Endpoint0.z << 25;
		Block.y |= (uint)Endpoint0.z >> 7;
		Block.y |= (uint)Endpoint1.x << 3;
		Block.y |= (uint)Endpoint1.y << 13;
		Block.y |= (uint)Endpoint1.z << 23;
		Block.z |= (uint)Endpoint1.z >> 9;
		Block.z |= Indices[0] << 1;
		Block.z |= Indices[1] << 4;
		Block.z |= Indices[2] << 8;
		Block.z |= Indices[3] << 12;
		Block.z |= Indices[4] << 16;
		Block.z |= Indices[5] << 20;
		Block.z |= Indices[6] << 24;
		Block.z |= Indices[7] << 28;
		Block.w |= Indices[8] << 0;
		Block.w |= Indices[9] << 4;
		Block.w |= Indices[10] << 8;
		Block.w |= Indices[11] << 12;
		Block.w |= Indices[12] << 16;
		Block.w |= Indices[13] << 20;
		Block.w |= Indices[14] << 24;
		Block.w |= Indices[15] << 28;
	}
	return Block;
}
RWTexture2D<uint4> RWAtlasBlock4 : register(u0);
Texture2D SourceEmissiveAtlas : register(t2);
void PS(
	float4 Position : SV_POSITION,
	float2 AtlasUV : TEXCOORD0
)
{
	float2 OneOverSourceAtlasSize=float2(0.0019531f,0.0019531f);
	uint2 WriteCoord = (uint2) Position.xy;
	{
		{
			float3 BlockTexels[16];
			ReadBlockRGB(SourceEmissiveAtlas,  D3DStaticPointClampedSampler, AtlasUV - OneOverSourceAtlasSize, OneOverSourceAtlasSize, BlockTexels);
			RWAtlasBlock4[WriteCoord] = CompressBC6HBlock(BlockTexels);
		}
	}
}