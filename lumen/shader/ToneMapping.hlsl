#include "GlobalConstant.hlsli"

Texture2D SceneColorTexture : register(t0);

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