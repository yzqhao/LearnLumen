
#include "Camera.h"

Camera::Camera()
{
	SetLens(0.25f*M_PI, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

Math::Vec3 Camera::GetPosition()const
{
	return Math::Vec3(mPosition.x, mPosition.y, mPosition.z);
}

void Camera::SetPosition(float x, float y, float z)
{
	mPosition.Set(x, y, z);
	mViewDirty = true;
}

void Camera::SetPosition(const Math::Vec3& v)
{
	mPosition = v;
	mViewDirty = true;
}

Math::Vec3 Camera::GetRight()const
{
	return mRight;
}

Math::Vec3 Camera::GetUp()const
{
	return mUp;
}

Math::Vec3 Camera::GetLook()const
{
	return mLook;
}

float Camera::GetNearZ()const
{
	return mNearZ;
}

float Camera::GetFarZ()const
{
	return mFarZ;
}

float Camera::GetAspect()const
{
	return mAspect;
}

float Camera::GetFovY()const
{
	return mFovY;
}

float Camera::GetFovX()const
{
	float halfWidth = 0.5f*GetNearWindowWidth();
	return 2.0f*atan(halfWidth / mNearZ);
}

float Camera::GetNearWindowWidth()const
{
	return mAspect * mNearWindowHeight;
}

float Camera::GetNearWindowHeight()const
{
	return mNearWindowHeight;
}

float Camera::GetFarWindowWidth()const
{
	return mAspect * mFarWindowHeight;
}

float Camera::GetFarWindowHeight()const
{
	return mFarWindowHeight;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	// cache properties
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf( 0.5f*mFovY );
	mFarWindowHeight  = 2.0f * mFarZ * tanf( 0.5f*mFovY );

	Math::Mat4::CreatePerspective(MATH_RAD_TO_DEG(mFovY), mAspect, mNearZ, mFarZ, &mProj);

	// ¶ÔÆëthe-forge
	mProj = Math::Mat4::IDENTITY;
	mProj.a22 = 1.777777779;
	mProj.a33 = 1.00009990;
	mProj.a43 = -0.111;
	mProj.a34 = 1.0;
	mProj.a44 = 0.0;
}

void Camera::LookAt(const Math::Vec3& pos, const Math::Vec3& target, const Math::Vec3& up)
{
	Math::Vec3 L = (target - pos).GetNormalized();
	Math::Vec3 R = Math::CrossProduct(up, L).GetNormalized();
	Math::Vec3 U = Math::CrossProduct(L, R);

	SetPosition(pos.x, pos.y, pos.z);
	mLook = L;
	mRight = R;
	mUp = U;

	mViewDirty = true;
}

Math::Mat4 Camera::GetView()const
{
	assert(!mViewDirty);
	return mView;
}

Math::Mat4 Camera::GetProj()const
{
	return mProj;
}

void Camera::Strafe(float d)
{
	mPosition += mRight * d;

	mViewDirty = true;
}

void Camera::Walk(float d)
{
	mPosition += mLook * d;

	mViewDirty = true;
}

void Camera::Pitch(float angle)
{
	// Rotate up and look vector about the right vector.

	Math::Mat4 R;
	R.Rotate(mRight, angle);

	mUp *= R;
	mLook *= R;

	mViewDirty = true;
}

void Camera::RotateY(float angle)
{
	// Rotate the basis vectors about the world y-axis.

	Math::Mat4 R;
	R.RotateY(angle);

	mRight *= R;
	mUp *= R;
	mLook *= R;

	mViewDirty = true;
}

void Camera::Roll(float angle)
{
	// Rotate the basis vectors about the world y-axis.

	Math::Mat4 R;
	R.RotateY(angle);

	mRight *= R;
	mUp *= R;

	mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
	if(mViewDirty)
	{
		Math::Vec3 R = mRight;
		Math::Vec3 U = mUp;
		Math::Vec3 L = mLook;
		Math::Vec3 P = mPosition;

		// Keep camera's axes orthogonal to each other and of unit length.
		L.Normalize();
		U = Math::CrossProduct(L, R).GetNormalized();

		// U, L already ortho-normal, so no need to normalize cross product.
		R = Math::CrossProduct(U, L);

		// Fill in the view matrix entries.
		float x = -Math::Dot(R, P);
		float y = -Math::Dot(U, P);
		float z = -Math::Dot(L, P);

		mRight = R;
		mUp = U;
		mLook = L;

		mView.a11 = mRight.x;
		mView.a21 = mRight.y;
		mView.a31 = mRight.z;
		mView.a41 = x;

		mView.a12 = mUp.x;
		mView.a22 = mUp.y;
		mView.a32 = mUp.z;
		mView.a42 = y;

		mView.a13 = mLook.x;
		mView.a23 = mLook.y;
		mView.a33 = mLook.z;
		mView.a43 = z;

		mView.a14 = 0.0f;
		mView.a24 = 0.0f;
		mView.a34 = 0.0f;
		mView.a44 = 1.0f;

		mViewDirty = false;
	}
}


