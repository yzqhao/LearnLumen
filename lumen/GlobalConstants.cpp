#include "GlobalConstants.h"

void GlobalConstants::SetProjectionMatrix(float* inMatrix)
{
    memcpy(mProjectionMatrix, inMatrix, sizeof(mProjectionMatrix));
}
void GlobalConstants::SetViewMatrix(float* inMatrix)
{
    memcpy(mViewMatrix, inMatrix, sizeof(mViewMatrix));
}
void GlobalConstants::SetWorldToClipMatrix(float* inMatrix)
{
    memcpy(mWorldToClipMatrix, inMatrix, sizeof(mWorldToClipMatrix));
}
void GlobalConstants::SetCameraPositionHighWS(float inX, float inY, float inZ, float inW)
{
    mCameraPositionHighWS[0] = inX;
    mCameraPositionHighWS[1] = inY;
    mCameraPositionHighWS[2] = inZ;
    mCameraPositionHighWS[3] = inW;
}
void GlobalConstants::SetCameraPositionLowWS(float inX, float inY, float inZ, float inW)
{
    mCameraPositionLowWS[0] = inX;
    mCameraPositionLowWS[1] = inY;
    mCameraPositionLowWS[2] = inZ;
    mCameraPositionLowWS[3] = inW;
}
void GlobalConstants::SetViewDirectionWS(float inX, float inY, float inZ, float inW)
{
    mViewDirectionWS[0] = inX;
    mViewDirectionWS[1] = inY;
    mViewDirectionWS[2] = inZ;
    mViewDirectionWS[3] = inW;
}
void GlobalConstants::SetViewRightWS(float inX, float inY, float inZ, float inW)
{
    mViewRightWS[0] = inX;
    mViewRightWS[1] = inY;
    mViewRightWS[2] = inZ;
    mViewRightWS[3] = inW;
}
void GlobalConstants::SetViewUpWS(float inX, float inY, float inZ, float inW)
{
    mViewUpWS[0] = inX;
    mViewUpWS[1] = inY;
    mViewUpWS[2] = inZ;
    mViewUpWS[3] = inW;
}
void GlobalConstants::SetScreenToTranslatedWorldMatrix(float* inMatrix)
{
    memcpy(mScreenToTranslatedWorld, inMatrix, sizeof(mScreenToTranslatedWorld));
}