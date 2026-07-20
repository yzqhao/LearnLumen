#include "GlobalConstant.hlsli"

Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D GBufferC : register(t2);
Texture2D SceneDepth : register(t3);
Texture2D ShadowMaskTexture : register(t4);

const static float View_MinRoughness = 0.02f;
const static float2 DeferredLightUniforms_DistanceFadeMAD = float2(0.0002f, -9.0f);
const static float4 View_InvDeviceZToWorldZTransform = float4(0.0f, 0.0f, 0.1f, -1.00000E-08f);
const static float4 View_BufferSizeAndInvSize = float4(960.0f, 540.0f, 0.00104f, 0.00185f);
const static float2 View_BufferToSceneTextureScale = float2(1.0f, 1.0f);
struct FDirectLighting
{
    float3 Diffuse;
    float3 Specular;
};
struct FShadowTerms
{
    float SurfaceShadow;
};
struct BxDFContext
{
    float NoV;
    float NoL;
    float VoL;
    float NoH;
    float VoH;
};
struct SurfaceInfo
{
    float3 NormalWS;
    float3 DiffuseColor;
    float3 SpecularColor;
    float Roughness;
    uint ShadingModelID;
    float Depth;
};
struct FLightAccumulator
{
    float3 TotalLightDiffuse;
    float3 TotalLightSpecular;
};
struct FDeferredLightingSplit
{
    float4 DiffuseLighting;
    float4 SpecularLighting;
};
struct DirectionalLightInfo
{
    float3 Color;
    float3 L;
    float Radius;
    float SphereSinAlpha;
    float NoL;
};
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
float Pow2(float inX)
{
    return inX * inX;
}
float Pow4(float x)
{
    float xx = x * x;
    return xx * xx;
}
float Pow5(float x)
{
    float xx = x * x;
    return xx * xx * x;
}
float sqrtFast(float x)
{
    int i = asint(x);
    i = 0x1FBD1DF5 + (i >> 1);
    return asfloat(i);
}
float4 Texture2DSampleLevel(Texture2D inTexture, SamplerState inSampler, float2 inTexcoord, float inMip)
{
    return inTexture.SampleLevel(inSampler, inTexcoord, inMip);
}
void Init(inout BxDFContext Context, float3 N, float3 V, float3 L)
{
    Context.NoL = dot(N, L);
    Context.NoV = dot(N, V);
    Context.VoL = dot(V, L);
    float InvLenH = rsqrt(2 + 2 * Context.VoL);
    Context.NoH = saturate((Context.NoL + Context.NoV) * InvLenH);
    Context.VoH = saturate(InvLenH + InvLenH * Context.VoL);
}
void SphereMaxNoH(inout BxDFContext Context, float SinAlpha)
{
    float CosAlpha = sqrt(1 - Pow2(SinAlpha));
    float RoL = 2 * Context.NoL * Context.NoV - Context.VoL;
    if (RoL >= CosAlpha)
    {
        Context.NoH = 1;
        Context.VoH = abs(Context.NoV);
    }
    else
    {
        float rInvLengthT = SinAlpha * rsqrt(1 - RoL * RoL);
        float NoTr = rInvLengthT * (Context.NoV - RoL * Context.NoL);
        float VoTr = rInvLengthT * (2 * Context.NoV * Context.NoV - 1 - RoL * Context.VoL);
        {
            float NxLoV = sqrt(saturate(1 - Pow2(Context.NoL) - Pow2(Context.NoV) - Pow2(Context.VoL) + 2 * Context.NoL * Context.NoV * Context.VoL));
            float NoBr = rInvLengthT * NxLoV;
            float VoBr = rInvLengthT * NxLoV * 2 * Context.NoV;
            float NoLVTr = Context.NoL * CosAlpha + Context.NoV + NoTr;
            float VoLVTr = Context.VoL * CosAlpha + 1 + VoTr;
            float p = NoBr * VoLVTr;
            float q = NoLVTr * VoLVTr;
            float s = VoBr * NoLVTr;
            float xNum = q * (-0.5 * p + 0.25 * VoBr * NoLVTr);
            float xDenom = p * p + s * (s - 2 * p) + NoLVTr * ((Context.NoL * CosAlpha + Context.NoV) * Pow2(VoLVTr) + q * (-0.5 * (VoLVTr + Context.VoL * CosAlpha) - 0.5));
            float TwoX1 = 2 * xNum / (Pow2(xDenom) + Pow2(xNum));
            float SinTheta = TwoX1 * xDenom;
            float CosTheta = 1.0 - TwoX1 * xNum;
            NoTr = CosTheta * NoTr + SinTheta * NoBr;
            VoTr = CosTheta * VoTr + SinTheta * VoBr;
        }
        Context.NoL = Context.NoL * CosAlpha + NoTr;
        Context.VoL = Context.VoL * CosAlpha + VoTr;
        float InvLenH = rsqrt(2 + 2 * Context.VoL);
        Context.NoH = saturate((Context.NoL + Context.NoV) * InvLenH);
        Context.VoH = saturate(InvLenH + InvLenH * Context.VoL);
    }
}
float3 Diffuse_Lambert(float3 DiffuseColor)
{
    return DiffuseColor * (1 / PI);
}
float D_GGX(float a2, float NoH)
{
    float d = (NoH * a2 - NoH) * NoH + 1;
    return a2 / (PI * d * d);
}
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
    float a = sqrt(a2);
    float Vis_SmithV = NoL * (NoV * (1 - a) + a);
    float Vis_SmithL = NoV * (NoL * (1 - a) + a);
    return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}
float3 F_Schlick(float3 SpecularColor, float VoH)
{
    float Fc = Pow5(1 - VoH);
    return saturate(50.0 * SpecularColor.g) * Fc + (1 - Fc) * SpecularColor;
}
float New_a2(float a2, float SinAlpha, float VoH)
{
    return a2 + 0.25 * SinAlpha * (3.0 * sqrtFast(a2) + SinAlpha) / (VoH + 0.001);
}
float EnergyNormalization(inout float a2, float VoH, float SphereSinAlpha)
{
    float Sphere_a2 = a2;
    float Energy = 1;
    if (SphereSinAlpha > 0)
    {
        Sphere_a2 = New_a2(a2, SphereSinAlpha, VoH);
        Energy = a2 / Sphere_a2;
    }
    return Energy;
}
float3 SpecularGGX(float Roughness, float3 SpecularColor, BxDFContext Context, float NoL, float SphereSinAlpha)
{
    float a2 = Pow4(Roughness);
    float Energy = EnergyNormalization(a2, Context.VoH, SphereSinAlpha);
    float D = D_GGX(a2, Context.NoH) * Energy;
    float Vis = Vis_SmithJointApprox(a2, Context.NoV, NoL);
    float3 F = F_Schlick(SpecularColor, Context.VoH);
    return (D * Vis) * F;
}
FDirectLighting DefaultLitBxDF(SurfaceInfo inSurfaceInfo, float3 N, float3 V, float3 L, float NoL, float SphereSinAlpha)
{
    inSurfaceInfo.Roughness = max(inSurfaceInfo.Roughness, View_MinRoughness);
    BxDFContext Context;
    FDirectLighting Lighting;
    Lighting.Diffuse = 0;
    Lighting.Specular = 0;
    [branch]
    if (NoL > 0.0f)
    {
        bool bHasAnisotropy = false;
        float NoV, VoH, NoH;
        Init(Context, N, V, L);
        NoV = Context.NoV;
        SphereMaxNoH(Context, SphereSinAlpha);
        Context.NoV = saturate(abs(Context.NoV) + 1e-5);
        Lighting.Diffuse = Diffuse_Lambert(inSurfaceInfo.DiffuseColor);
        Lighting.Diffuse *= NoL;
        Lighting.Specular = NoL * SpecularGGX(inSurfaceInfo.Roughness, inSurfaceInfo.SpecularColor, Context, NoL, SphereSinAlpha);
    }
    return Lighting;
}
float DistanceFromCameraFade(float SceneDepth)
{
    float Fade = saturate(SceneDepth * DeferredLightUniforms_DistanceFadeMAD.x + DeferredLightUniforms_DistanceFadeMAD.y);
    return Fade * Fade;
}
void GetShadowTermsBase(
    float SceneDepth,
    float4 LightAttenuation,
    inout FShadowTerms OutShadow)
{
    {
        float StaticShadowing = 1.0f;
        {
            float DynamicShadowFraction = DistanceFromCameraFade(SceneDepth);
            OutShadow.SurfaceShadow = lerp(LightAttenuation.x, StaticShadowing, DynamicShadowFraction);
            OutShadow.SurfaceShadow *= LightAttenuation.z;
        }
    }
}
FLightAccumulator AccumulateDynamicLighting(float3 inViewDirection, SurfaceInfo inSurfaceInfo, DirectionalLightInfo inDirectionalLightInfo, float4 LightAttenuation)
{
    FLightAccumulator LightAccumulator = (FLightAccumulator) 0;
    FShadowTerms Shadow;
    Shadow.SurfaceShadow = 1.0f;
    GetShadowTermsBase(inSurfaceInfo.Depth, LightAttenuation, Shadow);
    [branch]
    if (Shadow.SurfaceShadow > 0)
    {
        FDirectLighting Lighting;
        Lighting = DefaultLitBxDF(inSurfaceInfo, inSurfaceInfo.NormalWS, -inViewDirection, inDirectionalLightInfo.L, inDirectionalLightInfo.NoL, inDirectionalLightInfo.SphereSinAlpha);
        LightAccumulator.TotalLightDiffuse += Lighting.Diffuse * inDirectionalLightInfo.Color * Shadow.SurfaceShadow;
        LightAccumulator.TotalLightSpecular += Lighting.Specular * inDirectionalLightInfo.Color * Shadow.SurfaceShadow;
    }
    return LightAccumulator;
}
float4 GetDynamicLighting(float3 CameraVector, SurfaceInfo inSurfaceInfo, DirectionalLightInfo inDirectionalLightInfo, float4 LightAttenuation)
{
    FLightAccumulator LightAccumulator = AccumulateDynamicLighting(CameraVector, inSurfaceInfo, inDirectionalLightInfo, LightAttenuation);
    return float4(LightAccumulator.TotalLightDiffuse + LightAccumulator.TotalLightSpecular, 0);
}
float3 DecodeNormal(float3 N)
{
    return N * 2 - 1;
}
float3 DecodeNormalHelper(float3 SrcNormal)
{
    return SrcNormal * 2.0f - 1.0f;
}
float ConvertFromDeviceZ(float DeviceZ)
{
    return 1.0f / (DeviceZ * View_InvDeviceZToWorldZTransform[2] - View_InvDeviceZToWorldZTransform[3]);
}
float CalcSceneDepth(float2 ScreenUV)
{
    return ConvertFromDeviceZ(Texture2DSampleLevel(SceneDepth, gsamPointClamp, ScreenUV, 0).r);
}
float DielectricSpecularToF0(float Specular)
{
    return float(0.08f * Specular);
}
float3 ComputeF0(float Specular, float3 BaseColor, float Metallic)
{
    return lerp(DielectricSpecularToF0(Specular).xxx, BaseColor, Metallic.xxx);
}
void GBufferPostDecode(inout FGBufferData Ret, bool bGetNormalizedNormal)
{
    Ret.CustomData = 0.0f;
    Ret.PrecomputedShadowFactors = !(Ret.SelectiveOutputMask & 0x2) ? Ret.PrecomputedShadowFactors : ((Ret.SelectiveOutputMask & 0x4) ? float(0.0f) : float(1.0f));
    Ret.Velocity = !(Ret.SelectiveOutputMask & 0x8) ? Ret.Velocity : float(0.0f);
    bool bHasAnisotropy = (Ret.SelectiveOutputMask & 0x1);
    Ret.StoredBaseColor = Ret.BaseColor;
    Ret.StoredMetallic = Ret.Metallic;
    Ret.StoredSpecular = Ret.Specular;
    Ret.GBufferAO = Ret.GenericAO;
    Ret.DiffuseIndirectSampleOcclusion = 0x0;
    Ret.IndirectIrradiance = 1;
    if (bGetNormalizedNormal)
    {
        Ret.WorldNormal = normalize(Ret.WorldNormal);
    }
    [flatten]
    if (Ret.ShadingModelID == 9)
    {
        Ret.Metallic = 0.0;
    }
    {
        Ret.SpecularColor = ComputeF0(Ret.Specular, Ret.BaseColor, Ret.Metallic);
        Ret.DiffuseColor = Ret.BaseColor - Ret.BaseColor * Ret.Metallic;
        {
            Ret.DiffuseColor = Ret.DiffuseColor * float3(1.0f, 1.0f, 1.0f);
            Ret.SpecularColor = Ret.SpecularColor * 1.0f;
        }
    }
    if (bHasAnisotropy)
    {
        Ret.WorldTangent = float3(DecodeNormal(Ret.WorldTangent));
        Ret.Anisotropy = float(Ret.Anisotropy * 2.0f - 1.0f);
        if (bGetNormalizedNormal)
        {
            Ret.WorldTangent = normalize(Ret.WorldTangent);
        }
    }
    else
    {
        Ret.WorldTangent = 0;
        Ret.Anisotropy = 0;
    }
    Ret.SelectiveOutputMask = Ret.SelectiveOutputMask << 4;
}

FGBufferData DecodeGBufferDataDirect(float4 InMRT1,
    float4 InMRT2,
    float4 InMRT3,
    float4 InMRT4,
    float CustomNativeDepth,
    float4 AnisotropicData,
    uint CustomStencil,
    float SceneDepth,
    bool bGetNormalizedNormal)
{
    FGBufferData Ret = (FGBufferData) 0;
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
    GBufferPostDecode(Ret, bGetNormalizedNormal);
    Ret.CustomDepth = ConvertFromDeviceZ(CustomNativeDepth);
    Ret.CustomStencil = CustomStencil;
    Ret.Depth = SceneDepth;
    return Ret;
}
FGBufferData DecodeGBufferDataUV(float2 UV, bool bGetNormalizedNormal = true)
{
    float CustomNativeDepth = 0.0f;
    int2 IntUV = (int2) trunc(UV * View_BufferSizeAndInvSize.xy * View_BufferToSceneTextureScale.xy);
    uint CustomStencil = 0;
    float SceneDepth = CalcSceneDepth(UV);
    float4 AnisotropicData = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 InMRT1 = Texture2DSampleLevel(GBufferA, gsamPointClamp, UV, 0).xyzw;
    float4 InMRT2 = Texture2DSampleLevel(GBufferB, gsamPointClamp, UV, 0).xyzw;
    float4 InMRT3 = Texture2DSampleLevel(GBufferC, gsamPointClamp, UV, 0).xyzw;
    float4 InMRT4 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    FGBufferData Ret = DecodeGBufferDataDirect(InMRT1,
		InMRT2,
		InMRT3,
		InMRT4,
		CustomNativeDepth,
		AnisotropicData,
		CustomStencil,
		SceneDepth,
		bGetNormalizedNormal);
    return Ret;
}
FGBufferData GetGBufferData(float2 UV, bool bGetNormalizedNormal = true)
{
    return DecodeGBufferDataUV(UV, bGetNormalizedNormal);
}
SurfaceInfo GetSurfaceInfo(float2 UV, bool bGetNormalizedNormal = true)
{
    SurfaceInfo Out;
    FGBufferData GBuffer = GetGBufferData(UV, bGetNormalizedNormal);
    Out.NormalWS = GBuffer.WorldNormal;
    Out.DiffuseColor = GBuffer.DiffuseColor;
    Out.SpecularColor = GBuffer.SpecularColor;
    Out.Roughness = GBuffer.Roughness;
    Out.ShadingModelID = GBuffer.ShadingModelID;
    Out.Depth = GBuffer.Depth;
    return Out;
}
float SphereHorizonCosWrap(float NoL, float SinAlphaSqr)
{
    float SinAlpha = sqrt(SinAlphaSqr);
    if (NoL < SinAlpha)
    {
        NoL = max(NoL, -SinAlpha);
        NoL = Pow2(SinAlpha + NoL) / (4 * SinAlpha);
    }
    return NoL;
}
DirectionalLightInfo InitDirectionalLightInfo(float3 N, float Roughness)
{
    DirectionalLightInfo Out;
    Out.Color = float3(10.0f, 10.0f, 10.0f);
    Out.L = float3(-0.69466f, 0.0f, 0.71934f);
    Out.Radius = 0.00467f;

    float DistSqr = dot(Out.L, Out.L);
    float InvDist = rsqrt(DistSqr);
    float falloff = rcp(DistSqr + 1.0f);

    Roughness = max(Roughness, View_MinRoughness);
    float a = Pow2(Roughness);
    Out.SphereSinAlpha = saturate(Out.Radius * InvDist * (1 - a));

    float NoL = dot(N, Out.L);
    float SinAlphaSqr = saturate(Pow2(Out.Radius) * falloff);
    NoL = SphereHorizonCosWrap(NoL, SinAlphaSqr);
    Out.NoL = saturate(NoL);
    return Out;
}
//surface point - camera position ws = view direction (V)
void VS(in float2 inPositionNDC : POSITION, in float2 inTexcoord : TEXCOORD,
    out float4 outPosition : SV_POSITION,
    out float2 outTexcoord : TEXCOORD0,
    out float3 outCameraVector : TEXCOORD1)
{
    outPosition = float4(inPositionNDC, 0.0f, 1.0f);
    outTexcoord = inTexcoord;
    //ndc -> clip -> view -> translated world
    outCameraVector = mul(float4(inPositionNDC.xy, 1.0f, 0.0f), mScreenToTranslatedWorld).xyz;
}

//shading model id = 1,8,
void PS(
    in float4 inPosition : SV_POSITION,
    in float2 inTexcoord : TEXCOORD0,
    in float3 inCameraVector : TEXCOORD1,
    out float4 outRT0 : SV_TARGET0)
{
    float3 V = normalize(inCameraVector);
    //distance field shadow
    float4 outColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 lightAttenuation = ShadowMaskTexture.SampleLevel(gsamPointClamp, inTexcoord, 0.0f); //float4(1.0f,1.0f,1.0f,1.0f);
    SurfaceInfo SurfaceInfo = GetSurfaceInfo(inTexcoord); //g buffer -> surface pixel point
    [branch]
    if (SurfaceInfo.ShadingModelID > 0)
    {
        DirectionalLightInfo directionalLightInfo = InitDirectionalLightInfo(SurfaceInfo.NormalWS, SurfaceInfo.Roughness);
        float4 Radiance = GetDynamicLighting(V, SurfaceInfo, directionalLightInfo, lightAttenuation);
        outColor = Radiance;
    }
    outColor.rgba *= 2.4; //虚幻中的曝光数值
    outRT0 = outColor;
}