
#include "LumenApp.h"
#include "D3DImage.h"

using namespace Math;

static int sCurrentMipLevelIndex = 9;
static unsigned int mipLevels[] = { 0,1,2,3,4,5,6,7,8,10 };

static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

//
//LumenApp
//
LumenApp::LumenApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

LumenApp::~LumenApp()
{
}

bool LumenApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc, nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    InitScene();
    BuildBuffers();
    BuildDescriptorHeaps();
    BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO();
    BuildScreenFullGeometry();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();


	return true;
}

static Mat4 sProjectionMatrix, sViewMatrix;
static Vec4 sCameraPositionHighWS, sCameraPositionLowWS, sViewDirection, sViewRight, sViewUp;

static Mat4 Perspective(float inAngle, float inAspect, float inNearZ)
{
    Mat4 res;
    memset(res.m, 0, sizeof(float) * 16);
    float angleInRadius = inAngle * 3.1415926f / 180.0f;
    float halfAngle = angleInRadius / 2.0f;
    float tanHalfAngle = tanf(halfAngle);

    res.a11 = 1.0f / tanHalfAngle;
    res.a22 = inAspect / tanHalfAngle;
    res.a43 = inNearZ;
    res.a34 = 1.0f;

    Vec4 f(-665.967, -255.75, 1303.276, 1);
    f = f * res;

    return res;
}

void LumenApp::InitScene()
{
    sProjectionMatrix = Perspective(90.0f, float(mClientWidth) / float(mClientHeight), 10.0f);
    sCameraPositionHighWS = Vec4(1059.769897f, -833.207886f, 336.560120f);
    sCameraPositionLowWS = Vec4(0.000052f, 0.000010f, 0.000009f);
    sViewDirection = Vec4(-0.766594410f, 0.638702452f, -0.0662739053f);
    sViewUp = Vec4(-0.0509171449f, 0.0424225703f, 0.997801483f);
    sViewRight = Vec4(-0.640109718f, -0.768283486f, 0.0f);

    sViewMatrix = Mat4::IDENTITY;
    sViewMatrix.SetX(sViewRight.x, sViewRight.y, sViewRight.z);
    sViewMatrix.SetY(sViewUp.x, sViewUp.y, sViewUp.z);
    sViewMatrix.SetZ(sViewDirection.x, sViewDirection.y, sViewDirection.z);
    sViewMatrix.Transpose();

    Mat4 clipToView = sProjectionMatrix.GetInversed();
    Mat4 viewToWorld = sViewMatrix.GetInversed();
    Mat4 screenToClip;

    screenToClip.a33 = sProjectionMatrix.a33;
    screenToClip.a34 = sProjectionMatrix.a34;
    screenToClip.a43 = sProjectionMatrix.a43;
    screenToClip.a44 = 0.0f;
    Mat4 clipToWorld = clipToView * viewToWorld;
    Mat4 screenToWorld = screenToClip * clipToWorld;
    Mat4 worldToClip = sViewMatrix * sProjectionMatrix;
    Mat4 modelMatrix0, modelMatrix1;
    modelMatrix0.a11 = 10.0f; modelMatrix0.a22 = 10.0f; modelMatrix0.a33 = 0.1f;
    modelMatrix1.a11 = 10.0f; modelMatrix1.a22 = 10.0f; modelMatrix1.a33 = 0.1f;
    modelMatrix1.a43 = 500.0f;
    mGlobalConstants.SetProjectionMatrix(sProjectionMatrix.m);
    mGlobalConstants.SetViewMatrix(sViewMatrix.m);
    mGlobalConstants.SetWorldToClipMatrix(worldToClip.m);
    mGlobalConstants.SetCameraPositionHighWS(sCameraPositionHighWS.x, sCameraPositionHighWS.y, sCameraPositionHighWS.z, 1.0f);
    mGlobalConstants.SetCameraPositionLowWS(sCameraPositionLowWS.x, sCameraPositionLowWS.y, sCameraPositionLowWS.z, 1.0f);
    mGlobalConstants.SetViewDirectionWS(sViewDirection.x, sViewDirection.y, sViewDirection.z);
    mGlobalConstants.SetViewRightWS(sViewRight.x, sViewRight.y, sViewRight.z);
    mGlobalConstants.SetViewUpWS(sViewUp.x, sViewUp.y, sViewUp.z);
    mGlobalConstants.SetScreenToTranslatedWorldMatrix(screenToWorld.m);
    memcpy(mGlobalConstants.mModelMatrices, &modelMatrix0, sizeof(modelMatrix0));
    memcpy(mGlobalConstants.mModelMatrices + 16, &modelMatrix1, sizeof(modelMatrix1));
    Mat4 itModelMatrix0 = modelMatrix0.GetInversed();
    itModelMatrix0.Transpose();
    Mat4 itModelMatrix1 = modelMatrix1.GetInversed();
    itModelMatrix1.Transpose();
    memcpy(mGlobalConstants.mITModelMatrices, &itModelMatrix0, sizeof(itModelMatrix0));
    memcpy(mGlobalConstants.mITModelMatrices + 16, &itModelMatrix1, sizeof(itModelMatrix1));
}

void LumenApp::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void LumenApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void LumenApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void LumenApp::OnResize()
{
	D3DApp::OnResize();
}

void LumenApp::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();
}

void LumenApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    mObjectCB->CopyData(0, mGlobalConstants);
}

void LumenApp::Draw(const GameTimer& gt)
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc, mPSOs["box"]));

    ID3D12DescriptorHeap* descriptorHeaps[] = { mDescriptorHeap };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Math::Color::WHITE.getPtr(), 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    {   // PreZ
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        mCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsRT);
        mCommandList->ClearDepthStencilView(dsRT, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

        mCommandList->SetGraphicsRootSignature(mRootSignatures["PreZ"]);
        mCommandList->SetPipelineState(mPSOs["PreZ"]);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->IASetVertexBuffers(0, 1, &mCubeGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&mCubeGeo->IndexBufferView());
        mCommandList->DrawIndexedInstanced(mCubeGeo->IndexCount, 2, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   // BasePass
        D3D12_RESOURCE_BARRIER barriers[5];
        barriers[0] = InitResourceBarrier(mSceneColor->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[1] = InitResourceBarrier(mGBufferA->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[2] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[3] = InitResourceBarrier(mGBufferC->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[4] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[4] = { mCPUViews["SceneColorRTV"], mCPUViews["GBufferARTV"], mCPUViews["GBufferBRTV"], mCPUViews["GBufferCRTV"] };
        D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        mCommandList->OMSetRenderTargets(4, colorRT, FALSE, &dsRT);
        //mCommandList->ClearDepthStencilView(dsRT, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

        mCommandList->SetGraphicsRootSignature(mRootSignatures["BasePass"]);
        mCommandList->SetPipelineState(mPSOs["BasePass"]);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW VBOView[2] = { mCubeGeo->VertexBufferView(),mCubeGeo->VertexBufferView2() };
        mCommandList->IASetVertexBuffers(0, 2, VBOView);
        mCommandList->IASetIndexBuffer(&mCubeGeo->IndexBufferView());
        mCommandList->DrawIndexedInstanced(mCubeGeo->IndexCount, 2, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mSceneColor->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mGBufferA->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[2] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[3] = InitResourceBarrier(mGBufferC->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[4] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

// 
// app define
//
void LumenApp::BuildScreenFullGeometry()
{
    {
        mCubeGeo = std::make_unique<MeshGeometry>();

        const int vertexCount = 54;

        const UINT vbByteSize = (UINT)vertexCount * sizeof(float) * 3;
        const UINT ibByteSize = (UINT)3 * sizeof(std::uint16_t);

        mCubeGeo->VertexBufferGPU = mCubePositionBuffer->mResource;
        mCubeGeo->VertexBufferGPU2 = mCubeAttributeBuffer->mResource;
        mCubeGeo->IndexBufferGPU = mCubeIndexBuffer->mResource;

        mCubeGeo->VertexByteStride = sizeof(float) * 3;
        mCubeGeo->VertexBufferByteSize = vbByteSize;
        mCubeGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
        mCubeGeo->IndexBufferByteSize = mCubeIndexBuffer->mResource->GetDesc().Width;

        //6 x 4 x 2 x 3 => 48 x 3 = 144
        mCubeGeo->IndexCount = 144;
    }
    {
        mScreenFullGeo = std::make_unique<MeshGeometry>();

        std::vector<float> vertexData = {
            -3.0f, -1.0f, 0.0f, -1.0f, 1.0f,
            1.0f, 3.0f, 0.0f, 1.0f, -1.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 1.0f
        };
        short indexData[3] = { 0, 1, 2 };

        const UINT vbByteSize = (UINT)vertexData.size() * sizeof(float);
        const UINT ibByteSize = (UINT)3 * sizeof(std::uint16_t);

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &mScreenFullGeo->VertexBufferCPU));
        CopyMemory(mScreenFullGeo->VertexBufferCPU->GetBufferPointer(), vertexData.data(), vbByteSize);

        ThrowIfFailed(D3DCreateBlob(ibByteSize, &mScreenFullGeo->IndexBufferCPU));
        CopyMemory(mScreenFullGeo->IndexBufferCPU->GetBufferPointer(), indexData, ibByteSize);

        mScreenFullGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, vertexData.data(), vbByteSize, mScreenFullGeo->VertexBufferUploader);

        mScreenFullGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, indexData, ibByteSize, mScreenFullGeo->IndexBufferUploader);

        mScreenFullGeo->VertexByteStride = sizeof(float) * 5;
        mScreenFullGeo->VertexBufferByteSize = vbByteSize;
        mScreenFullGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
        mScreenFullGeo->IndexBufferByteSize = ibByteSize;

        mScreenFullGeo->IndexCount = 3;
    }
}

void LumenApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

    mDxcByteCodes["PreZVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\PreZ.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["BasePassVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["BasePassPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["DirectionalLightingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["DirectionalLightingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["ShadowMaskCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ShadowMask.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["ToneMappingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["ToneMappingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"PS", L"ps_6_6");


	mPosOnlyInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
    mInputLayout =
	{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENTX",0,DXGI_FORMAT_R8G8B8A8_SNORM,1,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENTZ",0,DXGI_FORMAT_R8G8B8A8_SNORM,1,4,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void LumenApp::BuildPSO()
{
	{   //PreZ
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout =  { mPosOnlyInputLayout.data(), (uint)mPosOnlyInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["PreZ"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["PreZVS"]->GetBufferPointer()),
            mDxcByteCodes["PreZVS"]->GetBufferSize()
        };
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.DSVFormat = mSceneDepthZ->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.DepthClipEnable = true;
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["PreZ"])));
    }
    {   //BasePass
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["BasePass"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["BasePassVS"]->GetBufferPointer()),
            mDxcByteCodes["BasePassVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["BasePassPS"]->GetBufferPointer()),
            mDxcByteCodes["BasePassPS"]->GetBufferSize()
        };
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 4;
        psoDesc.RTVFormats[0] = mSceneColor->mRTVFormat;
        psoDesc.RTVFormats[1] = mGBufferA->mRTVFormat;
        psoDesc.RTVFormats[2] = mGBufferB->mRTVFormat;
        psoDesc.RTVFormats[3] = mGBufferC->mRTVFormat;
        psoDesc.DSVFormat = mSceneDepthZ->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.DepthClipEnable = true;
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["BasePass"])));
    }
}

static void CreateTexture2DDSV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, D3DImage* inDSRT, D3D12_DSV_FLAGS inFlags=D3D12_DSV_FLAG_NONE)
{
    D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescDSV = {};
    d3dDescriptorHeapDescDSV.NumDescriptors = 1;
    d3dDescriptorHeapDescDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSViewDesc = {};
    d3dDSViewDesc.Format = inDSRT->mDSVFormat;
    d3dDSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    d3dDSViewDesc.Flags = inFlags;

    pDevice->CreateDepthStencilView(inDSRT->mResource, &d3dDSViewDesc, inMemory);
}

static void CreateTexture2DUAV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat, int inMipMapLevel)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = inFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = inMipMapLevel;
    pDevice->CreateUnorderedAccessView(inResource, nullptr, &uavDesc, inMemory);
}

static void CreateTexture2DSRV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat, int inMipMapLevel)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    pDevice->CreateShaderResourceView(inResource, &srvDesc, inMemory);
}

static void CreateTexture2DRTV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat)
{
    D3D12_RENDER_TARGET_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    pDevice->CreateRenderTargetView(inResource, &srvDesc, inMemory);
}

static void CreateRWStructuredBufferUAV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, int inArraySize, int inElementSize, DXGI_FORMAT inFormat = DXGI_FORMAT_UNKNOWN)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = inFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Buffer.NumElements = inArraySize;
    uavDesc.Buffer.StructureByteStride = inElementSize;
    uavDesc.Buffer.FirstElement = 0;
    pDevice->CreateUnorderedAccessView(inResource, nullptr, &uavDesc, inMemory);
}

static void CreateRWByteAddressBufferUAV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = inFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    uavDesc.Buffer.NumElements = inResource->GetDesc().Width / 4;
    uavDesc.Buffer.StructureByteStride = 0;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    pDevice->CreateUnorderedAccessView(inResource, nullptr, &uavDesc, inMemory);
}

static void CreateRWByteAddressBufferSRV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvDesc.Buffer.NumElements = inResource->GetDesc().Width / 4;
    srvDesc.Buffer.StructureByteStride = 0;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    pDevice->CreateShaderResourceView(inResource, &srvDesc, inMemory);
}

void LumenApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 200;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mDescriptorHeap)));

	int viewCount = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, mCbvSrvDescriptorSize);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mSceneColor->mResource, mSceneColor->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferA->mResource, mGBufferA->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferB->mResource, mGBufferB->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferC->mResource, mGBufferC->mSRVFormat, 0);
    viewCount = 0;
    mCPUViews["SceneColorSRV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
    mCPUViews["GBufferASRV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
    mCPUViews["GBufferBSRV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
    mCPUViews["GBufferCSRV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);

    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mSceneColor->mResource, mSceneColor->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferA->mResource, mGBufferA->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferB->mResource, mGBufferB->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferC->mResource, mGBufferC->mRTVFormat);
    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    mCPUViews["SceneColorRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferARTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferBRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferCRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 100;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(&mDsvDescriptorHeap)));

    viewCount = 0;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mDsvDescriptorSize);
    CreateTexture2DDSV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize), mSceneDepthZ);

    viewCount = 0;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mDsvDescriptorSize);
    mCPUViews["SceneDepthZDSV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
}

void LumenApp::BuildConstantBuffers()
{
    mObjectCB = std::make_unique<UploadBuffer<GlobalConstants>>(md3dDevice, 1, true);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(GlobalConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int CBufIndex = 0;
    cbAddress += CBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(GlobalConstants));

    mCbvOffset = 100;
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCbvCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        hCbvCpuDescriptor);
}

void LumenApp::BuildRootSignature()
{
    {   //PreZ
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[1];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ID3DBlob* serializedRootSig = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig, &errorBlob);

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignatures["PreZ"])));
    }
    {   //BasePass
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[1];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ID3DBlob* serializedRootSig = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig, &errorBlob);

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignatures["BasePass"])));
    }
}

D3DResource* LumenApp::InitBufferFromFile(const wchar_t* resname, const char* file)
{
    size_t fileSize = 0;
    unsigned char* fileContent = d3dUtil::LoadFileContent(file, fileSize);
    D3DResource* resource = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
    ID3D12Resource* uploadBuffer = nullptr;
    D3DResource* res = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
    res->mResource = d3dUtil::CreateDefaultBuffer(
        md3dDevice,
        mCommandList,
        fileContent,
        fileSize,
        uploadBuffer);
    res->mResource->SetName(resname);
    return res;
}

void LumenApp::BuildBuffers()
{
    int bufferWidth = ((mClientWidth + 8 - 1) / 8) * 8;//960
    int bufferHeight = mClientHeight;// ((inCanvasHeight + 7) / 8) * 8;//544
    {	//buffer
        mCubePositionBuffer = InitBufferFromFile(L"CubePosition", "Res/PositionOnly.data");
        mCubeIndexBuffer = InitBufferFromFile(L"CubeIndex", "Res/IndexBuffer.data");
        mCubeAttributeBuffer = InitBufferFromFile(L"CubeAttribute", "Res/TangentAndNormal.data");
        mDFSceneObject = InitBufferFromFile(L"DistanceFields.DFObjectData", "Res/DistanceFields.DFObjectData.data");
    }
    {
        //(8x8)work group
            //depth32stencil8,revert z
        mSceneDepthZ = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, {0.0f,0} });
        mSceneDepthZ->mResource->SetName(L"SceneDepthZ");
        mSceneColor = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mSceneColor->mResource->SetName(L"SceneColor");
        mGBufferA = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
            DXGI_FORMAT_R10G10B10A2_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R10G10B10A2_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferA->mResource->SetName(L"GBufferA");
        mGBufferB = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_B8G8R8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferB->mResource->SetName(L"GBufferB");
        mGBufferC = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferC->mResource->SetName(L"GBufferC");
        mToneMap = Init2DRTImage(md3dDevice, mCommandList, mClientWidth, mClientHeight, 0,
            DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
            DXGI_FORMAT_R10G10B10A2_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R10G10B10A2_UNORM, {0.0f,0.0f,0.0f,1.0f} });
        mToneMap->mResource->SetName(L"ToneMap");
        mShadowMaskTexture = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_B8G8R8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mShadowMaskTexture->mResource->SetName(L"ShadowMaskTexture");
    }
}
