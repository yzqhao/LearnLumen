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
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
void ReadBlockXY(Texture2D<float4> SourceTexture, SamplerState TextureSampler, float2 UV, float2 TexelUVSize, out float BlockX[16], out float BlockY[16])
{
	{
		float4 Red = SourceTexture.GatherRed(TextureSampler, UV);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UV);
		BlockX[0] = Red[3]; BlockY[0] = Green[3];
		BlockX[1] = Red[2]; BlockY[1] = Green[2];
		BlockX[4] = Red[0]; BlockY[4] = Green[0];
		BlockX[5] = Red[1]; BlockY[5] = Green[1];
	}
	{
		float2 UVOffset = UV + float2(2.f * TexelUVSize.x, 0);
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		BlockX[2] = Red[3]; BlockY[2] = Green[3];
		BlockX[3] = Red[2]; BlockY[3] = Green[2];
		BlockX[6] = Red[0]; BlockY[6] = Green[0];
		BlockX[7] = Red[1]; BlockY[7] = Green[1];
	}
	{
		float2 UVOffset = UV + float2(0, 2.f * TexelUVSize.y);
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		BlockX[8] = Red[3]; BlockY[8] = Green[3];
		BlockX[9] = Red[2]; BlockY[9] = Green[2];
		BlockX[12] = Red[0]; BlockY[12] = Green[0];
		BlockX[13] = Red[1]; BlockY[13] = Green[1];
	}
	{
		float2 UVOffset = UV + 2.f * TexelUVSize;
		float4 Red = SourceTexture.GatherRed(TextureSampler, UVOffset);
		float4 Green = SourceTexture.GatherGreen(TextureSampler, UVOffset);
		BlockX[10] = Red[3]; BlockY[10] = Green[3];
		BlockX[11] = Red[2]; BlockY[11] = Green[2];
		BlockX[14] = Red[0]; BlockY[14] = Green[0];
		BlockX[15] = Red[1]; BlockY[15] = Green[1];
	}
}
uint RoundToUInt(float X)
{
	return (uint)round(X);
}
void GetMinMax( in float Block[16], out float OutMin, out float OutMax )
{
	OutMin = Block[0];
	OutMax = Block[0];
	for (int i=1; i<16; ++i)
	{
		OutMin = min(OutMin, Block[i]);
		OutMax = max(OutMax, Block[i]);
	}
}
void GetMinMax( in float Block0[16], in float Block1[16], out float OutMin0, out float OutMax0, out float OutMin1, out float OutMax1 )
{
	OutMin0 = Block0[0];
	OutMax0 = Block0[0];
	OutMin1 = Block1[0];
	OutMax1 = Block1[0];
	for (int i=1; i<16; ++i)
	{
		OutMin0 = min(OutMin0, Block0[i]);
		OutMax0 = max(OutMax0, Block0[i]);
		OutMin1 = min(OutMin1, Block1[i]);
		OutMax1 = max(OutMax1, Block1[i]);
	}
}
void GetMinMax( in float3 Block[16], out float3 OutMin, out float3 OutMax )
{
	OutMin = Block[0];
	OutMax = Block[0];
	for (int i=1; i<16; ++i)
	{
		OutMin = min(OutMin, Block[i]);
		OutMax = max(OutMax, Block[i]);
	}
}
uint2 GetPackedAlphaBlockIndices( in float Block[16], in float MinAlpha, in float MaxAlpha )
{
	uint2 PackedIndices = 0;
	float Range = MinAlpha - MaxAlpha;
	float Scale = 7.f / Range;
	float Bias = -Scale * MaxAlpha;
	uint i = 0;
	for (i=0; i<5; ++i)
	{
		uint Index = RoundToUInt(Block[i] * Scale + Bias);
		Index += (Index > 0) - (7 * (Index == 7));
		PackedIndices.x |= (Index << (i*3 + 16));
	}
	{
		i = 5;
		uint Index = RoundToUInt(Block[i] * Scale + Bias);
		Index += (Index > 0) - (7 * (Index == 7));
		PackedIndices.x |= (Index << 31);
		PackedIndices.y |= (Index >> 1);
	}
	for (i=6; i<16; ++i)
	{
		uint Index = RoundToUInt(Block[i] * Scale + Bias);
		Index += (Index > 0) - (7 * (Index == 7 ? 1 : 0));
		PackedIndices.y |= (Index << (i*3 - 16));
	}
	return PackedIndices;
}
uint4 CompressBC5Block( in float BlockU[16], in float BlockV[16] )
{
	float MinU, MaxU, MinV, MaxV;
	GetMinMax( BlockU, BlockV, MinU, MaxU, MinV, MaxV );
	uint MinUUint = RoundToUInt(MinU * 255.f);
	uint MaxUUint = RoundToUInt(MaxU * 255.f);
	uint MinVUint = RoundToUInt(MinV * 255.f);
	uint MaxVUint = RoundToUInt(MaxV * 255.f);
	uint2 IndicesU = 0;
	if (MinUUint < MaxUUint)
	{
		IndicesU = GetPackedAlphaBlockIndices( BlockU, MinU, MaxU );
	}
	uint2 IndicesV = 0;
	if (MinVUint < MaxVUint)
	{
		IndicesV = GetPackedAlphaBlockIndices( BlockV, MinV, MaxV );
	}
	return uint4((MinUUint << 8) | MaxUUint | IndicesU.x, IndicesU.y, (MinVUint << 8) | MaxVUint | IndicesV.x, IndicesV.y);
}
RWTexture2D<uint4> RWAtlasBlock4:register(u0);
Texture2D SourceNormalAtlas:register(t2);
void PS(
	float4 Position : SV_POSITION,
	float2 AtlasUV : TEXCOORD0
)
{
	float2 OneOverSourceAtlasSize=float2(0.0019531f,0.0019531f);
	uint2 WriteCoord = (uint2) Position.xy;
	{
		{
			float BlockTexelsX[16];
			float BlockTexelsY[16];
			ReadBlockXY(SourceNormalAtlas,  gsamPointClamp, AtlasUV - OneOverSourceAtlasSize, OneOverSourceAtlasSize, BlockTexelsX, BlockTexelsY);
			RWAtlasBlock4[WriteCoord] = CompressBC5Block(BlockTexelsX, BlockTexelsY);
		}
	}
}