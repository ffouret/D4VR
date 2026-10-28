#pragma once
// Row-major matrices, column vectors, right-handed view space looking down -Z.
// The renderer adapter supplies eye poses/projections in that convention.
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace d4vr {
using Vector4 = std::array<double, 4>;
using Matrix4 = std::array<double, 16>;

inline Matrix4 identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
inline void finite(const Matrix4& matrix) {
    for (double value : matrix)
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite camera matrix");
}
inline Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
    finite(a); finite(b);
    Matrix4 result{};
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            for (unsigned k = 0; k < 4; ++k)
                result[row * 4 + column] += a[row * 4 + k] * b[k * 4 + column];
    finite(result);
    return result;
}
inline Vector4 transform(const Matrix4& matrix, const Vector4& vector) {
    finite(matrix);
    Vector4 result{};
    for (double value : vector)
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite position");
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned k = 0; k < 4; ++k)
            result[row] += matrix[row * 4 + k] * vector[k];
    for (double value : result)
        if (!std::isfinite(value)) throw std::invalid_argument("Position overflow");
    return result;
}
inline Matrix4 inverse(const Matrix4& matrix) {
    finite(matrix);
    std::array<std::array<double, 8>, 4> augmented{};
    double scale = 0;
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            double value = matrix[row * 4 + column];
            augmented[row][column] = value;
            scale = std::max(scale, std::abs(value));
        }
        augmented[row][row + 4] = 1;
    }
    for (unsigned column = 0; column < 4; ++column) {
        unsigned pivot = column;
        for (unsigned row = column + 1; row < 4; ++row)
            if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column])) pivot = row;
        if (std::abs(augmented[pivot][column]) <= scale * 1e-12)
            throw std::invalid_argument("Singular or ill-conditioned camera matrix");
        std::swap(augmented[column], augmented[pivot]);
        double divisor = augmented[column][column];
        for (double& value : augmented[column]) value /= divisor;
        for (unsigned row = 0; row < 4; ++row) {
            if (row == column) continue;
            double factor = augmented[row][column];
            for (unsigned k = 0; k < 8; ++k) augmented[row][k] -= factor * augmented[column][k];
        }
    }
    Matrix4 result{};
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            result[row * 4 + column] = augmented[row][column + 4];
    finite(result);
    return result;
}

// Tangents of the four optical angles. Clip depth is [0,1]; Y points up.
inline Matrix4 perspective(double left, double right, double down, double up, double near_z, double far_z) {
    for (double value : {left, right, down, up, near_z, far_z})
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite field of view");
    if (right <= left || up <= down || near_z <= 0 || far_z <= near_z)
        throw std::invalid_argument("Invalid field of view or clipping planes");
    Matrix4 result{2 / (right - left), 0, (right + left) / (right - left), 0,
                   0, 2 / (up - down), (up + down) / (up - down), 0,
                   0, 0, -far_z / (far_z - near_z), -far_z * near_z / (far_z - near_z),
                   0, 0, -1, 0};
    finite(result);
    return result;
}

// Apply to geometry clip coordinates BEFORE rasterization. No image/depth
// reprojection is performed. This does not duplicate game draws or submit XR.
inline Matrix4 eye_clip_from_game_clip(const Matrix4& game_projection,
                                       const Matrix4& eye_projection,
                                       const Matrix4& eye_from_game_view) {
    return multiply(multiply(eye_projection, eye_from_game_view), inverse(game_projection));
}
} // namespace d4vr
