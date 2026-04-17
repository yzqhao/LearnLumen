cbuffer GlobalConstants{
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
const static float4 View_InvDeviceZToWorldZTransform=float4(0.0f,0.0f, 0.1f, -1.00000E-08f);
//instance => sdf 
ByteAddressBuffer RectCoordBuffer:register(t0);
StructuredBuffer<float4> DFSceneObject:register(t1);
Texture2D LumenCardScene_NormalAtlas:register(t2);
Texture2D LumenCardScene_DepthAtlas:register(t3);
StructuredBuffer<float4>  LumenCardScene_CardData:register(t4);

RWTexture2D<float4> LumenSceneDirectLighting:register(u0);
RWTexture2D<float4> LumenSceneFinalLighting:register(u1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);

float length2(float2 v)
{
	return dot(v, v);
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
    PreViewTranslation.High=-mCameraPositionHighWS.xyz;
    PreViewTranslation.Low=-mCameraPositionLowWS.xyz;
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
//vs->ps,viewport -> rt,0~1->rt
[numthreads(1,1,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~11,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,
    uint3 inDispatchThreadId : SV_DispatchThreadID){
    if(all(inDispatchThreadId.xy<uint2(4096,4096))){//(0~11,0)
        FLumenCardData Card = GetLumenCardData(inDispatchThreadId.x);
        uint4 rect= Decode(RectCoordBuffer, inDispatchThreadId.x);
        float width=float(rect.z-rect.x);
        float height=float(rect.w-rect.y);
        for(uint y=rect.y;y<rect.w;y++){
            for(uint x=rect.x;x<rect.z;x++){
                //0~1
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
                if(CombinedAttenuation>0.0f){
                    color=float3(CombinedAttenuation,CombinedAttenuation,CombinedAttenuation);
                }
                LumenSceneDirectLighting[uint2(x,y)]=float4(color,1.0f);
                LumenSceneFinalLighting[uint2(x,y)]=float4(texcoord,0.0f,1.0f);
            }
        }
    }
}