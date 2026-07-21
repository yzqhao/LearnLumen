#include "../../GlobalConstant.hlsli"

StructuredBuffer<float4>  LumenCardScene_CardData:register(t0);
StructuredBuffer<float4>  LumenCardScene_CardPageData:register(t1);
Texture2D AlbedoAtlas:register(t2);
Texture2D EmissiveAtlas:register(t3);
StructuredBuffer<uint> CardTiles:register(t4);
Texture2D<float4> DirectLightingAtlas:register(t5);
Texture2D<float4> IndirectLightingAtlas:register(t6);
Texture2D<float4> RectCoordBuffer:register(t7);

RWTexture2D<float3> RWFinalLightingAtlas:register(u0);

SamplerState MinMagMipPointClampped:register(s0);
SamplerState MinMagMipLinearWrap:register(s1);
SamplerState MinMagMipLinearClampped:register(s2);
SamplerState MinMagLinearMipPointClampped:register(s3);
struct FTwoBandSHVector
{
	float4 V;
};
struct FTwoBandSHVectorRGB
{
	FTwoBandSHVector R;
	FTwoBandSHVector G;
	FTwoBandSHVector B;
};
uint3 Rand3DPCG16(int3 p)
{
	uint3 v = uint3(p);
	v = v * 1664525u + 1013904223u;
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	return v >> 16u;
}
float max3(float a, float b, float c)
{
	return max(a, max(b, c));
}
float3 select_internal(bool3   c, float a, float3 b) { return float3(c.x ? a   : b.x, c.y ? a   : b.y, c.z ? a   : b.z); }
bool3 IsFinite( float3 In) {	return (asuint(In) & 0x7F800000) != 0x7F800000; }
float3 MakeFinite( float3 In) {    return  select_internal( !IsFinite(In) , 0.0 , In ); }

float length2(float2 v)
{
	return dot(v, v);
}
float acosFast(float inX) 
{
    float x = abs(inX);
    float res = -0.156583f * x + (0.5 * PI);
    res *= sqrt(1.0f - x);
    return (inX >= 0) ? res : PI - res;
}
float2 acosFast( float2 x )
{
	return float2( acosFast(x.x), acosFast(x.y) );
}
float3 acosFast( float3 x )
{
	return float3( acosFast(x.x), acosFast(x.y), acosFast(x.z) );
}
float4 acosFast( float4 x )
{
	return float4( acosFast(x.x), acosFast(x.y), acosFast(x.z), acosFast(x.w) );
}
float4 Texture2DSampleLevel(Texture2D inTexture,SamplerState inSampler,float2 inTexcoord,float inMip){
    return inTexture.SampleLevel(inSampler,inTexcoord,inMip);
}
float4 Texture3DSampleLevel(Texture3D Tex, SamplerState Sampler, float3 UV, float Mip)
{
	return Tex.SampleLevel(Sampler, UV, Mip);
}
uint MurmurMix(uint Hash)
{
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}
float3 IntToColor(uint Index)
{
	uint Hash = MurmurMix(Index);
	float3 Color = float3
	(
		(Hash >>  0) & 255,
		(Hash >>  8) & 255,
		(Hash >> 16) & 255
	);
	return Color * (1.0f / 255.0f);
}
struct FLumenCardData
{
	float3x3 MeshCardsToLocalRotation;
	float3 MeshCardsOrigin;
	float3 MeshCardsExtent;
	float3x3 WorldToLocalRotation;
	float3 Origin;
	float3 LocalExtent;
	uint2 SizeInPages;
	uint PageTableOffset;
	uint2 HiResSizeInPages;
	uint HiResPageTableOffset;
	uint2 ResLevelToResLevelXYBias;
	bool bVisible;
	bool bHeightfield;
	uint AxisAlignedDirection;
	uint LightingChannelMask;
	float TexelSize;
};
FLumenCardData GetLumenCardData(uint CardId)
{
	FLumenCardData CardData = (FLumenCardData)0;
	uint BaseOffset = CardId * 10;
	float4 Vector0 = LumenCardScene_CardData[BaseOffset + 0];
	float4 Vector1 = LumenCardScene_CardData[BaseOffset + 1];
	float4 Vector2 = LumenCardScene_CardData[BaseOffset + 2];
	float4 Vector3 = LumenCardScene_CardData[BaseOffset + 3];
	float4 Vector4 = LumenCardScene_CardData[BaseOffset + 4];
	float4 Vector5 = LumenCardScene_CardData[BaseOffset + 5];
	float4 Vector6 = LumenCardScene_CardData[BaseOffset + 6];
	float4 Vector7 = LumenCardScene_CardData[BaseOffset + 7];
	float4 Vector8 = LumenCardScene_CardData[BaseOffset + 8];
	float4 Vector9 = LumenCardScene_CardData[BaseOffset + 9];
	float3 PositionHigh = Vector0.xyz;
	float3 PositionLow = float3(Vector1.w, Vector2.w, Vector3.w);
	CardData.WorldToLocalRotation[0] = Vector1.xyz;
	CardData.WorldToLocalRotation[1] = Vector2.xyz;
	CardData.WorldToLocalRotation[2] = Vector3.xyz;
	CardData.Origin = PositionHigh+PositionLow; 
	CardData.LocalExtent = abs(Vector4.xyz);
	uint Packed4W = asuint(Vector4.w);
	CardData.ResLevelToResLevelXYBias.x = (Packed4W >> 0) & 0xFF;
	CardData.ResLevelToResLevelXYBias.y = (Packed4W >> 8) & 0xFF;
	CardData.AxisAlignedDirection = (Packed4W >> 16) & 0xF;
	CardData.LightingChannelMask = (Packed4W >> 20) & 0xF;
	CardData.bVisible = (Packed4W >> 24) & 1;
	CardData.bHeightfield = (Packed4W >> 25) & 1;
	CardData.SizeInPages.x = (asuint(Vector5.x) >> 0) & 0xFFFF;
	CardData.SizeInPages.y = (asuint(Vector5.x) >> 16) & 0xFFFF;
	CardData.PageTableOffset = asuint(Vector5.y);
	CardData.HiResSizeInPages.x = (asuint(Vector5.z) >> 0) & 0xFFFF;
	CardData.HiResSizeInPages.y = (asuint(Vector5.z) >> 16) & 0xFFFF;
	CardData.HiResPageTableOffset = asuint(Vector5.w);
	CardData.MeshCardsToLocalRotation[0] = Vector6.xyz;
	CardData.MeshCardsToLocalRotation[1] = Vector7.xyz;
	CardData.MeshCardsToLocalRotation[2] = Vector8.xyz;
	CardData.MeshCardsOrigin = float3(Vector6.w, Vector7.w, Vector8.w);
	CardData.MeshCardsExtent = Vector9.xyz;
	CardData.TexelSize = Vector9.w;
	return CardData;
}
struct FLumenCardPageData
{
	uint CardIndex;
	bool bMapped;
	uint ResLevelPageTableOffset;
	uint2 ResLevelSizeInTiles;
	float2 SizeInTexels;
	float2 PhysicalAtlasCoord;
	float4 CardUVRect;
	float4 PhysicalAtlasUVRect;
	float2 CardUVTexelScale;
	float2 PhysicalAtlasUVTexelScale;
	uint LastDirectLightingUpdateFrameIndex;
	uint LastIndirectLightingUpdateFrameIndex;
	uint IndirectLightingTemporalIndex;
	uint DirectLightingTemporalIndex;
};
FLumenCardPageData GetLumenCardPageData(uint CardPageId)
{
	FLumenCardPageData CardPageData = (FLumenCardPageData) 0;
	uint BaseOffset = CardPageId * 5;
	float4 Vector0 = LumenCardScene_CardPageData[BaseOffset + 0];
	float4 Vector1 = LumenCardScene_CardPageData[BaseOffset + 1];
	float4 Vector2 = LumenCardScene_CardPageData[BaseOffset + 2];
	float4 Vector3 = LumenCardScene_CardPageData[BaseOffset + 3];
	float4 Vector4 = LumenCardScene_CardPageData[BaseOffset + 4];
	CardPageData.CardIndex = asuint(Vector0.x);
	CardPageData.ResLevelPageTableOffset = asuint(Vector0.y);
	CardPageData.SizeInTexels = Vector0.zw;
	CardPageData.CardUVRect = Vector1;
	CardPageData.PhysicalAtlasUVRect = Vector2;
	CardPageData.CardUVTexelScale = Vector3.xy;
	CardPageData.ResLevelSizeInTiles = asuint(Vector3.zw);
	CardPageData.LastDirectLightingUpdateFrameIndex = asuint(Vector4.x);
	CardPageData.LastIndirectLightingUpdateFrameIndex = asuint(Vector4.y);
	CardPageData.IndirectLightingTemporalIndex = asuint(Vector4.z);
	CardPageData.DirectLightingTemporalIndex = asuint(Vector4.w);
	CardPageData.bMapped = CardPageData.SizeInTexels.x > 0;
	CardPageData.PhysicalAtlasCoord = CardPageData.PhysicalAtlasUVRect.xy * LumenCardScene_PhysicalAtlasSize;
	CardPageData.PhysicalAtlasUVTexelScale = LumenCardScene_InvPhysicalAtlasSize;
	return CardPageData;
}
bool IsSurfaceCacheDepthValid(float Depth)
{
	return Depth < 1.0f;
}

float3 DecodeSurfaceCacheNormal(FLumenCardData Card, float2 EncodedNormal)
{
	float3 CardSpaceNormal;
	CardSpaceNormal.xy = EncodedNormal.xy * 2.0f - 1.0f;
	CardSpaceNormal.z = sqrt(max(1.0f - length2(CardSpaceNormal.xy), 0.0001f));
    //row major : row vector x row matrix
    //normal x it model matrix -> normal x inverse transpose model matrix -> normal x local to world matrix
    //Local -> World 
    // world -> local : inverse inverse transpose model matrix -> transpose model matrix
    // transpose local to world matrix x normal
	return normalize(
        mul(Card.WorldToLocalRotation, CardSpaceNormal)
    );
}
float3 GetCardLocalPosition(float3 CardLocalExtent, float2 CardUV, float Depth)
{
    //uv : 0~1
	CardUV.x = 1.0f - CardUV.x;//
	float3 LocalPosition;
	LocalPosition.xy = CardLocalExtent.xy * (1.0f - 2.0f * CardUV);//ortho
    //device z : 0~1,revert z
	LocalPosition.z = -(2.0f * Depth - 1.0f) * CardLocalExtent.z;
	return LocalPosition;
}
float3 GetCardWorldPosition(FLumenCardData Card, float2 CardUV, float Depth)
{
	float3 LocalPosition = GetCardLocalPosition(Card.LocalExtent, CardUV, Depth);
    //model matrix tranlate
    //position x model matrix : position x rotation matrix : position x local to world
    //world to local -> transpose local to world
    
	float3 WorldPosition = mul(Card.WorldToLocalRotation, LocalPosition) + Card.Origin;
	return WorldPosition;
}
float2 Hammersley16( uint Index, uint NumSamples, uint2 Random )
{
	float E1 = frac( (float)Index / NumSamples + float( Random.x ) * (1.0 / 65536.0) );
	float E2 = float( ( reversebits(Index) >> 16 ) ^ Random.y ) * (1.0 / 65536.0);
	return float2( E1, E2 );
}
uint2 GetProbeJitter(uint IndirectLightingTemporalIndex)
{
	uint TemporalIndex = (FixedJitterIndex < 0 ? IndirectLightingTemporalIndex : FixedJitterIndex);
	return Hammersley16(TemporalIndex % MaxFramesAccumulated, MaxFramesAccumulated, 0) * ProbeSpacingInRadiosityTexels;
}
struct FCardTileData
{
	uint CardPageIndex;
	uint2 TileCoord;
};
FCardTileData UnpackCardTileData(uint PackedTile)
{
	FCardTileData TileData;
	TileData.CardPageIndex = PackedTile & 0xFFFFFF;
	TileData.TileCoord.x = (PackedTile >> 24) & 0xF;
	TileData.TileCoord.y = (PackedTile >> 28) & 0xF;
	return TileData;
}
struct FRadiosityTexel
{
	bool bInsideAtlas;
	bool bHeightfield;
	bool bValid;
	float3 WorldPosition;
	float3 WorldNormal;
	float3x3 WorldToLocalRotation;
	uint2 AtlasCoord;
	uint2 CardCoord;
	uint IndirectLightingTemporalIndex;
};
float3 QuantizeForFloatRenderTarget(float3 Color, float E, const float3 QuantizationError)
{
	float3 Error = Color * QuantizationError;
	Error[0] = asfloat(asuint(Error[0]) & ~0x007FFFFF);
	Error[1] = asfloat(asuint(Error[1]) & ~0x007FFFFF);
	Error[2] = asfloat(asuint(Error[2]) & ~0x007FFFFF);
	return Color + Error * E;
}
float3 QuantizeForFloatRenderTarget(float3 Color, float E)
{
	return QuantizeForFloatRenderTarget(Color, E, TargetFormatQuantizationError);
}
float3 QuantizeForFloatRenderTarget(float3 Color, int3 P)
{
	uint2 Random = Rand3DPCG16(P).xy;
	float E = Hammersley16(0, 1, Random).x;
	return QuantizeForFloatRenderTarget(Color, E);
}
float CalculatePlaneWeight(float3 WorldPosition, float3 WorldNormal, float3 ProbeWorldPosition)
{
	float4 TexelPlane = float4(WorldNormal, dot(ProbeWorldPosition, WorldNormal));
	float PlaneDistance = abs(dot(float4(WorldPosition, -1), TexelPlane));
	float RelativeDistance = max(PlaneDistance / (length(ProbeWorldPosition - WorldPosition) + .01f), .1f);
	float DepthWeight = exp2(ProbePlaneWeightingDepthScale * (RelativeDistance * RelativeDistance));
	return DepthWeight > .01f ? 1 : 0;
}
uint2 GetRadiosityProbeAtlasCoord(FLumenCardPageData CardPage, FCardTileData CardTile, uint2 CoordInCardTile)
{
	uint2 AtlasCoord = CardPage.PhysicalAtlasCoord + CardTile.TileCoord * uint2(8, 8) + CoordInCardTile;
	return AtlasCoord >> ProbeSpacingInRadiosityTexelsDivideShift;
}
float3x3 GetTangentBasisFrisvad(float3 TangentZ)
{
	float3 TangentX;
	float3 TangentY;
	if (TangentZ.z < -0.9999999f)
	{
		TangentX = float3(0, -1, 0);
		TangentY = float3(-1, 0, 0);
	}
	else
	{
		float A = 1.0f / (1.0f + TangentZ.z);
		float B = -TangentZ.x * TangentZ.y * A;
		TangentX = float3(1.0f - TangentZ.x * TangentZ.x * A, B, -TangentZ.x);
		TangentY = float3(B, 1.0f - TangentZ.y * TangentZ.y * A, -TangentZ.y);
	}
	return float3x3( TangentX, TangentY, TangentZ );//TBN
}
float4 UniformSampleHemisphere( float2 E )
{
	float Phi = 2 * PI * E.x;
	float CosTheta = E.y;
	float SinTheta = sqrt( 1 - CosTheta * CosTheta );
	float3 H;
	H.x = SinTheta * cos( Phi );
	H.y = SinTheta * sin( Phi );
	H.z = CosTheta;
	float PDF = 1.0 / (2 * PI);
	return float4( H, PDF );
}

FTwoBandSHVector SHBasisFunction(float3 InputVector)
{
	FTwoBandSHVector Result;
	Result.V.x = 0.282095f; 
	Result.V.y = -0.488603f * InputVector.y;
	Result.V.z = 0.488603f * InputVector.z;
	Result.V.w = -0.488603f * InputVector.x;
	return Result;
}
FTwoBandSHVectorRGB MulSH(FTwoBandSHVectorRGB A, float Scalar)
{
	FTwoBandSHVectorRGB Result;
	Result.R.V = A.R.V * Scalar;
	Result.G.V = A.G.V * Scalar;
	Result.B.V = A.B.V * Scalar;
	return Result;
}
FTwoBandSHVectorRGB MulSH(FTwoBandSHVector A, float3 Color)
{
	FTwoBandSHVectorRGB Result;
	Result.R.V = A.V * Color.r;
	Result.G.V = A.V * Color.g;
	Result.B.V = A.V * Color.b;
	return Result;
}
FTwoBandSHVector AddSH(FTwoBandSHVector A, FTwoBandSHVector B)
{
	FTwoBandSHVector Result = A;
	Result.V += B.V;
	return Result;
}
FTwoBandSHVectorRGB AddSH(FTwoBandSHVectorRGB A, FTwoBandSHVectorRGB B)
{
	FTwoBandSHVectorRGB Result;
	Result.R = AddSH(A.R, B.R);
	Result.G = AddSH(A.G, B.G);
	Result.B = AddSH(A.B, B.B);
	return Result;
}
float DotSH(FTwoBandSHVector A,FTwoBandSHVector B)
{
	float Result = dot(A.V, B.V);
	return Result;
}
float3 DotSH(FTwoBandSHVectorRGB A,FTwoBandSHVector B)
{
	float3 Result = 0;
	Result.r = DotSH(A.R,B);
	Result.g = DotSH(A.G,B);
	Result.b = DotSH(A.B,B);
	return Result;
}
FTwoBandSHVector CalcDiffuseTransferSH(float3 Normal,float Exponent)
{
	FTwoBandSHVector Result = SHBasisFunction(Normal);
	float L0 =					2 * PI / (1 + 1 * Exponent							);
	float L1 =					2 * PI / (2 + 1 * Exponent							);
	Result.V.x *= L0;
	Result.V.yzw *= L1;
	return Result;
}
float3 DecodeSurfaceCacheAlbedo(float3 EncodedAlbedo)
{
	float3 Albedo = EncodedAlbedo * EncodedAlbedo;
	Albedo = min(saturate(pow(Albedo, DiffuseColorBoost)), 0.99f);
	return Albedo;
}
float3 CombineFinalLighting(float3 Albedo, float3 Emissive, float3 DirectLighting, float3 IndirectLighting)
{
	Albedo = DecodeSurfaceCacheAlbedo(Albedo);
	float3 DiffuseLambert = Albedo * (1 / PI);
	float3 FinalLighting = (DirectLighting + IndirectLighting) * DiffuseLambert + Emissive;
	FinalLighting = max(MakeFinite(FinalLighting), float3(0.0f, 0.0f, 0.0f));
	return FinalLighting;
}
//rect -> tile 8x8 -> 4x4 sub tile -> probe
[numthreads(8,8,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~1119,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,//(0~7,0~7,0)
    uint3 inDispatchThreadId : SV_DispatchThreadID){

	uint CardTileIndex = inGroupId.x;
	uint2 TexelCoordInTile = inGroupThreadId.xy;
    
	FCardTileData CardTile = UnpackCardTileData(CardTiles[CardTileIndex]);
	FLumenCardPageData CardPage = GetLumenCardPageData(CardTile.CardPageIndex);
	FLumenCardData Card = GetLumenCardData(CardPage.CardIndex);
	uint2 CoordInCardPage = 8 * CardTile.TileCoord + TexelCoordInTile;
	uint2 AtlasCoord = CardPage.PhysicalAtlasCoord + CoordInCardPage;
	float2 AtlasUV = CardPage.PhysicalAtlasUVRect.xy + CardPage.PhysicalAtlasUVTexelScale * (CoordInCardPage + 0.5);
	float2 IndirectLightingAtlasUV = AtlasUV;
	float3 Albedo = Texture2DSampleLevel(AlbedoAtlas, MinMagLinearMipPointClampped, AtlasUV, 0).xyz;
	float3 Emissive = Texture2DSampleLevel(EmissiveAtlas, MinMagLinearMipPointClampped, AtlasUV, 0).xyz;
	float3 DirectLighting = Texture2DSampleLevel(DirectLightingAtlas, MinMagLinearMipPointClampped, AtlasUV, 0).xyz;
	float3 IndirectLighting = Texture2DSampleLevel(IndirectLightingAtlas, MinMagLinearMipPointClampped, IndirectLightingAtlasUV, 0).xyz;
	
    float3 FinalLighting = CombineFinalLighting(Albedo, Emissive, DirectLighting, IndirectLighting);
	
    RWFinalLightingAtlas[AtlasCoord] = QuantizeForFloatRenderTarget(FinalLighting, int3(AtlasCoord, View_StateFrameIndexMod8.x + 2));
}