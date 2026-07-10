#ifndef GLOBAL_CONSTANTS
#define GLOBAL_CONSTANTS

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
    float4 mCameraPositionMeshCardCapture[12];
    float4x4 mViewMatrixMeshCardCapture[6];//+x,-x,... axis
    float4x4 mProjectionMatrixMeshCardCapture[2];
    
    uint4 View_StateFrameIndexMod8;//frame index mod 8
    int4 View_NumGlobalSDFClipmaps;
    float4 View_GlobalVolumeTranslatedCenterAndExtent[6];
    float4 View_GlobalVolumeTranslatedWorldToUVAddAndMul[6];
    float4 View_GlobalDistanceFieldMipTranslatedWorldToUVScale[6];
    float4 View_GlobalDistanceFieldMipTranslatedWorldToUVBias[6];
    float4 View_GlobalVolumeTexelSize;
    float4 View_GlobalDistanceFieldMipFactor;//
    float4 View_GlobalDistanceFieldMipTransition;
    float4 View_NotCoveredExpandSurfaceScale;
    float4 View_CoveredExpandSurfaceScale;
    float4 View_DitheredTransparencyTraceThreshold;
    float4 View_DitheredTransparencyStepThreshold;
    float4 View_NotCoveredMinStepScale;
    int4 View_GlobalDistanceFieldClipmapSizeInPages;
    float4 View_GlobalDistanceFieldInvPageAtlasSize;
    float4 View_GlobalDistanceFieldInvCoverageAtlasSize;
}

#endif
