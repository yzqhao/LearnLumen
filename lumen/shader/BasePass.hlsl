#include "GlobalConstant.hlsli"

void VS(in float3 inPositionMS : POSITION,
    in float4 inTangentX : TANGENTX,
    in float4 inTangentZ : TANGENTZ,
    in uint inInstanceID : SV_INSTANCEID,
    out float4 outPosition : SV_POSITION,
    out float3 outNormal : NORMAL)
{
    float4 positionWS = mul(float4(inPositionMS, 1.0f), mModelMatrices[inInstanceID]);
    positionWS = float4(positionWS.xyz - mCameraPositionHighWS.xyz - mCameraPositionLowWS.xyz, 1.0f); //translated world
    float4 positionVS = mul(positionWS, mViewMatrix);
    float4 positionCS = mul(positionVS, mProjectionMatrix);
    float3 normal = mul(float4(inTangentZ.xyz, 0.0f), mITModelMatrices[inInstanceID]).xyz;
    outNormal = normal;
    outPosition = positionCS;
}
//(0.0f,0.0f,1.0f) => (0.3,0.4,0.4) => 0.3*0.3+0.4*0.4+0.4*0.4 != 1.0
void PS(
    in float4 inPosition : SV_POSITION,
    in float3 inNormal : NORMAL,
    out float4 outRT0 : SV_TARGET0,
    out float4 outRT1 : SV_TARGET1,
    out float4 outRT2 : SV_TARGET2, //unorm
    out float4 outRT3 : SV_TARGET3)
{
    float3 normal = normalize(inNormal);
    normal = normal * 0.5 + 0.5;
    //scene color
    outRT0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    //normal + 0.333333f,r10g10b10a2(4:0,1,2,3)
    outRT1 = float4(normal, 0.333333f);
    //metallic + specular + roughness + [4 bit 0b1010 | 4 bit ShadingModel]
    uint lastCompoent = (10 << 4) | (1);
    outRT2 = float4(0.0f, 0.5f, 0.6407f, float(lastCompoent) / 255.0f);
    //base color + generic ao
    outRT3 = float4(0.9f, 0.9f, 0.9f, 1.0f);
}