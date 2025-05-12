/*
 * =======================================================================
 *
 * Gyro Space to Play - A plug-and-play Gyro Space Transformer code
 * Version 0.8.6
 *
 * Provides functionality for transforming gyro inputs into Local Space,
 * Player Space, and World Space, while handling sensitivity adjustments,
 * gravity alignment and dynamic orientation.
 *  
 * Compatible with both C and C++ environments.
 *
 * Based on the work by Jibb Smart (JoyShockMapper, GamepadMotionHelpers,
 * Fortnite v.19.30's Gyro Aim/Flick Stick implementation)
 *
 * =======================================================================
 */

 #ifndef GYROSPACE_HPP
 #define GYROSPACE_HPP
 
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Define a small epsilon if not already defined
#ifndef EPSILON
    #define EPSILON 1e-5
#endif

// Type Definitions
typedef struct {
    float x, y, z;
} Vector3;

typedef struct {
    float m[4][4];
} Matrix4;

// Utility Functions

/** Clamps a value between min and max. */
static inline float clamp(float value, float minVal, float maxVal) {
    return (value > maxVal) ? maxVal : (value < minVal) ? minVal : value;
}

/** Creates a new Vector3. */
static inline Vector3 Vec3_New(float x, float y, float z) {
    Vector3 result = { x, y, z };
    return result;
}

/** Adds two vectors. */
static inline Vector3 Vec3_Add(Vector3 a, Vector3 b) {
    return Vec3_New(a.x + b.x, a.y + b.y, a.z + b.z);
}

/** Subtracts vector b from vector a. */
static inline Vector3 Vec3_Subtract(Vector3 a, Vector3 b) {
    return Vec3_New(a.x - b.x, a.y - b.y, a.z - b.z);
}

/** Scales vector v by scalar. */
static inline Vector3 Vec3_Scale(Vector3 v, float scalar) {
    return Vec3_New(v.x * scalar, v.y * scalar, v.z * scalar);
}

/** Returns the dot product of two vectors. */
static inline float Vec3_Dot(Vector3 a, Vector3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

/** Returns the cross product of two vectors. */
static inline Vector3 Vec3_Cross(Vector3 a, Vector3 b) {
    return Vec3_New(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

/** Returns the magnitude of a vector. */
static inline float Vec3_Magnitude(Vector3 v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

/** Normalizes a vector to unit length. Returns (0,0,0) if length is negligible. */
static inline Vector3 Vec3_Normalize(Vector3 v) {
    float len = Vec3_Magnitude(v);
    if (len < EPSILON) return Vec3_New(0.0f, 0.0f, 0.0f);
    return Vec3_Scale(v, 1.0f / len);
}

/** Checks if a vector is near zero. */
static inline bool Vec3_IsZero(Vector3 v) {
    return (fabsf(v.x) < EPSILON && fabsf(v.y) < EPSILON && fabsf(v.z) < EPSILON);
}

/** Linearly interpolates between two vectors. */
static inline Vector3 Vec3_Lerp(Vector3 a, Vector3 b, float t) {
    t = clamp(t, 0.0f, 1.0f);
    return Vec3_Add(a, Vec3_Scale(Vec3_Subtract(b, a), t));
}

/** Reflects vector v about a given normal. */
static inline Vector3 Vec3_Reflect(Vector3 v, Vector3 normal) {
    return Vec3_Subtract(v, Vec3_Scale(normal, 2.0f * Vec3_Dot(v, normal)));
}


// Matrix Operations

/** Returns a 4x4 identity matrix. */
static inline Matrix4 Matrix4_Identity() {
    Matrix4 matrix = { {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    } };
    return matrix;
}

/**
 * Creates a transformation matrix from a normalized gravity vector.
 * The gravity vector defines the "up" (Y-axis) direction.
 */
static inline Matrix4 Matrix4_FromGravity(Vector3 gravNorm) {
    gravNorm = Vec3_Normalize(gravNorm);
    if (Vec3_IsZero(gravNorm))
        return Matrix4_Identity();
    
    Vector3 worldUp = Vec3_New(0.0f, 1.0f, 0.0f);
    Vector3 yAxis = gravNorm;
    Vector3 tempZ = Vec3_Cross(worldUp, yAxis);
    Vector3 zAxis = Vec3_IsZero(tempZ) ? Vec3_New(0.0f, 0.0f, 1.0f) : Vec3_Normalize(tempZ);
    Vector3 xAxis = Vec3_Normalize(Vec3_Cross(yAxis, zAxis));
    
    Matrix4 matrix = { {
        { xAxis.x, xAxis.y, xAxis.z, 0.0f },
        { yAxis.x, yAxis.y, yAxis.z, 0.0f },
        { zAxis.x, zAxis.y, zAxis.z, 0.0f },
        { 0.0f,    0.0f,    0.0f,    1.0f }
    } };
    return matrix;
}

/**
 * Multiplies a 4x4 matrix by a 3D vector (assumes w = 1).
 */
static inline Vector3 MultiplyMatrixVector(Matrix4 matrix, Vector3 vector) {
    float w = 1.0f;
    return Vec3_New(
        matrix.m[0][0]*vector.x + matrix.m[0][1]*vector.y + matrix.m[0][2]*vector.z + matrix.m[0][3]*w,
        matrix.m[1][0]*vector.x + matrix.m[1][1]*vector.y + matrix.m[1][2]*vector.z + matrix.m[1][3]*w,
        matrix.m[2][0]*vector.x + matrix.m[2][1]*vector.y + matrix.m[2][2]*vector.z + matrix.m[2][3]*w
    );
}


// Global Gravity Vector Management

/** Global gravity vector (default: (0, 1, 0)). */
static Vector3 gravNorm = { 0.0f, 1.0f, 0.0f };

/**
 * Updates the global gravity vector using a complementary filter.
 * Combines gyro and accelerometer data.
 */
static inline void UpdateGravityVector(Vector3 accel, Vector3 gyroRotation, float fusionFactor, float deltaTime) {
    if (fusionFactor < 0.0f || fusionFactor > 1.0f)
        return;
    
    if (isnan(accel.x) || isnan(accel.y) || isnan(accel.z) ||
        isnan(gyroRotation.x) || isnan(gyroRotation.y) || isnan(gyroRotation.z))
        return;
    
    Vector3 accelNorm = Vec3_Normalize(accel);
    
    if (Vec3_IsZero(accelNorm)) {
        Vector3 rotationDelta = Vec3_Cross(gyroRotation, gravNorm);
        gravNorm = Vec3_Add(gravNorm, Vec3_Scale(rotationDelta, deltaTime));
    } else {
        Vector3 rotationDelta = Vec3_Cross(gyroRotation, gravNorm);
        Vector3 rotatedGravity = Vec3_Add(gravNorm, Vec3_Scale(rotationDelta, deltaTime));
        gravNorm = Vec3_Lerp(rotatedGravity, accelNorm, fusionFactor);
    }
    
    if (!Vec3_IsZero(gravNorm))
        gravNorm = Vec3_Normalize(gravNorm);
    else
        gravNorm = Vec3_New(0.0f, 1.0f, 0.0f);
}

/** Sets the global gravity vector manually. */
static inline void SetGravityVector(float x, float y, float z) {
    if (isnan(x) || isnan(y) || isnan(z))
        return;
    
    Vector3 newGrav = Vec3_New(x, y, z);
    if (Vec3_Magnitude(newGrav) < EPSILON)
        return;
    
    gravNorm = Vec3_Normalize(newGrav);
}

/** Returns the current global gravity vector. */
static inline Vector3 GetGravityVector(void) {
    return gravNorm;
}
 
 // Dynamic Orientation Adjustment
 
 /**
  * Adapts gyro input for switching between normal controller grip and handheld-style grip.
  * This function takes yaw, pitch, and roll inputs and adjusts them to maintain consistent orientation.
  */
Vector3 TransformWithDynamicOrientation(float yaw_input, float pitch_input, float roll_input) {
    // Validate inputs.
    if (isnan(yaw_input) || isnan(pitch_input) || isnan(roll_input)) {
        return Vec3_New(0.0f, 0.0f, 0.0f);
    }

    // Clamp inputs to prevent extreme values.
    yaw_input   = clamp(yaw_input, -360.0f, 360.0f);
    pitch_input = clamp(pitch_input, -360.0f, 360.0f);
    roll_input  = clamp(roll_input, -360.0f, 360.0f);

    // Combine inputs into a single vector.
    Vector3 combined = Vec3_New(yaw_input, pitch_input, roll_input);
    
    // Remove the gravity component to adjust orientation.
    Vector3 adjusted = Vec3_Subtract(combined, Vec3_Scale(gravNorm, Vec3_Dot(combined, gravNorm)));
    
    // Normalize if necessary.
    if (Vec3_Magnitude(adjusted) > EPSILON)
        adjusted = Vec3_Normalize(adjusted);
    
    // Dynamic roll compensation based on gravity.
    float adjRoll = roll_input - Vec3_Dot(gravNorm, Vec3_New(0.0f, 0.0f, roll_input));
    
    return Vec3_New(adjusted.x, adjusted.y, adjRoll);
}
 
 // Gyro Space Transformation Function
 
 /**
  * These functions transform gyro inputs into different spaces (Local, Player, World)
  * while handling sensitivity adjustments and gravity alignment.
  */

 /**
  * Transforms gyro inputs to Local Space.
  *
  * Converts raw gyro input into direct motion scaling.
  * Preserves natural gyro responsiveness for intuitive movement.
  */
Vector3 TransformToLocalSpace(float yaw, float pitch, float roll, float couplingFactor) {
    // Adjust roll to compensate for yaw–roll coupling.
    // The rollCompensation factor reduces the influence of roll.
    float rollCompensation = 0.85f;
    float adjustedRoll = (roll * rollCompensation) - (yaw * couplingFactor);
    
    // Combine the inputs into a local gyro vector.
    // Here, yaw is reduced by the corrected roll and pitch remains unchanged.
    Vector3 localGyro = Vec3_New(yaw - adjustedRoll, pitch, adjustedRoll);
    
    return localGyro;
}
 
 /**
  * Transforms gyro inputs to Player Space.
  * Adjusts motion relative to the player's perspective while ensuring gravity alignment.
  */
Vector3 TransformToPlayerSpace(float yaw, float pitch, float roll, Vector3 gravity) {
    // Validate and normalize the gravity vector.
    if (Vec3_IsZero(gravity))
        gravity = Vec3_New(0.0f, 1.0f, 0.0f);
    else
        gravity = Vec3_Normalize(gravity);

    // Compute world-aligned yaw using gravity (mixes yaw and roll inputs).
    float worldYaw = yaw * gravity.y + roll * gravity.z;

    // Adjust roll to remove the unwanted influence from gravity.
    float adjustedRoll = roll - (roll * gravity.y);

    // Apply yaw relaxation to smooth out abrupt inversions.
    float yawRelaxation = 1.41f;
    float yawSign = (worldYaw >= 0) ? 1.0f : -1.0f;
    float yawMagnitude = Vec3_Magnitude(Vec3_New(yaw, adjustedRoll, 0.0f));
    float adjustedYaw = yawSign * fminf(fabsf(worldYaw) * yawRelaxation, yawMagnitude);

    // Transform the computed gyro vector into Player Space using a view matrix.
    Matrix4 playerViewMatrix = Matrix4_FromGravity(gravity);
    Vector3 playerGyro = MultiplyMatrixVector(playerViewMatrix, Vec3_New(adjustedYaw, pitch, adjustedRoll));

    return playerGyro;
}
 
 /**
  * Transforms gyro inputs to World Space.
  * Aligns input with the game world while maintaining spatial consistency.
  */
Vector3 TransformToWorldSpace(float yaw, float pitch, float roll, Vector3 gravity) {
    // Validate and normalize gravity.
    if (Vec3_IsZero(gravity)) {
        gravity = Vec3_New(0.0f, 1.0f, 0.0f);
    } else {
        gravity = Vec3_Normalize(gravity);
    }
    
    // Compute world yaw by mixing yaw and roll according to gravity.
    float worldYaw = yaw * gravity.y + roll * gravity.z;
    
    // Adjust roll to remove the component influenced by gravity.
    float adjustedRoll = roll - (roll * gravity.y);
    
    // Project the controller's local pitch axis (1, 0, 0) onto the horizontal plane.
    float gravDotPitch = Vec3_Dot(gravity, Vec3_New(1.0f, 0.0f, 0.0f));
    Vector3 pitchVector = Vec3_Subtract(Vec3_New(1.0f, 0.0f, 0.0f), Vec3_Scale(gravity, gravDotPitch));
    if (!Vec3_IsZero(pitchVector)) {
        pitchVector = Vec3_Normalize(pitchVector);
    }
    
    // Compute adjusted pitch based on the projected pitch axis.
    float adjustedPitch = Vec3_Dot(Vec3_New(pitch, 0.0f, adjustedRoll), pitchVector);
    
    // Transform the computed components into world space.
    Matrix4 worldViewMatrix = Matrix4_FromGravity(gravity);
    Vector3 worldGyro = MultiplyMatrixVector(worldViewMatrix, Vec3_New(worldYaw, adjustedPitch, adjustedRoll));
    
    return worldGyro;
}
 
#ifdef __cplusplus
}
#endif
 
#endif // GYROSPACE_H