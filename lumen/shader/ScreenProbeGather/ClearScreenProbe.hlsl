#include "../GlobalConstant.hlsli"

const static float PI = 3.1415926535897932f;

RWTexture2D<uint> RWScreenProbeSceneDepth:register(u0);
RWTexture2D<uint> RWScreenProbeWorldSpeed:register(u1);
RWTexture2D<unorm float2> RWScreenProbeWorldNormal:register(u2);
RWTexture2D<float4> RWScreenProbeTranslatedWorldPosition:register(u3);
RWTexture2D<uint> RWScreenTileAdaptiveProbeHeader:register(u4);//60x34
RWTexture2D<uint> RWScreenTileAdaptiveProbeIndices:register(u5);//(60 x 16)  x ( 34 x 16) = (960 x 544)
RWStructuredBuffer<uint> RWAdaptiveScreenProbeData:register(u6);
RWStructuredBuffer<uint> RWNumAdaptiveScreenProbes:register(u7);
RWBuffer<uint> RWCompactedTraceTexelAllocator:register(u8);
RWBuffer<uint> RWCompactedTraceTexelData:register(u9);
RWTexture2D<float> RWLumenScreenProbeGatherLightingProbabilityDensityFunction:register(u10);

SamplerState MinMagMipPointClampped:register(s0);
SamplerState MinMagMipLinearWrap:register(s1);
SamplerState MinMagMipLinearClampped:register(s2);
SamplerState MinMagLinearMipPointClampped:register(s3);

//rect -> tile 8x8 -> 4x4 sub tile -> probe
//60 x 51
//Dispatch(30,17,1)
[numthreads(8,8,1)]
void CS(uint3 inGroupId : SV_GroupID,//(0~1119,0,0)
    uint3 inGroupThreadId : SV_GroupThreadID,//(0~7,0~7,0)
    uint3 inDispatchThreadId : SV_DispatchThreadID){//(0~959,0~539,0)
    if(any(inDispatchThreadId.xy < uint2(960,540))){
        // -> read data from gbuffer
        RWScreenTileAdaptiveProbeIndices[inDispatchThreadId.xy]=0u;//R16_UINT
    }
    if(any(inDispatchThreadId.xy < uint2(60,34))){
        RWScreenTileAdaptiveProbeHeader[inDispatchThreadId.xy]=0u;
    }
    if(any(inDispatchThreadId.xy < uint2(60,51))){
        RWScreenProbeSceneDepth[inDispatchThreadId.xy]=0u;
        RWScreenProbeWorldSpeed[inDispatchThreadId.xy]=0u;
        RWScreenProbeWorldNormal[inDispatchThreadId.xy]=float2(0.0f,0.0f);
        RWScreenProbeTranslatedWorldPosition[inDispatchThreadId.xy]=float4(0.0f,0.0f,0.0f,0.0f);
    }
    uint linearIndex=inDispatchThreadId.x+inDispatchThreadId.y*960u;
    if(inDispatchThreadId.x==0u&&inDispatchThreadId.y==0u){
        RWNumAdaptiveScreenProbes[0]=0u;
        RWCompactedTraceTexelAllocator[0]=0u;
        RWCompactedTraceTexelAllocator[1]=0u;
    }
    if(linearIndex<1020u){
        RWAdaptiveScreenProbeData[linearIndex]=0u;
		RWCompactedTraceTexelData[linearIndex]=0u;
    }
    if(any(inDispatchThreadId.xy < uint2(480,408))){
        RWLumenScreenProbeGatherLightingProbabilityDensityFunction[inDispatchThreadId.xy]=0.0f;
    }
}