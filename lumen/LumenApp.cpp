
#include "LumenApp.h"
#include "D3DImage.h"

using namespace Math;

#define _4MB 4194304

static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers()
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

    const CD3DX12_STATIC_SAMPLER_DESC LLPClamp(
        4, // shaderRegister
        D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        6, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp, LLPClamp,
        anisotropicWrap, anisotropicClamp };
}

#define BEGIN_BARRIER() \
    { \
        std::vector<D3D12_RESOURCE_BARRIER> barriers;

#define PUSH_BARRIER(RES, SRC_STATE, DST_STATE) \
    barriers.push_back(InitResourceBarrier(RES->mUnderlyingResource, SRC_STATE, DST_STATE));

#define END_BARRIER(CMD_LIST) \
        CMD_LIST->ResourceBarrier(barriers.size(), barriers.data()); \
        barriers.clear(); \
    }

static unsigned int sFrameIndex = 0u;
static int sPingPongResourceIndex = 0;
int GetPingPongResourceIndexCurrentFrame() {
    return sPingPongResourceIndex;
}
int GetPingPongResourceIndexLastFrame() {
    return (sPingPongResourceIndex + 1) % 2;
}

static bool sUseTemporal = false;

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

    DirectLightTilesCB = std::make_unique<UploadBuffer<int>>(md3dDevice, 1120, false);
    DirectLightCardsCB = std::make_unique<UploadBuffer<int>>(md3dDevice, 1120, false);

    {
        const D3D12_RECT scissors[] = {
        { 0,0,64,8 },//8 tile
        { 128, 0, 128, 8 },//16 tile
        { 128, 8, 128, 8 },//16 tile
        { 64, 0, 64, 8 },//8 tile
        { 256, 0, 128, 128 },//256 tile
        { 384, 0, 128, 128 },//256 tile
        //512 + 48 = 560 x 2 = 1120

        { 0, 8, 64, 8 },
        { 128, 16, 128, 8 },
        { 128, 24, 128, 8 },
        { 64, 8, 64, 8 },
        { 512, 0, 128, 128 },
        { 640, 0, 128, 128 },
        };
        int rectData[12 * 4];
        unsigned int tilesData[1120];
        int tileIndex = 0;
        for (int i = 0; i < 12; i++) {
            int offset = i * 4;
            int left = scissors[i].left;
            int top = scissors[i].top;
            int right = scissors[i].left + scissors[i].right;
            int bottom = scissors[i].top + scissors[i].bottom;
            rectData[offset] = left;
            rectData[offset + 1] = top;
            rectData[offset + 2] = right;
            rectData[offset + 3] = bottom;
            for (int y = top; y < bottom; y += 8) {//0~7
                for (int x = left; x < right; x += 8) {
                    unsigned int tileLeft = (x - left) / 8;//0~15 -> 4 bit
                    unsigned int tileTop = (y - top) / 8;//0~15 -> 4 bit
                    tilesData[tileIndex++] = (tileTop << 28) | (tileLeft << 24) | i;//rect index
                }
            }
        }
        for (int i = 0; i < 1120; ++i)
        {
            DirectLightTilesCB->CopyData(i, tilesData[i]);
        }
    }

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

    mGlobalConstants.mFrameIndex = 0;
    mGlobalConstants.mFrameIndexMod8 = 0;
    mGlobalConstants.mMaxFramesAccumulated = 4;
    mGlobalConstants.mNumTracesPerProbe = 16;
    mGlobalConstants.mView_NumGlobalSDFClipmaps = 4;
    mGlobalConstants.mRadiosityAtlasSize[0] = 4096;
    mGlobalConstants.mRadiosityAtlasSize[1] = 4096;
    mGlobalConstants.mFixedJitterIndex = -1;
    mGlobalConstants.View_GlobalVolumeTexelSize[0] = 0.003968f;// 0.00397f;
    mGlobalConstants.View_GlobalDistanceFieldMipFactor[0] = 4.0f;
    mGlobalConstants.View_GlobalDistanceFieldMipTransition[0] = 0.625f;
    mGlobalConstants.View_NotCoveredExpandSurfaceScale[0] = 0.6f;
    mGlobalConstants.View_CoveredExpandSurfaceScale[0] = 1.0f;
    mGlobalConstants.View_DitheredTransparencyTraceThreshold[0] = 0.9f;
    mGlobalConstants.View_DitheredTransparencyStepThreshold[0] = 0.5f;
    mGlobalConstants.View_NotCoveredMinStepScale[0] = 4.0f;
    mGlobalConstants.View_GlobalDistanceFieldClipmapSizeInPages[0] = 36;
    mGlobalConstants.View_GlobalDistanceFieldInvPageAtlasSize[0] = 0.000977f;//0.00098f;
    mGlobalConstants.View_GlobalDistanceFieldInvPageAtlasSize[1] = 0.000977f;//0.00098f;
    mGlobalConstants.View_GlobalDistanceFieldInvPageAtlasSize[2] = 0.017857f;// 0.01786f;

    mGlobalConstants.View_GlobalDistanceFieldInvCoverageAtlasSize[0] = 0.001953f;//0.00195f;
    mGlobalConstants.View_GlobalDistanceFieldInvCoverageAtlasSize[1] = 0.001953f;//0.00195f;
    mGlobalConstants.View_GlobalDistanceFieldInvCoverageAtlasSize[2] = 0.035714f; //0.03571f;

    float View_GlobalVolumeTranslatedCenterAndExtent[] = {
        51.34113, -0.12544, -58.78236, 2500.00,
        51.34113, -0.12544, -58.78236, 5000.00,
        51.34113, 277.65234, 218.99541, 10000.00,
        51.34113, -277.9032, -336.56012, 20000.00,
        0.00, 0.00, 0.00, 1.00,
        0.00, 0.00, 0.00, 1.00
    };
    memcpy(mGlobalConstants.View_GlobalVolumeTranslatedCenterAndExtent, View_GlobalVolumeTranslatedCenterAndExtent, sizeof(View_GlobalVolumeTranslatedCenterAndExtent));
    float View_GlobalVolumeTranslatedWorldToUVAddAndMul[] = {
        0.48973, 0.50003, 0.51176, 0.0002,
        0.49487, 0.50001, 0.50588, 0.0001,
        0.49743, 0.48612, 0.48905, 0.00005,
        0.49872, 0.50695, 0.50841, 0.00002,
        0.00, 0.00, 0.00, 1.00,
        0.00, 0.00, 0.00, 1.00
    };
    memcpy(mGlobalConstants.View_GlobalVolumeTranslatedWorldToUVAddAndMul, View_GlobalVolumeTranslatedWorldToUVAddAndMul, sizeof(View_GlobalVolumeTranslatedWorldToUVAddAndMul));
    float View_GlobalDistanceFieldMipTranslatedWorldToUVScale[] = {
        0.0002, 0.0002, 0.00005, 0.00198,
        0.0001, 0.0001, 0.00002, 0.25198,
        0.00005, 0.00005, 0.00001, 0.50198,
        0.00002, 0.00002, 6.25000E-06, 0.75198,
        0.00, 0.00, 0.00, 1.00,
        0.00, 0.00, 0.00, 1.00
    };
    memcpy(mGlobalConstants.View_GlobalDistanceFieldMipTranslatedWorldToUVScale, View_GlobalDistanceFieldMipTranslatedWorldToUVScale, sizeof(View_GlobalDistanceFieldMipTranslatedWorldToUVScale));

    float View_GlobalDistanceFieldMipTranslatedWorldToUVBias[] = {
        0.48973f, 0.50003f, 0.12794f, 0.24802f,
        0.49487f, 0.50001f, 0.37647f, 0.49802f,
        0.49743f, 0.48612f, 0.62226f, 0.74802f,
        0.49872f, 0.50695f, 0.8771f, 0.99802f,
        0.00f, 0.00f, 0.00f, 1.00f,
        0.00f, 0.00f, 0.00f, 1.00f
    };
    memcpy(mGlobalConstants.View_GlobalDistanceFieldMipTranslatedWorldToUVBias, View_GlobalDistanceFieldMipTranslatedWorldToUVBias, sizeof(View_GlobalDistanceFieldMipTranslatedWorldToUVBias));
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

    // Fill HZB constants
    int inputW = ((mClientWidth + 8 - 1) / 8) * 8;
    int inputH = mClientHeight;
    mHZBConstants[0].DispatchThreadIdToBufferUV[0] = 2.0f / (float)inputW;
    mHZBConstants[0].DispatchThreadIdToBufferUV[1] = 2.0f / (float)inputH;
    mHZBConstants[0].DispatchThreadIdToBufferUV[2] = 0.0f;
    mHZBConstants[0].DispatchThreadIdToBufferUV[3] = 0.0f;
    mHZBConstants[0].PixelViewPortMinMax[0] = 0;
    mHZBConstants[0].PixelViewPortMinMax[1] = 0;
    mHZBConstants[0].PixelViewPortMinMax[2] = mClientWidth - 1;
    mHZBConstants[0].PixelViewPortMinMax[3] = mClientHeight - 1;
    mHZBConstants[0].InputViewportMaxBound[0] = ((float)inputW - 0.5f) / (float)inputW;
    mHZBConstants[0].InputViewportMaxBound[1] = ((float)inputH - 0.5f) / (float)inputH;
    mHZBConstants[0].InvSize[0] = 1.0f / (float)inputW;
    mHZBConstants[0].InvSize[1] = 1.0f / (float)inputH;
    mHZBCB[0]->CopyData(0, mHZBConstants[0]);

    int inputWH = 64;
    mHZBConstants[1].DispatchThreadIdToBufferUV[0] = 2.0f / (float)inputWH;
    mHZBConstants[1].DispatchThreadIdToBufferUV[1] = 2.0f / (float)inputWH;
    mHZBConstants[1].DispatchThreadIdToBufferUV[2] = 0.0f;
    mHZBConstants[1].DispatchThreadIdToBufferUV[3] = 0.0f;
    mHZBConstants[1].PixelViewPortMinMax[0] = 0;
    mHZBConstants[1].PixelViewPortMinMax[1] = 0;
    mHZBConstants[1].PixelViewPortMinMax[2] = inputWH - 1;
    mHZBConstants[1].PixelViewPortMinMax[3] = inputWH - 1;
    mHZBConstants[1].InputViewportMaxBound[0] = 1.0f;
    mHZBConstants[1].InputViewportMaxBound[1] = 1.0f;
    mHZBConstants[1].InvSize[0] = 1.0f / (float)inputWH;
    mHZBConstants[1].InvSize[1] = 1.0f / (float)inputWH;
    mHZBCB[1]->CopyData(0, mHZBConstants[1]);

    inputWH = 2;
    mHZBConstants[2].DispatchThreadIdToBufferUV[0] = 2.0f / (float)inputWH;
    mHZBConstants[2].DispatchThreadIdToBufferUV[1] = 2.0f / (float)inputWH;
    mHZBConstants[2].DispatchThreadIdToBufferUV[2] = 0.0f;
    mHZBConstants[2].DispatchThreadIdToBufferUV[3] = 0.0f;
    mHZBConstants[2].PixelViewPortMinMax[0] = 0;
    mHZBConstants[2].PixelViewPortMinMax[1] = 0;
    mHZBConstants[2].PixelViewPortMinMax[2] = inputWH - 1;
    mHZBConstants[2].PixelViewPortMinMax[3] = inputWH - 1;
    mHZBConstants[2].InputViewportMaxBound[0] = 1.0f;
    mHZBConstants[2].InputViewportMaxBound[1] = 1.0f;
    mHZBConstants[2].InvSize[0] = 1.0f / (float)inputWH;
    mHZBConstants[2].InvSize[1] = 1.0f / (float)inputWH;
    mHZBCB[2]->CopyData(0, mHZBConstants[2]);
}

void LumenApp::Draw(const GameTimer& gt)
{
    {
        sFrameIndex++;
        sPingPongResourceIndex = sFrameIndex % 2;
        if (sUseTemporal) {
            mGlobalConstants.mFrameIndexMod8 = sFrameIndex % 8;
            mGlobalConstants.mFrameIndex = sFrameIndex;
        }
    }
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
        barriers[0] = InitResourceBarrier(mSceneDepthZ->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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

        barriers[0] = InitResourceBarrier(mSceneDepthZ->mUnderlyingResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   // HZB
        SCOPED_EVENT(mCommandList, L"HZB");
        CD3DX12_GPU_DESCRIPTOR_HANDLE hzbCbvGpu0 = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mHZBCbvOffset, mCbvSrvDescriptorSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE hzbCbvGpu1 = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mHZBCbvOffset+1, mCbvSrvDescriptorSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE hzbCbvGpu2 = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mHZBCbvOffset+2, mCbvSrvDescriptorSize);
        {   // HZB0
            SCOPED_EVENT(mCommandList, L"ReduceHZB(mips[0:3] Closest Furthest) 512x512");

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["HZB0"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["HZB_UAV8"]);
            mCommandList->SetComputeRootDescriptorTable(0, hzbCbvGpu0);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["SceneDepthZSRV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["HZBFurthestMip0UAV"]);
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["HZBFurthestMip1UAV"]);
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["HZBFurthestMip2UAV"]);
            mCommandList->SetComputeRootDescriptorTable(5, mGPUViews["HZBFurthestMip3UAV"]);
            mCommandList->SetComputeRootDescriptorTable(6, mGPUViews["HZBClosestMip0UAV"]);
            mCommandList->SetComputeRootDescriptorTable(7, mGPUViews["HZBClosestMip1UAV"]);
            mCommandList->SetComputeRootDescriptorTable(8, mGPUViews["HZBClosestMip2UAV"]);
            mCommandList->SetComputeRootDescriptorTable(9, mGPUViews["HZBClosestMip3UAV"]);
            mCommandList->Dispatch(64, 64, 1);

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   // HZB1
            SCOPED_EVENT(mCommandList, L"ReduceHZB(mips[4:7] Furthest) 32x32");
            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["HZB1"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["HZB_UAV4"]);
            mCommandList->SetComputeRootDescriptorTable(0, hzbCbvGpu1);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["HZBFurthestMip3SRV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["HZBFurthestMip4UAV"]);
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["HZBFurthestMip5UAV"]);
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["HZBFurthestMip6UAV"]);
            mCommandList->SetComputeRootDescriptorTable(5, mGPUViews["HZBFurthestMip7UAV"]);
            mCommandList->Dispatch(32, 32, 1);

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   // HZB2
            SCOPED_EVENT(mCommandList, L"ReduceHZB(mips[4:7] Closest) 32x32");
            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["HZB2"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["HZB_UAV4"]);
            mCommandList->SetComputeRootDescriptorTable(0, hzbCbvGpu1);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["HZBClosestMip3SRV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["HZBClosestMip4UAV"]);
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["HZBClosestMip5UAV"]);
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["HZBClosestMip6UAV"]);
            mCommandList->SetComputeRootDescriptorTable(5, mGPUViews["HZBClosestMip7UAV"]);
            mCommandList->Dispatch(32, 32, 1);

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   // HZB3
            SCOPED_EVENT(mCommandList, L"ReduceHZB(mips[8:8] Furthest) 2x2");
            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["HZB3"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["HZB_UAV1"]);
            mCommandList->SetComputeRootDescriptorTable(0, hzbCbvGpu2);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["HZBFurthestMip7SRV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["HZBFurthestMip8UAV"]);
            mCommandList->Dispatch(32, 32, 1);

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBFurthest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   // HZB4
            SCOPED_EVENT(mCommandList, L"ReduceHZB(mips[8:8] Closest) 2x2");
            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["HZB4"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["HZB_UAV1"]);
            mCommandList->SetComputeRootDescriptorTable(0, hzbCbvGpu2);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["HZBClosestMip7SRV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["HZBClosestMip8UAV"]);
            mCommandList->Dispatch(32, 32, 1);

            BEGIN_BARRIER();
            PUSH_BARRIER(mHZBClosest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
    }
    {   //ClearCardCapture
        auto ExecuteMeshCardCapturePass = [&]() {
            SCOPED_EVENT(mCommandList, L"ClearCardCapturePass");
            mObjectCB->CopyData(0, mGlobalConstants);   // ����const buffer

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenCardCaptureEmissiveAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenCardCaptureDSAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            END_BARRIER(mCommandList);

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
            mCommandList->SetGraphicsRootShaderResourceView(0, mClearCardBuffer->mUnderlyingResource->GetGPUVirtualAddress());

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenCardCaptureEmissiveAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenCardCaptureDSAtlas, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        };
        SCOPED_EVENT(mCommandList, L"ClearCardPass");
        ExecuteMeshCardCapturePass();
    }
    {   //MeshCardCapturePass
        auto ExecuteMeshCardCapturePass = [&](int inIndex) {
            SCOPED_EVENT(mCommandList, L"MeshCardCapturePass");
            mObjectCB->CopyData(0, mGlobalConstants);

            D3D12_RESOURCE_BARRIER barriers[4];
            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            if (inIndex == 0) {
                //mCommandList->DiscardResource(mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureNormalAtlas->mUnderlyingResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, nullptr);
                //mCommandList->DiscardResource(mLumenCardCaptureDSAtlas->mUnderlyingResource, nullptr);
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

            barriers[0] = InitResourceBarrier(mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[1] = InitResourceBarrier(mLumenCardCaptureNormalAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[3] = InitResourceBarrier(mLumenCardCaptureDSAtlas->mUnderlyingResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
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
    {   // Copy to Surface Caches
        SCOPED_EVENT(mCommandList, L"Copy to Surface Caches");
        {   //CopyToSurfaceCacheDepth
            SCOPED_EVENT(mCommandList, L"CopyToSurfaceCacheDepth");

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenCardCaptureDSAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenSceneDepth, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            END_BARRIER(mCommandList);

            D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneDepthRTV"] };
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

            mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureNormalAtlasSRV"]);
            mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenCardCaptureDSAtlasSRV"]);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenCardCaptureDSAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneDepth, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //CompressToSurfaceCacheAlbedo
            SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheAlbedo");
            
            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenSceneAlbedo, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

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

            mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureAlbedoAtlasSRV"]);
            mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneAlbedoUAV"]);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneAlbedo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //CopyToSurfaceCacheOpacity
            SCOPED_EVENT(mCommandList, L"CopyToSurfaceCacheOpacity");

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenSceneOpacity, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            END_BARRIER(mCommandList);

            D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["LumenSceneOpacityRTV"] };
            mCommandList->OMSetRenderTargets(1, colorRT, FALSE, nullptr);
            float clearColor[] = { 0.0f,0.0f,0.0f,0.0f };
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

            mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureAlbedoAtlasSRV"]);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureAlbedoAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneOpacity, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //CompressToSurfaceCacheNormal
            SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheNormal");

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenSceneNormal, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

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

            mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureNormalAtlasSRV"]);
            mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneNormalUAV"]);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureNormalAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //CompressToSurfaceCacheEmissive
            SCOPED_EVENT(mCommandList, L"CompressToSurfaceCacheEmissive");

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureEmissiveAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            PUSH_BARRIER(mLumenSceneEmissive, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

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

            mCommandList->SetGraphicsRootShaderResourceView(0, mRectDataBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootShaderResourceView(1, mRectUVBuffer->mUnderlyingResource->GetGPUVirtualAddress());
            mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["LumenCardCaptureEmissiveAtlasSRV"]);
            mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["LumenSceneEmissiveUAV"]);

            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->DrawInstanced(6, 12, 0, 0);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenCardCaptureEmissiveAtlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneEmissive, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
    }
    {   // BasePass
        SCOPED_EVENT(mCommandList, L"BasePass");

        BEGIN_BARRIER();
        PUSH_BARRIER(mLightingChannels, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        PUSH_BARRIER(mSceneColors[GetPingPongResourceIndexCurrentFrame()], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        PUSH_BARRIER(mGBufferA, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        PUSH_BARRIER(mGBufferB, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        PUSH_BARRIER(mGBufferC, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        PUSH_BARRIER(mSceneDepthZ, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        END_BARRIER(mCommandList);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[5] = { mCPUViews[GetPingPongResourceIndexCurrentFrame() == 0 ? "SceneColorRTV0" : "SceneColorRTV1"], 
            mCPUViews["GBufferARTV"], mCPUViews["GBufferBRTV"], mCPUViews["GBufferCRTV"], mCPUViews["LightingChannelsRTV"] };
        D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        mCommandList->OMSetRenderTargets(5, colorRT, FALSE, &dsRT);
        float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
        mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);
        clearColor[3] = 0.0f;
        mCommandList->ClearRenderTargetView(colorRT[1], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[2], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[3], clearColor, 0, nullptr);
        float clearColor2[] = { 1.0f,1.0f,1.0f,1.0f };
        mCommandList->ClearRenderTargetView(colorRT[4], clearColor2, 0, nullptr);
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

        BEGIN_BARRIER();
        PUSH_BARRIER(mLightingChannels, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        PUSH_BARRIER(mSceneColors[GetPingPongResourceIndexCurrentFrame()], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        PUSH_BARRIER(mGBufferA, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        PUSH_BARRIER(mGBufferB, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        PUSH_BARRIER(mGBufferC, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        PUSH_BARRIER(mSceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
        END_BARRIER(mCommandList);
    }
    {   //ShadowMask
        SCOPED_EVENT(mCommandList, L"ShadowMask");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mShadowMaskTexture->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        mCommandList->SetPipelineState(mPSOs["ShadowMask"]);
        mCommandList->SetComputeRootSignature(mRootSignatures["ShadowMask"]);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
        mCommandList->SetComputeRootDescriptorTable(0, hCbvGpuDescriptor);
        mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["SceneDepthZSRV"]);
        mCommandList->SetComputeRootShaderResourceView(2, mDFSceneObject->mUnderlyingResource->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["ShadowMaskTextureUAV"]);

        mCommandList->Dispatch(120, 68, 1);

        barriers[0] = InitResourceBarrier(mShadowMaskTexture->mUnderlyingResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //DirectionalLighting
        SCOPED_EVENT(mCommandList, L"DirectionalLighting");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mSceneColors[GetPingPongResourceIndexCurrentFrame()]->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT = mCPUViews[GetPingPongResourceIndexCurrentFrame() == 0 ? "SceneColorRTV0" : "SceneColorRTV1"];
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

        barriers[0] = InitResourceBarrier(mSceneColors[GetPingPongResourceIndexCurrentFrame()]->mUnderlyingResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }
    {   //LumenSceneLighting
        SCOPED_EVENT(mCommandList, L"LumenSceneLighting");
        {   // LumenSceneLightingClear
            SCOPED_EVENT(mCommandList, L"LumenSceneLightingClear");

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenSceneDirectLighting, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenSceneIndirectLighting, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenSceneNumFramesAccumulatedAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenSceneFinalLighting, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenRadiosityTraceRadianceAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            PUSH_BARRIER(mLumenRadiosityFilteredTraceRadianceAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
            END_BARRIER(mCommandList);

            D3D12_CPU_DESCRIPTOR_HANDLE colorRT[6] = { mCPUViews["LumenSceneDirectLightingRTV"], mCPUViews["LumenSceneIndirectLightingRTV"], mCPUViews["LumenSceneNumFramesAccumulatedAtlasRTV"],
                mCPUViews["LumenSceneFinalLightingRTV"], mCPUViews["LumenRadiosityTraceRadianceAtlasRTV"], mCPUViews["LumenRadiosityFilteredTraceRadianceAtlasRTV"] };
            mCommandList->OMSetRenderTargets(6, colorRT, FALSE, nullptr);
            float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
            mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);
            mCommandList->ClearRenderTargetView(colorRT[1], clearColor, 0, nullptr);
            mCommandList->ClearRenderTargetView(colorRT[2], clearColor, 0, nullptr);
            mCommandList->ClearRenderTargetView(colorRT[3], clearColor, 0, nullptr);
            mCommandList->ClearRenderTargetView(colorRT[4], clearColor, 0, nullptr);
            mCommandList->ClearRenderTargetView(colorRT[5], clearColor, 0, nullptr);

            BEGIN_BARRIER();
            PUSH_BARRIER(mLumenSceneDirectLighting, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneIndirectLighting, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneNumFramesAccumulatedAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenSceneFinalLighting, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenRadiosityTraceRadianceAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenRadiosityFilteredTraceRadianceAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //DirectLighting
            SCOPED_EVENT(mCommandList, L"DirectLighting");
            D3D12_RESOURCE_BARRIER barriers[4];

            barriers[0] = InitResourceBarrier(mLumenSceneNormal->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[1] = InitResourceBarrier(mLumenSceneDepth->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[2] = InitResourceBarrier(mLumenSceneDirectLighting->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            barriers[3] = InitResourceBarrier(mLumenSceneFinalLighting->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);

            mCommandList->SetPipelineState(mPSOs["DirectLighting"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["DirectLighting"]);

            CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
            mCommandList->SetComputeRootDescriptorTable(0, hCbvGpuDescriptor);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["RectCoordBufferSRV"]);              // t0
            mCommandList->SetComputeRootShaderResourceView(2, DirectLightTilesCB->Resource()->GetGPUVirtualAddress());                     // t1 mGPUViews["TilesInfoSRV"]
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["DirectLightingNormalAtlasSRV"]);     // t2
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["DirectLightingDepthAtlasSRV"]);      // t3
            mCommandList->SetComputeRootDescriptorTable(5, mGPUViews["LumenCardDataSRV"]);                 // t4
            mCommandList->SetComputeRootDescriptorTable(6, mGPUViews["GDFPageAtlasSRV"]);                  // t5
            mCommandList->SetComputeRootDescriptorTable(7, mGPUViews["GDFCoverageAtlasSRV"]);              // t6
            mCommandList->SetComputeRootDescriptorTable(8, mGPUViews["GDFPageTableSRV"]);                  // t7
            mCommandList->SetComputeRootDescriptorTable(9, mGPUViews["GDFMipsSRV"]);                       // t8
            mCommandList->SetComputeRootDescriptorTable(10, mGPUViews["LumenSceneAlbedoSRV"]);             // t9
            mCommandList->SetComputeRootDescriptorTable(11, mGPUViews["LumenSceneEmissiveSRV"]);           // t10
            mCommandList->SetComputeRootDescriptorTable(12, mGPUViews["LumenSceneDirectLightingUAV"]);     // u0
            mCommandList->SetComputeRootDescriptorTable(13, mGPUViews["LumenSceneFinalLightingUAV"]);      // u1

            mCommandList->Dispatch(1120, 1, 1);

            barriers[0] = InitResourceBarrier(mLumenSceneNormal->mUnderlyingResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[1] = InitResourceBarrier(mLumenSceneDepth->mUnderlyingResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[2] = InitResourceBarrier(mLumenSceneDirectLighting->mUnderlyingResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            barriers[3] = InitResourceBarrier(mLumenSceneFinalLighting->mUnderlyingResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            mCommandList->ResourceBarrier(_countof(barriers), barriers);
        }
        {   // Radiosity
            SCOPED_EVENT(mCommandList, L"Radiosity");
            {   // LumenSceneLightingRadiosityClear
                SCOPED_EVENT(mCommandList, L"LumenSceneLightingRadiosityClear");

                BEGIN_BARRIER();
                PUSH_BARRIER(mLumenRadiosityProbeSHRedAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
                PUSH_BARRIER(mLumenRadiosityProbeSHGreenAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
                PUSH_BARRIER(mLumenRadiosityProbeSHBlueAtlas, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
                END_BARRIER(mCommandList);

                D3D12_CPU_DESCRIPTOR_HANDLE colorRT[3] = { mCPUViews["LumenRadiosityProbeSHRedAtlasRTV"], mCPUViews["LumenRadiosityProbeSHGreenAtlasRTV"], mCPUViews["LumenRadiosityProbeSHBlueAtlasRTV"] };
                mCommandList->OMSetRenderTargets(3, colorRT, FALSE, nullptr);
                float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
                mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);
                mCommandList->ClearRenderTargetView(colorRT[1], clearColor, 0, nullptr);
                mCommandList->ClearRenderTargetView(colorRT[2], clearColor, 0, nullptr);

                BEGIN_BARRIER();
                PUSH_BARRIER(mLumenRadiosityProbeSHRedAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
                PUSH_BARRIER(mLumenRadiosityProbeSHGreenAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
                PUSH_BARRIER(mLumenRadiosityProbeSHBlueAtlas, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
                END_BARRIER(mCommandList);
            }
            {   // DistanceFieldTracing 4x4 probes
                SCOPED_EVENT(mCommandList, L"DistanceFieldTracing 4x4 probes");

            }
        }
    }
    {   // LumenSceneProbeGather
        SCOPED_EVENT(mCommandList, L"LumenSceneProbeGather");
        {   //  ClearScreenProbe
            SCOPED_EVENT(mCommandList, L"ClearScreenProbe");

            BEGIN_BARRIER();
            PUSH_BARRIER(mScreenProbeSceneDepth, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mScreenProbeWorldSpeed, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mScreenProbeWorldNormal, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mScreenProbeTranslatedWorldPositions[GetPingPongResourceIndexCurrentFrame()], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mScreenTileAdapativeProbeHeader, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mScreenTileAdapativeProbeIndicies, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mAdaptiveScreenProbeData, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mNumAdaptiveScreenProbe, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mLumenScreenProbeGatherCompactedTraceTexelAllocator, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mLumenScreenProbeGatherCompactedTraceTexelData, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            PUSH_BARRIER(mLumenScreenProbeGatherLightingProbabilityDensityFunction, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            END_BARRIER(mCommandList);

            mCommandList->SetPipelineState(mPSOs["ClearScreenProbe"]);
            mCommandList->SetComputeRootSignature(mRootSignatures["ClearScreenProbe"]);

            CD3DX12_GPU_DESCRIPTOR_HANDLE hCbvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), mCbvOffset, mCbvSrvDescriptorSize);
            mCommandList->SetComputeRootDescriptorTable(0, mGPUViews["ScreenProbeSceneDepthUAV"]);
            mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["ScreenProbeWorldSpeedUAV"]);
            mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["ScreenProbeWorldNormalUAV"]);
            mCommandList->SetComputeRootDescriptorTable(3, mGPUViews["ScreenProbeTranslatedWorldPositionsUAV0"]);
            mCommandList->SetComputeRootDescriptorTable(4, mGPUViews["ScreenTileAdapativeProbeHeaderUAV"]);
            mCommandList->SetComputeRootDescriptorTable(5, mGPUViews["ScreenTileAdapativeProbeIndiciesUAV"]);
            mCommandList->SetComputeRootDescriptorTable(6, mGPUViews["AdaptiveScreenProbeDataUAV"]);
            mCommandList->SetComputeRootDescriptorTable(7, mGPUViews["NumAdaptiveScreenProbeUAV"]);
            mCommandList->SetComputeRootDescriptorTable(8, mGPUViews["CompactedTraceTexelAllocatorUAV"]);
            mCommandList->SetComputeRootDescriptorTable(9, mGPUViews["CompactedTraceTexelDataUAV"]);
            mCommandList->SetComputeRootDescriptorTable(10, mGPUViews["LumenScreenProbeGatherLightingProbabilityDensityFunctionUAV"]);

            mCommandList->Dispatch(120, 68, 1); //8x8

            BEGIN_BARRIER();
            PUSH_BARRIER(mScreenProbeSceneDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mScreenProbeWorldSpeed, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mScreenProbeWorldNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mScreenProbeTranslatedWorldPositions[GetPingPongResourceIndexCurrentFrame()], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mScreenTileAdapativeProbeHeader, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mScreenTileAdapativeProbeIndicies, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mAdaptiveScreenProbeData, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mNumAdaptiveScreenProbe, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenScreenProbeGatherCompactedTraceTexelAllocator, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenScreenProbeGatherCompactedTraceTexelData, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            PUSH_BARRIER(mLumenScreenProbeGatherLightingProbabilityDensityFunction, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            END_BARRIER(mCommandList);
        }
        {   //  UniformPlacementScreenProbe
            SCOPED_EVENT(mCommandList, L"UniformPlacementScreenProbe");

        }
    }
    {   //ToneMap
        SCOPED_EVENT(mCommandList, L"ToneMap");
        D3D12_RESOURCE_BARRIER barriers[1];
        barriers[0] = InitResourceBarrier(mToneMap->mUnderlyingResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT = mCPUViews["ToneMapRTV"];
        mCommandList->OMSetRenderTargets(1, &colorRT, FALSE, nullptr);

        mCommandList->SetPipelineState(mPSOs["ToneMap"]);
        mCommandList->SetGraphicsRootSignature(mRootSignatures["ToneMap"]);

        mCommandList->IASetVertexBuffers(0, 1, &mScreenFullGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&mScreenFullGeo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        mCommandList->SetGraphicsRootDescriptorTable(0, mGPUViews[GetPingPongResourceIndexCurrentFrame() == 0 ? "SceneColorSRV0" : "SceneColorSRV1"]);

        mCommandList->DrawIndexedInstanced(mScreenFullGeo->IndexCount, 1, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mToneMap->mUnderlyingResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
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

        mCubeGeo->VertexBufferGPU = mCubePositionBuffer->mUnderlyingResource;
        mCubeGeo->VertexBufferGPU2 = mCubeAttributeBuffer->mUnderlyingResource;
        mCubeGeo->IndexBufferGPU = mCubeIndexBuffer->mUnderlyingResource;

        mCubeGeo->VertexByteStride = sizeof(float) * 3;
        mCubeGeo->VertexBufferByteSize = vbByteSize;
        mCubeGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
        mCubeGeo->IndexBufferByteSize = mCubeIndexBuffer->mUnderlyingResource->GetDesc().Width;

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

    mDxcByteCodes["HZB0CS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\HZB\\HZB0.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["HZB1CS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\HZB\\HZB1.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["HZB2CS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\HZB\\HZB2.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["HZB3CS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\HZB\\HZB3.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["HZB4CS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\HZB\\HZB4.hlsl", nullptr, 0, L"CS", L"cs_6_6");

    mDxcByteCodes["ClearCardCaptureVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\ClearCardCapture.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["ClearCardCapturePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\ClearCardCapture.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["MeshCardCaptureVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\MeshCardCapture.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["MeshCardCapturePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\MeshCardCapture.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CopyToSurfaceCacheDepthVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CopyToSurfaceCacheDepth.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CopyToSurfaceCacheDepthPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CopyToSurfaceCacheDepth.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheAlbedoVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheAlbedo.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheAlbedoPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheAlbedo.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheNormalVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheNormal.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheNormalPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheNormal.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CompressToSurfaceCacheEmissiveVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheEmissive.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CompressToSurfaceCacheEmissivePS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CompressToSurfaceCacheEmissive.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["CopyToSurfaceCacheOpacityVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CopyToSurfaceCacheOpacity.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["CopyToSurfaceCacheOpacityPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\UpdateSurfaceCache\\CopyToSurfaceCacheOpacity.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    
    mDxcByteCodes["BasePassVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["BasePassPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\BasePass.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["DirectionalLightingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["DirectionalLightingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\DirectionalLighting.hlsl", nullptr, 0, L"PS", L"ps_6_6");
    mDxcByteCodes["ShadowMaskCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ShadowMask.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    mDxcByteCodes["ToneMappingVS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"VS", L"vs_6_6");
    mDxcByteCodes["ToneMappingPS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ToneMapping.hlsl", nullptr, 0, L"PS", L"ps_6_6");

    mDxcByteCodes["DirectLightingCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\LumenSceneLighting\\DirectLighting.hlsl", nullptr, 0, L"CS", L"cs_6_6");

    //LumenSceneProbeGather
    mDxcByteCodes["ClearScreenProbeCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ScreenProbeGather\\ClearScreenProbe.hlsl", nullptr, 0, L"CS", L"cs_6_6");
    //mDxcByteCodes["UniformPlacementScreenProbeCS"] = d3dUtil::DxcCompileShader(L"lumen\\shader\\ScreenProbeGather\\UniformPlacementScreenProbe.hlsl", nullptr, 0, L"CS", L"cs_6_6");

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
        psoDesc.NumRenderTargets = 5;
        psoDesc.RTVFormats[0] = mSceneColors[0]->mRTVFormat;
        psoDesc.RTVFormats[1] = mGBufferA->mRTVFormat;
        psoDesc.RTVFormats[2] = mGBufferB->mRTVFormat;
        psoDesc.RTVFormats[3] = mGBufferC->mRTVFormat;
        psoDesc.RTVFormats[4] = mLightingChannels->mRTVFormat;
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
        psoDesc.RTVFormats[0] = mSceneColors[0]->mRTVFormat;
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
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
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
    {   //ClearScreenProbe
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["ClearScreenProbe"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["ClearScreenProbeCS"]->GetBufferPointer()),
            mDxcByteCodes["ClearScreenProbeCS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["ClearScreenProbe"])));
    }
    {   //HZB0
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["HZB_UAV8"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["HZB0CS"]->GetBufferPointer()),
            mDxcByteCodes["HZB0CS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HZB0"])));
    }
    {   //HZB1
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["HZB_UAV4"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["HZB1CS"]->GetBufferPointer()),
            mDxcByteCodes["HZB1CS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HZB1"])));
    }
    {   //HZB2
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["HZB_UAV4"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["HZB2CS"]->GetBufferPointer()),
            mDxcByteCodes["HZB2CS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HZB2"])));
    }
    {   //HZB3
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["HZB_UAV1"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["HZB3CS"]->GetBufferPointer()),
            mDxcByteCodes["HZB3CS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HZB3"])));
    }
    {   //HZB4
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["HZB_UAV1"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["HZB4CS"]->GetBufferPointer()),
            mDxcByteCodes["HZB4CS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HZB4"])));
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

    pDevice->CreateDepthStencilView(inDSRT->mUnderlyingResource, &d3dDSViewDesc, inMemory);
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

static void CreateTexture3DSRV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;
    srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
    pDevice->CreateShaderResourceView(inResource, &srvDesc, inMemory);
}

static void CreateBufferSRV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, UINT64 inByteSize, UINT inStructureByteStride)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(inByteSize / inStructureByteStride);
    srvDesc.Buffer.StructureByteStride = inStructureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
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
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 1);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 2);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 3);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 4);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 5);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 6);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 7);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mRTVFormat, 8);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 1);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 2);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 3);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 4);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 5);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 6);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 7);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mRTVFormat, 8);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mSRVFormat, 3);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBFurthest->mUnderlyingResource, mHZBFurthest->mSRVFormat, 7);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mSRVFormat, 3);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mHZBClosest->mUnderlyingResource, mHZBClosest->mSRVFormat, 7);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLightingChannels->mUnderlyingResource, mLightingChannels->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mSceneColors[0]->mUnderlyingResource, mSceneColors[0]->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mSceneColors[1]->mUnderlyingResource, mSceneColors[1]->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferA->mUnderlyingResource, mGBufferA->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferB->mUnderlyingResource, mGBufferB->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGBufferC->mUnderlyingResource, mGBufferC->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mSceneDepthZ->mUnderlyingResource, mSceneDepthZ->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mShadowMaskTexture->mUnderlyingResource, mShadowMaskTexture->mFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mShadowMaskTexture->mUnderlyingResource, mShadowMaskTexture->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, mLumenCardCaptureAlbedoAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureNormalAtlas->mUnderlyingResource, mLumenCardCaptureNormalAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, mLumenCardCaptureEmissiveAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureDSAtlas->mUnderlyingResource, mLumenCardCaptureDSAtlas->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneAlbedo->mUnderlyingResource, mLumenSceneAlbedo->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneNormal->mUnderlyingResource, mLumenSceneNormal->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneEmissive->mUnderlyingResource, mLumenSceneEmissive->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneDirectLighting->mUnderlyingResource, mLumenSceneDirectLighting->mRTVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneFinalLighting->mUnderlyingResource, mLumenSceneFinalLighting->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneDepth->mUnderlyingResource, mLumenSceneDepth->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneNormal->mUnderlyingResource, mLumenSceneNormal->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneAlbedo->mUnderlyingResource, mLumenSceneAlbedo->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneEmissive->mUnderlyingResource, mLumenSceneEmissive->mSRVFormat, 0);
    //DirectLighting SRVs (t0-t10): RectCoordBuffer, TilesInfo, NormalAtlas, DepthAtlas, CardData, GDF textures, Albedo/Emissive atlases
    {
        D3D12_RESOURCE_DESC rectBufDesc = mRectDataBuffer->mUnderlyingResource->GetDesc();
        CreateBufferSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mRectDataBuffer->mUnderlyingResource, rectBufDesc.Width, sizeof(uint32_t) * 4);
    }
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneNormal->mUnderlyingResource, mLumenSceneNormal->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenSceneDepth->mUnderlyingResource, mLumenSceneDepth->mSRVFormat, 0);
    {
        D3D12_RESOURCE_DESC cardBufDesc = mLumenCards->mUnderlyingResource->GetDesc();
        CreateBufferSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCards->mUnderlyingResource, cardBufDesc.Width, sizeof(float) * 4);
    }
    CreateTexture3DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGSDFPageAtlas->mUnderlyingResource, mGSDFPageAtlas->mSRVFormat);
    CreateTexture3DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGSDFCoverageAtlas->mUnderlyingResource, mGSDFCoverageAtlas->mSRVFormat);
    CreateTexture3DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGSDFPageTable->mUnderlyingResource, mGSDFPageTable->mSRVFormat);
    CreateTexture3DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mGSDFMips->mUnderlyingResource, mGSDFMips->mSRVFormat);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, mLumenCardCaptureAlbedoAtlas->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, mLumenCardCaptureEmissiveAtlas->mSRVFormat, 0);
    //ScreenProbe
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeSceneDepth->mUnderlyingResource, mScreenProbeSceneDepth->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeSceneDepth->mUnderlyingResource, mScreenProbeSceneDepth->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeWorldSpeed->mUnderlyingResource, mScreenProbeWorldSpeed->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeWorldSpeed->mUnderlyingResource, mScreenProbeWorldSpeed->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeWorldNormal->mUnderlyingResource, mScreenProbeWorldNormal->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeWorldNormal->mUnderlyingResource, mScreenProbeWorldNormal->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeTranslatedWorldPositions[0]->mUnderlyingResource, mScreenProbeTranslatedWorldPositions[0]->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeTranslatedWorldPositions[0]->mUnderlyingResource, mScreenProbeTranslatedWorldPositions[0]->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeTranslatedWorldPositions[1]->mUnderlyingResource, mScreenProbeTranslatedWorldPositions[1]->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenProbeTranslatedWorldPositions[1]->mUnderlyingResource, mScreenProbeTranslatedWorldPositions[1]->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenTileAdapativeProbeHeader->mUnderlyingResource, mScreenTileAdapativeProbeHeader->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenTileAdapativeProbeHeader->mUnderlyingResource, mScreenTileAdapativeProbeHeader->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenTileAdapativeProbeIndicies->mUnderlyingResource, mScreenTileAdapativeProbeIndicies->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mScreenTileAdapativeProbeIndicies->mUnderlyingResource, mScreenTileAdapativeProbeIndicies->mRTVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenScreenProbeGatherLightingProbabilityDensityFunction->mUnderlyingResource, mLumenScreenProbeGatherLightingProbabilityDensityFunction->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenScreenProbeGatherLightingProbabilityDensityFunction->mUnderlyingResource, mLumenScreenProbeGatherLightingProbabilityDensityFunction->mRTVFormat, 0);
    CreateRWByteAddressBufferUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mAdaptiveScreenProbeData->mUnderlyingResource, DXGI_FORMAT_R32_TYPELESS);
    CreateRWByteAddressBufferUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mNumAdaptiveScreenProbe->mUnderlyingResource, DXGI_FORMAT_R32_TYPELESS);
    CreateRWByteAddressBufferUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenScreenProbeGatherCompactedTraceTexelAllocator->mUnderlyingResource, DXGI_FORMAT_R32_TYPELESS);
    CreateRWByteAddressBufferUAV(md3dDevice, hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize), mLumenScreenProbeGatherCompactedTraceTexelData->mUnderlyingResource, DXGI_FORMAT_R32_TYPELESS);

    mCPUViews["HZBFurthestMip0UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip1UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip2UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip3UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip4UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip5UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip6UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip7UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip8UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip0UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip1UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip2UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip3UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip4UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip5UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip6UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip7UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip8UAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip3SRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBFurthestMip7SRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip3SRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["HZBClosestMip7SRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LightingChannelsSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["SceneColorSRV0"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["SceneColorSRV1"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
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
    mCPUViews["LumenSceneAlbedoSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenSceneEmissiveSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    //DirectLighting SRVs (t0-t10)
    mCPUViews["RectCoordBufferSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["DirectLightingNormalAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["DirectLightingDepthAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardDataSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GDFPageAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GDFCoverageAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GDFPageTableSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["GDFMipsSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardSceneAlbedoAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenCardSceneEmissiveAtlasSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    //ScreenProbe
    mCPUViews["ScreenProbeSceneDepthSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeSceneDepthUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeWorldSpeedSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeWorldSpeedUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeWorldNormalSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeWorldNormalUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeTranslatedWorldPositionsSRV0"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeTranslatedWorldPositionsUAV0"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeTranslatedWorldPositionsSRV1"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenProbeTranslatedWorldPositionsUAV1"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenTileAdapativeProbeHeaderSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenTileAdapativeProbeHeaderUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenTileAdapativeProbeIndiciesSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["ScreenTileAdapativeProbeIndiciesUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenScreenProbeGatherLightingProbabilityDensityFunctionSRV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["LumenScreenProbeGatherLightingProbabilityDensityFunctionUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["AdaptiveScreenProbeDataUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["NumAdaptiveScreenProbeUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["CompactedTraceTexelAllocatorUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mCPUViews["CompactedTraceTexelDataUAV"] = hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);

    mGPUViews["HZBFurthestMip0UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip1UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip2UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip3UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip4UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip5UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip6UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip7UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip8UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip0UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip1UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip2UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip3UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip4UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip5UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip6UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip7UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip8UAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip3SRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBFurthestMip7SRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip3SRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["HZBClosestMip7SRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LightingChannelsSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["SceneColorSRV0"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["SceneColorSRV1"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
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
    mGPUViews["LumenSceneAlbedoSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenSceneEmissiveSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    //DirectLighting SRVs (t0-t10)
    mGPUViews["RectCoordBufferSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["DirectLightingNormalAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["DirectLightingDepthAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardDataSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GDFPageAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GDFCoverageAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GDFPageTableSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["GDFMipsSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardSceneAlbedoAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenCardSceneEmissiveAtlasSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    //ScreenProbe
    mGPUViews["ScreenProbeSceneDepthSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeSceneDepthUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeWorldSpeedSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeWorldSpeedUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeWorldNormalSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeWorldNormalUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeTranslatedWorldPositionsSRV0"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeTranslatedWorldPositionsUAV0"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeTranslatedWorldPositionsSRV1"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenProbeTranslatedWorldPositionsUAV1"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenTileAdapativeProbeHeaderSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenTileAdapativeProbeHeaderUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenTileAdapativeProbeIndiciesSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["ScreenTileAdapativeProbeIndiciesUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenScreenProbeGatherLightingProbabilityDensityFunctionSRV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["LumenScreenProbeGatherLightingProbabilityDensityFunctionUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["AdaptiveScreenProbeDataUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["NumAdaptiveScreenProbeUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["CompactedTraceTexelAllocatorUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    mGPUViews["CompactedTraceTexelDataUAV"] = hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);

    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    hCpuDescriptor.Offset(SwapChainBufferCount, mRtvDescriptorSize);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLightingChannels->mUnderlyingResource, mLightingChannels->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mSceneColors[0]->mUnderlyingResource, mSceneColors[0]->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mSceneColors[1]->mUnderlyingResource, mSceneColors[1]->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferA->mUnderlyingResource, mGBufferA->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferB->mUnderlyingResource, mGBufferB->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mGBufferC->mUnderlyingResource, mGBufferC->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mToneMap->mUnderlyingResource, mToneMap->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureAlbedoAtlas->mUnderlyingResource, mLumenCardCaptureAlbedoAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureNormalAtlas->mUnderlyingResource, mLumenCardCaptureNormalAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenCardCaptureEmissiveAtlas->mUnderlyingResource, mLumenCardCaptureEmissiveAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneDepth->mUnderlyingResource, mLumenSceneDepth->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneOpacity->mUnderlyingResource, mLumenSceneOpacity->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneDirectLighting->mUnderlyingResource, mLumenSceneDirectLighting->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneIndirectLighting->mUnderlyingResource, mLumenSceneIndirectLighting->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneNumFramesAccumulatedAtlas->mUnderlyingResource, mLumenSceneNumFramesAccumulatedAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenSceneFinalLighting->mUnderlyingResource, mLumenSceneFinalLighting->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenRadiosityTraceRadianceAtlas->mUnderlyingResource, mLumenRadiosityTraceRadianceAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenRadiosityFilteredTraceRadianceAtlas->mUnderlyingResource, mLumenRadiosityFilteredTraceRadianceAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenRadiosityProbeSHRedAtlas->mUnderlyingResource, mLumenRadiosityProbeSHRedAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenRadiosityProbeSHGreenAtlas->mUnderlyingResource, mLumenRadiosityProbeSHGreenAtlas->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(1, mRtvDescriptorSize), mLumenRadiosityProbeSHBlueAtlas->mUnderlyingResource, mLumenRadiosityProbeSHBlueAtlas->mRTVFormat);
    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    hCpuDescriptor.Offset(SwapChainBufferCount, mRtvDescriptorSize);

    mCPUViews["LightingChannelsRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["SceneColorRTV0"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["SceneColorRTV1"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferARTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferBRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["GBufferCRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["ToneMapRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureAlbedoAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureNormalAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenCardCaptureEmissiveAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneDepthRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneOpacityRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneDirectLightingRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneIndirectLightingRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneNumFramesAccumulatedAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenSceneFinalLightingRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenRadiosityTraceRadianceAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenRadiosityFilteredTraceRadianceAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenRadiosityProbeSHRedAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenRadiosityProbeSHGreenAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    mCPUViews["LumenRadiosityProbeSHBlueAtlasRTV"] = hCpuDescriptor.Offset(1, mRtvDescriptorSize);
    //ScreenProbe

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

    // HZB constant buffer
    mHZBCbvOffset = mCbvOffset + 1;
    for (int i = 0; i < 3; ++i)
    {
        mHZBCB[i] = std::make_unique<UploadBuffer<HZBConstants>>(md3dDevice, 1, true);
        D3D12_CONSTANT_BUFFER_VIEW_DESC hzbCbvDesc;
        hzbCbvDesc.BufferLocation = mHZBCB[i]->Resource()->GetGPUVirtualAddress();
        hzbCbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(HZBConstants));
        CD3DX12_CPU_DESCRIPTOR_HANDLE hzbCbvCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mHZBCbvOffset + i, mCbvSrvDescriptorSize);
        md3dDevice->CreateConstantBufferView(&hzbCbvDesc, hzbCbvCpuDescriptor);
    }
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
        slotRootParameter[1].InitAsConstants(4, 1);		// 4?32??

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
        CD3DX12_DESCRIPTOR_RANGE srvTable5;
        srvTable5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
        CD3DX12_DESCRIPTOR_RANGE srvTable6;
        srvTable6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
        CD3DX12_DESCRIPTOR_RANGE srvTable7;
        srvTable7.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
        CD3DX12_DESCRIPTOR_RANGE srvTable8;
        srvTable8.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
        CD3DX12_DESCRIPTOR_RANGE srvTable9;
        srvTable9.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);
        CD3DX12_DESCRIPTOR_RANGE srvTable10;
        srvTable10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);

        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[14];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);             // b0
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);             // t0 RectCoordBuffer
        //slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);             // t1 TilesInfo
        slotRootParameter[2].InitAsShaderResourceView(1);
        slotRootParameter[3].InitAsDescriptorTable(1, &srvTable2);             // t2 NormalAtlas
        slotRootParameter[4].InitAsDescriptorTable(1, &srvTable3);             // t3 DepthAtlas
        slotRootParameter[5].InitAsDescriptorTable(1, &srvTable4);             // t4 CardData
        slotRootParameter[6].InitAsDescriptorTable(1, &srvTable5);             // t5 GDFPageAtlas
        slotRootParameter[7].InitAsDescriptorTable(1, &srvTable6);             // t6 GDFCoverageAtlas
        slotRootParameter[8].InitAsDescriptorTable(1, &srvTable7);             // t7 GDFPageTable
        slotRootParameter[9].InitAsDescriptorTable(1, &srvTable8);             // t8 GDFMipTexture
        slotRootParameter[10].InitAsDescriptorTable(1, &srvTable9);            // t9 AlbedoAtlas
        slotRootParameter[11].InitAsDescriptorTable(1, &srvTable10);           // t10 EmissiveAtlas
        slotRootParameter[12].InitAsDescriptorTable(1, &uavTable0);            // u0
        slotRootParameter[13].InitAsDescriptorTable(1, &uavTable1);            // u1

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(14, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
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
    {   //ClearScreenProbe
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable1;
        uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        CD3DX12_DESCRIPTOR_RANGE uavTable2;
        uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE uavTable3;
        uavTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
        CD3DX12_DESCRIPTOR_RANGE uavTable4;
        uavTable4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
        CD3DX12_DESCRIPTOR_RANGE uavTable5;
        uavTable5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);
        CD3DX12_DESCRIPTOR_RANGE uavTable6;
        uavTable6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);
        CD3DX12_DESCRIPTOR_RANGE uavTable7;
        uavTable7.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 7);
        CD3DX12_DESCRIPTOR_RANGE uavTable8;
        uavTable8.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 8);
        CD3DX12_DESCRIPTOR_RANGE uavTable9;
        uavTable9.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 9);
        CD3DX12_DESCRIPTOR_RANGE uavTable10;
        uavTable10.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 10);

        CD3DX12_ROOT_PARAMETER slotRootParameter[11];
        slotRootParameter[0].InitAsDescriptorTable(1, &uavTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &uavTable1);
        slotRootParameter[2].InitAsDescriptorTable(1, &uavTable2);
        slotRootParameter[3].InitAsDescriptorTable(1, &uavTable3);
        slotRootParameter[4].InitAsDescriptorTable(1, &uavTable4);
        slotRootParameter[5].InitAsDescriptorTable(1, &uavTable5);
        slotRootParameter[6].InitAsDescriptorTable(1, &uavTable6);
        slotRootParameter[7].InitAsDescriptorTable(1, &uavTable7);
        slotRootParameter[8].InitAsDescriptorTable(1, &uavTable8);
        slotRootParameter[9].InitAsDescriptorTable(1, &uavTable9);
        slotRootParameter[10].InitAsDescriptorTable(1, &uavTable10);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(11, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            IID_PPV_ARGS(&mRootSignatures["ClearScreenProbe"])));
    }
    {   //HZB_UAV1
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER slotRootParameter[3];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            IID_PPV_ARGS(&mRootSignatures["HZB_UAV1"])));
    }
    {   //HZB_UAV8
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable1;
        uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        CD3DX12_DESCRIPTOR_RANGE uavTable2;
        uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE uavTable3;
        uavTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
        CD3DX12_DESCRIPTOR_RANGE uavTable4;
        uavTable4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
        CD3DX12_DESCRIPTOR_RANGE uavTable5;
        uavTable5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);
        CD3DX12_DESCRIPTOR_RANGE uavTable6;
        uavTable6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);
        CD3DX12_DESCRIPTOR_RANGE uavTable7;
        uavTable7.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 7);

        CD3DX12_ROOT_PARAMETER slotRootParameter[10];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);
        slotRootParameter[3].InitAsDescriptorTable(1, &uavTable1);
        slotRootParameter[4].InitAsDescriptorTable(1, &uavTable2);
        slotRootParameter[5].InitAsDescriptorTable(1, &uavTable3);
        slotRootParameter[6].InitAsDescriptorTable(1, &uavTable4);
        slotRootParameter[7].InitAsDescriptorTable(1, &uavTable5);
        slotRootParameter[8].InitAsDescriptorTable(1, &uavTable6);
        slotRootParameter[9].InitAsDescriptorTable(1, &uavTable7);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(10, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            IID_PPV_ARGS(&mRootSignatures["HZB_UAV8"])));
    }
    
    {   //HZB_UAV4
        CD3DX12_DESCRIPTOR_RANGE cbvTable0;
        cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE srvTable0;
        srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable0;
        uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable1;
        uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        CD3DX12_DESCRIPTOR_RANGE uavTable2;
        uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
        CD3DX12_DESCRIPTOR_RANGE uavTable3;
        uavTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

        CD3DX12_ROOT_PARAMETER slotRootParameter[6];
        slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
        slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
        slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);
        slotRootParameter[3].InitAsDescriptorTable(1, &uavTable1);
        slotRootParameter[4].InitAsDescriptorTable(1, &uavTable2);
        slotRootParameter[5].InitAsDescriptorTable(1, &uavTable3);

        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            IID_PPV_ARGS(&mRootSignatures["HZB_UAV4"])));
    }
}

static D3DResource* InitBufferResource(ID3D12Device* pDevice, UINT64 inBufferSize, UINT64 inAlignment = 0llu, D3D12_RESOURCE_STATES inInitialState = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS inFlags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
{
    D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
    d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//gpu
    D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
    d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d3d12ResourceDesc.Alignment = inAlignment;
    d3d12ResourceDesc.Width = inBufferSize;
    d3d12ResourceDesc.Height = 1;
    d3d12ResourceDesc.DepthOrArraySize = 1;
    d3d12ResourceDesc.MipLevels = 1;
    d3d12ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    d3d12ResourceDesc.SampleDesc.Count = 1;
    d3d12ResourceDesc.SampleDesc.Quality = 0;
    d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d3d12ResourceDesc.Flags = inFlags;

    ID3D12Resource* bufferObject = nullptr;
    HRESULT hResult = pDevice->CreateCommittedResource(
        &d3dHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &d3d12ResourceDesc,
        inInitialState,
        nullptr,
        IID_PPV_ARGS(&bufferObject)
    );
    D3DResource* resource = new D3DResource(inInitialState);
    resource->mUnderlyingResource = bufferObject;
    return resource;
}

D3DResource* LumenApp::InitBufferFromFile(const wchar_t* resname, const char* file)
{
    size_t fileSize = 0;
    unsigned char* fileContent = d3dUtil::LoadFileContent(file, fileSize);
    ID3D12Resource* uploadBuffer = nullptr;
    D3DResource* res = new D3DResource(D3D12_RESOURCE_STATE_COMMON);
    res->mUnderlyingResource = d3dUtil::CreateDefaultBuffer(
        md3dDevice,
        mCommandList,
        fileContent,
        fileSize,
        uploadBuffer);
    res->mUnderlyingResource->SetName(resname);
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

        mGSDFPageAtlas = D3DImage::InitTextureFromFile(md3dDevice, mCommandList, L"Res/GlobalDistanceField.PageAtlas.dds");
        mGSDFCoverageAtlas = D3DImage::InitTextureFromFile(md3dDevice, mCommandList, L"Res/GlobalDistanceField.CoverageAtlas.dds");
        mGSDFPageTable = D3DImage::InitTextureFromFile(md3dDevice, mCommandList, L"Res/GlobalDistanceField.PageTableCombinedAtlas.dds");
        mGSDFMips = D3DImage::InitTextureFromFile(md3dDevice, mCommandList, L"Res/GlobalDistanceField.SDFMips.dds");
    }
    {   // HZB
        mHZBFurthest = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, { DXGI_FORMAT_R16_FLOAT, {0.0f,0} }, 9
        );
        mHZBFurthest->mUnderlyingResource->SetName(L"HZBFurthest");
        mHZBClosest = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, { DXGI_FORMAT_R16_FLOAT, {0.0f,0} }, 9
        );
        mHZBClosest->mUnderlyingResource->SetName(L"HZBClosest");
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
        mClearCardBuffer->mUnderlyingResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mClearCardBuffer->mUnderlyingResource->SetName(L"ClearCardBuffer");
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
        mRectDataBuffer->mUnderlyingResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mRectDataBuffer->mUnderlyingResource->SetName(L"RectDataBuffer");
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
        mRectUVBuffer->mUnderlyingResource = d3dUtil::CreateDefaultBuffer(
            md3dDevice,
            mCommandList,
            rectData,
            sizeof(rectData),
            uploadBuffer);
        mRectUVBuffer->mUnderlyingResource->SetName(L"RectUVBuffer");
    }
    {
        //(8x8)work group
            //depth32stencil8,revert z
        mSceneDepthZ = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, {0.0f,0} });
        mSceneDepthZ->mUnderlyingResource->SetName(L"SceneDepthZ");
        mLightingChannels = Init2DRTImage(md3dDevice, mCommandList, mClientWidth, mClientHeight, 0,
            DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_UINT,
            DXGI_FORMAT_R8_UINT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8_UINT, {1.0f,1.0f,1.0f,1.0f} });
        mLightingChannels->mUnderlyingResource->SetName(L"LightingChannels");
        mSceneColors[0] = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mSceneColors[0]->mUnderlyingResource->SetName(L"SceneColor[0]");
        mSceneColors[1] = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mSceneColors[1]->mUnderlyingResource->SetName(L"SceneColor[1]");
        mGBufferA = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
            DXGI_FORMAT_R10G10B10A2_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R10G10B10A2_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferA->mUnderlyingResource->SetName(L"GBufferA");
        mGBufferB = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_B8G8R8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferB->mUnderlyingResource->SetName(L"GBufferB");
        mGBufferC = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, {0.0f,0.0f,0.0f,0.0f} });
        mGBufferC->mUnderlyingResource->SetName(L"GBufferC");
        mToneMap = Init2DRTImage(md3dDevice, mCommandList, mClientWidth, mClientHeight, 0,
            DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
            DXGI_FORMAT_R10G10B10A2_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R10G10B10A2_UNORM, {0.0f,0.0f,0.0f,1.0f} });
        mToneMap->mUnderlyingResource->SetName(L"ToneMap");
        mShadowMaskTexture = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
            DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_B8G8R8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mShadowMaskTexture->mUnderlyingResource->SetName(L"ShadowMaskTexture");

        mLumenCardCaptureAlbedoAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureAlbedoAtlas->mUnderlyingResource->SetName(L"Lumen.CardCaptureAlbedoAtlas");
        mLumenCardCaptureNormalAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureNormalAtlas->mUnderlyingResource->SetName(L"Lumen.CardCaptureNormalAtlas");
        mLumenCardCaptureEmissiveAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
            DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,0.0f} });
        mLumenCardCaptureEmissiveAtlas->mUnderlyingResource->SetName(L"Lumen.CardCaptureEmissiveAtlas");
        mLumenCardCaptureDSAtlas = Init2DRTImage(md3dDevice, mCommandList, 512, 512, 0,
            DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, {0.0f,0} });
        mLumenCardCaptureDSAtlas->mUnderlyingResource->SetName(L"Lumen.CardCaptureDepthStencilAtlas");

        mLumenSceneDepth = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
            DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM,
            DXGI_FORMAT_R16_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R16_UNORM, { 1.0f,0.0f,0.0f,1.0f } });
        mLumenSceneDepth->mUnderlyingResource->SetName(L"Lumen.SceneDepth");

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC7_UNORM_SRGB,
                DXGI_FORMAT_BC7_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneAlbedo = Init2DRTImage3(md3dDevice10, mCommandList, 4096, 4096, DXGI_FORMAT_BC7_TYPELESS,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneAlbedo->mUnderlyingResource->SetName(L"Lumen.SceneAlbedo");
        }
        mLumenSceneOpacity = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
            DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
            DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            { DXGI_FORMAT_R8_UNORM, { 0.0f,0.0f,0.0f,0.0f } });
        mLumenSceneOpacity->mUnderlyingResource->SetName(L"Lumen.SceneOpacity");

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneNormal = Init2DRTImage3(md3dDevice10, mCommandList, 4096, 4096, DXGI_FORMAT_BC5_UNORM,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneNormal->mUnderlyingResource->SetName(L"Lumen.SceneNormal");
        }

        {
            DXGI_FORMAT castableFormats[] = {
                DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_R32G32B32A32_UINT
            };
            mLumenSceneEmissive = Init2DRTImage3(md3dDevice10, mCommandList, 4096, 4096, DXGI_FORMAT_BC6H_UF16,
                DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, castableFormats, _countof(castableFormats));
            mLumenSceneEmissive->mUnderlyingResource->SetName(L"Lumen.SceneEmissive");
        }
        //lumen scene lighting
        {
            //direct lighting
            mLumenSceneDirectLighting = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneDirectLighting->mUnderlyingResource->SetName(L"Lumen.SceneDirectLighting");
            mLumenSceneIndirectLighting = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneIndirectLighting->mUnderlyingResource->SetName(L"Lumen.SceneIndirectLighting");
            mLumenSceneNumFramesAccumulatedAtlas = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
                DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R8_UNORM, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneNumFramesAccumulatedAtlas->mUnderlyingResource->SetName(L"Lumen.SceneNumFramesAccumulatedAtlas");
            mLumenSceneFinalLighting = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenSceneFinalLighting->mUnderlyingResource->SetName(L"Lumen.SceneFinalLighting");
            mLumenRadiosityTraceRadianceAtlas = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadiosityTraceRadianceAtlas->mUnderlyingResource->SetName(L"Lumen.Radiosity.TraceRadianceAtlas");
            mLumenRadiosityFilteredTraceRadianceAtlas = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadiosityFilteredTraceRadianceAtlas->mUnderlyingResource->SetName(L"Lumen.Radiosity.FilteredTraceRadianceAtlas");

            mLumenRadiosityProbeSHRedAtlas = Init2DRTImage(md3dDevice, mCommandList, 1024, 1024, 0,
                DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadiosityProbeSHRedAtlas->mUnderlyingResource->SetName(L"Lumen.Radiosity.ProbeSHRedAtlas");
            mLumenRadiosityProbeSHGreenAtlas = Init2DRTImage(md3dDevice, mCommandList, 1024, 1024, 0,
                DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadiosityProbeSHGreenAtlas->mUnderlyingResource->SetName(L"Lumen.Radiosity.ProbeSHGreenAtlas");
            mLumenRadiosityProbeSHBlueAtlas = Init2DRTImage(md3dDevice, mCommandList, 1024, 1024, 0,
                DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R16G16B16A16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadiosityProbeSHBlueAtlas->mUnderlyingResource->SetName(L"Lumen.Radiosity.ProbeSHBlueAtlas");

            mLumenRadianceCacheRadianceProbeAtlasTextureSource = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadianceCacheRadianceProbeAtlasTextureSource->mUnderlyingResource->SetName(L"Lumen.RadianceCache.RadianceProbeAtlasTextureSource");
            mLumenRadianceCacheDepthProbeAtlasTexture = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
                DXGI_FORMAT_R16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R16_FLOAT, {0.0f,0} });
            mLumenRadianceCacheDepthProbeAtlasTexture->mUnderlyingResource->SetName(L"Lumen.RadianceCache.DepthProbeAtlasTexture");
            mLumenRadianceCacheFilteredRadianceProbeAtlasTexture = Init2DRTImage(md3dDevice, mCommandList, 4096, 4096, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadianceCacheFilteredRadianceProbeAtlasTexture->mUnderlyingResource->SetName(L"Lumen.RadianceCache.FilteredRadianceProbeAtlasTexture");
            mLumenRadianceCacheFinalRadianceAtlas = Init2DRTImage(md3dDevice, mCommandList, 4352, 4352, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenRadianceCacheFinalRadianceAtlas->mUnderlyingResource->SetName(L"Lumen.RadianceCache.FinalRadianceAtlas");

            mLumenScreenProbeGatherTraceHit = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
                DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT,
                DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R32_UINT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenScreenProbeGatherTraceHit->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.TraceHit");
            mLumenScreenProbeGatherTraceRadiance = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
                DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
            mLumenScreenProbeGatherTraceRadiance->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.TraceRadiance");
            mLumenScreenProbeGatherScreenProbeHitDistance = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
                DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
                DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R8_UNORM, {0.0f,0.0f,0.0f,1.0f} });
            mLumenScreenProbeGatherScreenProbeHitDistance->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.ScreenProbeHitDistance");
            mLumenScreenProbeGatherScreenProbeTraceMoving = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
                DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
                DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R8_UNORM, {0.0f,0.0f,0.0f,1.0f} });
            mLumenScreenProbeGatherScreenProbeTraceMoving->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.ScreenProbeTraceMoving");
        }
        {   //StochasticLightingDepthHistorys
            mStochasticLightingDepthHistorys[0] = Init2DRTImage(md3dDevice, mCommandList, mClientWidth, mClientHeight, 0,
                DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT,
                DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R32_FLOAT, {1.0f,1.0f,1.0f,1.0f} });
            mStochasticLightingDepthHistorys[0]->mUnderlyingResource->SetName(L"StochasticLighting.DepthHistorys[0]");
            mStochasticLightingDepthHistorys[1] = Init2DRTImage(md3dDevice, mCommandList, mClientWidth, mClientHeight, 0,
                DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT,
                DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                { DXGI_FORMAT_R32_FLOAT, {1.0f,1.0f,1.0f,1.0f} });
            mStochasticLightingDepthHistorys[1]->mUnderlyingResource->SetName(L"StochasticLighting.DepthHistorys[1]");
        }
    }
    {   //probe gather
        //960x540 -> 60 x 34 : 17 => 60 x 51
        mScreenProbeSceneDepth = Init2DRTImage(md3dDevice, mCommandList, 60, 51, 0,
            DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT,
            DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R32_UINT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenProbeSceneDepth->mUnderlyingResource->SetName(L"Lumen.ScreenProbeSceneDepth");
        mScreenProbeWorldSpeed = Init2DRTImage(md3dDevice, mCommandList, 60, 51, 0,
            DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT,
            DXGI_FORMAT_R16_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R16_UINT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenProbeWorldSpeed->mUnderlyingResource->SetName(L"Lumen.ScreenProbeWorldSpeed");
        mScreenProbeWorldNormal = Init2DRTImage(md3dDevice, mCommandList, 60, 51, 0,
            DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM,
            DXGI_FORMAT_R8G8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R8G8_UNORM, {0.0f,0.0f,0.0f,1.0f} });
        mScreenProbeWorldNormal->mUnderlyingResource->SetName(L"Lumen.ScreenProbeWorldNormal");
        mScreenProbeTranslatedWorldPositions[0] = Init2DRTImage(md3dDevice, mCommandList, 60, 51, 0,
            DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R32G32B32A32_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenProbeTranslatedWorldPositions[0]->mUnderlyingResource->SetName(L"Lumen.ScreenProbeTranslatedWorldPosition[0]");
        mScreenProbeTranslatedWorldPositions[1] = Init2DRTImage(md3dDevice, mCommandList, 60, 51, 0,
            DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R32G32B32A32_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenProbeTranslatedWorldPositions[1]->mUnderlyingResource->SetName(L"Lumen.ScreenProbeTranslatedWorldPosition[1]");

        mLumenScreenProbeGatherScreenProbeRadiances[0] = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
            DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
            DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mLumenScreenProbeGatherScreenProbeRadiances[0]->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.ScreenProbeRadiances[0]");
        mLumenScreenProbeGatherScreenProbeRadiances[1] = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
            DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
            DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R11G11B10_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mLumenScreenProbeGatherScreenProbeRadiances[1]->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.ScreenProbeRadiances[1]");
        mScreenTileAdapativeProbeHeader = Init2DRTImage(md3dDevice, mCommandList, 60, 34, 0,
            DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT,
            DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R32_UINT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenTileAdapativeProbeHeader->mUnderlyingResource->SetName(L"Lumen.ScreenTileAdapativeProbeHeader");
        mScreenTileAdapativeProbeIndicies = Init2DRTImage(md3dDevice, mCommandList, 960, 544, 0,
            DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT,
            DXGI_FORMAT_R16_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R16_UINT, {0.0f,0.0f,0.0f,1.0f} });
        mScreenTileAdapativeProbeIndicies->mUnderlyingResource->SetName(L"Lumen.ScreenTileAdapativeProbeIndicies");

        mAdaptiveScreenProbeData = InitBufferResource(md3dDevice, _4MB, 0);
        mAdaptiveScreenProbeData->mUnderlyingResource->SetName(L"Lumen.AdaptiveScreenProbeData");
        mNumAdaptiveScreenProbe = InitBufferResource(md3dDevice, 16, 0);
        mNumAdaptiveScreenProbe->mUnderlyingResource->SetName(L"Lumen.NumAdaptiveScreenProbe");
        mLumenScreenProbeGatherCompactedTraceTexelAllocator = InitBufferResource(md3dDevice, 8, 0);
        mLumenScreenProbeGatherCompactedTraceTexelAllocator->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.CompactedTraceTexelAllocator");
        mLumenScreenProbeGatherCompactedTraceTexelData = InitBufferResource(md3dDevice, _4MB, 0);
        mLumenScreenProbeGatherCompactedTraceTexelData->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.CompactedTraceTexelData");

        mLumenScreenProbeGatherLightingProbabilityDensityFunction = Init2DRTImage(md3dDevice, mCommandList, 480, 408, 0,
            DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
            DXGI_FORMAT_R16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            { DXGI_FORMAT_R16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
        mLumenScreenProbeGatherLightingProbabilityDensityFunction->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.LightingProbabilityDensityFunction");

        mLumenScreenProbeGatherCompactedTraceTexelAllocator = InitBufferResource(md3dDevice, 16, 0);
        mLumenScreenProbeGatherCompactedTraceTexelAllocator->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.CompactedTraceTexelAllocator");
        mLumenScreenProbeGatherCompactedTraceTexelData = InitBufferResource(md3dDevice, _4MB, 0);
        mLumenScreenProbeGatherCompactedTraceTexelData->mUnderlyingResource->SetName(L"Lumen.ScreenProbeGather.CompactedTraceTexelData");
    }
}
