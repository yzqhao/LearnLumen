#ifndef GLOBAL_CONSTANTS
#define GLOBAL_CONSTANTS

#include "StaticSample.hlsli"

const static float PI = 3.1415926535897932f;

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
    
    uint View_StateFrameIndexMod8;//frame index mod 8
	uint FrameIndex;
	int MaxFramesAccumulated;
	int NumTracesPerProbe;
    int View_NumGlobalSDFClipmaps;
	int2 RadiosityAtlasSize;
	int FixedJitterIndex;

    float4 View_GlobalVolumeTranslatedCenterAndExtent[6];
    float4 View_GlobalVolumeTranslatedWorldToUVAddAndMul[6];
    float4 View_GlobalDistanceFieldMipTranslatedWorldToUVScale[6];
    float4 View_GlobalDistanceFieldMipTranslatedWorldToUVBias[6];
    
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
	uint NumUniformScreenProbes = 2040u;
	uint MaxNumAdaptiveProbes = 1020u;
	bool bSupportsHairScreenTraces=false;
    
    float4 View_GlobalDistanceFieldInvPageAtlasSize;
    float4 View_GlobalDistanceFieldInvCoverageAtlasSize;
}

#endif
