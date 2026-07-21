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
    
    float4x4 View_ClipToPrevClip;
	float4x4 View_PrevScreenToTranslatedWorld;
    float4 ClipmapCornerTWSAndCellSizeForMark[6];
    float4 ClipmapCornerTWSAndCellSize[6];
    float4 RadianceProbeSettings[6];
	float4 View_InvDeviceZToWorldZTransform;
	int HemisphereProbeResolution;
	int ProbeSpacingInRadiosityTexels;
	int ProbeSpacingInRadiosityTexelsDivideShift;
	int RadiosityTileSize;
	float2 LumenCardScene_PhysicalAtlasSize;
	float2 LumenCardScene_InvPhysicalAtlasSize;
	int4 View_ViewRectMinAndSize;
	float4 View_BufferSizeAndInvSize;
	float4 View_ScreenPositionScaleBias;
	float4 View_TemporalAAJitter;
	float4 View_TemporalAAParams;
	float4 View_ViewRectMin;
	float4 View_ViewSizeAndInvSize;
	float View_PreExposure;
	float View_OneOverPreExposure;
	float View_ProjectionDepthThicknessScale;
	float View_bSubsurfacePostprocessEnabled;
	uint2 ScreenProbeViewSize;
	uint2 ScreenProbeAtlasViewSize;
	int2 ViewportTileDimensionsWithOverflow;
	float2 InvProbeFinalRadianceAtlasResolution;
	float View_bCheckerboardSubsurfaceProfileRendering;
	float3 TargetFormatQuantizationError;
	float4 ProbeHistoryScreenPositionScaleBias;
	float4 ImportanceSamplingHistoryUVMinMax;
	float2 PrevSceneColorBilinearUVMin;
	float2 PrevSceneColorBilinearUVMax;
	float4 PrevScreenPositionScaleBias;
	float4 PrevScreenPositionScaleBiasForDepth;
	float4 HZBUvFactorAndInvFactor;
	float4 HZBUVToScreenUVScaleBias;
	float2 HZBBaseTexelSize;
	float2 SampleRadianceProbeUVMul;
	float2 SampleRadianceProbeUVAdd;
	float2 SampleRadianceAtlasUVMul;
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
	int3 BlueNoise_ModuloMasks;
	int View_StateFrameIndex;
	int3 BlueNoise_Dimensions;
	float MaxHalfFloat;
	int2 ViewportTileDimensions;// 64 int2
	int CullByDistanceFromCamera;
	int CompactForFarField;
	uint PlacementDownsampleFactor;
	float DiffuseColorBoost;
	float CompactionTracingEndDistanceFromCamera;
	float CompactionMaxTraceDistance;
}

#endif
