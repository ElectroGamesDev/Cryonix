#include "Maths.h"

namespace cl
{
    Matrix4 Matrix4::Perspective(float fov, float aspect, float nearPlane, float farPlane)
    {
        Matrix4 result;
        float rad = fov * PI / 180.0f;
        float tanHalfFov = std::tanf(rad * 0.5f);

        result.m[0] = 1.0f / (aspect * tanHalfFov);
        result.m[5] = 1.0f / tanHalfFov;
        result.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result.m[11] = -1.0f;
        result.m[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
        result.m[15] = 0.0f;

        return result;
    }

    Matrix4 Matrix4::LookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        Vector3 zAxis = (eye - target).Normalize();
        Vector3 xAxis = Vector3::Cross(up, zAxis).Normalize();
        Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

        Matrix4 result;
        result.m[0] = xAxis.x;
        result.m[1] = yAxis.x;
        result.m[2] = zAxis.x;
        result.m[3] = 0.0f;

        result.m[4] = xAxis.y;
        result.m[5] = yAxis.y;
        result.m[6] = zAxis.y;
        result.m[7] = 0.0f;

        result.m[8] = xAxis.z;
        result.m[9] = yAxis.z;
        result.m[10] = zAxis.z;
        result.m[11] = 0.0f;

        result.m[12] = -Vector3::Dot(xAxis, eye);
        result.m[13] = -Vector3::Dot(yAxis, eye);
        result.m[14] = -Vector3::Dot(zAxis, eye);
        result.m[15] = 1.0f;

        return result;
    }

    Matrix4 Matrix4::Translate(const Vector3& translation)
    {
        Matrix4 result;
        result.m[12] = translation.x;
        result.m[13] = translation.y;
        result.m[14] = translation.z;
        return result;
    }

    Matrix4 Matrix4::RotateX(float angle)
    {
        // Angle in degrees, convert to radians
        float rad = angle * PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        Matrix4 result = Identity();
        result.m[5] = c;
        result.m[6] = s;
        result.m[9] = -s;
        result.m[10] = c;
        return result;
    }

    Matrix4 Matrix4::RotateY(float angle)
    {
        // Angle in degrees, convert to radians
        float rad = angle * PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        Matrix4 result = Identity();
        result.m[0] = c;
        result.m[2] = -s;
        result.m[8] = s;
        result.m[10] = c;
        return result;
    }

    Matrix4 Matrix4::RotateZ(float angle)
    {
        // Angle in degrees, convert to radians
        float rad = angle * PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        Matrix4 result = Identity();
        result.m[0] = c;
        result.m[1] = s;
        result.m[4] = -s;
        result.m[5] = c;
        return result;
    }

    Matrix4 Matrix4::RotateYawPitchRoll(float yaw, float pitch, float roll)
    {
        // Yaw (Y), Pitch (X), Roll (Z) - all in degrees
        // Combined rotation: Ry * Rx * Rz
        Matrix4 rotY = RotateY(yaw);
        Matrix4 rotX = RotateX(pitch);
        Matrix4 rotZ = RotateZ(roll);

        return rotY * rotX * rotZ;
    }

    Matrix4 Matrix4::Rotate(float angle, const Vector3& axis)
    {
        float rad = angle * PI / 180.0f;

        float c = std::cosf(rad);
        float s = std::sinf(rad);
        float t = 1.0f - c;

        Vector3 a = axis.Normalize();

        Matrix4 result;
        result.m[0] = t * a.x * a.x + c;
        result.m[1] = t * a.x * a.y + s * a.z;
        result.m[2] = t * a.x * a.z - s * a.y;
        result.m[3] = 0.0f;

        result.m[4] = t * a.x * a.y - s * a.z;
        result.m[5] = t * a.y * a.y + c;
        result.m[6] = t * a.y * a.z + s * a.x;
        result.m[7] = 0.0f;

        result.m[8] = t * a.x * a.z + s * a.y;
        result.m[9] = t * a.y * a.z - s * a.x;
        result.m[10] = t * a.z * a.z + c;
        result.m[11] = 0.0f;

        result.m[12] = 0.0f;
        result.m[13] = 0.0f;
        result.m[14] = 0.0f;
        result.m[15] = 1.0f;

        return result;
    }

    Matrix4 Matrix4::Scale(const Vector3& scale)
    {
        Matrix4 result;
        result.m[0] = scale.x;
        result.m[5] = scale.y;
        result.m[10] = scale.z;
        return result;
    }

    Matrix4 Matrix4::FromQuaternion(const Quaternion& q)
    {
        Matrix4 result;

        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;

        result.m[0] = 1.0f - 2.0f * (yy + zz);
        result.m[1] = 2.0f * (xy + wz);
        result.m[2] = 2.0f * (xz - wy);
        result.m[3] = 0.0f;

        result.m[4] = 2.0f * (xy - wz);
        result.m[5] = 1.0f - 2.0f * (xx + zz);
        result.m[6] = 2.0f * (yz + wx);
        result.m[7] = 0.0f;

        result.m[8] = 2.0f * (xz + wy);
        result.m[9] = 2.0f * (yz - wx);
        result.m[10] = 1.0f - 2.0f * (xx + yy);
        result.m[11] = 0.0f;

        result.m[12] = 0.0f;
        result.m[13] = 0.0f;
        result.m[14] = 0.0f;
        result.m[15] = 1.0f;

        return result;
    }

    Vector3 Matrix4::TransformPoint(const Vector3& v) const
    {
        return Vector3(
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
        );
    }

    Vector3 Matrix4::TransformDirection(const Vector3& v) const
    {
        return Vector3(
            m[0] * v.x + m[4] * v.y + m[8] * v.z,
            m[1] * v.x + m[5] * v.y + m[9] * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z
        );
    }

    Matrix4 Matrix4::operator*(const Matrix4& other) const
    {
        Matrix4 result;

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += m[k * 4 + row] * other.m[col * 4 + k];

                result.m[col * 4 + row] = sum;
            }
        }

        return result;
    }

    Quaternion Quaternion::Normalize() const
    {
        float len = std::sqrtf(x * x + y * y + z * z + w * w);
        if (len > 1e-6f)
            return Quaternion(x / len, y / len, z / len, w / len);

        return Quaternion(0, 0, 0, 1);
    }

    Quaternion Quaternion::operator*(const Quaternion& other) const
    {
        return Quaternion(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
    }

    Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float degrees)
    {
        float rad = degrees * PI / 180.0f * 0.5f;
        float s = std::sinf(rad);
        Vector3 a = axis.Normalize();
        return Quaternion(a.x * s, a.y * s, a.z * s, std::cosf(rad));
    }

    Quaternion Quaternion::FromEuler(float yaw, float pitch, float roll)
    {
        Quaternion qY = FromAxisAngle(Vector3(0, 1, 0), yaw);
        Quaternion qX = FromAxisAngle(Vector3(1, 0, 0), pitch);
        Quaternion qZ = FromAxisAngle(Vector3(0, 0, 1), roll);
        return qY * qX * qZ;
    }

    Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion result;

        float cosHalfTheta = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        Quaternion b_adjusted = b;
        if (cosHalfTheta < 0.0f)
        {
            b_adjusted.x = -b.x;
            b_adjusted.y = -b.y;
            b_adjusted.z = -b.z;
            b_adjusted.w = -b.w;
            cosHalfTheta = -cosHalfTheta;
        }

        if (cosHalfTheta > 0.9995f)
        {
            result.x = a.x + (b_adjusted.x - a.x) * t;
            result.y = a.y + (b_adjusted.y - a.y) * t;
            result.z = a.z + (b_adjusted.z - a.z) * t;
            result.w = a.w + (b_adjusted.w - a.w) * t;
            return result.Normalize();
        }

        float halfTheta = acosf(cosHalfTheta);
        float sinHalfTheta = sqrtf(1.0f - cosHalfTheta * cosHalfTheta);

        if (fabsf(sinHalfTheta) < 0.001f)
        {
            result.x = (a.x + b_adjusted.x) * 0.5f;
            result.y = (a.y + b_adjusted.y) * 0.5f;
            result.z = (a.z + b_adjusted.z) * 0.5f;
            result.w = (a.w + b_adjusted.w) * 0.5f;
            return result;
        }

        float ratioA = sinf((1.0f - t) * halfTheta) / sinHalfTheta;
        float ratioB = sinf(t * halfTheta) / sinHalfTheta;

        result.x = a.x * ratioA + b_adjusted.x * ratioB;
        result.y = a.y * ratioA + b_adjusted.y * ratioB;
        result.z = a.z * ratioA + b_adjusted.z * ratioB;
        result.w = a.w * ratioA + b_adjusted.w * ratioB;

        return result;
    }

    Vector3 Quaternion::ToEuler() const
    {
        Matrix4 mat = ToMatrix();
        float r32 = mat.m[6];
        float phi_rad; // roll (z)
        float theta_rad; // pitch (x)
        float psi_rad; // yaw (y)
        float eps = 1e-6f;
        float cos_phi = std::sqrtf(1.0f - r32 * r32);
        if (cos_phi < eps)
        {
            phi_rad = (PI / 2.0f) * (r32 > 0.0f ? 1.0f : -1.0f);
            theta_rad = 0.0f; // Arbitrary
            if (r32 > 0.0f)
                psi_rad = std::atan2f(mat.m[1], mat.m[0]) - theta_rad;
            else
                psi_rad = std::atan2f(mat.m[1], mat.m[0]) + theta_rad;
        }
        else
        {
            phi_rad = std::asinf(r32);
            theta_rad = std::atan2f(-mat.m[2] / cos_phi, mat.m[10] / cos_phi);
            psi_rad = std::atan2f(-mat.m[4] / cos_phi, mat.m[5] / cos_phi);
        }
        return Vector3(
            theta_rad * 180.0f / PI, // pitch (x)
            psi_rad * 180.0f / PI, // yaw (y)
            phi_rad * 180.0f / PI // roll (z)
        );
    }

    Matrix4 Quaternion::ToMatrix() const
    {
        Quaternion q = Normalize();
        float xx = q.x * q.x;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float xw = q.x * q.w;
        float yy = q.y * q.y;
        float yz = q.y * q.z;
        float yw = q.y * q.w;
        float zz = q.z * q.z;
        float zw = q.z * q.w;

        Matrix4 result = Matrix4::Identity();
        result.m[0] = 1 - 2 * (yy + zz);
        result.m[1] = 2 * (xy + zw);
        result.m[2] = 2 * (xz - yw);
        result.m[4] = 2 * (xy - zw);
        result.m[5] = 1 - 2 * (xx + zz);
        result.m[6] = 2 * (yz + xw);
        result.m[8] = 2 * (xz + yw);
        result.m[9] = 2 * (yz - xw);
        result.m[10] = 1 - 2 * (xx + yy);

        return result;
    }
}