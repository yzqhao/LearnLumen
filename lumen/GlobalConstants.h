#pragma once

#include "../common/d3dUtil.h"

struct GlobalConstants
{
    union {
        struct {
            float mProjectionMatrix[16];
            float mViewMatrix[16];
            float mWorldToClipMatrix[16];
            float mCameraPositionHighWS[4];
            float mCameraPositionLowWS[4];
            float mViewDirectionWS[4];
            float mViewRightWS[4];
            float mViewUpWS[4];
            float mModelMatrices[32];
            float mITModelMatrices[32];
            float mScreenToTranslatedWorld[16];
            // mesh card
            float mCameraPositionMeshCardCapture[48];
            float mViewMatrixMeshCardCapture[96];//+x,-x,... axis
            float mProjectionMatrixMeshCardCapture[32];

            unsigned int View_StateFrameIndexMod8[4];//frame index mod 8
            int View_NumGlobalSDFClipmaps[4];
            float View_GlobalVolumeTranslatedCenterAndExtent[24];
            float View_GlobalVolumeTranslatedWorldToUVAddAndMul[24];
            float View_GlobalDistanceFieldMipTranslatedWorldToUVScale[24];
            float View_GlobalDistanceFieldMipTranslatedWorldToUVBias[24];

            float View_GlobalVolumeTexelSize[4];
            float View_GlobalDistanceFieldMipFactor[4];//
            float View_GlobalDistanceFieldMipTransition[4];
            float View_NotCoveredExpandSurfaceScale[4];
            float View_CoveredExpandSurfaceScale[4];
            float View_DitheredTransparencyTraceThreshold[4];
            float View_DitheredTransparencyStepThreshold[4];
            float View_NotCoveredMinStepScale[4];
            int View_GlobalDistanceFieldClipmapSizeInPages[4];
            float View_GlobalDistanceFieldInvPageAtlasSize[4];
            float View_GlobalDistanceFieldInvCoverageAtlasSize[4];
        };
        float mData[1024];
    };
    void SetProjectionMatrix(float* inMatrix);
    void SetViewMatrix(float* inMatrix);
    void SetWorldToClipMatrix(float* inMatrix);
    void SetCameraPositionHighWS(float inX, float inY, float inZ, float inW = 0.0f);
    void SetCameraPositionLowWS(float inX, float inY, float inZ, float inW = 0.0f);
    void SetViewDirectionWS(float inX, float inY, float inZ, float inW = 0.0f);
    void SetViewRightWS(float inX, float inY, float inZ, float inW = 0.0f);
    void SetViewUpWS(float inX, float inY, float inZ, float inW = 0.0f);
    void SetScreenToTranslatedWorldMatrix(float* inMatrix);
};