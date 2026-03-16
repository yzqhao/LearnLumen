cbuffer GlobalConstants
{
    float4x4 mProjectionMatrix;
    float4x4 mViewMatrix;
    float4x4 mWorldToClipMatrix;
    float4 mCameraPositionHighWS;
    float4 mCameraPositionLowWS;
    float4 mViewDirectionWS;
    float4 mViewRightWS;
    float4 mViewUpWS;
    float4x4 mModelMatrices[2];
    float4x4 mITModelMatrices[2];
    float4x4 mScreenToTranslatedWorld;
}
const static float4 View_InvDeviceZToWorldZTransform = float4(0.0f, 0.0f, 0.1f, -1.00000E-08f);
//instance => sdf 
StructuredBuffer<float4> DFSceneObject : register(t0);
Texture2D SceneDepthTexture : register(t1);

RWTexture2D<float4> ShadowMaskTexture : register(u0);
SamplerState MinMagMipPoint : register(s0);
SamplerState MinMagLinearMipPointSampler : register(s1);

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
        float4(1.0f, 0.0f, 0.0f, 0.0f),
        float4(0.0f, 1.0f, 0.0f, 0.0f),
        float4(0.0f, 0.0f, 1.0f, 0.0f),
        float4(Translation, 1.0f)), M);
}
precise float3 MakePrecise(in precise float3 v)
{
    precise float3 pv = v;
    return pv;
}
float3 DFFastLocalSubtractDemote(FDFVector3 Lhs, float3 Rhs)
{
    const float3 High = MakePrecise((Lhs.High) - (Rhs));
    const float3 Sum = MakePrecise((High) + (Lhs.Low));
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
FDFObjectData LoadDFObjectDataFromBuffer(StructuredBuffer<float4> SourceBuffer, uint ObjectIndex)
{
    float3 PositionHigh = SourceBuffer[ObjectIndex * 10 + 0].xyz;
    FDFObjectData Data;
    float4 V0 = SourceBuffer[ObjectIndex * 10 + 1];
    float4 V1 = SourceBuffer[ObjectIndex * 10 + 2];
    float4 V2 = SourceBuffer[ObjectIndex * 10 + 3];
    float4x4 RelativeWorldToVolume = transpose(float4x4(V0, V1, V2, float4(0.0f, 0.0f, 0.0f, 1.0f)));
    Data.WorldToVolume = MakeDFInverseMatrix4x3(PositionHigh, RelativeWorldToVolume);
    float4 Vector3 = SourceBuffer[ObjectIndex * 10 + 4];
    Data.VolumePositionExtent = Vector3.xyz;
    Data.VolumeSurfaceBias = abs(Vector3.w);
    Data.bMostlyTwoSided = Vector3.w < 0.0f;
    float4 Vector4 = SourceBuffer[ObjectIndex * 10 + 5];
    Data.MinMaxDrawDistance2 = Vector4.xy;
    Data.SelfShadowBias = Vector4.z;
    Data.GPUSceneInstanceIndex = asuint(Vector4.w);
    V0 = SourceBuffer[ObjectIndex * 10 + 6];
    V1 = SourceBuffer[ObjectIndex * 10 + 7];
    V2 = SourceBuffer[ObjectIndex * 10 + 8];
    float4x4 VolumeToRelativeWorld = transpose(float4x4(V0, V1, V2, float4(0.0f, 0.0f, 0.0f, 1.0f)));
    Data.VolumeToWorld = MakeDFMatrix(PositionHigh, VolumeToRelativeWorld);
    float4 Vector8 = SourceBuffer[ObjectIndex * 10 + 9];
    Data.VolumeToWorldScale = Vector8.xyz;
    Data.VolumeScale = min(Data.VolumeToWorldScale.x, min(Data.VolumeToWorldScale.y, Data.VolumeToWorldScale.z));
    Data.AssetIndex = asuint(Vector8.w);
    return Data;
}
FDFObjectData LoadDFObjectData(uint ObjectIndex)
{
    return LoadDFObjectDataFromBuffer(DFSceneObject, ObjectIndex);
}
float ShadowRayTraceThroughCulledObjects(
    float3 TranslatedWorldRayStart,
    float3 TranslatedWorldRayEnd)
{
    FDFVector3 PreViewTranslation;
    PreViewTranslation.High = -mCameraPositionHighWS.xyz;
    PreViewTranslation.Low = -mCameraPositionLowWS.xyz;
    for (uint ObjectIndex = 0; ObjectIndex < 2u; ObjectIndex++)
    {
        FDFObjectData DFObjectData = LoadDFObjectData(ObjectIndex);
        //asset => sdf
        //ray from tranlated world => volume space
        float4x4 TranslatedWorldToVolume = DFFastToTranslatedWorld(DFObjectData.WorldToVolume, PreViewTranslation);
        float3 VolumeRayStart = mul(float4(TranslatedWorldRayStart, 1), TranslatedWorldToVolume).xyz;
        float3 VolumeRayEnd = mul(float4(TranslatedWorldRayEnd, 1), TranslatedWorldToVolume).xyz;
        float2 IntersectionTimes = LineBoxIntersect(VolumeRayStart, VolumeRayEnd, -DFObjectData.VolumePositionExtent, DFObjectData.VolumePositionExtent);
        if (IntersectionTimes.x < IntersectionTimes.y)
        {
            return 0.0f;
        }
    }
    return 1.0f;
}
//vs->ps,viewport -> rt,0~1->rt
[numthreads(8, 8, 1)]
void CS(uint3 inGroupId : SV_GroupID, //(0~119,0~67,0)
    uint3 inGroupThreadId : SV_GroupThreadID, //(0~7,0~7,0)
    uint3 inDispatchThreadId : SV_DispatchThreadID)
{ //(0~959,0~543,0)
    if (all(inDispatchThreadId.xy < uint2(960, 540)))
    {
        //rt => thread -> pixel 
        //|---|
        //|   |
        //|---|
        //(0,0)=>(0.5,0.5)
        float2 canvasSize = float2(960.0f, 540.0f);
        //screen coord (0~959,0~539)
        float2 uv = (float2(inDispatchThreadId.xy) + float2(0.5, 0.5)) / canvasSize; //(0.0~1.0)
        //ndc y
        float2 ndc_coord = (uv - 0.5) * float2(2.0f, -2.0f); //(-1,1)
        //(0,0)=>(0,0),(1,0),(0,1),(1,1)=>(0.5,0.5),(1.5,0.5),(0.5,1.5),(1.5,1.5)
        float deviceZ = SceneDepthTexture.GatherRed(MinMagLinearMipPointSampler, float2(inDispatchThreadId.xy + uint2(1, 1)) / canvasSize).r; //0~1
        //pcf
        float sceneDepth = 1.0f / (deviceZ * View_InvDeviceZToWorldZTransform[2] - View_InvDeviceZToWorldZTransform[3]);
        //perspective divide=>xy/w w=>z
        float3 positionWS = mul(float4(ndc_coord * sceneDepth, sceneDepth, 1), mScreenToTranslatedWorld).xyz;
        
        float3 LightDirection = float3(-0.69466f, 0.0f, 0.71934f);

        float TraceDistance = 10000.0f;
        float3 TranslatedWorldRayStart = positionWS;
        float3 TranslatedWorldRayEnd = positionWS + LightDirection * TraceDistance;
        float Shadow = ShadowRayTraceThroughCulledObjects(
            TranslatedWorldRayStart,
            TranslatedWorldRayEnd);
        //pixel world position => light
        ShadowMaskTexture[inDispatchThreadId.xy] = float4(Shadow, 1.0, 1.0f, 1.0f);
    }
}