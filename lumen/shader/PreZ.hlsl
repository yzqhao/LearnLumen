#include "GlobalConstant.hlsli"

float4 VS(in float3 inPositionMS : POSITION, in uint inInstanceID : SV_INSTANCEID) : SV_POSITION
{
    float4 positionWS = mul(float4(inPositionMS, 1.0f), mModelMatrices[inInstanceID]);
    positionWS = float4(positionWS.xyz - mCameraPositionHighWS.xyz - mCameraPositionLowWS.xyz, 1.0f); //translated world
    float4 positionVS = mul(positionWS, mViewMatrix);
    float4 positionCS = mul(positionVS, mProjectionMatrix);
    return positionCS;
}