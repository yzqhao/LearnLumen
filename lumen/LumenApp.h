#pragma once
#include <array>
#include "../common/d3dApp.h"
#include "../math/Color.h"
#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../common/d3dUtil.h"
#include "../common/UploadBuffer.h"
#include "../../../common/d3dx12.h"
#include "GlobalConstants.h"

struct D3DImage;

class LumenApp : public D3DApp
{
public:
	LumenApp(HINSTANCE hInstance);
	~LumenApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);

	void BuildShadersAndInputLayout();
	void BuildPSO();
    void BuildBuffers();
    void BuildScreenFullGeometry();

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();

    void InitScene();

private:
    D3DResource* InitBufferFromFile(const wchar_t* resname, const char* file);

    struct Misc
    {
        unsigned int misc[4];  //0:camera pos index,1:view matrix,2:projection matrix
    } mMisc;

    UINT mCbvSrvDescriptorSize = 0;

    ID3D12DescriptorHeap* mDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* mDsvDescriptorHeap{};

    std::unordered_map<std::string, ID3DBlob*> mByteCodes;
    std::unordered_map<std::string, IDxcBlob*> mDxcByteCodes;
    std::unordered_map<std::string, ID3D12RootSignature*> mRootSignatures;
    std::unordered_map<std::string, ID3D12PipelineState*> mPSOs;
    std::unordered_map<std::string, CD3DX12_GPU_DESCRIPTOR_HANDLE> mGPUViews;
    std::unordered_map<std::string, CD3DX12_CPU_DESCRIPTOR_HANDLE> mCPUViews;

    D3DResource* mCubePositionBuffer;
    D3DResource* mCubeIndexBuffer;
    D3DResource* mCubeAttributeBuffer;
    D3DResource* mDFSceneObject;
    D3DResource* mLumenCards;
    D3DResource* mClearCardBuffer;
    D3DResource* mRectDataBuffer;
    D3DResource* mRectUVBuffer;

    D3DImage* mGSDFPageAtlas;
    D3DImage* mGSDFCoverageAtlas;
    D3DImage* mGSDFPageTable;
    D3DImage* mGSDFMips;

    D3DImage* mSceneDepthZ;
    D3DImage* mLightingChannels;
    D3DImage* mSceneColors[2];
    D3DImage* mGBufferA;
    D3DImage* mGBufferB;
    D3DImage* mGBufferC;
    D3DImage* mToneMap;
    D3DImage* mShadowMaskTexture;
    //lumen
    D3DImage* mLumenCardCaptureAlbedoAtlas;
    D3DImage* mLumenCardCaptureNormalAtlas;
    D3DImage* mLumenCardCaptureEmissiveAtlas;
    D3DImage* mLumenCardCaptureDSAtlas;
    D3DImage* mLumenSceneDepth;
    D3DImage* mLumenSceneAlbedo;
    D3DImage* mLumenSceneOpacity;
    D3DImage* mLumenSceneNormal;
    D3DImage* mLumenSceneEmissive;
    //lumen scene lighting
    D3DImage* mLumenSceneDirectLighting;
    D3DImage* mLumenSceneIndirectLighting;
    D3DImage* mLumenSceneNumFramesAccumulatedAtlas;
    D3DImage* mLumenSceneFinalLighting;
    D3DImage* mLumenRadiosityTraceRadianceAtlas;
    D3DImage* mLumenRadiosityFilteredTraceRadianceAtlas;
    D3DImage* mLumenRadiosityProbeSHRedAtlas;
    D3DImage* mLumenRadiosityProbeSHGreenAtlas;
    D3DImage* mLumenRadiosityProbeSHBlueAtlas;
    D3DImage* mLumenRadianceCacheRadianceProbeAtlasTextureSource;
    D3DImage* mLumenRadianceCacheDepthProbeAtlasTexture;
    D3DImage* mLumenRadianceCacheFilteredRadianceProbeAtlasTexture;
    D3DImage* mLumenRadianceCacheFinalRadianceAtlas;
    D3DImage* mLumenScreenProbeGatherTraceHit;
    D3DImage* mLumenScreenProbeGatherTraceRadiance;
    D3DImage* mLumenScreenProbeGatherScreenProbeHitDistance;
    D3DImage* mLumenScreenProbeGatherScreenProbeTraceMoving;
    //lumen screen space
    D3DImage* mScreenProbeSceneDepth;
    D3DImage* mScreenProbeWorldSpeed;
    D3DImage* mScreenProbeWorldNormal;
    D3DImage* mScreenProbeTranslatedWorldPositions[2];
    D3DImage* mScreenTileAdapativeProbeHeader;
    D3DImage* mScreenTileAdapativeProbeIndicies;
    D3DResource* mAdaptiveScreenProbeData;
    D3DResource* mNumAdaptiveScreenProbe;
    D3DResource* mLumenScreenProbeGatherCompactedTraceTexelAllocator;
    D3DResource* mLumenScreenProbeGatherCompactedTraceTexelData;
    D3DImage* mLumenScreenProbeGatherLightingProbabilityDensityFunction;
    D3DImage* mLumenScreenProbeGatherScreenProbeRadiances[2];

    D3DImage* mStochasticLightingDepthHistorys[2];//no need to clear because always full screen render every frame

    
	std::vector<D3D12_INPUT_ELEMENT_DESC> mPosOnlyInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mQuadInputLayout;

    GlobalConstants mGlobalConstants;
    std::unique_ptr<UploadBuffer<GlobalConstants>> mObjectCB = nullptr;
    UINT mCbvOffset;

    std::unique_ptr<MeshGeometry> mCubeGeo = nullptr;
    std::unique_ptr<MeshGeometry> mScreenFullGeo = nullptr;
};

