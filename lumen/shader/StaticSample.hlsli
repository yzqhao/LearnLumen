
#ifndef STATIC_SAMPLE_HLSL
#define STATIC_SAMPLE_HLSL

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamLLPClamp : register(s4);
SamplerState gsamAnisotropicWrap : register(s5);
SamplerState gsamAnisotropicClamp : register(s6);

#endif