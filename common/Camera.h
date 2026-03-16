
#ifndef CAMERA_H
#define CAMERA_H

#include "d3dUtil.h"

class Camera
{
public:

	Camera();
	~Camera();

	// Get/Set world camera position.
	Math::Vec3 GetPosition()const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const Math::Vec3& v);

	// Get camera basis vectors.
	Math::Vec3 GetRight()const;
	Math::Vec3 GetUp()const;
	Math::Vec3 GetLook()const;

	// Get frustum properties.
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;

	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(const Math::Vec3& pos, const Math::Vec3& target, const Math::Vec3& up);

	// Get View/Proj matrices.
	Math::Mat4 GetView()const;
	Math::Mat4 GetProj()const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void RotateY(float angle);
	void Roll(float angle);		// 习题，游戏中不常用

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

private:

	// Camera coordinate system with coordinates relative to world space.
	Math::Vec3 mPosition = { 0.0f, 0.0f, 0.0f };
	Math::Vec3 mRight = { 1.0f, 0.0f, 0.0f };
	Math::Vec3 mUp = { 0.0f, 1.0f, 0.0f };
	Math::Vec3 mLook = { 0.0f, 0.0f, 1.0f };

	// Cache frustum properties.
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	// Cache View/Proj matrices.
	Math::Mat4 mView{};
	Math::Mat4 mProj{};
};

#endif // CAMERA_H