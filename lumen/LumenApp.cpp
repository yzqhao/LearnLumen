
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
    modelMatrix0.a11 = 10.0f; modelMatrix0.a22 = 10.0f; modelMatrix0.a33 = 0.10009765625f;
    //Surface Cache -> depth : uint4 RotateScale -> instance data
    //sdf -> 
    modelMatrix1.a11 = 10.0f; modelMatrix1.a22 = 10.0f; modelMatrix1.a33 = 0.10009765625f;
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

    // mesh card
    mGlobalConstants.mCameraPositionMeshCardCapture[0] = -550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[1] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[2] = 500.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[3] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[4] = 550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[5] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[6] = 500.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[7] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[8] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[9] = -550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[10] = 500.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[11] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[12] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[13] = 550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[14] = 500.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[15] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[16] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[17] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[18] = 494.5f;
    mGlobalConstants.mCameraPositionMeshCardCapture[19] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[20] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[21] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[22] = 505.5f;
    mGlobalConstants.mCameraPositionMeshCardCapture[23] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[24] = -550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[25] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[26] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[27] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[28] = 550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[29] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[30] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[31] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[32] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[33] = -550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[34] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[35] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[36] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[37] = 550.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[38] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[39] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[40] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[41] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[42] = -5.5f;
    mGlobalConstants.mCameraPositionMeshCardCapture[43] = 1.0f;

    mGlobalConstants.mCameraPositionMeshCardCapture[44] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[45] = 0.0f;
    mGlobalConstants.mCameraPositionMeshCardCapture[46] = 5.5f;
    mGlobalConstants.mCameraPositionMeshCardCapture[47] = 1.0f;

    Mat4 viewMatrixMeshCardCaptrue0 = {
                0.0f,0.0f,1.0f,0.0f,
                1.0f,0.0f,0.0f,0.0f,
                0.0f,1.0f,0.0f,0.0f,
                0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture, &viewMatrixMeshCardCaptrue0, sizeof(viewMatrixMeshCardCaptrue0));
    Mat4 viewMatrixMeshCardCaptrue1 = {
        0.0f,0.0f,-1.0f,0.0f,
        -1.0f,0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture + 16, &viewMatrixMeshCardCaptrue1, sizeof(viewMatrixMeshCardCaptrue1));
    Mat4 viewMatrixMeshCardCaptrue2 = {
        -1.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture + 32, &viewMatrixMeshCardCaptrue2, sizeof(viewMatrixMeshCardCaptrue1));
    Mat4 viewMatrixMeshCardCaptrue3 = {
        1.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,-1.0f,0.0f,
        0.0f,1.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture + 48, &viewMatrixMeshCardCaptrue3, sizeof(viewMatrixMeshCardCaptrue1));
    Mat4 viewMatrixMeshCardCaptrue4 = {
        1.0f,0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,0.0f,
        0.0f,0.0f,1.0f,0.0f,
        0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture + 64, &viewMatrixMeshCardCaptrue4, sizeof(viewMatrixMeshCardCaptrue1));
    Mat4 viewMatrixMeshCardCaptrue5 = {
        -1.0f,0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,0.0f,
        0.0f,0.0f,-1.0f,0.0f,
        0.0f,0.0f,0.0f,1.0f
    };
    memcpy(mGlobalConstants.mViewMatrixMeshCardCapture + 80, &viewMatrixMeshCardCaptrue5, sizeof(viewMatrixMeshCardCaptrue1));
    Mat4 projectionMatrixMeshCardCapture0 = {
        0.002f,0.0f,0.0f,0.0f,
        0.0f,0.2f,0.0f,0.0f,
        0.0f,0.0f,-0.005f,0.0f,
        0.0f,0.0f,1.0f,1.0f
    };
    memcpy(mGlobalConstants.mProjectionMatrixMeshCardCapture, &projectionMatrixMeshCardCapture0, sizeof(projectionMatrixMeshCardCapture0));

    Mat4 projectionMatrixMeshCardCapture1 = {
        0.002f,0.0f,0.0f,0.0f,
        0.0f,0.002f,0.0f,0.0f,
        0.0f,0.0f,-0.5f,0.0f,
        0.0f,0.0f,1.0f,1.0f
    };
    memcpy(mGlobalConstants.mProjectionMatrixMeshCardCapture + 16, &projectionMatrixMeshCardCapture1, sizeof(projectionMatrixMeshCardCapture1));
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
        SCOPED_EVENT(mCommandList, L"PreZ");
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
    {   //ClearCardCapture
        auto ExecuteMeshCardCapturePass = [&]() {
            SCOPED_EVENT(mCommandList, L"ClearCardCapturePass");
            mObjectCB->CopyData(0, mGlobalConstants);   // ����const buffer

            D3D12_RESOURCE_BARRIER barriers[4];
            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            D3D12_CPU_DESCRIPTOR_HANDLE colorRT[3] = { mCPUViews["LumenCardCaptureAlbedoAtlasRTV"], mCPUViews["LumenCardCaptureNormalAtlasRTV"], mCPUViews["LumenCardCaptureEmissiveAtlasRTV"] };
            D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
            mCommandList->OMSetRenderTargets(3, colorRT, FALSE, &dsRT);
            float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };

            D3D12_RECT scissor = { 0,0,512,512 };
            D3D12_VIEWPORT viewport = {
                0.0f,0.0f,512.0f,512.0f,0.0f,1.0f
            };

            mCommandList->SetGraphicsRootSignature(mRootSignatures["ClearCardCapture"]);
            mCommandList->SetPipelineState(mPSOs["ClearCardCapture"]);

            mCommandList->RSSetViewports(1, &viewport);
            mCommandList->RSSetScissorRects(1, &scissor);

            const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            mCommandList->OMSetBlendFactor(blendFactor);
            mCommandList->OMSetStencilRef(0x84);

            //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
            //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
            mCommandList->SetGraphicsRootShaderResourceView(0, mClearCardBuffer->mResource->GetGPUVirtualAddress());

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            };
        SCOPED_EVENT(mCommandList, L"ClearCardPass");
        ExecuteMeshCardCapturePass();
    }
    {   //MeshCardCapturePass
        auto ExecuteMeshCardCapturePass = [&](int inIndex) {
            SCOPED_EVENT(mCommandList, L"MeshCardCapturePass");
            mObjectCB->CopyData(0, mGlobalConstants);   // ����const buffer

            D3D12_RESOURCE_BARRIER barriers[4];
            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            if (inIndex == 0) {
                //mCommandList->DiscardResource(mLumenCardCaptureAlbedoAtlas->mResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureNormalAtlas->mResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureEmissiveAtlas->mResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureDSAtlas->mResource, nullptr);
            }

            D3D12_CPU_DESCRIPTOR_HANDLE colorRT[3] = { mCPUViews["LumenCardCaptureAlbedoAtlasRTV"], mCPUViews["LumenCardCaptureNormalAtlasRTV"], mCPUViews["LumenCardCaptureEmissiveAtlasRTV"] };
            D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
            mCommandList->OMSetRenderTargets(3, colorRT, FALSE, &dsRT);
            float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };

            static const D3D12_VIEWPORT sViewports[] = {
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f},
                {0,0,64.0f,8.0f,0.0f,1.0f}
            };
            static const D3D12_RECT sScissors[] = {
                { 0,0,64,8 },
                { 128, 0, 128, 8 },
                { 128, 8, 128, 8 },
                { 64, 0, 64, 8 },
                { 256, 0, 128, 128 },
                { 384, 0, 128, 128 },

                { 0, 8, 64, 8 },
                { 128, 16, 128, 8 },
                { 128, 24, 128, 8 },
                { 64, 8, 64, 8 },
                { 0, 128, 128, 128 },
                { 128, 128, 128, 128 },
            };

            mCommandList->SetGraphicsRootSignature(mRootSignatures["MeshCardCapture"]);
            mCommandList->SetPipelineState(mPSOs["MeshCardCapture"]);

            D3D12_RECT scissor = sScissors[inIndex];
            D3D12_VIEWPORT viewport = {
                float(scissor.left),float(scissor.top),float(scissor.right),float(scissor.bottom),0.0f,1.0f
            };
            scissor.right += scissor.left;
            scissor.bottom += scissor.top;
            mCommandList->RSSetViewports(1, &viewport);
            mCommandList->RSSetScissorRects(1, &scissor);

            const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            mCommandList->OMSetBlendFactor(blendFactor);
            mCommandList->OMSetStencilRef(0x84);

            CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
            mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
            mCommandList->SetGraphicsRoot32BitConstants(1, 4, &mMisc, 0);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3D12_VERTEX_BUFFER_VIEW VBOView[2] = { mCubeGeo->VertexBufferView(),mCubeGeo->VertexBufferView2() };
            mCommandList->IASetVertexBuffers(0, 2, VBOView);
            mCommandList->IASetIndexBuffer(&mCubeGeo->IndexBufferView());
            mCommandList->DrawIndexedInstanced(mCubeGeo->IndexCount, 1, 0, 0, 0);

            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            };
        SCOPED_EVENT(mCommandList, L"UpdateSurfaceCache");
        mMisc.misc[0] = 0;//camera position
        mMisc.misc[1] = 0;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(0);

        mMisc.misc[0] = 1;//camera position
        mMisc.misc[1] = 1;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(1);

        mMisc.misc[0] = 2;//camera position
        mMisc.misc[1] = 2;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(2);

        mMisc.misc[0] = 3;//camera position
        mMisc.misc[1] = 3;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(3);

        mMisc.misc[0] = 4;//camera position
        mMisc.misc[1] = 4;//view matrix
        mMisc.misc[2] = 1;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(4);

        mMisc.misc[0] = 5;//camera position
        mMisc.misc[1] = 5;//view matrix
        mMisc.misc[2] = 1;//projection matrix
        mMisc.misc[3] = 1;//model matrix
        ExecuteMeshCardCapturePass(5);
        //bottom cube
        mMisc.misc[0] = 6;//camera position
        mMisc.misc[1] = 0;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(6);

        mMisc.misc[0] = 7;//camera position
        mMisc.misc[1] = 1;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(7);

        mMisc.misc[0] = 8;//camera position
        mMisc.misc[1] = 2;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(8);

        mMisc.misc[0] = 9;//camera position
        mMisc.misc[1] = 3;//view matrix
        mMisc.misc[2] = 0;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(9);

        mMisc.misc[0] = 10;//camera position
        mMisc.misc[1] = 4;//view matrix
        mMisc.misc[2] = 1;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(10);

        mMisc.misc[0] = 11;//camera position
        mMisc.misc[1] = 5;//view matrix
        mMisc.misc[2] = 1;//projection matrix
        mMisc.misc[3] = 0;//model matrix
        ExecuteMeshCardCapturePass(11);
    }
    {   //CopyToSurfaceCacheDepth
        SCOPED_EVENT(mCommandList, L"CopyToSurfaceCacheDepth");

        D3D12_RESOURCE_BARRIER barriers[3];
        barriers[0] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[2] = InitResourceBarrier(mLumenSceneDepth->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneDepthRTV"] };
        //D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
        mCommandList->OMSetRenderTargets(1, colorRT, FALSE, nullptr);

        D3D12_RECT scissor = { 0,0,4096,4096 };
        D3D12_VIEWPORT viewport = {
            0.0f,0.0f,4096,4096,0.0f,1.0f
        };

        mCommandList->SetGraphicsRootSignature(mRootSignatures["CopyToSurfaceCacheDepth"]);
        mCommandList->SetPipelineState(mPSOs["CopyToSurfaceCacheDepth"]);

        mCommandList->RSSetViewports(1, &viewport);
        mCommandList->RSSetScissorRects(1, &scissor);
        float clearColor[] = { 1.0f,0.0f,0.0f,1.0f };
        mCommandList->ClearRenderTargetView(colorRT[0],
            clearColor, 0, nullptr);

        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        mCommandList->OMSetBlendFactor(blendFactor);
        mCommandList->OMSetStencilRef(0x84);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureNormalAtlasSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenCardCaptureDSAtlasSRV"]);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawInstanced(6, 12, 0, 0);

        barriers[0] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[2] = InitResourceBarrier(mLumenSceneDepth->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //CompressToSurfaceCacheAlbedo
        SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheAlbedo");

        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        //D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneDepthRTV"] };
        //D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
        mCommandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

        D3D12_RECT scissor = { 0,0,1024,1024 };
        D3D12_VIEWPORT viewport = {
            0.0f,0.0f,1024,1024,0.0f,1.0f
        };

        mCommandList->SetGraphicsRootSignature(mRootSignatures["CompressToSurfaceCacheAlbedo"]);
        mCommandList->SetPipelineState(mPSOs["CompressToSurfaceCacheAlbedo"]);

        mCommandList->RSSetViewports(1, &viewport);
        mCommandList->RSSetScissorRects(1, &scissor);

        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        mCommandList->OMSetBlendFactor(blendFactor);
        mCommandList->OMSetStencilRef(0x84);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureAlbedoAtlasSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneAlbedoUAV"]);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawInstanced(6, 12, 0, 0);

        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //CopyToSurfaceCacheOpacity
        SCOPED_EVENT(mCommandList, L"CopyToSurfaceCacheOpacity");

        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mLumenSceneOpacity->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneOpacityRTV"] };
        mCommandList->OMSetRenderTargets(1, colorRT, FALSE, nullptr);
        float clearColor[] = { 1.0f,0.0f,0.0f,1.0f };
        mCommandList->ClearRenderTargetView(colorRT[0],
            clearColor, 0, nullptr);

        D3D12_RECT scissor = { 0,0,4096,4096 };
        D3D12_VIEWPORT viewport = {
            0.0f,0.0f,4096,4096,0.0f,1.0f
        };

        mCommandList->SetGraphicsRootSignature(mRootSignatures["CopyToSurfaceCacheOpacity"]);
        mCommandList->SetPipelineState(mPSOs["CopyToSurfaceCacheOpacity"]);

        mCommandList->RSSetViewports(1, &viewport);
        mCommandList->RSSetScissorRects(1, &scissor);

        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        mCommandList->OMSetBlendFactor(blendFactor);
        mCommandList->OMSetStencilRef(0x84);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureAlbedoAtlasSRV"]);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawInstanced(6, 12, 0, 0);

        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mLumenSceneOpacity->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //CompressToSurfaceCacheNormal
        SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheNormal");

        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        //D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneDepthRTV"] };
        //D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
        mCommandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

        D3D12_RECT scissor = { 0,0,1024,1024 };
        D3D12_VIEWPORT viewport = {
            0.0f,0.0f,1024,1024,0.0f,1.0f
        };

        mCommandList->SetGraphicsRootSignature(mRootSignatures["CompressToSurfaceCacheAlbedo"]);
        mCommandList->SetPipelineState(mPSOs["CompressToSurfaceCacheNormal"]);

        mCommandList->RSSetViewports(1, &viewport);
        mCommandList->RSSetScissorRects(1, &scissor);

        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        mCommandList->OMSetBlendFactor(blendFactor);
        mCommandList->OMSetStencilRef(0x84);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureNormalAtlasSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneNormalUAV"]);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawInstanced(6, 12, 0, 0);

        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //CompressToSurfaceCacheEmissive
        SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheEmissive");

        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        //D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneDepthRTV"] };
        //D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mCPUViews["LumenCardCaptureDSAtlasDSV"];
        mCommandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

        D3D12_RECT scissor = { 0,0,1024,1024 };
        D3D12_VIEWPORT viewport = {
            0.0f,0.0f,1024,1024,0.0f,1.0f
        };

        mCommandList->SetGraphicsRootSignature(mRootSignatures["CompressToSurfaceCacheAlbedo"]);
        mCommandList->SetPipelineState(mPSOs["CompressToSurfaceCacheEmissive"]);

        mCommandList->RSSetViewports(1, &viewport);
        mCommandList->RSSetScissorRects(1, &scissor);

        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        mCommandList->OMSetBlendFactor(blendFactor);
        mCommandList->OMSetStencilRef(0x84);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        //mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mResource->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureEmissiveAtlasSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneEmissiveUAV"]);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawInstanced(6, 12, 0, 0);

        barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mLumenSceneAlbedo->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   // BasePass
        SCOPED_EVENT(mCommandList, L"BasePass");
        D3D12_RESOURCE_BARRIER barriers[5];
        barriers[0] = InitResourceBarrier(mSceneColor->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[1] = InitResourceBarrier(mGBufferA->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[2] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[3] = InitResourceBarrier(mGBufferC->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[4] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[4] = { mCPUViews["SceneColorRTV"], mCPUViews["GBufferARTV"], mCPUViews["GBufferBRTV"], mCPUViews["GBufferCRTV"] };
        D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        mCommandList->OMSetRenderTargets(4, colorRT, FALSE, &dsRT);
        float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
        mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);
        clearColor[3] = 0.0f;
        mCommandList->ClearRenderTargetView(colorRT[1], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[2], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[3], clearColor, 0, nullptr);
        //mCommandList->ClearDepthStencilView(dsRT, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

        // Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
        mCommandList->RSSetViewports(1, &mScreenViewport);
        mCommandList->RSSetScissorRects(1, &mScissorRect);

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
        barriers[4] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //ShadowMask
        SCOPED_EVENT(mCommandList, L"ShadowMask");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mShadowMaskTexture->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        mCommandList->SetPipelineState(mPSOs["ShadowMask"]);
        mCommandList->SetComputeRootSignature(mRootSignatures["ShadowMask"]);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        mCommandList->SetComputeRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["SceneDepthZSRV"]);
        mCommandList->SetComputeRootShaderResourceView(2, mDFSceneObject->mResource->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["ShadowMaskTextureUAV"]);

        mCommandList->Dispatch(120, 68, 1);

        barriers[0] = InitResourceBarrier(mShadowMaskTexture->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //DirectionalLighting
        SCOPED_EVENT(mCommandList, L"DirectionalLighting");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mSceneColor->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT = mCPUViews["SceneColorRTV"];
        mCommandList->OMSetRenderTargets(1, &colorRT, FALSE, nullptr);

        mCommandList->SetPipelineState(mPSOs["DirectionalLighting"]);
        mCommandList->SetGraphicsRootSignature(mRootSignatures["DirectionalLighting"]);

        mCommandList->IASetVertexBuffers(0, 1, &mScreenFullGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&mScreenFullGeo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetGraphicsRootDescriptorTable(1, mGPUViews["GBufferASRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["GBufferBSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["GBufferCSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(4, mGPUViews["SceneDepthZSRV"]);
        mCommandList->SetGraphicsRootDescriptorTable(5, mGPUViews["ShadowMaskTextureSRV"]);

        mCommandList->DrawIndexedInstanced(mScreenFullGeo->IndexCount, 1, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mSceneColor->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //LumenSceneLighting
        SCOPED_EVENT(mCommandList, L"LumenSceneLighting");
        {   //DirectLighting
            SCOPED_EVENT(mCommandList, L"DirectLighting");
            D3D12_RESOURCE_BARRIER barriers[3];
            barriers[0] = InitResourceBarrier(mLumenSceneNormal->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[1] = InitResourceBarrier(mLumenSceneDepth->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[2] = InitResourceBarrier(mLumenSceneDirectLighting->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            barriers[2] = InitResourceBarrier(mLumenSceneFinalLighting->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            mCommandList->SetPipelineState(mPSOs["DirectLighting"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["DirectLighting"]);

            CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
            mCommandList->SetComputeRootDescriptorTable(0, hCbvGpuDescriptor);
            mCommandList->SetComputeRootShaderResourceView(1, mRectDataBuffer->mResource->GetGPUVirtualAddress());
            mCommandList->SetComputeRootShaderResourceView(2, mDFSceneObject->mResource->GetGPUVirtualAddress());
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["LumenSceneNormalSRV"]);   //mLumenSceneNormal->mResource->GetGPUVirtualAddress()
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["LumenSceneDepthSRV"]);   //mLumenSceneDepth->mResource->GetGPUVirtualAddress()
            mCommandList->SetComputeRootShaderResourceView(5, mLumenCards->mResource->GetGPUVirtualAddress());
            mCommandList->SetComputeRootDescriptorTable(6, mGPUViews["LumenSceneDirectLightingUAV"]);
            mCommandList->SetComputeRootDescriptorTable(7, mGPUViews["LumenSceneFinalLightingUAV"]);

            mCommandList->Dispatch(12, 1, 1);

            barriers[0] = InitResourceBarrier(mLumenSceneNormal->mResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[1] = InitResourceBarrier(mLumenSceneDepth->mResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenSceneDirectLighting->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenSceneFinalLighting->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);
        }
    }
    {   //ToneMap
        SCOPED_EVENT(mCommandList, L"ToneMap");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mToneMap->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT = mCPUViews["ToneMapRTV"];
        mCommandList->OMSetRenderTargets(1, &colorRT, FALSE, nullptr);

        mCommandList->SetPipelineState(mPSOs["ToneMap"]);
        mCommandList->SetGraphicsRootSignature(mRootSignatures["ToneMap"]);

        mCommandList->IASetVertexBuffers(0, 1, &mScreenFullGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&mScreenFullGeo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        mCommandList->SetGraphicsRootDescriptorTable(0, mGPUViews["SceneColorSRV"]);

        mCommandList->DrawIndexedInstanced(mScreenFullGeo->IndexCount, 1, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mToneMap->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
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
    mDxcByteCodes["ClearCardCaptureVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ClearCardCapture.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["ClearCardCapturePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ClearCardCapture.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["MeshCardCaptureVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\MeshCardCapture.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["MeshCardCapturePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\MeshCardCapture.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CopyToSurfaceCacheDepthVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CopyToSurfaceCacheDepth.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CopyToSurfaceCacheDepthPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CopyToSurfaceCacheDepth.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheAlbedoVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheAlbedo.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheAlbedoPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheAlbedo.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheNormalVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheNormal.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheNormalPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheNormal.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheEmissiveVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheEmissive.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheEmissivePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CompressToSurfaceCacheEmissive.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CopyToSurfaceCacheOpacityVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CopyToSurfaceCacheOpacity.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CopyToSurfaceCacheOpacityPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\CopyToSurfaceCacheOpacity.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["BasePassVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["BasePassPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["DirectionalLightingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["DirectionalLightingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["ShadowMaskCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ShadowMask.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["ToneMappingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["ToneMappingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["DirectLightingCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectLighting.hlsl", nullptr, 0, L"CS", L"cs_6_6");


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
    mQuadInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
    {   //ShadowMask
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["ShadowMask"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ShadowMaskCS"]->GetBufferPointer()),
            mDxcByteCodes["ShadowMaskCS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["ShadowMask"])));
    }
    {   //DirectionalLighting
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mQuadInputLayout.data(), (uint)mQuadInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["DirectionalLighting"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["DirectionalLightingVS"]->GetBufferPointer()),
            mDxcByteCodes["DirectionalLightingVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["DirectionalLightingPS"]->GetBufferPointer()),
            mDxcByteCodes["DirectionalLightingPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mSceneColor->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["DirectionalLighting"])));
    }
    {   //ToneMap
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mQuadInputLayout.data(), (uint)mQuadInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["ToneMap"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ToneMappingVS"]->GetBufferPointer()),
            mDxcByteCodes["ToneMappingVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ToneMappingPS"]->GetBufferPointer()),
            mDxcByteCodes["ToneMappingPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mToneMap->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["ToneMap"])));
    }
    {   //MeshCardCapture
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["MeshCardCapture"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["MeshCardCaptureVS"]->GetBufferPointer()),
            mDxcByteCodes["MeshCardCaptureVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["MeshCardCapturePS"]->GetBufferPointer()),
            mDxcByteCodes["MeshCardCapturePS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 3;
        psoDesc.RTVFormats[0] = mLumenCardCaptureAlbedoAtlas->mRTVFormat;
        psoDesc.RTVFormats[1] = mLumenCardCaptureNormalAtlas->mRTVFormat;
        psoDesc.RTVFormats[2] = mLumenCardCaptureEmissiveAtlas->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = mLumenCardCaptureDSAtlas->mRTVFormat;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["MeshCardCapture"])));
    }
    {   //ClearCardCapture
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["ClearCardCapture"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ClearCardCaptureVS"]->GetBufferPointer()),
            mDxcByteCodes["ClearCardCaptureVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ClearCardCapturePS"]->GetBufferPointer()),
            mDxcByteCodes["ClearCardCapturePS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 3;
        psoDesc.RTVFormats[0] = mLumenCardCaptureAlbedoAtlas->mRTVFormat;
        psoDesc.RTVFormats[1] = mLumenCardCaptureNormalAtlas->mRTVFormat;
        psoDesc.RTVFormats[2] = mLumenCardCaptureEmissiveAtlas->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = mLumenCardCaptureDSAtlas->mRTVFormat;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["ClearCardCapture"])));
    }
    {   //CopyToSurfaceCacheDepth
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["CopyToSurfaceCacheDepth"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CopyToSurfaceCacheDepthVS"]->GetBufferPointer()),
            mDxcByteCodes["CopyToSurfaceCacheDepthVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CopyToSurfaceCacheDepthPS"]->GetBufferPointer()),
            mDxcByteCodes["CopyToSurfaceCacheDepthPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mLumenSceneDepth->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["CopyToSurfaceCacheDepth"])));
    }
    {   //CompressToSurfaceCacheAlbedo
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["CompressToSurfaceCacheAlbedo"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheAlbedoVS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheAlbedoVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheAlbedoPS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheAlbedoPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["CompressToSurfaceCacheAlbedo"])));
    }
    {   //CompressToSurfaceCacheNormal
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["CompressToSurfaceCacheAlbedo"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheNormalVS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheNormalVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheNormalPS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheNormalPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["CompressToSurfaceCacheNormal"])));
    }
    {   //CompressToSurfaceCacheEmissive
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["CompressToSurfaceCacheAlbedo"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheEmissiveVS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheEmissiveVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CompressToSurfaceCacheEmissivePS"]->GetBufferPointer()),
            mDxcByteCodes["CompressToSurfaceCacheEmissivePS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["CompressToSurfaceCacheEmissive"])));
    }
    {   //CopyToSurfaceCacheOpacity
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["CopyToSurfaceCacheOpacity"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CopyToSurfaceCacheOpacityVS"]->GetBufferPointer()),
            mDxcByteCodes["CopyToSurfaceCacheOpacityVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["CopyToSurfaceCacheOpacityPS"]->GetBufferPointer()),
            mDxcByteCodes["CopyToSurfaceCacheOpacityPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mLumenSceneOpacity->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["CopyToSurfaceCacheOpacity"])));
    }
    {   //DirectLighting
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["DirectLighting"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["DirectLightingCS"]->GetBufferPointer()),
            mDxcByteCodes["DirectLightingCS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["DirectLighting"])));
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
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mSceneColor->mResource, mSceneColor->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferA->mResource, mGBufferA->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferB->mResource, mGBufferB->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferC->mResource, mGBufferC->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mSceneDepthZ->mResource, mSceneDepthZ->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mShadowMaskTexture->mResource, mShadowMaskTexture->mFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mShadowMaskTexture->mResource, mShadowMaskTexture->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureAlbedoAtlas->mResource, mLumenCardCaptureAlbedoAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureNormalAtlas->mResource, mLumenCardCaptureNormalAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureEmissiveAtlas->mResource, mLumenCardCaptureEmissiveAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureDSAtlas->mResource, mLumenCardCaptureDSAtlas->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneAlbedo->mResource, mLumenSceneAlbedo->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneNormal->mResource, mLumenSceneNormal->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneEmissive->mResource, mLumenSceneEmissive->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneDirectLighting->mResource, mLumenSceneDirectLighting->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneFinalLighting->mResource, mLumenSceneFinalLighting->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneDepth->mResource, mLumenSceneDepth->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneNormal->mResource, mLumenSceneNormal->mSRVFormat, 0);

    mCPUViews["SceneColorSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GBufferASRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GBufferBSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GBufferCSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["SceneDepthZSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ShadowMaskTextureUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ShadowMaskTextureSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardCaptureAlbedoAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardCaptureNormalAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardCaptureEmissiveAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardCaptureDSAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneAlbedoUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneNormalUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneEmissiveUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneDirectLightingUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneFinalLightingUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneDepthSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneNormalSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);

    mGPUViews["SceneColorSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GBufferASRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GBufferBSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GBufferCSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["SceneDepthZSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ShadowMaskTextureUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ShadowMaskTextureSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardCaptureAlbedoAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardCaptureNormalAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardCaptureEmissiveAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardCaptureDSAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneAlbedoUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneNormalUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneEmissiveUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneDirectLightingUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneFinalLightingUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneDepthSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneNormalSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);

    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    hCpuDescriptor.Offset(SwapChainBufferCount, mRtvDescriptorSize);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mSceneColor->mResource, mSceneColor->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferA->mResource, mGBufferA->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferB->mResource, mGBufferB->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferC->mResource, mGBufferC->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mToneMap->mResource, mToneMap->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureAlbedoAtlas->mResource, mLumenCardCaptureAlbedoAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureNormalAtlas->mResource, mLumenCardCaptureNormalAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureEmissiveAtlas->mResource, mLumenCardCaptureEmissiveAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneDepth->mResource, mLumenSceneDepth->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneOpacity->mResource, mLumenSceneOpacity->mRTVFormat);
    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    hCpuDescriptor.Offset(SwapChainBufferCount, mRtvDescriptorSize);
    mCPUViews["SceneColorRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferARTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferBRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferCRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["ToneMapRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureAlbedoAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureNormalAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureEmissiveAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneDepthRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneOpacityRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);

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
    CreateTexture2DDSV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize), mLumenCardCaptureDSAtlas);

    viewCount = 0;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mDsvDescriptorSize);
    mCPUViews["SceneDepthZDSV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
    mCPUViews["LumenCardCaptureDSAtlasDSV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
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
    {   //ShadowMask
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[4];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
        //slotRootParameter[1].InitAsShaderResourceView(0);
        slotRootParameter[2].InitAsShaderResourceView(1);
        slotRootParameter[3].InitAsDescriptorTable(1, &uavTable0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["ShadowMask"])));
    }
    {   //DirectionalLighting
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_DESCRIPTOR_RANGE srvTable0, srvTable1, srvTable2, srvTable3, srvTable4;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        srvTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        srvTable4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

        CD3DX12_ROOT_PARAMETER slotRootParameter[6];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);
        slotRootParameter[3].InitAsDescriptorTable(1, &srvTable2);
        slotRootParameter[4].InitAsDescriptorTable(1, &srvTable3);
        slotRootParameter[5].InitAsDescriptorTable(1, &srvTable4);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["DirectionalLighting"])));
    }
    {   //ToneMap
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[1];
        slotRootParameter[0].InitAsDescriptorTable(1, &srvTable0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["ToneMap"])));
    }
    {   //MeshCardCapture
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[2];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsConstants(4, 1);		// 4个32位值

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["MeshCardCapture"])));
    }
    {   //ClearCardCapture
        CD3DX12_ROOT_PARAMETER slotRootParameter[1];
        slotRootParameter[0].InitAsShaderResourceView(0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["ClearCardCapture"])));
    }
    {   //CopyToSurfaceCacheDepth
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE srvTable1;
        srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

        CD3DX12_ROOT_PARAMETER slotRootParameter[4];
        slotRootParameter[0].InitAsShaderResourceView(0);
        slotRootParameter[1].InitAsShaderResourceView(1);
        slotRootParameter[2].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[3].InitAsDescriptorTable(1, &srvTable1);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["CopyToSurfaceCacheDepth"])));
    }
    {   //CompressToSurfaceCacheAlbedo
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[4];
        slotRootParameter[0].InitAsShaderResourceView(0);
        slotRootParameter[1].InitAsShaderResourceView(1);
        slotRootParameter[2].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[3].InitAsDescriptorTable(1, &uavTable0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["CompressToSurfaceCacheAlbedo"])));
    }
    {   //CopyToSurfaceCacheOpacity
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

        CD3DX12_ROOT_PARAMETER slotRootParameter[3];
        slotRootParameter[0].InitAsShaderResourceView(0);
        slotRootParameter[1].InitAsShaderResourceView(1);
        slotRootParameter[2].InitAsDescriptorTable(1, &srvTable0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["CopyToSurfaceCacheOpacity"])));
    }
    {   //DirectLighting
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable1;
        uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE srvTable1;
        srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        CD3DX12_DESCRIPTOR_RANGE srvTable2;
        srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE srvTable3;
        srvTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        CD3DX12_DESCRIPTOR_RANGE srvTable4;
        srvTable4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[8];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsShaderResourceView(0);
        slotRootParameter[2].InitAsShaderResourceView(1);
        //slotRootParameter[3].InitAsShaderResourceView(2);
        //slotRootParameter[4].InitAsShaderResourceView(3);
        slotRootParameter[3].InitAsDescriptorTable(1, &srvTable2);
        slotRootParameter[4].InitAsDescriptorTable(1, &srvTable3);
        slotRootParameter[5].InitAsShaderResourceView(4);
        slotRootParameter[6].InitAsDescriptorTable(1, &uavTable0);
        slotRootParameter[7].InitAsDescriptorTable(1, &uavTable1);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(8, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
            IID_PPV_ARGS(&mRootSignatures["DirectLighting"])));
    }
}

D3DResource* LumenApp::InitBufferFromFile(const wchar_t* resname, const char* file)
{
    size_t fileSize = 0;
    unsigned char* fileContent = d3dUtil::LoadFileContent(file, fileSize);
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
        mLumenCards = InitBufferFromFile(L"Lumen.Cards", "Res/Lumen.Cards.data");
    }
    {   //ClearCardBuffer
        const D3D12_RECT scissors[] = {
            { 0,0,64,8 },
            { 128, 0, 128, 8 },
            { 128, 8, 128, 8 },
            { 64, 0, 64, 8 },
            { 256, 0, 128, 128 },
            { 384, 0, 128, 128 },

            { 0, 8, 64, 8 },
            { 128, 16, 128, 8 },
            { 128, 24, 128, 8 },
            { 64, 8, 64, 8 },
            { 0, 128, 128, 128 },
            { 128, 128, 128, 128 },
        };
        int rectData[12 * 4];
        for (int i = 0; i < 12; i++) {
            int offset = i * 4;
            rectData[offset] = scissors[i].left;
            rectData[offset + 1] = scissors[i].top;
            rectData[offset + 2] = scissors[i].left + scissors[i].right;
            rectData[offset + 3] = scissors[i].top + scissors[i].bottom;
        }
        ID3D12Resource* uploadBuffer = nullptr;
        mClearCardBuffer = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
        mClearCardBuffer->mResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mClearCardBuffer->mResource->SetName(L"ClearCardBuffer");
    }
    {   //RectDataBuffer
        const D3D12_RECT scissors[] = {
            { 0,0,64,8 },
            { 128, 0, 128, 8 },
            { 128, 8, 128, 8 },
            { 64, 0, 64, 8 },
            { 256, 0, 128, 128 },
            { 384, 0, 128, 128 },

            { 0, 8, 64, 8 },
            { 128, 16, 128, 8 },
            { 128, 24, 128, 8 },
            { 64, 8, 64, 8 },
            { 512, 0, 128, 128 },
            { 640, 0, 128, 128 },
        };
        int rectData[12 * 4];
        for (int i = 0; i < 12; i++) {
            int offset = i * 4;
            rectData[offset] = scissors[i].left;
            rectData[offset + 1] = scissors[i].top;
            rectData[offset + 2] = scissors[i].left + scissors[i].right;
            rectData[offset + 3] = scissors[i].top + scissors[i].bottom;
        }
        ID3D12Resource* uploadBuffer = nullptr;
        mRectDataBuffer = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
        mRectDataBuffer->mResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mRectDataBuffer->mResource->SetName(L"RectDataBuffer");
    }
    {   //RectUVBuffer
        const D3D12_RECT scissors[] = {
            { 0,0,64,8 },
            { 128, 0, 128, 8 },
            { 128, 8, 128, 8 },
            { 64, 0, 64, 8 },
            { 256, 0, 128, 128 },
            { 384, 0, 128, 128 },

            { 0, 8, 64, 8 },
            { 128, 16, 128, 8 },
            { 128, 24, 128, 8 },
            { 64, 8, 64, 8 },
            { 0, 128, 128, 128 },
            { 128, 128, 128, 128 },
        };
        int rectData[12 * 4];
        for (int i = 0; i < 12; i++) {
            int offset = i * 4;
            rectData[offset] = scissors[i].left;
            rectData[offset + 1] = scissors[i].top;
            rectData[offset + 2] = scissors[i].left + scissors[i].right;
            rectData[offset + 3] = scissors[i].top + scissors[i].bottom;
        }
        ID3D12Resource* uploadBuffer = nullptr;
        mRectUVBuffer = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
        mRectUVBuffer->mResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mRectUVBuffer->mResource->SetName(L"RectUVBuffer");
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

        mLumenCardCaptureAlbedoAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureAlbedoAtlas->mResource->SetName(L"Lumen.CardCaptureAlbedoAtlas");
        mLumenCardCaptureNormalAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureNormalAtlas->mResource->SetName(L"Lumen.CardCaptureNormalAtlas");
        mLumenCardCaptureEmissiveAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
            DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureEmissiveAtlas->mResource->SetName(L"Lumen.CardCaptureEmissiveAtlas");
        mLumenCardCaptureDSAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, {0.0f,0} });
        mLumenCardCaptureDSAtlas->mResource->SetName(L"Lumen.CardCaptureDepthStencilAtlas");

        mLumenSceneDepth = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
            DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM,
            DXGI_FORMAT_R16_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R16_UNORM, { 1.0f,0.0f,0.0f,1.0f } });
        mLumenSceneDepth->mResource->SetName(L"Lumen.SceneDepth");

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC7_UNORM_SRGB,
                DXGI_FORMAT_BC7_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneAlbedo = Init2DRTImage3(md3dDevice10, 4096, 4096, DXGI_FORMAT_BC7_TYPELESS,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneAlbedo->mResource->SetName(L"Lumen.SceneAlbedo");
        }
        mLumenSceneOpacity = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
            DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
            DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8_UNORM, { 0.0f,0.0f,0.0f,0.0f } });
        mLumenSceneOpacity->mResource->SetName(L"Lumen.SceneOpacity");

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneNormal = Init2DRTImage3(md3dDevice10, 4096, 4096, DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneNormal->mResource->SetName(L"Lumen.SceneNormal");
        }

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneEmissive = Init2DRTImage3(md3dDevice10, 4096, 4096, DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneEmissive->mResource->SetName(L"Lumen.SceneEmissive");
        }
        //lumen scene lighting
        {
            //direct lighting
            mLumenSceneDirectLighting = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneDirectLighting->mResource->SetName(L"Lumen.SceneDirectLighting");
            mLumenSceneFinalLighting = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneFinalLighting->mResource->SetName(L"Lumen.SceneFinalLighting");
        }
    }
}
