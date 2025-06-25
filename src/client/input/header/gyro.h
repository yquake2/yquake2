//----------------------------------------------------------------------------
//
//  Copyright (c) 2018-2021 Julian "Jibb" Smart
//  Copyright (c) 2021-2024 Nicolas Lessard
//  Copyright (c) 2025 ceski
//
//  Permission is hereby granted, free of charge, to any person obtaining
//  a copy of this software and associated documentation files (the
//  "Software"), to deal in the Software without restriction, including
//  without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to
//  permit persons to whom the Software is furnished to do so, subject to
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//----------------------------------------------------------------------------

#ifndef __GYRO_H__
#define __GYRO_H__

#include "../../../common/header/shared.h"

// Default accelerometer magnitude (1 g).
#define GYRO_DEFAULT_ACCEL 1.0f

// Sensitivity
#define GYRO_MIN_SENS 0.0f           // RWS
#define GYRO_MAX_SENS 20.0f          // RWS
#define GYRO_STEP_SENS 0.1f          // RWS

// Tightening
#define GYRO_MIN_TIGHT_THRESH 0.0f   // deg/s
#define GYRO_MAX_TIGHT_THRESH 50.0f  // deg/s
#define GYRO_STEP_TIGHT_THRESH 0.5f  // deg/s

// Smoothing
#define GYRO_MIN_SMOOTH_THRESH 0.0f  // deg/s
#define GYRO_MAX_SMOOTH_THRESH 50.0f // deg/s
#define GYRO_STEP_SMOOTH_THRESH 0.5f // deg/s
#define GYRO_MIN_SMOOTH_WINDOW 0.0f  // seconds
#define GYRO_MAX_SMOOTH_WINDOW 0.5f  // seconds

// Acceleration
#define GYRO_MIN_ACCEL_MULT 1.0f     // RWS multiplier
#define GYRO_MAX_ACCEL_MULT 20.0f    // RWS multiplier
#define GYRO_STEP_ACCEL_MULT 0.1f    // RWS multiplier
#define GYRO_MIN_ACCEL_THRESH 0.0f   // deg/s
#define GYRO_MAX_ACCEL_THRESH 300.0f // deg/s
#define GYRO_STEP_ACCEL_THRESH 5.0f  // deg/s

typedef enum
{
	GYRO_SPACE_LOCAL,
	GYRO_SPACE_PLAYER,
	GYRO_SPACE_WORLD,
} gyro_space_t;

typedef enum
{
	GYRO_LOCAL_ROLL_OFF,
	GYRO_LOCAL_ROLL_ON,
	GYRO_LOCAL_ROLL_INVERT,
} gyro_local_roll_t;

// Processes accumulated samples and returns pitch rate and yaw rate in
// radians/second. If no new samples were accumulated since the last call to
// this function, then the previous samples are used.
void IN_GYRO_Process(float delta_time, float *out_pitch_rate,
					 float *out_yaw_rate);

// Accumulates accelerometer samples (x, y, z in units of gravity, g).
void IN_GYRO_AccumulateAccelSamples(vec3_t accelerometer_data);

// Accumulates gyro samples (pitch, yaw, roll in radians/second).
void IN_GYRO_AccumulateGyroSamples(vec3_t gyro_data);

// Returns the average magnitude of the accumulated accelerometer samples (g).
// If there are no samples, then default gravity (1 g) is returned.
float IN_GYRO_AverageAccelSamples(void);

// Returns the average of the accumulated gyro samples (radians/second).
// If there are no samples, then a zero vector is returned.
void IN_GYRO_AverageGyroSamples(vec3_t out_gyro_average);

// Discards accumulated samples.
void IN_GYRO_ResetSamples(void);

// Resets all state variables and discards accumulated samples. Settings and
// calibration values are preserved.
void IN_GYRO_ResetState(void);

//
// Functions for updating internal settings based on related cvars.
//

// Updates current gyro space.
void IN_GYRO_UpdateGyroSpace(float gyro_space);

// Updates roll behavior when using local space.
void IN_GYRO_UpdateLocalRoll(float local_roll);

// Updates pitch and yaw sensitivities (RWS).
void IN_GYRO_UpdateSensitivity(float pitch_sens, float yaw_sens);

// Updates tightening threshold (deg/s).
void IN_GYRO_UpdateTightening(float tightening);

// Updates smoothing threshold (deg/s).
void IN_GYRO_UpdateSmoothing(float smoothing);

// Updates smoothing window (seconds).
void IN_GYRO_UpdateSmoothingWindow(float smoothing_window);

// Updates use of gyro acceleration.
void IN_GYRO_UpdateAcceleration(float use_acceleration);

// Updates RWS multiplier.
void IN_GYRO_UpdateAccelMult(float accel_mult);

// Updates lower and upper gyro thresholds for applying accel (deg/s).
void IN_GYRO_UpdateAccelThresh(float lower_accel, float upper_accel);

// Updates calibrated accelerometer magnitude (units of gravity, g) and gyro
// calibration offsets (radians/second).
void IN_GYRO_UpdateCalibration(float accel_magnitude, vec3_t gyro_offset);

#endif // __GYRO_H__
