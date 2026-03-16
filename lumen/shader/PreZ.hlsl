cbuffer GlobalConstants : register(b0)
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
}

float4 VS(in float3 inPositionMS : POSITION, in uint inInstanceID : SV_INSTANCEID) : SV_POSITION
{
    float4 positionWS = mul(float4(inPositionMS, 1.0f), mModelMatrices[inInstanceID]);
    positionWS = float4(positionWS.xyz - mCameraPositionHighWS.xyz - mCameraPositionLowWS.xyz, 1.0f); //translated world
    float4 positionVS = mul(positionWS, mViewMatrix);
    float4 positionCS = mul(positionVS, mProjectionMatrix);
    return positionCS;
}