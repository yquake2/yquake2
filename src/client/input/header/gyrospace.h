/*
 * =======================================================================
 *
 * Gyro Space to Play - A plug-and-play Gyro Space Transformer code
 * Version 0.8.0
 *
 * Provides functionality for transforming gyro inputs into Local Space,
 * Player Space, and World Space, while handling sensitivity adjustments
 * and gravity alignment. Compatible with both C and C++ environments.
 *
 * Based on the work by Jibb Smart (JoyShockMapper, GamepadMotionHelpers,
 * Fortnite v.19.30's Gyro Aim/Flick Stick implementation)
 *
 * =======================================================================
 */

 #ifndef GYROSPACE_HPP
 #define GYROSPACE_HPP
 
 #ifdef __cplusplus
 #include <cmath>
 #include <cstdio>
 #include <cstdbool>
 #include <cstdint>
 extern "C" {
 #else
 #include "math.h"
 #include "stdbool.h"
 #include "stdint.h"
 #endif
 
  // Ensure `s32` and `u32` are correctly defined
 #ifndef s32
 #define s32 int32_t
 #endif
 
 #ifndef u32
 #define u32 uint32_t
 #endif
 
 // Apply correct boolean type definitions
 #ifndef bool
 #define bool s32
 #endif
 
 #ifndef ubool
 #define ubool u32
 #endif
 
 #ifndef EPSILON
 #define EPSILON 1e-5
 #endif
 
  // Debugging and Logging 
 #ifdef ENABLE_DEBUG_LOGS
 #ifdef __cplusplus
 #include <iostream>
 #define DEBUG_LOG(fmt, ...) std::cout << fmt
 #else
 #define DEBUG_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
 #endif
 #else
 #define DEBUG_LOG(fmt, ...)
 #endif
 
 // Type Definitions
 
 typedef struct {
 float x, y, z;
 } Vector3;
 
 typedef struct {
 float m[4][4];
 } Matrix4;
 
 // Utility Functions
 
 /* Scalar Operations */
 
 /**
  * Clamps a value between a minimum and maximum.
  */
 static inline float clamp(float value, float min, float max) {
 return (value > max) ? max : (value < min) ? min : value;
 }
 
 /* Vector Creation */
 
 /**
  * Creates a new vector with given x, y, z values.
  */
 static inline Vector3 Vec3_New(float x, float y, float z) {
     Vector3 result;
     result.x = x;
     result.y = y;
     result.z = z;
     return result;
 }
 
 /* Basic Vector Operations */
 
 /**
  * Adds two vectors component-wise.
  */
 static inline Vector3 Vec3_Add(Vector3 a, Vector3 b) {
 return Vec3_New(a.x + b.x, a.y + b.y, a.z + b.z);
 }
 
 /**
  * Subtracts one vector from another.
  */
 static inline Vector3 Vec3_Subtract(Vector3 a, Vector3 b) {
 return Vec3_New(a.x - b.x, a.y - b.y, a.z - b.z);
 }
 
 /**
  * Scales a vector by a scalar value.
  */
 static inline Vector3 Vec3_Scale(Vector3 v, float scalar) {
 return Vec3_New(v.x * scalar, v.y * scalar, v.z * scalar);
 }
 
 /* Advanced Vector Operations */
 
 /**
  * Computes the dot product of two vectors.
  */
 static inline float Vec3_Dot(Vector3 a, Vector3 b) {
 return a.x * b.x + a.y * b.y + a.z * b.z;
 }
 
 /**
  * Computes the cross product of two vectors.
  */
 static inline Vector3 Vec3_Cross(Vector3 a, Vector3 b) {
 return Vec3_New(
 a.y * b.z - a.z * b.y,
 a.z * b.x - a.x * b.z,
 a.x * b.y - a.y * b.x
 );
 }
 
 /**
  * Computes the magnitude (length) of a vector.
  */
 static inline float Vec3_Magnitude(Vector3 v) {
 return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
 }
 
 /**
  * Normalizes a vector (scales it to unit length).
  */
 static inline Vector3 Vec3_Normalize(Vector3 vector) {
 float length = Vec3_Magnitude(vector);

 // Avoid division by zero if the vector is near zero
 if (length < EPSILON) {
 return Vec3_New(0.0f, 0.0f, 0.0f);
 }

 // Normalize the vector
 return Vec3_Scale(vector, 1.0f / length);
 }
 
 /**
  * Checks if a vector is near zero (all components close to zero).
  */
 static inline bool Vec3_IsZero(Vector3 v) {
 return (fabsf(v.x) < EPSILON && fabsf(v.y) < EPSILON && fabsf(v.z) < EPSILON);
 }
 
 /* Specialized Vector Operations */
 
 /**
  * Performs linear interpolation between two vectors.
  */
 static inline Vector3 Vec3_Lerp(Vector3 a, Vector3 b, float t) {
 // Clamp t to [0, 1] to ensure interpolation
 t = clamp(t, 0.0f, 1.0f);
 return Vec3_Add(a, Vec3_Scale(Vec3_Subtract(b, a), t));
 }
 
 /**
  * Reflects a vector against a normal.
  */
 static inline Vector3 Vec3_Reflect(Vector3 v, Vector3 normal) {
 return Vec3_Subtract(v, Vec3_Scale(normal, 2.0f * Vec3_Dot(v, normal)));
 }
 
 // Matrix Operations
 
 /**
  * Returns an identity matrix.
  */
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
  * Creates a transformation matrix based on the gravity vector.
  * The gravity vector determines the "up" direction in the sensor frame.
  * This matrix transforms vectors from the sensor frame to a world frame
  * where the Y-axis is up.
  */
 static inline Matrix4 Matrix4_FromGravity(Vector3 gravNorm) {
     // Ensure gravNorm is normalized
     gravNorm = Vec3_Normalize(gravNorm);
 
     // If gravNorm is near zero after normalization attempt, return identity or handle error
     if (Vec3_IsZero(gravNorm)) {
         DEBUG_LOG("Warning: Gravity vector is near zero, cannot create valid matrix. Returning Identity.\n");
         return Matrix4_Identity();
     }
 
     // World up vector
     Vector3 worldUp = Vec3_New(0.0f, 1.0f, 0.0f);
 
     Vector3 xAxis, yAxis, zAxis;
 
     // The Y-axis of the sensor frame in world coordinates is the normalized gravity vector
     yAxis = gravNorm;
 
     // Calculate the Z-axis. It should be orthogonal to the world up vector and the gravity vector.
     // Use cross product of worldUp and yAxis.
     Vector3 tempZ = Vec3_Cross(worldUp, yAxis);
 
     // Handle the case where gravNorm is parallel to worldUp (tempZ is near zero)
     if (Vec3_IsZero(tempZ)) {
         // If gravNorm is (0, 1, 0) or (0, -1, 0), choose a default orthogonal Z-axis.
         // Use (0, 0, 1) as a default Z-axis if Y-axis is along the world Y-axis.
         zAxis = Vec3_New(0.0f, 0.0f, 1.0f);
     } else {
         zAxis = Vec3_Normalize(tempZ);
     }
 
     // Calculate the X-axis. It should be orthogonal to Y-axis and Z-axis.
     // Use cross product of yAxis and zAxis.
     xAxis = Vec3_Cross(yAxis, zAxis);
     // xAxis should be normalized if yAxis and zAxis are orthonormal, but normalize to be safe.
     xAxis = Vec3_Normalize(xAxis);
 
     // Construct the rotation matrix. The rows of the matrix are the basis vectors
     // of the sensor frame expressed in world coordinates.
     Matrix4 matrix = { {
         {xAxis.x, xAxis.y, xAxis.z, 0.0f},
         {yAxis.x, yAxis.y, yAxis.z, 0.0f},
         {zAxis.x, zAxis.y, zAxis.z, 0.0f},
         {0.0f,    0.0f,    0.0f,    1.0f}
     } };
 
     return matrix;
 }
 
 /**
  * Multiplies a matrix by a vector (row-major order).
  * Transforms the vector from the coordinate space represented by the matrix
  * to the standard basis.
  */
 static inline Vector3 MultiplyMatrixVector(Matrix4 matrix, Vector3 vector) {
 // Assume w = 1.0 for 3D vectors for transformation
 float w = 1.0f;
 
 return Vec3_New(
 matrix.m[0][0] * vector.x + matrix.m[0][1] * vector.y + matrix.m[0][2] * vector.z + matrix.m[0][3] * w,
 matrix.m[1][0] * vector.x + matrix.m[1][1] * vector.y + matrix.m[1][2] * vector.z + matrix.m[1][3] * w,
 matrix.m[2][0] * vector.x + matrix.m[2][1] * vector.y + matrix.m[2][2] * vector.z + matrix.m[2][3] * w
 );
 }
 
 // Global Gravity Vector Management
 
 /**
  * Global gravity vector (default set to (0, 1, 0) - representing world up).
  */
 static Vector3 gravNorm = { 0.0f, 1.0f, 0.0f };
 
 /**
  * Updates the global gravity vector using sensor fusion (complementary filter).
  * Blends gyro-integrated orientation with accelerometer reading.
  */
 static inline void UpdateGravityVector(Vector3 accel, Vector3 gyroRotation, float fusionFactor, float deltaTime) {
     // Validate fusionFactor
     if (fusionFactor < 0.0f || fusionFactor > 1.0f) {
         DEBUG_LOG("Error: Invalid fusionFactor (%f). Must be between 0.0 and 1.0.\n", fusionFactor);
         return;
     }
 
     // Validate inputs for NaN values
     if (isnan(accel.x) || isnan(accel.y) || isnan(accel.z) ||
         isnan(gyroRotation.x) || isnan(gyroRotation.y) || isnan(gyroRotation.z)) {
         DEBUG_LOG("Error: NaN values detected in sensor inputs. Skipping update.\n");
         return;
     }
 
     // Normalize accelerometer input
     Vector3 accelNorm = Vec3_Normalize(accel);
 
     // If accelerometer reading is near zero, skip accelerometer correction
     if (Vec3_IsZero(accelNorm)) {
          DEBUG_LOG("Warning: Accelerometer reading is near zero. Skipping accelerometer correction.\n");
          // Only apply gyro rotation
          Vector3 rotationDelta = Vec3_Cross(gyroRotation, gravNorm);
          gravNorm = Vec3_Add(gravNorm, Vec3_Scale(rotationDelta, deltaTime));
     } else {
         // Compute gyro-induced gravity shift
         Vector3 rotationDelta = Vec3_Cross(gyroRotation, gravNorm);
         Vector3 rotatedGravity = Vec3_Add(gravNorm, Vec3_Scale(rotationDelta, deltaTime));
 
         // Blend gyro-predicted gravity with accelerometer reading
         // Use fusionFactor directly as the interpolation weight
         gravNorm = Vec3_Lerp(rotatedGravity, accelNorm, fusionFactor);
     }
 
 
     // Final normalization step to ensure accuracy
     if (!Vec3_IsZero(gravNorm)) {
         gravNorm = Vec3_Normalize(gravNorm);
     } else {
         DEBUG_LOG("Warning: Gravity vector became near-zero after update. Resetting to default (0,1,0).\n");
         gravNorm = Vec3_New(0.0f, 1.0f, 0.0f);
     }
 
     DEBUG_LOG("Updated Gravity Vector - gravNorm = (%f, %f, %f)\n", gravNorm.x, gravNorm.y, gravNorm.z);
 }
 
 /**
  * Sets the gravity vector manually and ensures normalization.
  */
 static inline void SetGravityVector(float x, float y, float z) {
     // Validate inputs
     if (isnan(x) || isnan(y) || isnan(z)) {
         DEBUG_LOG("Error: Gravity vector contains NaN values. Resetting to default (0,1,0).\n");
         gravNorm = Vec3_New(0.0f, 1.0f, 0.0f);
         return;
     }
 
     Vector3 newGravNorm = Vec3_New(x, y, z);
 
     // Ensure gravity vector isn't near-zero
     if (Vec3_Magnitude(newGravNorm) < EPSILON) {
         DEBUG_LOG("Warning: Gravity vector magnitude too small. Retaining previous value.\n");
         return;
     }
 
     // Normalize and update the gravity vector
     gravNorm = Vec3_Normalize(newGravNorm);
 
     DEBUG_LOG("SetGravityVector called - gravNorm = (%f, %f, %f)\n", gravNorm.x, gravNorm.y, gravNorm.z);
 }
 
 /**
  * Retrieves the current normalized gravity vector.
  */
 static inline Vector3 GetGravityVector(void) {
     DEBUG_LOG("GetGravityVector called - gravNorm = (%f, %f, %f)\n", gravNorm.x, gravNorm.y, gravNorm.z);
     return gravNorm;
 }
 
 
 // Dynamic Orientation Adjustment
 
 /**
  * Adapts gyro input for switching between normal controller grip and handheld-style grip.
  * This function takes yaw, pitch, and roll inputs and adjusts them to maintain consistent orientation.
  */
 Vector3 TransformWithDynamicOrientation(float yaw_input, float pitch_input, float roll_input) {
 
     // Validate Inputs
     if (isnan(yaw_input) || isnan(pitch_input) || isnan(roll_input)) {
         DEBUG_LOG("Error: NaN detected in inputs. Returning zero vector.\n");
         return Vec3_New(0.0f, 0.0f, 0.0f);
     }
 
     // Clamp Inputs to Prevent Extreme Values
     yaw_input = clamp(yaw_input, -360.0f, 360.0f);
     pitch_input = clamp(pitch_input, -360.0f, 360.0f);
     roll_input = clamp(roll_input, -360.0f, 360.0f);
 
     // Combine Inputs Into a Single Vector
     Vector3 combinedVector = Vec3_New(yaw_input, pitch_input, roll_input);
 
     // Adjust Orientation Based on Gravity
     Vector3 adjustedVector = Vec3_Subtract(combinedVector, Vec3_Scale(gravNorm, Vec3_Dot(combinedVector, gravNorm)));
 
     // Normalize Only if Needed (Avoiding Erratic Behavior)
     if (Vec3_Magnitude(adjustedVector) > EPSILON) {
         adjustedVector = Vec3_Normalize(adjustedVector);
     }
 
     // Dynamic Roll Compensation (Grip-Based)
     float adjustedRoll = roll_input - Vec3_Dot(gravNorm, Vec3_New(0.0f, 0.0f, roll_input));
 
     // Debug Logs
     DEBUG_LOG("Dynamic Orientation Adjustment (Raw Inputs): Yaw = %f, Pitch = %f, Roll = %f\n", yaw_input, pitch_input, roll_input);
 
     // Return Final Orientation
     return Vec3_New(adjustedVector.x, adjustedVector.y, adjustedRoll);
 }
 
 // Gyro Space Transformation Function
 
 /**
  * These functions transform gyro inputs into different spaces (Local, Player, World)
  * while handling sensitivity adjustments and gravity alignment.
  */
 
 
 // C wrapper for gyro transformation functions, ensuring compatibility with both C and C++.
 #ifdef __cplusplus
 extern "C" {
 #endif
 
     Vector3 TransformToLocalSpace(float yaw, float pitch, float roll, float coupling_factor);
 
     Vector3 TransformToPlayerSpace(float yaw, float pitch, float roll, Vector3 gravity_vector);
 
     Vector3 TransformToWorldSpace(float yaw, float pitch, float roll, Vector3 gravity_vector);
 
 #ifdef __cplusplus
 }
 #endif
 
 /**
  * Transforms gyro inputs to Local Space.
  *
  * Converts raw gyro input into direct motion scaling.
  * Preserves natural gyro responsiveness for intuitive movement.
  */
 Vector3 TransformToLocalSpace(float yaw, float pitch, float roll, float couplingFactor) {
     // Adjust roll to compensate for yaw-roll coupling
     float rollCompensation = 0.85f; // Reduces roll influence
     float adjustedRoll = (roll * rollCompensation) - (yaw * couplingFactor);
 
     // Ensure balanced proportional motion
     Vector3 localGyro = Vec3_New(yaw - adjustedRoll, pitch, adjustedRoll);
 
     // Return the transformed vector
     return localGyro;
 }
 
 /**
  * Transforms gyro inputs to Player Space.
  * Adjusts motion relative to the player's perspective while ensuring gravity alignment.
  */
 Vector3 TransformToPlayerSpace(float yaw, float pitch, float roll, Vector3 gravity) {
     // Validate gravity vector
     if (Vec3_IsZero(gravity)) {
         gravity = Vec3_New(0.0f, 1.0f, 0.0f);
     } else {
         gravity = Vec3_Normalize(gravity);
     }
 
     // Compute world-aligned yaw using gravity influence
     float worldYaw = yaw * gravity.y + roll * gravity.z;
 
     // Adjust roll to prevent gravity distortions
     float adjustedRoll = roll - (roll * gravity.y);
 
     // Refine yaw magnitude to prevent sudden inversion
     float yawRelaxation = 1.41f;
     float yawSign = (worldYaw >= 0) ? 1.0f : -1.0f;
     float yawMagnitude = Vec3_Magnitude(Vec3_New(yaw, adjustedRoll, 0.0f));
     float adjustedYaw = yawSign * fminf(fabsf(worldYaw) * yawRelaxation, yawMagnitude);
 
     // Apply Player View Matrix transformation
     Matrix4 playerViewMatrix = Matrix4_FromGravity(gravity);
     Vector3 playerGyro = MultiplyMatrixVector(playerViewMatrix, Vec3_New(adjustedYaw, pitch, adjustedRoll));
 
     return playerGyro;
 }
 
 /**
  * Transforms gyro inputs to World Space.
  * Aligns input with the game world while maintaining spatial consistency.
  */
 Vector3 TransformToWorldSpace(float yaw, float pitch, float roll, Vector3 gravity) {
     // Validate gravity vector
     if (Vec3_IsZero(gravity)) {
         gravity = Vec3_New(0.0f, 1.0f, 0.0f);
     } else {
         gravity = Vec3_Normalize(gravity);
     }
 
     // Compute yaw influence with gravity correction
     float worldYaw = yaw * gravity.y + roll * gravity.z;
 
     // Refine roll to prevent unwanted gravity interference
     float adjustedRoll = roll - (roll * gravity.y);
 
     // Calculate pitch axis projection onto the gravity plane
     float gravDotPitch = Vec3_Dot(gravity, Vec3_New(1.0f, 0.0f, 0.0f));
     Vector3 pitchVector = Vec3_Subtract(Vec3_New(1.0f, 0.0f, 0.0f), Vec3_Scale(gravity, gravDotPitch));
 
     if (!Vec3_IsZero(pitchVector)) {
         pitchVector = Vec3_Normalize(pitchVector);
     }
 
     // Adjust pitch for better alignment with the gravity plane
     float adjustedPitch = Vec3_Dot(Vec3_New(pitch, 0.0f, adjustedRoll), pitchVector);
 
     // Apply World View Matrix transformation
     Matrix4 worldViewMatrix = Matrix4_FromGravity(gravity);
     Vector3 worldGyro = MultiplyMatrixVector(worldViewMatrix, Vec3_New(worldYaw, adjustedPitch, adjustedRoll));
 
     return worldGyro;
 }
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif // GYROSPACE_H