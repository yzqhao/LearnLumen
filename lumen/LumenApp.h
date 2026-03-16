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

    D3DImage* mSceneDepthZ;
    D3DImage* mSceneColor;
    D3DImage* mGBufferA;
    D3DImage* mGBufferB;
    D3DImage* mGBufferC;
    D3DImage* mToneMap;
    D3DImage* mShadowMaskTexture;

    
	std::vector<D3D12_INPUT_ELEMENT_DESC> mPosOnlyInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mQuadInputLayout;

    GlobalConstants mGlobalConstants;
    std::unique_ptr<UploadBuffer<GlobalConstants>> mObjectCB = nullptr;
    UINT mCbvOffset;

    std::unique_ptr<MeshGeometry> mCubeGeo = nullptr;
    std::unique_ptr<MeshGeometry> mScreenFullGeo = nullptr;
};

