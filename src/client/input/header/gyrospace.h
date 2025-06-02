/*
 * =======================================================================
 *
 * Gyro Space to Play - A plug-and-play Gyro Space Transformer code
 * Version 0.9.1-preview
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

// Global Gravity Vector Management

/** Global gravity vector (default: (0, 1, 0)). */
static Vector3 gravNorm = { 0.0f, 1.0f, 0.0f };

/** Sets the global gravity vector manually (should be called with raw accel data). */
static inline void SetGravityVector(float x, float y, float z) {
    if (isnan(x) || isnan(y) || isnan(z))
        return;

    Vector3 newGrav = Vec3_New(x, y, z);
    if (Vec3_Magnitude(newGrav) < EPSILON)
        return;

    // Prevent gravity from being exactly (0,0,1) unless explicitly allowed
    if (fabsf(newGrav.x) < EPSILON && fabsf(newGrav.y) < EPSILON && fabsf(newGrav.z - 1.0f) < EPSILON) {
        // Fallback to Y-up
        gravNorm = Vec3_New(0.0f, 1.0f, 0.0f);
        return;
    }

    gravNorm = Vec3_Normalize(newGrav);
}

/**
 * Updates the global gravity vector using the raw accelerometer vector.
 * This is a generic approach for all input APIs.
 */
static inline void UpdateGravityVector(Vector3 accel, Vector3 gyroRotation, float fusionFactor, float deltaTime) {
    // Use only the raw accelerometer vector for gravity
    SetGravityVector(accel.x, accel.y, accel.z);
}

/** Returns the current global gravity vector. */
static inline Vector3 GetGravityVector(void) {
    return gravNorm;
}
 
 // Gyro Space Transformation Function
 
 /**
  * These functions transform gyro inputs into different spaces (Local, Player, World)
  * while handling sensitivity adjustments and gravity alignment.
  */

/* Transforms gyro inputs to Local Space.
 *
 * Converts raw gyro input into direct motion scaling.
 * Preserves natural gyro responsiveness for intuitive movement.
 */
static inline Vector3 TransformToLocalSpace(float yaw, float pitch, float roll, float couplingFactor) {
    // Adjust roll to compensate for yaw–roll coupling, accounting for Natural Sensitivity Scale.
    float adjustedRoll = (roll * 0.85f) - (yaw * couplingFactor);
    
    // Combine the inputs into a local gyro vector.
    Vector3 localGyro = Vec3_New(yaw - adjustedRoll, pitch, adjustedRoll);
    
    return localGyro;
}
 
/**
 * Transforms gyro inputs to Player Space.
 * Adjusts motion relative to the player's perspective while ensuring gravity alignment.
 */
static inline Vector3 TransformToPlayerSpace(float yaw, float pitch, float roll) {

    //  Player space yaw: combine yaw and roll, use gravity for direction 
    float worldYaw = yaw * gravNorm.y + roll * gravNorm.z;

    //  Yaw relaxation: buffer zone for local aiming freedom 
    float yawRelaxFactor = 2.0f; // 1.41f for ~45°, 2.0f for ~60° buffer
    float yawSign = (worldYaw >= 0) ? 1.0f : -1.0f;
    float combinedYawRoll = sqrtf(yaw * yaw + roll * roll);
    float adjustedYaw = yawSign * fminf(fabsf(worldYaw) * yawRelaxFactor, combinedYawRoll);

    //  Local pitch: use directly 
    float adjustedPitch = pitch;

    Vector3 playerGyro = Vec3_New(adjustedYaw, adjustedPitch, 0);
    return playerGyro;
}
 
/**
 * Transforms gyro inputs to World Space.
 * Aligns input with the game world while maintaining spatial consistency.
 */
static inline Vector3 TransformToWorldSpace(float yaw, float pitch, float roll) {
    Vector3 gyro = Vec3_New(yaw, pitch, roll);
    Vector3 gravity = GetGravityVector();
    gravity = Vec3_Normalize(gravity);

    // World axes
    Vector3 worldFwd = Vec3_New(0.0f, 0.0f, 1.0f); // Z+
    if (fabsf(Vec3_Dot(gravity, worldFwd)) > 0.99f) {
        worldFwd = Vec3_New(1.0f, 0.0f, 0.0f); // X+ fallback
    }

    Vector3 worldRight = Vec3_Normalize(Vec3_Cross(gravity, worldFwd));
    Vector3 worldFwdProj = Vec3_Normalize(Vec3_Cross(worldRight, gravity));

    // Calculate world axes rotations
    float worldYaw   = Vec3_Dot(gyro, gravity);
    float worldPitch = Vec3_Dot(gyro, worldRight);
    float worldRoll  = Vec3_Dot(gyro, worldFwdProj);

    // Output as (pitch, yaw, roll) for typical FPS/game engines
    return Vec3_New(worldPitch, worldYaw, worldRoll);
}
 
#ifdef __cplusplus
}
#endif
 
#endif // GYROSPACE_HPP