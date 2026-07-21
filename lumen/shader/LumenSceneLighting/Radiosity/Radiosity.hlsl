#include "../../GlobalConstant.hlsli"

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
Buffer<uint4> RectCoordBuffer:register(t0);
StructuredBuffer<uint> CardTileData:register(t1);
Texture2D LumenCardScene_NormalAtlas:register(t2);
Texture2D LumenCardScene_DepthAtlas:register(t3);
StructuredBuffer<float4>  LumenCardScene_CardData:register(t4);
/*StructuredBuffer<float4>  LumenCardScene_MeshCardsData;
ByteAddressBuffer  LumenCardScene_PageTableBuffer;
ByteAddressBuffer  LumenCardScene_SceneInstanceIndexToMeshCardsIndexBuffer;
*/
Texture3D  View_GlobalDistanceFieldPageAtlasTexture:register(t5);
Texture3D  View_GlobalDistanceFieldCoverageAtlasTexture:register(t6);
Texture3D<uint>  View_GlobalDistanceFieldPageTableTexture:register(t7);
Texture3D  View_GlobalDistanceFieldMipTexture:register(t8);
Texture2D LumenCardScene_AlbedoAtlas:register(t9);
Texture2D LumenCardScene_EmissiveAtlas:register(t10);
StructuredBuffer<float4>  LumenCardScene_CardPageData:register(t11);
Texture2D  BlueNoise_Vec2Texture:register(t12);
StructuredBuffer<uint4> GlobalDistanceFieldPageObjectGridBuffer:register(t13);
ByteAddressBuffer  LumenCardScene_SceneInstanceIndexToMeshCardsIndexBuffer:register(t14);
StructuredBuffer<float4>  LumenCardScene_MeshCardsData:register(t15);
ByteAddressBuffer  LumenCardScene_PageTableBuffer:register(t16);
Texture2D FinalLightingAtlas:register(t17);

RWTexture2D<float3> RWTraceRadianceAtlas:register(u0);

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

//aabb ray
float2 LineBoxIntersect(float3 RayOrigin, float3 RayEnd, float3 BoxMin, float3 BoxMax)
{
    float3 InvRayDir = 1.0f / (RayEnd - RayOrigin);
    float3 FirstPlaneIntersections = (BoxMin - RayOrigin) * InvRayDir;
    float3 SecondPlaneIntersections = (BoxMax - RayOrigin) * InvRayDir;
    float3 ClosestPlaneIntersections = min(FirstPlaneIntersections, SecondPlaneIntersections);
    float3 FurthestPlaneIntersections = max(FirstPlaneIntersections, SecondPlaneIntersections);
    float2 BoxIntersections;
    BoxIntersections.x = max(ClosestPlaneIntersections.x, max(ClosestPlaneIntersections.y, ClosestPlaneIntersections.z));
    BoxIntersections.y = min(FurthestPlaneIntersections.x, min(FurthestPlaneIntersections.y, FurthestPlaneIntersections.z));
    return saturate(BoxIntersections);
}
float4x4 Make4x3Matrix(float4x4 M)
{
    float4x4 Result;
    Result[0] = float4(M[0].xyz, 0.0f);
    Result[1] = float4(M[1].xyz, 0.0f);
    Result[2] = float4(M[2].xyz, 0.0f);
    Result[3] = float4(M[3].xyz, 1.0f);
    return Result;
}
struct FDFVector3
{
    float3 High;
    float3 Low;
};
struct FDFMatrix
{
    float4x4 M;
    float3 PostTranslation; 
};
struct FDFInverseMatrix
{
    float4x4 M;
    float3 PreTranslation; 
};
struct FDFObjectData
{
    float3 VolumePositionExtent;
    float VolumeSurfaceBias;
    bool bMostlyTwoSided;
    float VolumeScale;
    float SelfShadowBias;
    float2 MinMaxDrawDistance2;
    uint GPUSceneInstanceIndex;
    FDFInverseMatrix WorldToVolume;
    FDFMatrix VolumeToWorld;
    float3 VolumeToWorldScale;
    uint AssetIndex;
};
FDFMatrix MakeDFMatrix(float3 PostTranslation, float4x4 InMatrix)
{
    FDFMatrix Result;
    Result.PostTranslation = PostTranslation;
    Result.M = InMatrix;
    return Result;
}
FDFInverseMatrix MakeDFInverseMatrix4x3(float3 PreTranslation, float4x4 InMatrix)
{
    FDFInverseMatrix Result;
    Result.PreTranslation = PreTranslation;
    Result.M = Make4x3Matrix(InMatrix);
    return Result;
}
float4x4 MultiplyTranslation(float3 Translation, float4x4 M)
{
    return mul(float4x4(
        float4(1.0f,0.0f,0.0f,0.0f),
        float4(0.0f,1.0f,0.0f,0.0f),
        float4(0.0f,0.0f,1.0f,0.0f),
        float4(Translation,1.0f)),M);
}
precise float3 MakePrecise(in precise float3 v) { precise float3 pv = v; return pv; }
float3 DFFastLocalSubtractDemote(FDFVector3 Lhs, float3 Rhs)
{
    const float3 High =   MakePrecise( ( Lhs.High ) - ( Rhs ) );
    const float3 Sum =   MakePrecise( ( High ) + ( Lhs.Low ) );
    return Sum;
}
float4x4 DFFastMultiplyTranslationDemote(FDFVector3 Lhs, FDFInverseMatrix Rhs)
{
    float3 Translation = DFFastLocalSubtractDemote(Lhs, Rhs.PreTranslation);
    float4x4 Result = MultiplyTranslation(Translation, Rhs.M);
    return Result;
}
FDFVector3 MakeDFVector3(float3 High, float3 Low)
{
    FDFVector3 Result;
    Result.High = High;
    Result.Low = Low;
    return Result;
}
FDFVector3 DFNegate(FDFVector3 Value)
{
    return MakeDFVector3(-Value.High, -Value.Low);
}
float4x4 DFFastToTranslatedWorld(FDFInverseMatrix WorldToLocal, FDFVector3 PreViewTranslation)
{
    return DFFastMultiplyTranslationDemote(DFNegate(PreViewTranslation), WorldToLocal);
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
struct FLumenSurfaceCacheData
{
	bool bValid;
	float Depth;
	float3 Albedo;
	float3 Emissive;
	float3 WorldPosition;
	float3 WorldNormal;
};
FLumenSurfaceCacheData GetSurfaceCacheData(FLumenCardData Card, float2 CardUV, float2 AtlasUV)
{
	float Depth = Texture2DSampleLevel(LumenCardScene_DepthAtlas,  gsamPointClamp, AtlasUV, 0).x;
	FLumenSurfaceCacheData SurfaceCacheData;
	SurfaceCacheData.Depth = Depth;
	SurfaceCacheData.bValid = IsSurfaceCacheDepthValid(Depth);
	SurfaceCacheData.Albedo = float3(0.0f, 0.0f, 0.0f);
	SurfaceCacheData.Emissive = float3(0.0f, 0.0f, 0.0f);
	float2 NormalXY = float2(0.5f, 0.5f);
	if (SurfaceCacheData.bValid)
	{
		NormalXY = Texture2DSampleLevel(LumenCardScene_NormalAtlas,  gsamPointClamp, AtlasUV, 0).xy;
	}
	SurfaceCacheData.WorldNormal = DecodeSurfaceCacheNormal(Card, NormalXY);
	SurfaceCacheData.WorldPosition = GetCardWorldPosition(Card, CardUV, SurfaceCacheData.Depth);
	return SurfaceCacheData;
}
struct FGlobalSDFTraceInput
{
	float3 TranslatedWorldRayStart;
	float3 WorldRayDirection;
	float MinTraceDistance;
	float MaxTraceDistance;
	float StepFactor;
	float MinStepFactor;
	bool bExpandSurfaceUsingRayTimeInsteadOfMaxDistance;
	float InitialMaxDistance;
	float VoxelSizeRelativeBias;
	float VoxelSizeRelativeRayEndBias;
	bool bDitheredTransparency;
	float2 DitherScreenCoord;
};
FGlobalSDFTraceInput SetupGlobalSDFTraceInput(float3 InTranslatedWorldRayStart, float3 InWorldRayDirection, float InMinTraceDistance, float InMaxTraceDistance, float InStepFactor, float InMinStepFactor)
{
	FGlobalSDFTraceInput TraceInput;
	TraceInput.TranslatedWorldRayStart = InTranslatedWorldRayStart;
	TraceInput.WorldRayDirection = InWorldRayDirection;
	TraceInput.MinTraceDistance = InMinTraceDistance;
	TraceInput.MaxTraceDistance = InMaxTraceDistance;
	TraceInput.StepFactor = InStepFactor;
	TraceInput.MinStepFactor = InMinStepFactor;
	TraceInput.bExpandSurfaceUsingRayTimeInsteadOfMaxDistance = true;
	TraceInput.InitialMaxDistance = 0;
	TraceInput.VoxelSizeRelativeBias = 0.0f;
	TraceInput.VoxelSizeRelativeRayEndBias = 0.0f;
	TraceInput.bDitheredTransparency = false;
	TraceInput.DitherScreenCoord = float2(0, 0);
	return TraceInput;
}
float GetCardBiasForShadowing(float3 L, float3 WorldNormal, float BiasValue)
{
	float SurfaceBias = BiasValue;
	float SlopeScaledSurfaceBias = 2.0f * BiasValue;
	return SurfaceBias + SlopeScaledSurfaceBias * saturate(1 - dot(L, WorldNormal));//scale + bias
}
float InterleavedGradientNoise( float2 uv, float FrameId )
{
	uv += FrameId * (float2(47, 17) * 0.695f);
    const float3 magic = float3( 0.06711056f, 0.00583715f, 52.9829189f );
    return frac(magic.z * frac(dot(uv, magic.xy)));
}
float ComputeDistanceFromBoxToPointInside(float3 BoxCenter, float3 BoxExtent, float3 InPoint)
{
	float3 DistancesToMin = max(InPoint - BoxCenter + BoxExtent, 0);
	float3 DistancesToMax = max(BoxCenter + BoxExtent - InPoint, 0);
	float3 ClosestDistances = min(DistancesToMin, DistancesToMax);
	return min(ClosestDistances.x, min(ClosestDistances.y, ClosestDistances.z));
}
uint ComputeGlobalDistanceFieldClipmapIndex(float3 TranslatedWorldPosition)
{
	uint FoundClipmapIndex = 0;
	for (uint ClipmapIndex = 0; ClipmapIndex < View_NumGlobalSDFClipmaps.x; ClipmapIndex++)
	{
		float DistanceFromClipmap = ComputeDistanceFromBoxToPointInside(View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].xyz, View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].www, TranslatedWorldPosition);
		if (DistanceFromClipmap > View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].w * View_GlobalVolumeTexelSize.x)
		{
			FoundClipmapIndex = ClipmapIndex;
			break;
		}
	}
	return FoundClipmapIndex;
}
float3 ComputeGlobalUV(float3 TranslatedWorldPosition, uint ClipmapIndex)
{
	float4 TranslatedWorldToUVAddAndMul = View_GlobalVolumeTranslatedWorldToUVAddAndMul[ClipmapIndex];
	float3 UV = frac(TranslatedWorldPosition * TranslatedWorldToUVAddAndMul.www + TranslatedWorldToUVAddAndMul.xyz); 
	UV = frac(UV); 
	return UV;
}
float3 ComputeGlobalMipUV(float3 TranslatedWorldPosition, uint ClipmapIndex)
{
	float3 MipUV = saturate(TranslatedWorldPosition * View_GlobalDistanceFieldMipTranslatedWorldToUVScale[ClipmapIndex].xyz + View_GlobalDistanceFieldMipTranslatedWorldToUVBias[ClipmapIndex].xyz);
	float MipUVMinZ = View_GlobalDistanceFieldMipTranslatedWorldToUVScale[ClipmapIndex].w;
	float MipUVMaxZ = View_GlobalDistanceFieldMipTranslatedWorldToUVBias[ClipmapIndex].w;
	MipUV.z = clamp(MipUV.z, MipUVMinZ, MipUVMaxZ);
	return MipUV;
}
float DecodeGlobalDistanceFieldPageDistance(float EncodedDistance, float ClipmapInfluenceRange)
{
	return (EncodedDistance * 2.0f - 1.0f) * ClipmapInfluenceRange;
}
struct FGlobalDistanceFieldPage
{
	uint PageIndex;
	bool bValid;
	bool bCoverage;
};
uint3 GlobalDistanceFieldPageLinearIndexToPageAtlasOffset(FGlobalDistanceFieldPage Page)
{
	uint3 PageAtlasOffset;
	PageAtlasOffset.x = Page.PageIndex & 0x7F;
	PageAtlasOffset.y = (Page.PageIndex >> 7) & 0x7F;
	PageAtlasOffset.z = Page.PageIndex >> 14;
	return PageAtlasOffset;
}
FGlobalDistanceFieldPage UnpackGlobalDistanceFieldPage(uint PackedPage)
{
	FGlobalDistanceFieldPage Page;
	Page.PageIndex = PackedPage & 0x00FFFFFF;
	Page.bCoverage = PackedPage & 0x80000000;
	Page.bValid = PackedPage < 0xFFFFFFFF;
	return Page;
}
FGlobalDistanceFieldPage GetGlobalDistanceFieldPage(float3 VolumeUV, uint ClipmapIndex)
{
	int4 PageTableCoord = int4(saturate(VolumeUV) * View_GlobalDistanceFieldClipmapSizeInPages.x + int3(0, 0, ClipmapIndex * View_GlobalDistanceFieldClipmapSizeInPages.x), 0);
	uint PackedPage = View_GlobalDistanceFieldPageTableTexture.Load(PageTableCoord);
	return UnpackGlobalDistanceFieldPage(PackedPage);
}
void ComputeGlobalDistanceFieldPageUV(float3 VolumeUV, FGlobalDistanceFieldPage Page, out float3 OutPageUV, out float3 OutCoveragePageUV)
{
	uint3 PageAtlasOffset = GlobalDistanceFieldPageLinearIndexToPageAtlasOffset(Page);
	float3 VolumePageUV = frac(VolumeUV * View_GlobalDistanceFieldClipmapSizeInPages.x);
	float3 PageAtlasCoord = PageAtlasOffset * 8 + VolumePageUV * (8 - 1) + 0.5f;
	OutPageUV = PageAtlasCoord * View_GlobalDistanceFieldInvPageAtlasSize.xyz;
	float3 CoveragePageAtlasCoord = PageAtlasOffset * 4 + VolumePageUV * (4 - 1) + 0.5f;
	OutCoveragePageUV = CoveragePageAtlasCoord * View_GlobalDistanceFieldInvCoverageAtlasSize.xyz;
}
float3 ComputeGlobalDistanceFieldPageUV(float3 VolumeUV, FGlobalDistanceFieldPage Page)
{
	uint3 PageAtlasOffset = GlobalDistanceFieldPageLinearIndexToPageAtlasOffset(Page);
	float3 VolumePageUV = frac(VolumeUV * View_GlobalDistanceFieldClipmapSizeInPages.x);
	float3 PageAtlasCoord = PageAtlasOffset * 8 + VolumePageUV * (8 - 1) + 0.5f;
	float3 PageUV = PageAtlasCoord * View_GlobalDistanceFieldInvPageAtlasSize.xyz;
	return PageUV;
}
struct FGlobalSDFTraceResult
{
	float HitTime;
	uint HitClipmapIndex;
	uint TotalStepsTaken;
	float ExpandSurfaceAmount;
};
FGlobalSDFTraceResult RayTraceGlobalDistanceField(FGlobalSDFTraceInput TraceInput)
{
	FGlobalSDFTraceResult TraceResult;
	TraceResult.HitTime = -1.0f;
	TraceResult.HitClipmapIndex = 0;
	TraceResult.TotalStepsTaken = 0;
	TraceResult.ExpandSurfaceAmount = 0;
	float TraceNoise = InterleavedGradientNoise(TraceInput.DitherScreenCoord.xy, View_StateFrameIndexMod8.x);
	uint MinClipmapIndex = ComputeGlobalDistanceFieldClipmapIndex(TraceInput.TranslatedWorldRayStart + TraceInput.MinTraceDistance * TraceInput.WorldRayDirection);
	float MaxDistance = TraceInput.InitialMaxDistance;
	float MinRayTime = TraceInput.MinTraceDistance;
	[loop]
	for (uint ClipmapIndex = MinClipmapIndex; ClipmapIndex < View_NumGlobalSDFClipmaps.x && TraceResult.HitTime < 0.0f; ++ClipmapIndex)
	{
		float ClipmapVoxelExtent = View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].w * View_GlobalVolumeTexelSize.x;
		float MinStepSize = TraceInput.MinStepFactor * ClipmapVoxelExtent;
		float ExpandSurfaceDistance = ClipmapVoxelExtent;
		float ClipmapRayBias = ClipmapVoxelExtent * TraceInput.VoxelSizeRelativeBias;
		float ClipmapRayLength = TraceInput.MaxTraceDistance - ClipmapVoxelExtent * TraceInput.VoxelSizeRelativeRayEndBias;
		float3 GlobalVolumeTranslatedCenter = View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].xyz;
		float GlobalVolumeExtent = View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].w - ClipmapVoxelExtent;
		float3 TranslatedWorldRayEnd = TraceInput.TranslatedWorldRayStart + TraceInput.WorldRayDirection * ClipmapRayLength;
		float2 IntersectionTimes = LineBoxIntersect(TraceInput.TranslatedWorldRayStart, TranslatedWorldRayEnd, GlobalVolumeTranslatedCenter - GlobalVolumeExtent.xxx, GlobalVolumeTranslatedCenter + GlobalVolumeExtent.xxx);
		IntersectionTimes.xy *= ClipmapRayLength;
		IntersectionTimes.x = max(IntersectionTimes.x, MinRayTime);
		IntersectionTimes.x = max(IntersectionTimes.x, ClipmapRayBias);
		if (IntersectionTimes.x < IntersectionTimes.y)
		{
			MinRayTime = IntersectionTimes.y;
			float SampleRayTime = IntersectionTimes.x;
			const float ClipmapInfluenceRange = 4 * 2.0f * View_GlobalVolumeTranslatedCenterAndExtent[ClipmapIndex].w * View_GlobalVolumeTexelSize.x;
			uint StepIndex = 0;
			const uint MaxSteps = 256;
			[loop]
			for (; StepIndex < MaxSteps; ++StepIndex)
			{
				float3 SampleTranslatedWorldPosition = TraceInput.TranslatedWorldRayStart + TraceInput.WorldRayDirection * SampleRayTime;
				float3 ClipmapVolumeUV = ComputeGlobalUV(SampleTranslatedWorldPosition, ClipmapIndex);
				float3 MipUV = ComputeGlobalMipUV(SampleTranslatedWorldPosition, ClipmapIndex);
				float DistanceFieldMipValue = Texture3DSampleLevel(View_GlobalDistanceFieldMipTexture,  gsamLinearClamp, MipUV, 0).x;
				float DistanceField = DecodeGlobalDistanceFieldPageDistance(DistanceFieldMipValue, View_GlobalDistanceFieldMipFactor.x * ClipmapInfluenceRange);
				float Coverage = 1;
				FGlobalDistanceFieldPage Page = GetGlobalDistanceFieldPage(ClipmapVolumeUV, ClipmapIndex);
				if (Page.bValid && DistanceFieldMipValue < View_GlobalDistanceFieldMipTransition.x)
				{
					float3 PageUV = ComputeGlobalDistanceFieldPageUV(ClipmapVolumeUV, Page);
					if (Page.bCoverage)
					{
						float3 CoveragePageUV;
						ComputeGlobalDistanceFieldPageUV(ClipmapVolumeUV, Page, PageUV, CoveragePageUV);
						Coverage = Texture3DSampleLevel(View_GlobalDistanceFieldCoverageAtlasTexture,  gsamLinearWrap, CoveragePageUV, 0).x;
					}
					float DistanceFieldValue = Texture3DSampleLevel(View_GlobalDistanceFieldPageAtlasTexture,  gsamLinearWrap, PageUV, 0).x;
					DistanceField = DecodeGlobalDistanceFieldPageDistance(DistanceFieldValue, ClipmapInfluenceRange);
				}
				MaxDistance = max(DistanceField, MaxDistance);
				float ExpandSurfaceTime = TraceInput.bExpandSurfaceUsingRayTimeInsteadOfMaxDistance ? SampleRayTime - ClipmapRayBias : MaxDistance;
				float ExpandSurfaceScale = lerp(View_NotCoveredExpandSurfaceScale.x, View_CoveredExpandSurfaceScale.x, Coverage);
				const float ExpandSurfaceFalloff = 2.0f * ExpandSurfaceDistance;
				const float ExpandSurfaceAmount = ExpandSurfaceDistance * saturate(ExpandSurfaceTime / ExpandSurfaceFalloff) * ExpandSurfaceScale;
				float StepNoise = InterleavedGradientNoise(TraceInput.DitherScreenCoord.xy, View_StateFrameIndexMod8.x * MaxSteps + StepIndex);
				if (DistanceField < ExpandSurfaceAmount 
					&& (!TraceInput.bDitheredTransparency || (StepNoise * (1 - Coverage) <= View_DitheredTransparencyStepThreshold.x && TraceNoise * (1 - Coverage) <= View_DitheredTransparencyTraceThreshold.x)))
				{
					TraceResult.HitTime = max(SampleRayTime + DistanceField - ExpandSurfaceAmount, 0.0f);
					TraceResult.HitClipmapIndex = ClipmapIndex;
					TraceResult.ExpandSurfaceAmount = ExpandSurfaceAmount;
					break;
				}
				float LocalMinStepSize = MinStepSize * lerp(View_NotCoveredMinStepScale.x, 1.0f, Coverage);
				float StepDistance = max(DistanceField * TraceInput.StepFactor, LocalMinStepSize);
				SampleRayTime += StepDistance;
				if (SampleRayTime > IntersectionTimes.y || TraceResult.HitTime >= 0.0f)
				{
					break;
				}
			}
			TraceResult.TotalStepsTaken += StepIndex;
		}
	}
	return TraceResult;
}
bool GlobalSDFTraceResultIsHit(FGlobalSDFTraceResult TraceResult)
{ 
	return TraceResult.HitTime >= 0.0f;
}
float TraceOffscreenShadows(uint2 TexelCoord, float3 TranslatedWorldPosition, float3 L, float3 WorldNormal)
{
	float ShadowFactor = 1.0f;
	float TraceDistance = 20000.0f;
	float GlobalSDFShadowRayBias = 1.0f;
    {
		FGlobalSDFTraceInput TraceInput = SetupGlobalSDFTraceInput(TranslatedWorldPosition, L, 0.0f, TraceDistance, 1.0f, 1.0f);
		TraceInput.VoxelSizeRelativeBias = GetCardBiasForShadowing(L, WorldNormal, GlobalSDFShadowRayBias);
		TraceInput.DitherScreenCoord = TexelCoord;
		FGlobalSDFTraceResult SDFResult = RayTraceGlobalDistanceField(TraceInput);
		ShadowFactor = GlobalSDFTraceResultIsHit(SDFResult) ? 0.0f : 1.0f;
	}
	return ShadowFactor;
}
float InverseExposureLerp(float Exposure, float Alpha)
{
	float LerpLogScale = -Alpha * log(Exposure);
	float Scale = exp(LerpLogScale);
	return Scale;
}
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
float2 Hammersley16( uint Index, uint NumSamples, uint2 Random )
{
	float E1 = frac( (float)Index / NumSamples + float( Random.x ) * (1.0 / 65536.0) );
	float E2 = float( ( reversebits(Index) >> 16 ) ^ Random.y ) * (1.0 / 65536.0);
	return float2( E1, E2 );
}
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
	//TargetFormatQuantizationError 0.01563, 0.01563, 0.03125 192 float3
	float3 TargetFormatQuantizationError=float3(0.01563f, 0.01563f, 0.03125f);
	return QuantizeForFloatRenderTarget(Color, E, TargetFormatQuantizationError);
}
float3 QuantizeForFloatRenderTarget(float3 Color, int3 P)
{
	uint2 Random = Rand3DPCG16(P).xy;
	float E = Hammersley16(0, 1, Random).x;
	return QuantizeForFloatRenderTarget(Color, E);
}
float3 DecodeSurfaceCacheAlbedo(float3 EncodedAlbedo)
{
	float DiffuseColorBoost=1.0f;
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
FCardTileData GetCardTile(uint CardTileIndex)
{
	return UnpackCardTileData(CardTileData[CardTileIndex]);
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
FRadiosityTexel GetRadiosityTexel(FLumenCardPageData CardPage, uint2 CoordInCardPage)
{
	FRadiosityTexel RadiosityTexel = (FRadiosityTexel)0;
	RadiosityTexel.bInsideAtlas = false;
	RadiosityTexel.bHeightfield = false;
	RadiosityTexel.bValid = false;
	RadiosityTexel.WorldPosition = float3(0.0f, 0.0f, 0.0f);
	RadiosityTexel.WorldNormal = float3(0.0f, 0.0f, 0.0f);
	FLumenCardData Card = GetLumenCardData(CardPage.CardIndex);
	float2 AtlasUV = CardPage.PhysicalAtlasUVRect.xy + CardPage.PhysicalAtlasUVTexelScale * (CoordInCardPage + 0.5f * 1);
	float2 CardUV = CardPage.CardUVRect.xy + CardPage.CardUVTexelScale * (CoordInCardPage + 0.5f * 1);
	if (all(CoordInCardPage < (uint2)CardPage.SizeInTexels))
	{
		int2 RadiosityAtlasSize=int2(4096,4096);
		RadiosityTexel.bInsideAtlas = true;
		RadiosityTexel.bHeightfield = Card.bHeightfield;
		RadiosityTexel.AtlasCoord = AtlasUV * RadiosityAtlasSize;
		RadiosityTexel.CardCoord = (CardPage.CardUVRect.xy * CardPage.ResLevelSizeInTiles) * 8 + CoordInCardPage;
		RadiosityTexel.IndirectLightingTemporalIndex = CardPage.IndirectLightingTemporalIndex;
		RadiosityTexel.WorldToLocalRotation = Card.WorldToLocalRotation;
		{
			FLumenSurfaceCacheData SurfaceCacheData = GetSurfaceCacheData(Card, CardUV, AtlasUV);
			RadiosityTexel.bValid = SurfaceCacheData.bValid;
			RadiosityTexel.WorldPosition = SurfaceCacheData.WorldPosition;
			RadiosityTexel.WorldNormal = SurfaceCacheData.WorldNormal;
		}
	}
	return RadiosityTexel;
}
FRadiosityTexel GetRadiosityTexelFromCardTile(uint CardTileIndex, uint2 CoordInCardTile)
{
	FRadiosityTexel RadiosityTexel = (FRadiosityTexel)0;
	RadiosityTexel.bInsideAtlas = false;
	if (CardTileIndex < 1120)
	{
		FCardTileData CardTile = GetCardTile(CardTileIndex);
		uint2 CoordInCardPage = CardTile.TileCoord * 8 + CoordInCardTile;
		FLumenCardPageData CardPage = GetLumenCardPageData(CardTile.CardPageIndex);
		RadiosityTexel = GetRadiosityTexel(CardPage, CoordInCardPage);
	}
	return RadiosityTexel;
}
float2 BlueNoiseVec2(uint2 ScreenCoord, uint FrameIndex)
{
	uint3 WrappedCoordinate = uint3(ScreenCoord, FrameIndex) & BlueNoise_ModuloMasks;
	uint3 TextureCoordinate = uint3(WrappedCoordinate.x, WrappedCoordinate.z * BlueNoise_Dimensions.y + WrappedCoordinate.y, 0);
	return BlueNoise_Vec2Texture.Load(TextureCoordinate, 0).xy;
}
float2 GetProbeTexelCenter(uint IndirectLightingTemporalIndex, uint2 ProbeTileCoord)
{
	uint TemporalIndex = (FixedJitterIndex < 0 ? IndirectLightingTemporalIndex : FixedJitterIndex);
	return BlueNoiseVec2(ProbeTileCoord, TemporalIndex); 
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
void GetRadiosityRay(FRadiosityTexel RadiosityTexel, uint2 ProbeCoord, uint2 TracingTexelCoord, out float3 WorldRayDirection, out float ConeHalfAngle, out float PDF)
{
	float2 ProbeTexelCenter = GetProbeTexelCenter(RadiosityTexel.IndirectLightingTemporalIndex, ProbeCoord);
	float2 ProbeUV = (TracingTexelCoord + ProbeTexelCenter) / float(HemisphereProbeResolution);
	float3 LocalRayDirection;

	float4 Sample = UniformSampleHemisphere(ProbeUV);
	LocalRayDirection = Sample.xyz;
	PDF = Sample.w;

	float3x3 TangentBasis = GetTangentBasisFrisvad(RadiosityTexel.WorldNormal);
	WorldRayDirection = mul(LocalRayDirection, TangentBasis);
	ConeHalfAngle = acosFast(1.0f - 1.0f / (float)(NumTracesPerProbe));
}
struct FConeTraceInput
{
	float3 ConeOrigin;
	float3 ConeTranslatedOrigin;
	float3 ConeDirection;
	float ConeAngle; 
	float TanConeAngle;
	float ConeStartRadius;
	float MinSampleRadius;
	float MinTraceDistance;
	float MaxTraceDistance;
	float StepFactor;
	float VoxelTraceStartDistance;
	float SDFStepFactor;
	float MinSDFStepFactor;
	bool bExpandSurfaceUsingRayTimeInsteadOfMaxDistance;
	float InitialMaxDistance;
	bool bDitheredTransparency;
	uint2 DitherScreenCoord;
	bool bUseEpsilonTraceForHeightfields;
	bool bHiResSurface;
	bool bZeroRadianceIfRayStartsInsideGeometry;
	bool bCalculateHitVelocity;
	uint NumMeshSDFs;
	uint MeshSDFStartOffset;
	uint MeshSDFBitmaskStartOffset;
	float CardInterpolateInfluenceRadius;
	uint NumHeightfields;
	uint HeightfieldStartOffset;
	void Setup(
		float3 InConeOrigin,
		float3 InConeTranslatedOrigin,
		float3 InConeDirection,
		float InConeAngle,
		float InMinSampleRadius,
		float InMinTraceDistance,
		float InMaxTraceDistance,
		float InStepFactor)
	{
		ConeOrigin = InConeOrigin;
		ConeTranslatedOrigin = InConeTranslatedOrigin;
		ConeDirection = InConeDirection;
		ConeAngle = InConeAngle;
		TanConeAngle = tan(ConeAngle);
		ConeStartRadius = 0;
		MinSampleRadius = InMinSampleRadius;
		MinTraceDistance = InMinTraceDistance;
		MaxTraceDistance = InMaxTraceDistance;
		StepFactor = InStepFactor;
		VoxelTraceStartDistance = InMaxTraceDistance;
		SDFStepFactor = 1.0f;
		MinSDFStepFactor = 1.0f;
		bExpandSurfaceUsingRayTimeInsteadOfMaxDistance = true;
		InitialMaxDistance = 0;
		bDitheredTransparency = false;
		DitherScreenCoord = uint2(0, 0);
		bHiResSurface = false;
		bCalculateHitVelocity = false;
		bUseEpsilonTraceForHeightfields = true;
		bZeroRadianceIfRayStartsInsideGeometry = false;
	}
};
float CalculateVoxelTraceStartDistance(float MinTraceDistance, float MaxTraceDistance, float MaxMeshSDFTraceDistance, bool bContinueCardTracing)
{
	float VoxelTraceStartDistance = MaxTraceDistance;
	if (View_NumGlobalSDFClipmaps.x > 0)
	{
		VoxelTraceStartDistance = MinTraceDistance;
		if (bContinueCardTracing)
		{
			VoxelTraceStartDistance = max(VoxelTraceStartDistance, MaxMeshSDFTraceDistance);
		}
	}
	return VoxelTraceStartDistance;
}
struct FConeTraceResult
{
	float3 Lighting;
	float Transparency;
	float NumSteps;
	float NumOverlaps;
	float OpaqueHitDistance;
	float ExpandSurfaceAmount;
	float3 Debug;
	float3 GeometryWorldNormal;
	float3 WorldVelocity;
};
float GlobalDistanceFieldSampleClipmap(float3 ClipmapVolumeUV, uint ClipmapIndex)
{
	float DistanceFieldValue = 1.0f;
	ClipmapVolumeUV = frac(ClipmapVolumeUV);
	FGlobalDistanceFieldPage Page = GetGlobalDistanceFieldPage(ClipmapVolumeUV, ClipmapIndex);
	if (Page.bValid)
	{
		float3 PageUV = ComputeGlobalDistanceFieldPageUV(ClipmapVolumeUV, Page);
		DistanceFieldValue = Texture3DSampleLevel(View_GlobalDistanceFieldPageAtlasTexture,  gsamLinearWrap, PageUV, 0).x;
	}
	return DistanceFieldValue;
}
float3 GlobalDistanceFieldPageCentralDiff(float3 ClipmapVolumeUV, uint ClipmapIndex)
{
	float3 TexelOffset = 0.5f * View_GlobalVolumeTexelSize.x;
	float R = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(+TexelOffset.x, 0, 0), ClipmapIndex);
	float L = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(-TexelOffset.x, 0, 0), ClipmapIndex);
	float F = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(0, +TexelOffset.y, 0), ClipmapIndex);
	float B = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(0, -TexelOffset.y, 0), ClipmapIndex);
	float U = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(0, 0, +TexelOffset.z), ClipmapIndex);
	float D = GlobalDistanceFieldSampleClipmap(ClipmapVolumeUV + float3(0, 0, -TexelOffset.z), ClipmapIndex);
	return float3(R - L, F - B, U - D);
}
float3 ComputeGlobalDistanceFieldNormal(float3 SampleTranslatedWorldPosition, uint ClipmapIndex, float3 FallbackNormal)
{
	float3 ClipmapVolumeUV = ComputeGlobalUV(SampleTranslatedWorldPosition, ClipmapIndex);
	float3 DistanceFieldGradient = GlobalDistanceFieldPageCentralDiff(ClipmapVolumeUV, ClipmapIndex);
	float DistanceFieldGradientLength = length(DistanceFieldGradient);
	float3 DistanceFieldNormal = DistanceFieldGradientLength > 0.001f ? DistanceFieldGradient / DistanceFieldGradientLength : FallbackNormal;
	return DistanceFieldNormal;
}
struct FLumenCardSample
{
	uint CardIndex;
	uint CardPageIndex;
	float2 PhysicalAtlasUV;
	float4 TexelBilinearWeights;
	float2 IndirectLightingPhysicalAtlasUV;
	uint2 PackedFeedback;
	bool bValid;
};
struct FCardSampleAccumulator
{
	FLumenCardSample CardSample;
	bool bHeightfield;
	bool bValidMeshCardsIndex;
	float MaxSampleWeight;
	float OpacitySum;
	float3 DirectLightingSum;
	float3 IndirectLightingSum;
	float3 FinalLightingSum;
	float SampleWeightSum;
};
void InitCardSampleAccumulator(inout FCardSampleAccumulator CardSampleAccumulator)
{
	CardSampleAccumulator.MaxSampleWeight = 0.0f;
	CardSampleAccumulator.OpacitySum = 0.0f;
	CardSampleAccumulator.DirectLightingSum = 0.0f;
	CardSampleAccumulator.IndirectLightingSum = 0.0f;
	CardSampleAccumulator.FinalLightingSum = 0.0f;
	CardSampleAccumulator.SampleWeightSum = 0.0f;
	CardSampleAccumulator.CardSample = (FLumenCardSample) 0;
	CardSampleAccumulator.CardSample.bValid = false;
	CardSampleAccumulator.bHeightfield = false;
	CardSampleAccumulator.bValidMeshCardsIndex = false;
}
uint ZOrder3DEncode(uint3 Coord, const uint SizeLog2)
{
    uint Index = 0;
    [unroll]
    for (uint i = 0; i < SizeLog2; i++)
    {
        Index |= ((Coord.x >> i) & 0x1) << (3 * i + 0);
        Index |= ((Coord.y >> i) & 0x1) << (3 * i + 1);
        Index |= ((Coord.z >> i) & 0x1) << (3 * i + 2);
    }
    return Index;
}
struct FObjectGridCellIndex
{
	uint GPUSceneInstanceIndex;
	bool bValid;
};
FObjectGridCellIndex UnpackObjectGridCellIndex(uint PackedIndex)
{
	FObjectGridCellIndex ObjectGridCellIndex;
	ObjectGridCellIndex.bValid = PackedIndex < 0xFFFFFFFF;
	ObjectGridCellIndex.GPUSceneInstanceIndex = PackedIndex & 0xFFFFFF;
	return ObjectGridCellIndex;
}
uint GetMeshCardsIndexFromSceneInstanceIndex(uint SceneInstanceIndex)
{
	const uint MeshCardsIndex = LumenCardScene_SceneInstanceIndexToMeshCardsIndexBuffer.Load(4 * SceneInstanceIndex);
	return MeshCardsIndex;
}
struct FLumenMeshCardsData
{
	float3 WorldOrigin;
	float3x3 WorldToLocalRotation;
	uint NumCards;
	uint CardOffset;
	bool bHeightfield;
	bool bMostlyTwoSided;
	uint CardLookup[6];
};
FLumenMeshCardsData GetLumenMeshCardsData(uint MeshCardsId)
{
	uint BaseOffset = MeshCardsId * 6;
	FLumenMeshCardsData MeshCardsData;
	float4 V0 = LumenCardScene_MeshCardsData[BaseOffset + 0];
	float4 V1 = LumenCardScene_MeshCardsData[BaseOffset + 1];
	float4 V2 = LumenCardScene_MeshCardsData[BaseOffset + 2];
	float4 V3 = LumenCardScene_MeshCardsData[BaseOffset + 3];
	float3 PositionHigh = V0.xyz;
	float3 PositionLow = float3(V1.w, V2.w, V3.w);
	MeshCardsData.WorldToLocalRotation[0] = V1.xyz;
	MeshCardsData.WorldToLocalRotation[1] = V2.xyz;
	MeshCardsData.WorldToLocalRotation[2] = V3.xyz;
	MeshCardsData.WorldOrigin = PositionHigh + PositionLow; 
	uint4 V4 = asuint(LumenCardScene_MeshCardsData[BaseOffset + 4]);
	uint4 V5 = asuint(LumenCardScene_MeshCardsData[BaseOffset + 5]);
	MeshCardsData.CardOffset = V4.x;
	MeshCardsData.NumCards = V4.y & 0xFFFF;
	MeshCardsData.bHeightfield = V4.y & 0x10000 ? true : false;
	MeshCardsData.bMostlyTwoSided = V4.y & 0x20000 ? true : false;
	MeshCardsData.CardLookup[0] = V4.z;
	MeshCardsData.CardLookup[1] = V4.w;
	MeshCardsData.CardLookup[2] = V5.x;
	MeshCardsData.CardLookup[3] = V5.y;
	MeshCardsData.CardLookup[4] = V5.z;
	MeshCardsData.CardLookup[5] = V5.w;
	return MeshCardsData;
}
float2 SamplePositonToCardUV(FLumenCardData Card, float2 LocalSamplePosition)
{
	float2 CardUV = saturate(float2(+0.5f, -0.5f) * (LocalSamplePosition / Card.LocalExtent.xy) + 0.5f);
	return CardUV;
}
int2 select_internal(bool2   c, int2 a, int b) { return int2(c.x ? a.x : b  , c.y ? a.y : b  ); }
int2 select_internal(bool2   c, int a, int2 b) { return int2(c.x ? a   : b.x, c.y ? a   : b.y); }
float2 select_internal(bool2   c, float a, float b) { return float2(c.x ? a   : b  , c.y ? a   : b  ); }
uint2 ResLevelXYToSizeInPages(uint2 ResLevelXY)
{
	return  select_internal( ResLevelXY > 7 , 1u << (ResLevelXY - 7) , 1 );
}
FLumenCardSample ComputeSurfaceCacheSample(FLumenCardData Card, uint CardIndex, float2 LocalSamplePosition, float SampleRadius, bool bHiResSurface)
{
	float2 CardUV = min(SamplePositonToCardUV(Card, LocalSamplePosition), 0.999999f);
	uint2 SizeInPages = Card.SizeInPages;
	uint PageTableOffset = Card.PageTableOffset;
	if (bHiResSurface)
	{
		SizeInPages = Card.HiResSizeInPages;
		PageTableOffset = Card.HiResPageTableOffset;
	}
	uint2 PageCoord = CardUV * SizeInPages;
	uint LinearPageCoord = PageCoord.x + PageCoord.y * SizeInPages.x;
	const uint PageTableIndex = PageTableOffset + LinearPageCoord;
	const uint2 PageTableValue = LumenCardScene_PageTableBuffer.Load2(8 * PageTableIndex);
	uint2 AtlasBias;
	AtlasBias.x = ((PageTableValue.x >> 0) & 0xFFF) * 8;
	AtlasBias.y = ((PageTableValue.x >> 12) & 0xFFF) * 8;
	uint2 ResLevelXY;
	ResLevelXY.x = (PageTableValue.x >> 24) & 0xF;
	ResLevelXY.y = (PageTableValue.x >> 28) & 0xF;
	const uint CardPageIndex = PageTableValue.y;
	SizeInPages = ResLevelXYToSizeInPages(ResLevelXY);
	PageCoord = CardUV * SizeInPages;
	uint2 AtlasScale =  select_internal( ResLevelXY > 7 , 128 , (1u << ResLevelXY) );
	float2 PageUV = frac(CardUV * SizeInPages);
	float2 MinUVBorder =  select_internal( PageCoord.xy == 0 , 0.0f , 0.5f );
	float2 MaxUVBorder =  select_internal( PageCoord.xy + 1 == SizeInPages.xy , 0.0f , 0.5f );
	float2 CoordInPage = (PageUV * (AtlasScale - MinUVBorder - MaxUVBorder)) + MinUVBorder;
	CoordInPage = clamp(CoordInPage, 0.5f, AtlasScale - 1.0f - 0.5f);
	float2 PhysicalAtlasUV = (CoordInPage + AtlasBias) * LumenCardScene_InvPhysicalAtlasSize;
	float2 IndirectLightingPhysicalAtlasUV = PhysicalAtlasUV;
	uint2 PackedFeedback = 0;
	float2 FracUV = frac(PhysicalAtlasUV * LumenCardScene_PhysicalAtlasSize + 0.5f + 1.0f / 512.0f);
	float4 TexelBilinearWeights;
	TexelBilinearWeights.x = (1.0 - FracUV.x) * (FracUV.y);
	TexelBilinearWeights.y = (FracUV.x) * (FracUV.y);
	TexelBilinearWeights.z = (FracUV.x) * (1 - FracUV.y);
	TexelBilinearWeights.w = (1 - FracUV.x) * (1 - FracUV.y);
	FLumenCardSample CardSample;
	CardSample.CardIndex = CardIndex;
	CardSample.CardPageIndex = CardPageIndex;
	CardSample.PhysicalAtlasUV = PhysicalAtlasUV;
	CardSample.TexelBilinearWeights = TexelBilinearWeights;
	CardSample.IndirectLightingPhysicalAtlasUV = IndirectLightingPhysicalAtlasUV;
	CardSample.bValid = ResLevelXY.x > 0;
	CardSample.PackedFeedback = PackedFeedback;
	return CardSample;
}
float3 SampleSurfaceCacheAtlas(Texture2D AtlasTexture, float2 AtlasUV, float4 TexelWeights)
{
	float4 SampleX4 = AtlasTexture.GatherRed( gsamPointClamp, AtlasUV);
	float4 SampleY4 = AtlasTexture.GatherGreen( gsamPointClamp, AtlasUV);
	float4 SampleZ4 = AtlasTexture.GatherBlue( gsamPointClamp, AtlasUV);
	float3 Sample;
	Sample.x = dot(SampleX4, TexelWeights);
	Sample.y = dot(SampleY4, TexelWeights);
	Sample.z = dot(SampleZ4, TexelWeights);
	return Sample;
}
void SampleLumenCard(
	float3 MeshCardsSpacePosition,
	float3 MeshCardsSpaceNormal,
	float SampleRadius,
	float SurfaceCacheBias,
	uint CardIndex,
	float3 AxisWeights,
	bool bHiResSurface,
	bool bHeightfield,
	inout FCardSampleAccumulator CardSampleAccumulator)
{
	if (CardIndex < LumenCardScene_NumCards)
	{
		FLumenCardData LumenCardData = GetLumenCardData(CardIndex);
		if (LumenCardData.bVisible)
		{
			float3 CardSpacePosition = mul(MeshCardsSpacePosition - LumenCardData.MeshCardsOrigin, LumenCardData.MeshCardsToLocalRotation);
			if (all(abs(CardSpacePosition) <= LumenCardData.LocalExtent + 0.5f * SurfaceCacheBias))
			{
				CardSpacePosition.xy = clamp(CardSpacePosition.xy, -LumenCardData.LocalExtent.xy, LumenCardData.LocalExtent.xy);
				FLumenCardSample CardSample = ComputeSurfaceCacheSample(LumenCardData, CardIndex, CardSpacePosition.xy, SampleRadius, bHiResSurface);
				if (CardSample.bValid)
				{
					float NormalWeight = 1.0f;
					if (!bHeightfield)
					{
						if (LumenCardData.AxisAlignedDirection < 2)
						{
							NormalWeight = AxisWeights.x;
						}
						else if (LumenCardData.AxisAlignedDirection < 4)
						{
							NormalWeight = AxisWeights.y;
						}
						else
						{
							NormalWeight = AxisWeights.z;
						}
					}
					if (NormalWeight > 0.0f)
					{
						float4 TexelDepths = LumenCardScene_DepthAtlas.Gather( gsamPointClamp, CardSample.PhysicalAtlasUV, 0.0f);
						float NormalizedHitDistance = -(CardSpacePosition.z / LumenCardData.LocalExtent.z) * 0.5f + 0.5f;
						float BiasTreshold = SurfaceCacheBias / LumenCardData.LocalExtent.z;
						float BiasFalloff = 0.25f * BiasTreshold;
						float4 TexelVisibility = 0.0f;
						for (uint TexelIndex = 0; TexelIndex < 4; ++TexelIndex)
						{
							if (IsSurfaceCacheDepthValid(TexelDepths[TexelIndex]))
							{
								if (bHeightfield)
								{
									TexelVisibility[TexelIndex] = 1.0f;
								}
								else
								{
									TexelVisibility[TexelIndex] = 1.0f - saturate((abs(NormalizedHitDistance - TexelDepths[TexelIndex]) - BiasTreshold) / BiasFalloff);
								}
							}
						}
						float4 TexelWeights = CardSample.TexelBilinearWeights * TexelVisibility;
						float CardSampleWeight = NormalWeight * dot(TexelWeights, 1.0f);
						if (CardSampleWeight > 0.0f)
						{
							float TexelWeightSum = dot(TexelWeights, 1.0f);
							TexelWeights /= TexelWeightSum;
							float Opacity = 1.0f;//SampleSurfaceCacheAtlas(OpacityAtlas, CardSample.PhysicalAtlasUV, TexelWeights).x;
							float3 DirectLighting =  float3(0.0f,0.0f,0.0f);//SampleSurfaceCacheAtlas(DirectLightingAtlas, CardSample.PhysicalAtlasUV, TexelWeights);
							float3 IndirectLighting = float3(0.0f,0.0f,0.0f);//SampleSurfaceCacheAtlas(IndirectLightingAtlas, CardSample.IndirectLightingPhysicalAtlasUV, TexelWeights);
							float3 FinalLighting = SampleSurfaceCacheAtlas(FinalLightingAtlas, CardSample.PhysicalAtlasUV, TexelWeights);
							CardSampleAccumulator.OpacitySum += Opacity * CardSampleWeight;
							CardSampleAccumulator.DirectLightingSum += DirectLighting * CardSampleWeight;
							CardSampleAccumulator.IndirectLightingSum += IndirectLighting * CardSampleWeight;
							CardSampleAccumulator.FinalLightingSum += FinalLighting * CardSampleWeight;
							CardSampleAccumulator.SampleWeightSum += CardSampleWeight;
							if (CardSampleWeight > CardSampleAccumulator.MaxSampleWeight)
							{
								CardSampleAccumulator.CardSample = CardSample;
								CardSampleAccumulator.MaxSampleWeight = CardSampleWeight;
							}
						}
					}
				}
			}
		}
	}
}
void SampleLumenMeshCards(
	uint MeshCardsIndex, 
	float3 WorldSpacePosition, 
	float3 WorldSpaceNormal, 
	float SampleRadius,		
	float SurfaceCacheBias,	
	bool bHiResSurface,		
	inout FCardSampleAccumulator CardSampleAccumulator
	)
{
	if (MeshCardsIndex < LumenCardScene_NumMeshCards)
	{
		FLumenMeshCardsData MeshCardsData = GetLumenMeshCardsData(MeshCardsIndex);
		if (MeshCardsData.bMostlyTwoSided)
		{
			SurfaceCacheBias += 50.0f;
		}
		float3 MeshCardsSpacePosition = mul(WorldSpacePosition - MeshCardsData.WorldOrigin, MeshCardsData.WorldToLocalRotation);
		float3 MeshCardsSpaceNormal = mul(WorldSpaceNormal, MeshCardsData.WorldToLocalRotation);
		uint CardMask = 0;
		float3 AxisWeights = MeshCardsSpaceNormal * MeshCardsSpaceNormal;
		if (AxisWeights.x > 0.0f)
		{
			CardMask |= MeshCardsData.CardLookup[MeshCardsSpaceNormal.x < 0.0f ? 0 : 1];
		}
		if (AxisWeights.y > 0.0f)
		{
			CardMask |= MeshCardsData.CardLookup[MeshCardsSpaceNormal.y < 0.0f ? 2 : 3];
		}
		if (AxisWeights.z > 0.0f)
		{
			CardMask |= MeshCardsData.CardLookup[MeshCardsSpaceNormal.z < 0.0f ? 4 : 5];
		}
		{
			uint CulledCardMask = 0;
			while (CardMask != 0)
			{
				const uint NextBitIndex = firstbitlow(CardMask);
				const uint NextBitMask = 1u << NextBitIndex;
				CardMask ^= NextBitMask;
				uint CardIndex = MeshCardsData.CardOffset + NextBitIndex;
				FLumenCardData LumenCardData = GetLumenCardData(CardIndex);
				if (all(abs(MeshCardsSpacePosition - LumenCardData.MeshCardsOrigin) <= LumenCardData.MeshCardsExtent + 0.5f * SurfaceCacheBias))
				{
					CulledCardMask |= NextBitMask;
				}
			}
			CardMask = CulledCardMask;
		}
		if (MeshCardsData.bHeightfield)
		{
			CardMask = (1 << 0);
		}
		while (CardMask != 0)
		{
			const uint NextBitIndex = firstbitlow(CardMask);
			CardMask ^= 1u << NextBitIndex;
			uint CardIndex = MeshCardsData.CardOffset + NextBitIndex;
			FLumenCardData LumenCardData = GetLumenCardData(CardIndex);
			if (LumenCardData.bVisible)
			{
				SampleLumenCard(
					MeshCardsSpacePosition,
					MeshCardsSpaceNormal,
					SampleRadius,
					SurfaceCacheBias,
					CardIndex,
					AxisWeights,
					bHiResSurface,
					MeshCardsData.bHeightfield,
					CardSampleAccumulator);
			}
		}
		CardSampleAccumulator.bHeightfield = CardSampleAccumulator.bHeightfield || MeshCardsData.bHeightfield;
		CardSampleAccumulator.bValidMeshCardsIndex = true;
	}
}
struct FSurfaceCacheSample
{
	float3 Radiance;
	float3 DirectLighting;
	float3 IndirectLighting;
	float Opacity;
	bool bValid;
	bool bHeightfield;
};
FSurfaceCacheSample InitSurfaceCacheSample()
{
	FSurfaceCacheSample SurfaceCacheSample;
	SurfaceCacheSample.Radiance = 0.0f;
	SurfaceCacheSample.DirectLighting = 0.0f;
	SurfaceCacheSample.IndirectLighting = 0.0f;
	SurfaceCacheSample.Opacity = 0.0f;
	SurfaceCacheSample.bValid = false;
	SurfaceCacheSample.bHeightfield = false;
	return SurfaceCacheSample;
}
FSurfaceCacheSample EvaluateRayHitFromCardSampleAccumulator(
	uint2 DitherScreenCoord,
	float3 HitWorldPosition,
	float3 HitWorldNormal,
	FCardSampleAccumulator CardSampleAccumulator)
{
	FSurfaceCacheSample SurfaceCacheSample = InitSurfaceCacheSample();
	SurfaceCacheSample.bHeightfield = CardSampleAccumulator.bHeightfield;
	if (CardSampleAccumulator.SampleWeightSum > 0.0f)
	{
		SurfaceCacheSample.Opacity = CardSampleAccumulator.OpacitySum / CardSampleAccumulator.SampleWeightSum;
		SurfaceCacheSample.bValid = true;
		SurfaceCacheSample.Radiance = CardSampleAccumulator.FinalLightingSum / CardSampleAccumulator.SampleWeightSum;
		SurfaceCacheSample.DirectLighting = CardSampleAccumulator.DirectLightingSum / CardSampleAccumulator.SampleWeightSum;
		SurfaceCacheSample.IndirectLighting = CardSampleAccumulator.IndirectLightingSum / CardSampleAccumulator.SampleWeightSum;
	}
	return SurfaceCacheSample;
}
void EvaluateGlobalDistanceFieldHit(FConeTraceInput TraceInput, FGlobalSDFTraceResult SDFTraceResult, inout FConeTraceResult ConeTraceResult)
{
	const float3 SampleWorldPosition = TraceInput.ConeOrigin + TraceInput.ConeDirection * SDFTraceResult.HitTime;
	const float3 SampleTranslatedWorldPosition = TraceInput.ConeTranslatedOrigin + TraceInput.ConeDirection * SDFTraceResult.HitTime;
	const float3 SampleWorldNormal = ComputeGlobalDistanceFieldNormal(SampleTranslatedWorldPosition, SDFTraceResult.HitClipmapIndex, -TraceInput.ConeDirection);
	const float ClipmapVoxelExtent = View_GlobalVolumeTranslatedCenterAndExtent[SDFTraceResult.HitClipmapIndex].w * View_GlobalVolumeTexelSize.x;
	float3 GridSampleTranslatedWorldPosition = SampleTranslatedWorldPosition + SampleWorldNormal * ClipmapVoxelExtent;
	float3 CardSampleWorldPosition = SampleWorldPosition + TraceInput.ConeDirection * 0.5f * SDFTraceResult.ExpandSurfaceAmount;
	float3 Radiance = 0.0f;
	float RadianceFactor = 1.0f;
	if (TraceInput.bExpandSurfaceUsingRayTimeInsteadOfMaxDistance)
	{
		RadianceFactor = smoothstep(1.5f * ClipmapVoxelExtent, 2.0f * ClipmapVoxelExtent, SDFTraceResult.HitTime);
	}
	if (TraceInput.bZeroRadianceIfRayStartsInsideGeometry && SDFTraceResult.HitTime <= TraceInput.MinTraceDistance)
	{
		RadianceFactor = 0.0f;
	}
	float3 ClipmapVolumeUV = ComputeGlobalUV(GridSampleTranslatedWorldPosition, SDFTraceResult.HitClipmapIndex);
	FGlobalDistanceFieldPage Page = GetGlobalDistanceFieldPage(ClipmapVolumeUV, SDFTraceResult.HitClipmapIndex);
	if (RadianceFactor > 0.0f && Page.bValid)
	{
		FCardSampleAccumulator CardSampleAccumulator;
		InitCardSampleAccumulator(CardSampleAccumulator);
		float3 PageTableCoord = saturate(ClipmapVolumeUV) * View_GlobalDistanceFieldClipmapSizeInPages.x;
		uint3 CellCoordInPage = frac(frac(PageTableCoord)) * 4;
		uint CellOffsetInPage = ZOrder3DEncode(CellCoordInPage, log2(4));
		uint4 DistanceFieldObjectGridCell = GlobalDistanceFieldPageObjectGridBuffer[(4 * 4 * 4) * Page.PageIndex + CellOffsetInPage];
		for (uint ObjectIndexInList = 0; ObjectIndexInList < 4; ++ObjectIndexInList)
		{
			FObjectGridCellIndex GridCellIndex = UnpackObjectGridCellIndex(DistanceFieldObjectGridCell[ObjectIndexInList]);
			if (GridCellIndex.bValid)
			{
				uint MeshCardsIndex = GetMeshCardsIndexFromSceneInstanceIndex(GridCellIndex.GPUSceneInstanceIndex);
				if (MeshCardsIndex < LumenCardScene_NumMeshCards)
				{
					float SurfaceCacheBias = 3.0f * ClipmapVoxelExtent;
					float SampleRadius = TraceInput.ConeStartRadius + TraceInput.TanConeAngle * SDFTraceResult.HitTime;
					SampleLumenMeshCards(
						MeshCardsIndex,
						SampleWorldPosition,
						SampleWorldNormal,
						SampleRadius,
						SurfaceCacheBias,
						TraceInput.bHiResSurface,
						CardSampleAccumulator
					);
					if (CardSampleAccumulator.SampleWeightSum >= 0.9f)
					{
						break;	
					}
				}
			}
			else
			{
				break;
			}
		}
		FSurfaceCacheSample SurfaceCacheSample = EvaluateRayHitFromCardSampleAccumulator(
				TraceInput.DitherScreenCoord,
				SampleWorldPosition,
				SampleWorldNormal,
				CardSampleAccumulator
			);
		Radiance = RadianceFactor * SurfaceCacheSample.Radiance;
	}
	ConeTraceResult.Lighting = Radiance;
	ConeTraceResult.Transparency = 0.0f;
	ConeTraceResult.OpaqueHitDistance = SDFTraceResult.HitTime;
	ConeTraceResult.GeometryWorldNormal = SampleWorldNormal;
}
void RayTraceGlobalDistanceField(
	FConeTraceInput TraceInput,
	inout FConeTraceResult OutResult)
{
	FGlobalSDFTraceResult SDFTraceResult;
	{
		FGlobalSDFTraceInput SDFTraceInput = SetupGlobalSDFTraceInput(TraceInput.ConeTranslatedOrigin, TraceInput.ConeDirection, TraceInput.MinTraceDistance, TraceInput.MaxTraceDistance, TraceInput.SDFStepFactor, TraceInput.MinSDFStepFactor);
		SDFTraceInput.bDitheredTransparency = TraceInput.bDitheredTransparency;
		SDFTraceInput.DitherScreenCoord = TraceInput.DitherScreenCoord;
		SDFTraceInput.bExpandSurfaceUsingRayTimeInsteadOfMaxDistance = TraceInput.bExpandSurfaceUsingRayTimeInsteadOfMaxDistance;
		SDFTraceInput.InitialMaxDistance = TraceInput.InitialMaxDistance;
		SDFTraceResult = RayTraceGlobalDistanceField(SDFTraceInput);
	}
	OutResult = (FConeTraceResult)0;
	OutResult.Lighting = float3(0.0f, 0.0f, 0.0f);
	OutResult.Transparency = 1.0f;
	OutResult.NumSteps = SDFTraceResult.TotalStepsTaken;
	OutResult.OpaqueHitDistance = TraceInput.MaxTraceDistance;
	OutResult.ExpandSurfaceAmount = SDFTraceResult.ExpandSurfaceAmount;
	if (GlobalSDFTraceResultIsHit(SDFTraceResult))
	{
		EvaluateGlobalDistanceFieldHit(TraceInput, SDFTraceResult, OutResult);
	}
}
uint2 GetProbeJitter(uint IndirectLightingTemporalIndex)
{
	uint TemporalIndex = (FixedJitterIndex < 0 ? IndirectLightingTemporalIndex : FixedJitterIndex);
	return Hammersley16(TemporalIndex % MaxFramesAccumulated, MaxFramesAccumulated, 0) * ProbeSpacingInRadiosityTexels;
}
//rect -> tile 8x8 -> 4x4 sub tile -> probe
[numthreads(8,8,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~1119,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,//(0~7,0~7,0)
    uint3 inDispatchThreadId : SV_DispatchThreadID){

	uint2 subtileCoord=inGroupThreadId.xy >> 2;//(0,0),(1,0),(0,1),(1,1)
	uint CardTileIndex = inGroupId.x;
	FCardTileData CardTile = GetCardTile(CardTileIndex);
	FLumenCardPageData CardPage = GetLumenCardPageData(CardTile.CardPageIndex);
	uint2 CoordInCardTile=subtileCoord*4 + GetProbeJitter(CardPage.IndirectLightingTemporalIndex);//(0,0),(4,0),(0,4),(4,4)
	uint2 TraceTexelCoord=uint2(inGroupThreadId.x%4,inGroupThreadId.y%4);//(0~3,0~3)

	uint tileInfoPackedTileData = CardTileData[CardTileIndex];
	uint rectIndex=tileInfoPackedTileData & 0xFFFFFF;
	uint4 rect=RectCoordBuffer[rectIndex];

	uint tileCoordXInAtlas=((tileInfoPackedTileData >> 24) & 0xF)*8 + rect.x;//abs coord in atlas
	uint tileCoordYInAtlas=((tileInfoPackedTileData >> 28) & 0xF)*8 + rect.y;
	uint2 atlasCoord = uint2(tileCoordXInAtlas + inGroupThreadId.x ,tileCoordYInAtlas + inGroupThreadId.y );

	FRadiosityTexel RadiosityTexel = GetRadiosityTexelFromCardTile(CardTileIndex, CoordInCardTile);
	if (RadiosityTexel.bInsideAtlas)
	{
		float3 Radiance = IntToColor(CardTileIndex+1);
		float TraceHitDistance = MaxTraceDistance;
		if (RadiosityTexel.bValid)
		{
			float3 WorldPosition = RadiosityTexel.WorldPosition;
			float3 WorldNormal = RadiosityTexel.WorldNormal;
			float3 WorldRayDirection;
			float ConeHalfAngle;
			float PDF;
			GetRadiosityRay(RadiosityTexel, RadiosityTexel.CardCoord >> ProbeSpacingInRadiosityTexelsDivideShift, TraceTexelCoord, WorldRayDirection, ConeHalfAngle, PDF);
			WorldPosition += WorldNormal * SurfaceBias;

			float VoxelTraceStartDistance = CalculateVoxelTraceStartDistance(MinTraceDistance, MaxTraceDistance, MaxMeshSDFTraceDistance, false);
			float3 SamplePosition = WorldPosition + SurfaceBias * WorldRayDirection;
			float3 TranslatedSamplePosition = SamplePosition - (mCameraPositionHighWS.xyz+mCameraPositionLowWS.xyz);

			FConeTraceInput TraceInput;
			TraceInput.Setup(SamplePosition, TranslatedSamplePosition, WorldRayDirection, ConeHalfAngle, 0, MinTraceDistance, MaxTraceDistance, 1);
			TraceInput.VoxelTraceStartDistance = VoxelTraceStartDistance;
			TraceInput.SDFStepFactor = 1;
			TraceInput.DitherScreenCoord = (RadiosityTexel.CardCoord >> ProbeSpacingInRadiosityTexelsDivideShift) + TraceTexelCoord;
			FConeTraceResult TraceResult = (FConeTraceResult)0;
			TraceResult.Transparency = 1;
			{
				RayTraceGlobalDistanceField(TraceInput, TraceResult);
			}
			if (TraceResult.Transparency < 0.5f)
			{
				Radiance = TraceResult.Lighting;
			}
			else
			{
				Radiance = float3(0.0f,0.0f,0.0f);
			}
			float MaxLighting = max3(Radiance.x, Radiance.y, Radiance.z);
			if (MaxLighting > MaxRayIntensity * View_OneOverPreExposure)
			{
				Radiance *= MaxRayIntensity * View_OneOverPreExposure / MaxLighting;
			}
		}
		RWTraceRadianceAtlas[atlasCoord] = Radiance;
	}
}