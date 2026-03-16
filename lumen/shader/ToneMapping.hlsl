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

Texture2D SceneColorTexture : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);

//lumen -> time/fps
void VS(in float2 inPositionNDC : POSITION, in float2 inTexcoord : TEXCOORD,
    out float4 outPosition : SV_POSITION,
    out float2 outTexcoord : TEXCOORD0)
{
    outPosition = float4(inPositionNDC, 0.0f, 1.0f);
    outTexcoord = inTexcoord;
}

void PS(
    in float4 inPosition : SV_POSITION,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outRT0 : SV_TARGET0)
{
    //distance field shadow
    float exposure = 2.4f;
    float4 sceneColor = SceneColorTexture.SampleLevel(gsamPointClamp, inTexcoord, 0.0f);
    sceneColor /= exposure;
    //+bloom
    //color grading
    outRT0 = sceneColor;
}