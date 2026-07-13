#include "../GlobalConstant.hlsli"

const static float PI = 3.1415926535897932f;

Texture2D SceneDepth:register(t0);
Texture2D GBufferA:register(t1);
Texture2D GBufferB:register(t2);

RWTexture2D<uint> RWScreenProbeSceneDepth:register(u0);
RWTexture2D<uint> RWScreenProbeWorldSpeed:register(u1);//jitter -> 0~7 -> 
RWTexture2D<unorm float2> RWScreenProbeWorldNormal:register(u2);
RWTexture2D<float4> RWScreenProbeTranslatedWorldPosition:register(u3);

SamplerState MinMagMipPointClampped:register(s0);
SamplerState MinMagMipLinearWrap:register(s1);
SamplerState MinMagMipLinearClampped:register(s2);
SamplerState MinMagLinearMipPointClampped:register(s3);
float2 select_internal(bool2   c, float2 a, float2 b) { return float2(c.x ? a.x : b.x, c.y ? a.y : b.y); }
float2 Hammersley16( uint Index, uint NumSamples, uint2 Random )
{
	float E1 = frac( (float)Index / NumSamples + float( Random.x ) * (1.0 / 65536.0) );
	float E2 = float( ( reversebits(Index) >> 16 ) ^ Random.y ) * (1.0 / 65536.0);
	return float2( E1, E2 );
}
float2 UnitVectorToOctahedron( float3 N )
{
	N.xy /= dot( 1, abs(N) );
	if( N.z <= 0 )
	{
		N.xy = ( 1 - abs(N.yx) ) *  select_internal( N.xy >= 0 , float2(1,1) , float2(-1,-1) );
	}
	return N.xy;
}
uint2 GetScreenTileJitter(uint TemporalIndex)
{
	return Hammersley16(TemporalIndex, 8, 0) * ScreenProbeDownsampleFactor;
}
uint2 GetUniformScreenProbeScreenPosition(uint2 ScreenTileCoord)
{
	uint2 ScreenJitter = GetScreenTileJitter((FixedJitterIndex < 0 ? View_StateFrameIndexMod8.x : FixedJitterIndex));
	uint2 ScreenProbeScreenPosition = min((uint2)(View_ViewRectMinAndSize.xy + ScreenTileCoord * ScreenProbeDownsampleFactor + ScreenJitter), (uint2)(View_ViewRectMinAndSize.xy + View_ViewRectMinAndSize.zw) - 1);
	return ScreenProbeScreenPosition;
}

struct FGBufferData
{
	float3 WorldNormal;
	float3 WorldTangent;
	float3 DiffuseColor;
	float3 SpecularColor;
	float3 BaseColor;
	float Metallic;
	float Specular;
	float4 CustomData;
	float GenericAO;
	float IndirectIrradiance;
	float4 PrecomputedShadowFactors;
	float Roughness;
	float Anisotropy;
	float GBufferAO;
	uint DiffuseIndirectSampleOcclusion;
	uint ShadingModelID;
	uint SelectiveOutputMask;
	float PerObjectGBufferData;
	float CustomDepth;
	uint CustomStencil;
	float Depth;
	float4 Velocity;
	float3 StoredBaseColor;
	float StoredSpecular;
	float StoredMetallic;
	float Curvature;
};
struct FLumenMaterialData
{
	float SceneDepth;
	float3 DiffuseAlbedo;
	float3 WorldNormal;
	float  Roughness;
	float  TopLayerRoughness;
	float  MaterialAO;
	uint   ShadingID;
	uint   DiffuseIndirectSampleOcclusion;
	bool   bNeedsSeparateLightAccumulation; 
	bool   bRequiresBxDFImportanceSampling;
	bool   bIsSLW;
	bool   bIsHair;
	bool   bHasBackfaceDiffuse;
	bool   bIsFrontLayerTranslucency;
	float  Anisotropy;
	FGBufferData GBufferData;
};
float ConvertFromDeviceZ(float DeviceZ)
{
	return DeviceZ * View_InvDeviceZToWorldZTransform[0] + View_InvDeviceZToWorldZTransform[1] + 1.0f / (DeviceZ * View_InvDeviceZToWorldZTransform[2] - View_InvDeviceZToWorldZTransform[3]);
}
float CalcSceneDepth(uint2 PixelPos)
{
	float DeviceZ = SceneDepth.Load(int3(PixelPos, 0)).r;//0.0~1.0
	return ConvertFromDeviceZ(DeviceZ);
}
float3 DecodeNormalHelper(float3 SrcNormal)
{
	return SrcNormal * 2.0f - 1.0f;
}
bool CheckerFromPixelPos(uint2 PixelPos)
{
	uint TemporalAASampleIndex = uint(View_TemporalAAParams.x);
	return (PixelPos.x + PixelPos.y + TemporalAASampleIndex) % 2;
}
bool HasCustomGBufferData(int ShadingModelID)
{
	return ShadingModelID == 2
		|| ShadingModelID == 3
		|| ShadingModelID == 4
		|| ShadingModelID == 5
		|| ShadingModelID == 6
		|| ShadingModelID == 7
		|| ShadingModelID == 8
		|| ShadingModelID == 9;
}

float DielectricSpecularToF0(float Specular)
{
	return float(0.08f * Specular);
}
float3 ComputeF0(float Specular, float3 BaseColor, float Metallic)
{
	return lerp(DielectricSpecularToF0(Specular).xxx, BaseColor, Metallic.xxx);
}
void GBufferPostDecode(inout FGBufferData Ret, bool bChecker, bool bGetNormalizedNormal)
{
	Ret.CustomData = HasCustomGBufferData(Ret.ShadingModelID) ? Ret.CustomData : float(0.0f);
	Ret.PrecomputedShadowFactors = !(Ret.SelectiveOutputMask & 0x2) ? Ret.PrecomputedShadowFactors : ((Ret.SelectiveOutputMask & 0x4) ? float(0.0f) : float(1.0f));
	Ret.Velocity = !(Ret.SelectiveOutputMask & 0x8) ? Ret.Velocity : float(0.0f);
	bool bHasAnisotropy = (Ret.SelectiveOutputMask & 0x1);
	Ret.StoredBaseColor = Ret.BaseColor;
	Ret.StoredMetallic = Ret.Metallic;
	Ret.StoredSpecular = Ret.Specular;
	Ret.GBufferAO = Ret.GenericAO;
	Ret.DiffuseIndirectSampleOcclusion = 0x0;
	Ret.IndirectIrradiance = 1;
	if(bGetNormalizedNormal)
	{
		Ret.WorldNormal = normalize(Ret.WorldNormal);
	}
	[flatten]
	if( Ret.ShadingModelID == 9 )
	{
		Ret.Metallic = 0.0;
	}
	{
		Ret.SpecularColor = ComputeF0(Ret.Specular, Ret.BaseColor, Ret.Metallic);
		Ret.DiffuseColor = Ret.BaseColor - Ret.BaseColor * Ret.Metallic;
		{
			Ret.DiffuseColor = Ret.DiffuseColor;
			Ret.SpecularColor = Ret.SpecularColor;
		}
	}
    Ret.WorldTangent = 0;
    Ret.Anisotropy = 0;
	Ret.SelectiveOutputMask = Ret.SelectiveOutputMask << 4;
}
FGBufferData  DecodeGBufferDataDirect(float4 InMRT1,
	float4 InMRT2,
	float4 InMRT3,
	float4 InMRT4,
	float CustomNativeDepth,
	float4 AnisotropicData,
	uint CustomStencil,
	float SceneDepth,
	bool bGetNormalizedNormal,
	bool bChecker)
{
	FGBufferData Ret = (FGBufferData)0;
	float3 WorldNormal_Compressed = 0.0f;
	WorldNormal_Compressed.x = InMRT1.x;
	WorldNormal_Compressed.y = InMRT1.y;
	WorldNormal_Compressed.z = InMRT1.z;
	Ret.PerObjectGBufferData.x = InMRT1.w;
	Ret.Metallic.x = InMRT2.x;
	Ret.Specular.x = InMRT2.y;
	Ret.Roughness.x = InMRT2.z;
	Ret.ShadingModelID.x = (((uint((float(InMRT2.w) * 255.0f) + .5f) >> 0) & 0x0f) << 0);
	Ret.SelectiveOutputMask.x = (((uint((float(InMRT2.w) * 255.0f) + .5f) >> 4) & 0x0f) << 0);
	Ret.BaseColor.x = InMRT3.x;
	Ret.BaseColor.y = InMRT3.y;
	Ret.BaseColor.z = InMRT3.z;
	Ret.GenericAO.x = InMRT3.w;
	Ret.CustomData.x = InMRT4.x;
	Ret.CustomData.y = InMRT4.y;
	Ret.CustomData.z = InMRT4.z;
	Ret.CustomData.w = InMRT4.w;
	Ret.WorldNormal = DecodeNormalHelper(WorldNormal_Compressed);
	Ret.WorldTangent = AnisotropicData.xyz;
	Ret.Anisotropy = AnisotropicData.w;
	GBufferPostDecode(Ret,bChecker,bGetNormalizedNormal);
	Ret.CustomDepth = ConvertFromDeviceZ(CustomNativeDepth);
	Ret.CustomStencil = CustomStencil;
	Ret.Depth = SceneDepth;
	return Ret;
}
FGBufferData DecodeGBufferDataUint(uint2 PixelPos, bool bGetNormalizedNormal = true)
{
	float CustomNativeDepth = 0.0f;
	uint CustomStencil = 0u;
	float SceneDepth = CalcSceneDepth(PixelPos);
	float4 AnisotropicData = float4(0.0f,0.0f,0.0f,0.0f);
	float4 InMRT1 = GBufferA.Load(int3(PixelPos, 0)).xyzw;
	float4 InMRT2 = GBufferB.Load(int3(PixelPos, 0)).xyzw;
	float4 InMRT3 = float4(0.0f,0.0f,0.0f,0.0f);
	float4 InMRT4 = float4(0.0f,0.0f,0.0f,0.0f);
	FGBufferData Ret = DecodeGBufferDataDirect(InMRT1,
		InMRT2,
		InMRT3,
		InMRT4,
		CustomNativeDepth,
		AnisotropicData,
		CustomStencil,
		SceneDepth,
		bGetNormalizedNormal,
		CheckerFromPixelPos(PixelPos));
	return Ret;
}
struct FScreenSpaceData
{
	FGBufferData GBuffer;
	float AmbientOcclusion;
};
FGBufferData GetGBufferDataUint(uint2 PixelPos, bool bGetNormalizedNormal = true)
{
    return DecodeGBufferDataUint(PixelPos,bGetNormalizedNormal);
}
float GetClearCoatRoughness(FGBufferData GBuffer)
{
	return GBuffer.ShadingModelID == 4 ? GBuffer.CustomData.y : GBuffer.Roughness;
}
bool UseSubsurfaceProfile(int ShadingModel)
{
	return ShadingModel == 5 || ShadingModel == 9;
}
bool RequiresBxDFImportanceSampling(uint ShadingModelID)
{
	switch (ShadingModelID)
	{
	case 7:
		return true;
	default:
		return false;
	}
}
bool IsValid(FLumenMaterialData In)
{
	return In.ShadingID != 0;
}
FLumenMaterialData InternalReadMaterialData_GBuffer(const FGBufferData GBufferData)
{
	FLumenMaterialData Out = (FLumenMaterialData)0;
	Out.SceneDepth = GBufferData.Depth;
	Out.WorldNormal = GBufferData.WorldNormal;
	Out.DiffuseAlbedo = GBufferData.BaseColor;
	Out.Roughness = GBufferData.Roughness;
	Out.Anisotropy = GBufferData.Anisotropy;
	Out.TopLayerRoughness = GetClearCoatRoughness(GBufferData);
	Out.MaterialAO = GBufferData.GBufferAO;
	Out.ShadingID = GBufferData.ShadingModelID;
	Out.DiffuseIndirectSampleOcclusion = GBufferData.DiffuseIndirectSampleOcclusion;
	Out.bNeedsSeparateLightAccumulation = UseSubsurfaceProfile(GBufferData.ShadingModelID);
	Out.bIsSLW = GBufferData.ShadingModelID == 10;
	Out.bIsHair = GBufferData.ShadingModelID == 7;
	Out.bHasBackfaceDiffuse = GBufferData.ShadingModelID == 6 || GBufferData.ShadingModelID == 2;
	Out.bRequiresBxDFImportanceSampling = RequiresBxDFImportanceSampling(GBufferData.ShadingModelID);
	Out.bIsFrontLayerTranslucency = false;
	Out.GBufferData = GBufferData;
	return Out;
}
FLumenMaterialData InternalReadMaterialData_GBuffer(uint2 InPixelPos) 	{ return InternalReadMaterialData_GBuffer(GetGBufferDataUint(InPixelPos)); }
FLumenMaterialData ReadMaterialData(uint2 InPixelPos)
{
	return InternalReadMaterialData_GBuffer(InPixelPos);
}
bool IsHair(FLumenMaterialData In)
{
	return In.bIsHair || In.ShadingID == 7;
}
bool HasBackfaceDiffuse(FLumenMaterialData In)
{
	return In.bHasBackfaceDiffuse || In.ShadingID == 6 || In.ShadingID == 2;
}
bool SupportsScreenTraces(FLumenMaterialData In, bool bAllowHairScreenTraces)
{
	return !IsHair(In) || bAllowHairScreenTraces;
}
float ConvertToDeviceZ(float SceneDepth)
{
	[flatten]
	if (false)//IsOrthoProjection())
	{
		return SceneDepth ;//* View_ViewToClip[2][2] + View_ViewToClip[3][2];
	}
	else
	{
		return 1.0f / ((SceneDepth + View_InvDeviceZToWorldZTransform[3]) * View_InvDeviceZToWorldZTransform[2]);
	}
}
struct FScreenProbeMaterial
{
	float3 WorldNormal;
	float SceneDepth;
	bool bIsValid;
	bool bHasBackfaceDiffuse;
	bool bSupportsScreenTraces;
};
FScreenProbeMaterial GetScreenProbeMaterial(uint2 PixelPos)
{
	const FLumenMaterialData Material = ReadMaterialData(PixelPos);
	FScreenProbeMaterial Out;
	Out.WorldNormal = Material.WorldNormal;
	Out.SceneDepth = Material.SceneDepth;
	Out.bIsValid = IsValid(Material);
	Out.bHasBackfaceDiffuse = HasBackfaceDiffuse(Material);
	Out.bSupportsScreenTraces = SupportsScreenTraces(Material, bSupportsHairScreenTraces);
	return Out;
}
float3 GetHistoryScreenPosition(float2 ScreenPosition, float2 ScreenUV, float DeviceZ)
{
	float3 HistoryScreenPosition = float3(ScreenPosition, DeviceZ);
	bool bIsDynamicPixel = false;
	{
		float4 ThisClip = float4(HistoryScreenPosition, 1);
		float4 PrevClip = mul(ThisClip, View_ClipToPrevClip); 
		float3 PrevScreen = PrevClip.xyz / PrevClip.w;
		float3 Velocity = HistoryScreenPosition - PrevScreen;
		float4 EncodedVelocity = float4(0.0f,0.0f,0.0f,0.0f);
		HistoryScreenPosition -= Velocity;
	}
	return HistoryScreenPosition;
}
float3 GetHistoryScreenPositionIncludingTAAJitter(float2 ScreenPosition, float2 ScreenUV, float DeviceZ)
{
	float3 HistoryScreenPosition = GetHistoryScreenPosition(ScreenPosition - View_TemporalAAJitter.xy, ScreenUV, DeviceZ);
	HistoryScreenPosition.xy += View_TemporalAAJitter.zw;
	return HistoryScreenPosition;
}
float2 GetScreenPositionForProjectionType(float2 ScreenPosition, float SceneDepth)
{
	return  ScreenPosition * SceneDepth;
}

float3 GetPrevTranslatedWorldPosition(float2 HistoryScreenPosition, float HistorySceneDepth)
{
	float3 PrevPositionTranslatedWorld = mul(float4(GetScreenPositionForProjectionType(HistoryScreenPosition.xy, HistorySceneDepth), HistorySceneDepth, 1), View_PrevScreenToTranslatedWorld).xyz;
	float3 PreViewTranslationOffset = float3(0.0f,0.0f,0.0f);//DFFastLocalSubtractDemote(GetPrimaryView().PreViewTranslation, GetPrimaryView().PrevPreViewTranslation);
	float3 PrevTranslatedWorldPosition = PrevPositionTranslatedWorld + PreViewTranslationOffset;
	return PrevTranslatedWorldPosition;
}
float3 GetPrevTranslatedWorldPosition(float3 HistoryScreenPosition)
{
	return GetPrevTranslatedWorldPosition(HistoryScreenPosition.xy, ConvertFromDeviceZ(HistoryScreenPosition.z));
}
uint EncodeScreenProbeSpeed(float ProbeSpeed, bool bTwoSidedFoliage, bool bSupportsScreenTraces)
{
	return (f32tof16(ProbeSpeed) & 0x7FFE)  | (bTwoSidedFoliage ? 0x8000 : 0) | (bSupportsScreenTraces ? 0x1 : 0);
}
void WriteDownsampledProbeMaterial(float2 ScreenUV, uint2 ScreenProbeAtlasCoord, FScreenProbeMaterial ProbeMaterial)
{
	float EncodedDepth = ProbeMaterial.SceneDepth;
	if (!ProbeMaterial.bIsValid)
	{
		EncodedDepth *= -1.0f;
	}
	RWScreenProbeSceneDepth[ScreenProbeAtlasCoord] = asuint(EncodedDepth);
	RWScreenProbeWorldNormal[ScreenProbeAtlasCoord] = UnitVectorToOctahedron(ProbeMaterial.WorldNormal) * 0.5 + 0.5;
	float3 ProbeWorldVelocity;
	float3 ProbeTranslatedWorldPosition;
	{
		float2 ProbeScreenPosition = (ScreenUV - View_ScreenPositionScaleBias.wz) / View_ScreenPositionScaleBias.xy;
		float ProbeDeviceZ = ConvertToDeviceZ(ProbeMaterial.SceneDepth);
		float3 ProbeHistoryScreenPosition = GetHistoryScreenPositionIncludingTAAJitter(ProbeScreenPosition, ScreenUV, ProbeDeviceZ);
		ProbeTranslatedWorldPosition = mul(float4(GetScreenPositionForProjectionType(ProbeScreenPosition, ProbeMaterial.SceneDepth), ProbeMaterial.SceneDepth, 1), mScreenToTranslatedWorld).xyz;
		ProbeWorldVelocity = ProbeTranslatedWorldPosition - GetPrevTranslatedWorldPosition(ProbeHistoryScreenPosition);
	}
	RWScreenProbeWorldSpeed[ScreenProbeAtlasCoord] = EncodeScreenProbeSpeed(length(ProbeWorldVelocity), ProbeMaterial.bHasBackfaceDiffuse, ProbeMaterial.bSupportsScreenTraces);
	RWScreenProbeTranslatedWorldPosition[ScreenProbeAtlasCoord] = float4(ProbeTranslatedWorldPosition, 0.0f);
}
//rect -> tile 8x8 -> 4x4 sub tile -> probe
//60 x 51
//Dispatch(8,5,1)
[numthreads(8,8,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~1119,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,//(0~7,0~7,0)
    uint3 inDispatchThreadId : SV_DispatchThreadID){//(0~63,0~39,0)
    uint2 ScreenProbeAtlasCoord = inDispatchThreadId.xy;
    uint2 ScreenProbeAtlasViewSize = uint2(60, 51);

    if(any(ScreenProbeAtlasCoord < ScreenProbeAtlasViewSize)){//texture / rt size
        //uint2(60, 51) x 16 => (960,544)
        uint2 ScreenProbeScreenPosition = GetUniformScreenProbeScreenPosition(ScreenProbeAtlasCoord);
		float2 ScreenUV = (ScreenProbeScreenPosition + .5f) * View_BufferSizeAndInvSize.zw;
		WriteDownsampledProbeMaterial(ScreenUV, ScreenProbeAtlasCoord, GetScreenProbeMaterial(ScreenProbeScreenPosition));
    }
}