#include "../GlobalConstant.hlsli"

const static float4 View_InvDeviceZToWorldZTransform=float4(0.0f,0.0f, 0.1f, -1.00000E-08f);
//instance => sdf 
Buffer<uint4> RectCoordBuffer:register(t0);
StructuredBuffer<uint> TilesInfo:register(t1);
Texture2D LumenCardScene_NormalAtlas:register(t2);
Texture2D LumenCardScene_DepthAtlas:register(t3);
StructuredBuffer<float4>  LumenCardScene_CardData:register(t4);
Texture3D  View_GlobalDistanceFieldPageAtlasTexture:register(t5);
Texture3D  View_GlobalDistanceFieldCoverageAtlasTexture:register(t6);
Texture3D<uint>  View_GlobalDistanceFieldPageTableTexture:register(t7);
Texture3D  View_GlobalDistanceFieldMipTexture:register(t8);
Texture2D LumenCardScene_AlbedoAtlas:register(t9);
Texture2D LumenCardScene_EmissiveAtlas:register(t10);

RWTexture2D<float4> LumenSceneDirectLighting:register(u0);
RWTexture2D<float4> LumenSceneFinalLighting:register(u1);

float length2(float2 v)
{
	return dot(v, v);
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
    float Depth = Texture2DSampleLevel(LumenCardScene_DepthAtlas, gsamPointClamp, AtlasUV, 0).x;
	FLumenSurfaceCacheData SurfaceCacheData;
	SurfaceCacheData.Depth = Depth;
	SurfaceCacheData.bValid = IsSurfaceCacheDepthValid(Depth);
	SurfaceCacheData.Albedo = float3(0.0f, 0.0f, 0.0f);
	SurfaceCacheData.Emissive = float3(0.0f, 0.0f, 0.0f);
	float2 NormalXY = float2(0.5f, 0.5f);
	if (SurfaceCacheData.bValid)
	{
        NormalXY = Texture2DSampleLevel(LumenCardScene_NormalAtlas, gsamPointClamp, AtlasUV, 0).xy;
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
float3 select_internal(bool3   c, float a, float3 b) { return float3(c.x ? a   : b.x, c.y ? a   : b.y, c.z ? a   : b.z); }
bool3 IsFinite( float3 In) {	return (asuint(In) & 0x7F800000) != 0x7F800000; }
float3 MakeFinite( float3 In) {    return  select_internal( !IsFinite(In) , 0.0 , In ); }

float3 CombineFinalLighting(float3 Albedo, float3 Emissive, float3 DirectLighting, float3 IndirectLighting)
{
	Albedo = DecodeSurfaceCacheAlbedo(Albedo);
	float3 DiffuseLambert = Albedo * (1 / PI);
	float3 FinalLighting = (DirectLighting + IndirectLighting) * DiffuseLambert + Emissive;
	FinalLighting = max(MakeFinite(FinalLighting), float3(0.0f, 0.0f, 0.0f));
	return FinalLighting;
}
//tile 8x8
//vs->ps,viewport -> rt,0~1->rt
[numthreads(8,8,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~1119,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,
    uint3 inDispatchThreadId : SV_DispatchThreadID){
	uint tileIndex=inGroupId.x;
	uint tileInfoPackedTileData = TilesInfo[tileIndex];
	uint rectIndex=tileInfoPackedTileData & 0xFFFFFF;
	uint4 rect=RectCoordBuffer[rectIndex];
	uint tileCoordXInAtlas=((tileInfoPackedTileData >> 24) & 0xF)*8 + rect.x;//abs coord in atlas
	uint tileCoordYInAtlas=((tileInfoPackedTileData >> 28) & 0xF)*8 + rect.y;
	float width=float(rect.z-rect.x);
	float height=float(rect.w-rect.y);
	
	FLumenCardData Card = GetLumenCardData(rectIndex);
	uint2 atlasCoord = uint2(tileCoordXInAtlas + inGroupThreadId.x ,tileCoordYInAtlas + inGroupThreadId.y );
	uint2 texelInCardPageCoord = atlasCoord - rect.xy;//uint2(x-rect.x,y-rect.y);
	
	float2 texcoord=(float2(texelInCardPageCoord)+float2(0.5f,0.5f))/float2(width,height);//0~1
	float2 atlasUV=(float2(atlasCoord)+float2(0.5f,0.5f))/float2(4096.0f,4096.0f);

	FLumenSurfaceCacheData SurfaceCacheData = GetSurfaceCacheData(Card, texcoord, atlasUV);
	
	float3 WorldNormal = SurfaceCacheData.WorldNormal;
	float3 WorldPosition = SurfaceCacheData.WorldPosition;
	float3 TranslatedWorldPosition = WorldPosition - (mCameraPositionHighWS.xyz+mCameraPositionLowWS.xyz);
	float3 L = float3(-0.69466f, 0.0f, 0.71934f);
	float3 ToLight = L;
	float CombinedAttenuation = 1.0f;
	float Attenuation = 1.0f;
	float LightMask = 1.0f;
	CombinedAttenuation *= saturate(dot(WorldNormal, L));
	float3 color=float3(0.0f,0.0f,0.0f);
	float3 finalLighting=float3(0.0f,0.0f,0.0f);
	if(CombinedAttenuation>0.0f){//is lit(maybe)
		//sdf shadow, mesh sdf
		float shadowFactor = TraceOffscreenShadows(texelInCardPageCoord, TranslatedWorldPosition, L, WorldNormal);
		if(shadowFactor>0.0f){
			//pbr directional light
			//DeferredLightData.Color = PackedLight.Color * InverseExposureLerp(Exposure, PackedLight.InverseExposureBlend);
			float3 LightColor = float3(10.0f,10.0f,10.0f)*InverseExposureLerp(2.4f,0.0f);
			float3 irradiance = LightColor * CombinedAttenuation;
			color = QuantizeForFloatRenderTarget(irradiance, int3(atlasCoord, View_StateFrameIndexMod8.x + 1));
			//
			float3 Albedo = Texture2DSampleLevel(LumenCardScene_AlbedoAtlas, gsamLLPClamp, atlasUV, 0).xyz;
			float3 Emissive = Texture2DSampleLevel(LumenCardScene_EmissiveAtlas, gsamLLPClamp, atlasUV, 0).xyz;
			float3 IndirectLighting=float3(0.0f,0.0f,0.0f);
			finalLighting = CombineFinalLighting(Albedo, Emissive, color, IndirectLighting);
			finalLighting = QuantizeForFloatRenderTarget(finalLighting, int3(atlasCoord, View_StateFrameIndexMod8.x + 2));
		}else{
			//
		}
	}else{
		//is in shadow
	}
	LumenSceneDirectLighting[atlasCoord]=float4(color,1.0f);
	LumenSceneFinalLighting[atlasCoord]=float4(finalLighting,1.0f);
            
    /*if(all(inDispatchThreadId.xy<uint2(4096,4096))){//(0~11,0)
        FLumenCardData Card = GetLumenCardData(inDispatchThreadId.x);
        uint4 rect=RectCoordBuffer[inDispatchThreadId.x];
        for(uint y=rect.y;y<rect.w;y++){
            for(uint x=rect.x;x<rect.z;x++){
                //0~1
				uint2 atlasCoord=uint2(x,y);
                uint2 texelInCardPageCoord = uint2(x-rect.x,y-rect.y);
                float2 texcoord=(float2(x-rect.x,y-rect.y)+float2(0.5f,0.5f))/float2(width,height);//0~1
                float2 atlasUV=(float2(x,y)+float2(0.5f,0.5f))/float2(4096.0f,4096.0f);
                FLumenSurfaceCacheData SurfaceCacheData = GetSurfaceCacheData(Card, texcoord, atlasUV);
                
                float3 WorldNormal = SurfaceCacheData.WorldNormal;
                float3 WorldPosition = SurfaceCacheData.WorldPosition;
                float3 TranslatedWorldPosition = WorldPosition - (mCameraPositionHighWS.xyz+mCameraPositionLowWS.xyz);
                float3 L = float3(-0.69466f, 0.0f, 0.71934f);
                float3 ToLight = L;
                float CombinedAttenuation = 1.0f;
                float Attenuation = 1.0f;
                float LightMask = 1.0f;
                CombinedAttenuation *= saturate(dot(WorldNormal, L));
                float3 color=float3(0.0f,0.0f,0.0f);
				float3 finalLighting=float3(0.0f,0.0f,0.0f);
                if(CombinedAttenuation>0.0f){//is lit(maybe)
                    //sdf shadow, mesh sdf
                    float shadowFactor = TraceOffscreenShadows(texelInCardPageCoord, TranslatedWorldPosition, L, WorldNormal);
                    if(shadowFactor>0.0f){
						//pbr directional light
						//DeferredLightData.Color = PackedLight.Color * InverseExposureLerp(Exposure, PackedLight.InverseExposureBlend);
						float3 LightColor = float3(10.0f,10.0f,10.0f)*InverseExposureLerp(2.4f,0.0f);
						float3 irradiance = LightColor * CombinedAttenuation;
                        color = QuantizeForFloatRenderTarget(irradiance, int3(atlasCoord, View_StateFrameIndexMod8.x + 1));
						//
						float3 Albedo = Texture2DSampleLevel(LumenCardScene_AlbedoAtlas, MinMagLinearMipPointClampped, atlasUV, 0).xyz;
						float3 Emissive = Texture2DSampleLevel(LumenCardScene_EmissiveAtlas, MinMagLinearMipPointClampped, atlasUV, 0).xyz;
						float3 IndirectLighting=float3(0.0f,0.0f,0.0f);
						finalLighting = CombineFinalLighting(Albedo, Emissive, color, IndirectLighting);
						finalLighting = QuantizeForFloatRenderTarget(finalLighting, int3(atlasCoord, View_StateFrameIndexMod8.x + 2));
                    }else{
                        //
                    }
                }else{
                    //is in shadow
                }
                LumenSceneDirectLighting[atlasCoord]=float4(color,1.0f);
                LumenSceneFinalLighting[atlasCoord]=float4(finalLighting,1.0f);
            }
        }
    }*/
}