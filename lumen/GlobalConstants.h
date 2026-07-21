#pragma once

#include "../common/d3dUtil.h"
#include "../math/Vec4.h"

struct GlobalConstants
{
    GlobalConstants() { memset(mData, 0, sizeof(mData)); }
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

            unsigned int mFrameIndexMod8;
            unsigned int mFrameIndex;
            int mMaxFramesAccumulated;
            int mNumTracesPerProbe;
            int mView_NumGlobalSDFClipmaps;
            int mRadiosityAtlasSize[2];
            int mFixedJitterIndex;
            //unsigned int View_StateFrameIndexMod8[4];//frame index mod 8
            //int View_NumGlobalSDFClipmaps[4];
            float View_GlobalVolumeTranslatedCenterAndExtent[24];
            float View_GlobalVolumeTranslatedWorldToUVAddAndMul[24];
            float View_GlobalDistanceFieldMipTranslatedWorldToUVScale[24];
            float View_GlobalDistanceFieldMipTranslatedWorldToUVBias[24];

            float View_GlobalVolumeTexelSize;
            float View_GlobalDistanceFieldMipFactor;
            float View_GlobalDistanceFieldMipTransition;
            float View_NotCoveredExpandSurfaceScale;
            float InvClipmapFadeSizeForMark;
            float SurfaceBias;
            float MinPDFToTrace;
            float MinTraceDistance;
            float MaxTraceDistance;
            float MaxMeshSDFTraceDistance;
            float MaxRayIntensity;
            float ProbePlaneWeightingDepthScale;
            float ScreenProbeDownsampleFactor;
            float SupersampleDistanceFromCameraSq;
            float DownsampleDistanceFromCameraSq;
            float StepFactor;

            float View_CoveredExpandSurfaceScale;
            float View_DitheredTransparencyTraceThreshold;
            float View_DitheredTransparencyStepThreshold;
            float View_NotCoveredMinStepScale;
            float MinSampleRadius;
            float SpatialFilterMaxRadianceHitAngle;
            float InvClipmapFadeSize;
            float ReprojectionRadiusScale;
            float PrevInvPreExposure;
            float ScreenTraceNoFallbackThicknessScale;
            float HistoryDepthTestRelativeThickness;
            float RelativeDepthThickness ;
            float PrevSceneColorPreExposureCorrection;
            float MaxHierarchicalScreenTraceIterations;
            float NumThicknessStepsToDetermineCertainty;
            float RelativeSpeedDifferenceToConsiderLightingMoving;

            int View_GlobalDistanceFieldClipmapSizeInPages;
            unsigned int NumUniformScreenProbes;
            unsigned int MaxNumAdaptiveProbes;
            unsigned int bSupportsHairScreenTraces;

            float View_GlobalDistanceFieldInvPageAtlasSize[4];
            float View_GlobalDistanceFieldInvCoverageAtlasSize[4];
            
            float View_ClipToPrevClip[16];
            float View_PrevScreenToTranslatedWorld[16];
            Math::Vec4 ClipmapCornerTWSAndCellSizeForMark[6];
            Math::Vec4 ClipmapCornerTWSAndCellSize[6];
            Math::Vec4 RadianceProbeSettings[6];
            float View_InvDeviceZToWorldZTransform[4];//float4(0.0f,0.0f, 0.1f, -1.00000E-08f)
            int mHemisphereProbeResolution;
            int mProbeSpacingInRadiosityTexels;
            int mProbeSpacingInRadiosityTexelsDivideShift;
            int mRadiosityTileSize;
            float LumenCardScene_PhysicalAtlasSize[2];
            float LumenCardScene_InvPhysicalAtlasSize[2];
            int View_ViewRectMinAndSize[4];
            float View_BufferSizeAndInvSize[4];
            float View_ScreenPositionScaleBias[4];
            float View_TemporalAAJitter[4];
            float View_TemporalAAParams[4];
            float View_ViewRectMin[4];
            float View_ViewSizeAndInvSize[4];
            float View_PreExposure;
            float View_OneOverPreExposure;
            float View_ProjectionDepthThicknessScale;
            float View_bSubsurfacePostprocessEnabled;
            unsigned int ScreenProbeViewSize[2];
            unsigned int ScreenProbeAtlasViewSize[2];
            int ViewportTileDimensionsWithOverflow[2];
            float InvProbeFinalRadianceAtlasResolution[2];
            float View_bCheckerboardSubsurfaceProfileRendering;
            float TargetFormatQuantizationError[3];
            float ProbeHistoryScreenPositionScaleBias[4];
            float ImportanceSamplingHistoryUVMinMax[4];
            float PrevSceneColorBilinearUVMin[2];
            float PrevSceneColorBilinearUVMax[2];
            float PrevScreenPositionScaleBias[4];
            float PrevScreenPositionScaleBiasForDepth[4];
            float HZBUvFactorAndInvFactor[4];
            float HZBUVToScreenUVScaleBias[4];
            float HZBBaseTexelSize[2];
            float SampleRadianceProbeUVMul[2];
            float SampleRadianceProbeUVAdd[2];
            float SampleRadianceAtlasUVMul[2];
            int LumenCardScene_NumMeshCards;
            int LumenCardScene_NumCards;
            int RadianceProbeClipmapResolutionForMark;
            int NumRadianceProbeClipmapsForMark;
            int RadianceProbeClipmapResolution;
            int NumRadianceProbeClipmaps;
            int MaxNumProbes;
            int FrameNumber;
            int ForcedUniformLevel;
            int RadianceProbeResolution;
            int ProbeAtlasResolutionModuloMask;
            int ProbeAtlasResolutionDivideShift;
            int FinalProbeResolution;
            int FinalRadianceAtlasMaxMip;
            int ScreenProbeRayDirectionFrameIndex;
            int ScreenProbeTracingOctahedronResolution;
            int ScreenProbeGatherOctahedronResolution;
            int MaxImportanceSamplingOctahedronResolution;
            int ScreenProbeLightSampleResolutionXY;
            int SkipFoliageHits;
            int SkipHairHits;
            int MinimumTracingThreadOccupancy;
            int OverrideCacheOcclusionLighting;
            int ShowBlackRadianceCacheLighting;
            int ScreenProbeGatherOctahedronResolutionWithBorder;
            int DefaultDiffuseIntegrationMethod;
            int MaxClosurePerPixel;
            int ApplyMaterialAO;
            int LumenReflectionInputIsSSR;
            int bLumenSupportBackfaceDiffuse;
            int bLumenReflectionInputIsSSR;
            int bVisualizeDiffuseIndirect;
            float DebugForceTracesMoving;
            float ScreenProbeGatherMaxMip;
            float MaxRoughnessToEvaluateRoughSpecularForFoliage;
            float MaxRoughnessToTrace;
            float MaxRoughnessToTraceForFoliage;
            float InvRoughnessFadeLength;
            float MaxRoughnessToEvaluateRoughSpecular;
            float MaxAOMultibounceAlbedo;
            float LumenFoliageOcclusionStrength;
            float LumenReflectionSpecularScale;
            float LumenReflectionContrast;
            float FullResolutionJitterWidth;
            int BlueNoise_ModuloMasks[3];
            int View_StateFrameIndex;
            int BlueNoise_Dimensions[3];
            float MaxHalfFloat;
            int ViewportTileDimensions[2];// 64 int2
            int CullByDistanceFromCamera;
            int CompactForFarField;
            unsigned int PlacementDownsampleFactor;
            float DiffuseColorBoost;
            float CompactionTracingEndDistanceFromCamera;
            float CompactionMaxTraceDistance;
            float HistoryScreenPositionScaleBias[4];
            float HistoryUVMinMax[4];
            float HistoryDistanceThreshold;
            float InvFractionOfLightingMovingForFastUpdateMode;
            float MaxFastUpdateModeAmount;
            float reserved;
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