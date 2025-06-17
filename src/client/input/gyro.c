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

#include "header/gyro.h"

#define EPSILONF 1.0e-6f
#define DEG2RAD(x) (float)((x) * M_PI / 180.0)

// Smoothing samples.
#define MAX_SAMPLES 256

// Player space relax factor (60 degrees is 2.094 radians).
#define RELAX_FACTOR_60 2.0943952f

// World space side reduction threshold.
#define SIDE_THRESH 0.125f

// Gravity vector constants.
#define SMOOTH_HALF_RATE 4.0f // 1 / (0.25 seconds)
#define SHAKINESS_MIN_THRESH 0.01f
#define SHAKINESS_MAX_THRESH 0.4f
#define COR_STILL_RATE 1.0f
#define COR_SHAKY_RATE 0.1f
#define COR_GYRO_FACTOR 0.1f
#define COR_GYRO_MIN_THRESH 0.05f
#define COR_GYRO_MAX_THRESH 0.25f
#define COR_MIN_SPEED 0.01f

typedef struct
{
	// Motion
	vec3_t accel;                    // Accelerometer x, y, z (g).
	float accel_magnitude;           // Accelerometer magnitude (g).
	vec3_t gravity;                  // Gravity vector (g).
	vec3_t smooth_accel;             // Smoothed accelerometer (g).
	float shakiness;                 // Shakiness (g).
	vec3_t gyro;                     // Gyro pitch, yaw, roll (radians/s).
	vec3_t gyro_offset;              // Gyro calibration offsets (radians/s).

	// Gyro Space
	gyro_space_t gyro_space;         // Current gyro space.
	gyro_local_roll_t local_roll;    // Roll behavior when using local space.

	// Smoothing
	qboolean use_smoothing;          // Use gyro smoothing?
	unsigned int index;              // Smoothing sample index.
	float smooth_pitch[MAX_SAMPLES]; // Pitch smoothing samples (radians/s).
	float smooth_yaw[MAX_SAMPLES];   // Yaw smoothing sample (radians/s).
	float smooth_window;             // Smoothing window (seconds).
	float lower_smooth;              // Lower smoothing threshold (radian/s).
	float upper_smooth;              // Upper smoothing threshold (radian/s).

	// Tightening
	qboolean use_tightening;         // Use gyro tightening?
	float tightening;                // Tightening threshold (radian/s).

	// Acceleration
	qboolean use_acceleration;       // Use gyro acceleration?
	qboolean accel_requested;        // User requested gyro acceleration?
	float accel_mult;                // RWS multiplier.
	float lower_accel;               // Lower threshold for accel (radian/s).
	float upper_accel;               // Upper threshold for accel (radian/s).

	// Sensitivity
	float min_pitch_sens;            // Min pitch sensitivity (RWS).
	float max_pitch_sens;            // Max pitch sensitivity (RWS).
	float min_yaw_sens;              // Min yaw sensitivity (RWS).
	float max_yaw_sens;              // Max yaw sensitivity (RWS).
} gyro_motion_t;

typedef struct
{
	vec3_t accel;             // Accumulated accelerometer samples (g).
	unsigned int accel_count; // Accelerometer sample count.
	vec3_t gyro;              // Accumulated gyro samples (radians/s).
	unsigned int gyro_count;  // Gyro sample count.
} gyro_samples_t;

static gyro_motion_t motion;
static gyro_samples_t samples;

static void ApplySensitivity(void)
{
	if (motion.use_acceleration)
	{
		float mag = Q_magnitude(motion.gyro[0], motion.gyro[1]);
		mag -= motion.lower_accel;
		mag = Q_max(0.0f, mag);

		const float denom = motion.upper_accel - motion.lower_accel;
		float accel_factor;
		if (denom > EPSILONF)
		{
			accel_factor = mag / denom;
			accel_factor = Q_clamp(accel_factor, 0.0f, 1.0f);
		}
		else
		{
			accel_factor = mag > 0.0f ? 1.0f : 0.0f;
		}
		const float no_accel_factor = 1.0f - accel_factor;

		motion.gyro[0] *= (motion.min_pitch_sens * no_accel_factor
						   + motion.max_pitch_sens * accel_factor);

		motion.gyro[1] *= (motion.min_yaw_sens * no_accel_factor
						   + motion.max_yaw_sens * accel_factor);
	}
	else
	{
		motion.gyro[0] *= motion.min_pitch_sens;
		motion.gyro[1] *= motion.min_yaw_sens;
	}
}

static void ApplyTightening(void)
{
	if (motion.use_tightening)
	{
		const float magnitude = Q_magnitude(motion.gyro[0], motion.gyro[1]);

		if (magnitude < motion.tightening)
		{
			const float factor = magnitude / motion.tightening;
			motion.gyro[0] *= factor;
			motion.gyro[1] *= factor;
		}
	}
}

static void GetSmoothGyro(float delta_time, float raw_factor,
						  float *smooth_pitch, float *smooth_yaw)
{
	const float smooth_factor = 1.0f - raw_factor;
	motion.index = (motion.index + (MAX_SAMPLES - 1)) % MAX_SAMPLES;
	motion.smooth_pitch[motion.index] = motion.gyro[0] * smooth_factor;
	motion.smooth_yaw[motion.index] = motion.gyro[1] * smooth_factor;

	const float sample_window =
		Q_clamp(delta_time, EPSILONF, motion.smooth_window);
	float sample_count = motion.smooth_window / sample_window;
	sample_count = Q_clamp(sample_count, 1.0f, MAX_SAMPLES);
	const unsigned int num_samples = lroundf(sample_count);

	*smooth_pitch = motion.smooth_pitch[motion.index] / num_samples;
	*smooth_yaw = motion.smooth_yaw[motion.index] / num_samples;

	for (unsigned int i = 1; i < num_samples; i++)
	{
		const unsigned int index = (motion.index + i) % MAX_SAMPLES;
		*smooth_pitch += motion.smooth_pitch[index] / num_samples;
		*smooth_yaw += motion.smooth_yaw[index] / num_samples;
	}
}

static void GetRawGyro(float *raw_factor, float *raw_pitch, float *raw_yaw)
{
	const float magnitude = Q_magnitude(motion.gyro[0], motion.gyro[1]);
	const float denom = motion.upper_smooth - motion.lower_smooth;
	float factor;

	if (denom > EPSILONF)
	{
		factor = (magnitude - motion.lower_smooth) / denom;
		factor = Q_clamp(factor, 0.0f, 1.0f);
	}
	else
	{
		factor = magnitude < motion.lower_smooth ? 0.0f : 1.0f;
	}

	*raw_factor = factor;
	*raw_pitch = motion.gyro[0] * factor;
	*raw_yaw = motion.gyro[1] * factor;
}

static void ApplySmoothing(float delta_time)
{
	if (motion.use_smoothing)
	{
		float raw_factor, raw_pitch, raw_yaw, smooth_pitch, smooth_yaw;

		GetRawGyro(&raw_factor, &raw_pitch, &raw_yaw);
		GetSmoothGyro(delta_time, raw_factor, &smooth_pitch, &smooth_yaw);

		motion.gyro[0] = raw_pitch + smooth_pitch;
		motion.gyro[1] = raw_yaw + smooth_yaw;
	}
}

static void CalcGravityVector(float delta_time)
{
	// Convert gyro input to reverse rotation.
	const float angle_speed = VectorLength(motion.gyro);
	const float angle = angle_speed * delta_time;
	vec3_t negative_gyro;
	VectorInverse2(motion.gyro, negative_gyro);
	quat_t reverse_rotation;
	QuatAngleAxis(negative_gyro, angle, reverse_rotation);

	// Rotate gravity vector.
	RotateVectorByUnitQuat(motion.gravity, reverse_rotation);

	// Check accelerometer magnitude now.
	const float accel_magnitude = VectorLength(motion.accel);
	if (accel_magnitude <= EPSILONF)
	{
		return;
	}
	vec3_t accel_norm;
	VectorScale(motion.accel, 1.0f / accel_magnitude, accel_norm);

	// Shakiness/smoothness.
	RotateVectorByUnitQuat(motion.smooth_accel, reverse_rotation);
	const float smooth_factor = exp2f(-delta_time * SMOOTH_HALF_RATE);
	motion.shakiness *= smooth_factor;
	vec3_t delta_accel;
	VectorSubtract(motion.accel, motion.smooth_accel, delta_accel);
	const float delta_accel_magnitude = VectorLength(delta_accel);
	motion.shakiness = Q_max(motion.shakiness, delta_accel_magnitude);
	VectorLerp(motion.accel, motion.smooth_accel, smooth_factor,
			   motion.smooth_accel);

	// Find the difference between gravity and raw acceleration.
	vec3_t new_gravity, gravity_delta, gravity_direction;
	VectorScale(accel_norm, -motion.accel_magnitude, new_gravity);
	VectorSubtract(new_gravity, motion.gravity, gravity_delta);
	VectorNormalize2(gravity_delta, gravity_direction);
	const float gravity_delta_len_squared = VectorLengthSquared(gravity_delta);

	// Calculate correction rate.
	float still_or_shaky = (motion.shakiness - SHAKINESS_MIN_THRESH)
						   / (SHAKINESS_MAX_THRESH - SHAKINESS_MIN_THRESH);
	still_or_shaky = Q_clamp(still_or_shaky, 0.0f, 1.0f);
	float correction_rate =
		COR_STILL_RATE + (COR_SHAKY_RATE - COR_STILL_RATE) * still_or_shaky;

	// Limit correction rate in proportion to gyro rate.
	const float angle_speed_adjusted = angle_speed * COR_GYRO_FACTOR;
	const float correction_limit = Q_max(angle_speed_adjusted, COR_MIN_SPEED);
	if (correction_rate > correction_limit)
	{
		const float gravity_delta_magnitude = sqrtf(gravity_delta_len_squared);
		float close_factor = (gravity_delta_magnitude - COR_GYRO_MIN_THRESH)
							 / (COR_GYRO_MAX_THRESH - COR_GYRO_MIN_THRESH);
		close_factor = Q_clamp(close_factor, 0.0f, 1.0f);
		correction_rate = correction_limit
						  + (correction_rate - correction_limit) * close_factor;
	}

	// Apply correction to gravity vector.
	vec3_t correction;
	VectorScale(gravity_direction, correction_rate * delta_time, correction);
	if (VectorLengthSquared(correction) < gravity_delta_len_squared)
	{
		VectorAdd(motion.gravity, correction, motion.gravity);
	}
	else
	{
		VectorCopy(new_gravity, motion.gravity);
	}
}

static void ApplyWorldSpace(void)
{
	// Normalize gravity vector.
	vec3_t grav_norm;
	VectorNormalize2(motion.gravity, grav_norm);

	// Dot product, all components.
	const float world_yaw = -_DotProduct(motion.gyro, grav_norm);

	// Project local pitch axis onto gravity plane.
	const float grav_dot_pitch_axis = grav_norm[0];
	vec3_t grav_norm_scaled;
	VectorScale(grav_norm, grav_dot_pitch_axis, grav_norm_scaled);
	vec3_t pitch_axis = {1.0f, 0.0f, 0.0f};
	VectorSubtract(pitch_axis, grav_norm_scaled, pitch_axis);

	// Normalize pitch axis, ignore zero magnitude.
	const float pitch_axis_magnitude = VectorNormalize(pitch_axis);
	if (pitch_axis_magnitude > 0.0f)
	{
		// Controller orientation info for smoothing over boundaries.
		const float flatness = fabsf(grav_norm[1]);
		const float upness = fabsf(grav_norm[2]);
		float side_reduction =
			(Q_max(flatness, upness) - SIDE_THRESH) / SIDE_THRESH;
		side_reduction = Q_clamp(side_reduction, 0.0f, 1.0f);

		// Pitch component.
		motion.gyro[0] = _DotProduct(motion.gyro, pitch_axis) * side_reduction;
	}
	else
	{
		motion.gyro[0] = 0.0f;
	}

	// Yaw component.
	motion.gyro[1] = world_yaw;
}

static void ApplyPlayerSpace(void)
{
	// Normalize gravity vector.
	vec3_t grav_norm;
	VectorNormalize2(motion.gravity, grav_norm);

	// Dot product, yaw and roll components.
	const float world_yaw =
		motion.gyro[1] * grav_norm[1] + motion.gyro[2] * grav_norm[2];

	const float world_part = fabsf(world_yaw) * RELAX_FACTOR_60;
	const float gyro_part = Q_magnitude(motion.gyro[1], motion.gyro[2]);

	// Yaw component.
	motion.gyro[1] = -Q_signf(world_yaw) * Q_min(world_part, gyro_part);
}

static void ApplyLocalSpace(void)
{
	switch (motion.local_roll)
	{
		case GYRO_LOCAL_ROLL_OFF:
			// Ignore roll component.
			break;

		case GYRO_LOCAL_ROLL_ON:
			motion.gyro[1] -= motion.gyro[2];
			break;

		case GYRO_LOCAL_ROLL_INVERT:
			motion.gyro[1] += motion.gyro[2];
			break;
	}
}

static void ApplyGyroSpace(float delta_time)
{
	switch (motion.gyro_space)
	{
		case GYRO_SPACE_LOCAL:
			ApplyLocalSpace();
			break;

		case GYRO_SPACE_PLAYER:
			CalcGravityVector(delta_time);
			ApplyPlayerSpace();
			break;

		case GYRO_SPACE_WORLD:
			CalcGravityVector(delta_time);
			ApplyWorldSpace();
			break;
	}
}

static void PrepareSamples(void)
{
	if (samples.accel_count)
	{
		VectorScale(samples.accel, 1.0f / samples.accel_count, samples.accel);
		samples.accel_count = 0;
	}

	if (samples.gyro_count)
	{
		VectorScale(samples.gyro, 1.0f / samples.gyro_count, samples.gyro);
		VectorSubtract(samples.gyro, motion.gyro_offset, samples.gyro);
		samples.gyro_count = 0;
	}

	VectorCopy(samples.accel, motion.accel);
	VectorCopy(samples.gyro, motion.gyro);
}

void IN_GYRO_Process(float delta_time, float *out_pitch_rate,
					 float *out_yaw_rate)
{
	PrepareSamples();

	if (!IsZeroVector(motion.accel) || !IsZeroVector(motion.gyro))
	{
		ApplyGyroSpace(delta_time);
		ApplySmoothing(delta_time);
		ApplyTightening();
		ApplySensitivity();
	}

	*out_pitch_rate = motion.gyro[0];
	*out_yaw_rate = motion.gyro[1];
}

void IN_GYRO_AccumulateAccelSamples(vec3_t accelerometer_data)
{
	if (!samples.accel_count)
	{
		memset(samples.accel, 0, sizeof(samples.accel));
	}

	VectorAdd(samples.accel, accelerometer_data, samples.accel);
	samples.accel_count++;
}

void IN_GYRO_AccumulateGyroSamples(vec3_t gyro_data)
{
	if (!samples.gyro_count)
	{
		memset(samples.gyro, 0, sizeof(samples.gyro));
	}

	VectorAdd(samples.gyro, gyro_data, samples.gyro);
	samples.gyro_count++;
}

float IN_GYRO_AverageAccelSamples(void)
{
	if (samples.accel_count)
	{
		vec3_t average;
		VectorScale(samples.accel, 1.0f / samples.accel_count, average);
		return VectorLength(average);
	}
	else
	{
		return GYRO_DEFAULT_ACCEL;
	}
}

void IN_GYRO_AverageGyroSamples(vec3_t out_gyro_average)
{
	if (samples.gyro_count)
	{
		VectorScale(samples.gyro, 1.0f / samples.gyro_count, out_gyro_average);
	}
	else
	{
		out_gyro_average[0] = 0.0f;
		out_gyro_average[1] = 0.0f;
		out_gyro_average[2] = 0.0f;
	}
}

void IN_GYRO_ResetSamples(void)
{
	memset(&samples, 0, sizeof(samples));
}

void IN_GYRO_ResetState(void)
{
	memset(motion.accel, 0, sizeof(motion.accel));
	memset(motion.gravity, 0, sizeof(motion.gravity));
	memset(motion.smooth_accel, 0, sizeof(motion.smooth_accel));
	motion.shakiness = 0.0f;
	memset(motion.gyro, 0, sizeof(motion.gyro));
	motion.index = 0;
	memset(motion.smooth_pitch, 0, sizeof(motion.smooth_pitch));
	memset(motion.smooth_yaw, 0, sizeof(motion.smooth_yaw));

	IN_GYRO_ResetSamples();
}

void IN_GYRO_UpdateGyroSpace(float gyro_space)
{
	motion.gyro_space =
		lroundf(Q_clamp(gyro_space, GYRO_SPACE_LOCAL, GYRO_SPACE_WORLD));
}

void IN_GYRO_UpdateLocalRoll(float local_roll)
{
	motion.local_roll = lroundf(
		Q_clamp(local_roll, GYRO_LOCAL_ROLL_OFF, GYRO_LOCAL_ROLL_INVERT));
}

void IN_GYRO_UpdateSensitivity(float pitch_sens, float yaw_sens)
{
	motion.min_pitch_sens = Q_clamp(pitch_sens, GYRO_MIN_SENS, GYRO_MAX_SENS);
	motion.min_yaw_sens = Q_clamp(yaw_sens, GYRO_MIN_SENS, GYRO_MAX_SENS);
	motion.max_pitch_sens = motion.min_pitch_sens * motion.accel_mult;
	motion.max_yaw_sens = motion.min_yaw_sens * motion.accel_mult;
}

void IN_GYRO_UpdateTightening(float tightening)
{
	motion.tightening =
		Q_clamp(tightening, GYRO_MIN_TIGHT_THRESH, GYRO_MAX_TIGHT_THRESH);
	motion.tightening = DEG2RAD(motion.tightening);
	motion.use_tightening = (motion.tightening > EPSILONF);
}

void IN_GYRO_UpdateSmoothing(float smoothing)
{
	motion.upper_smooth =
		Q_clamp(smoothing, GYRO_MIN_SMOOTH_THRESH, GYRO_MAX_SMOOTH_THRESH);
	motion.upper_smooth = DEG2RAD(motion.upper_smooth);
	motion.lower_smooth = motion.upper_smooth * 0.5f;
}

void IN_GYRO_UpdateSmoothingWindow(float smoothing_window)
{
	motion.smooth_window = Q_clamp(smoothing_window, GYRO_MIN_SMOOTH_WINDOW,
								   GYRO_MAX_SMOOTH_WINDOW);
	motion.use_smoothing = (motion.smooth_window > EPSILONF);
}

void IN_GYRO_UpdateAcceleration(float use_acceleration)
{
	motion.accel_requested = lroundf(Q_clamp(use_acceleration, 0.0f, 1.0f));
	motion.use_acceleration =
		(motion.accel_requested
		 && motion.accel_mult > GYRO_MIN_ACCEL_MULT + EPSILONF);
}

void IN_GYRO_UpdateAccelMult(float accel_mult)
{
	motion.accel_mult =
		Q_clamp(accel_mult, GYRO_MIN_ACCEL_MULT, GYRO_MAX_ACCEL_MULT);
	motion.max_pitch_sens = motion.min_pitch_sens * motion.accel_mult;
	motion.max_yaw_sens = motion.min_yaw_sens * motion.accel_mult;
	motion.use_acceleration =
		(motion.accel_requested
		 && motion.accel_mult > GYRO_MIN_ACCEL_MULT + EPSILONF);
}

void IN_GYRO_UpdateAccelThresh(float lower_accel, float upper_accel)
{
	motion.lower_accel =
		Q_clamp(lower_accel, GYRO_MIN_ACCEL_THRESH, GYRO_MAX_ACCEL_THRESH);
	motion.upper_accel =
		Q_clamp(upper_accel, motion.lower_accel, GYRO_MAX_ACCEL_THRESH);
	motion.lower_accel = DEG2RAD(motion.lower_accel);
	motion.upper_accel = DEG2RAD(motion.upper_accel);
}

void IN_GYRO_UpdateCalibration(float accel_magnitude, vec3_t gyro_offset)
{
	motion.accel_magnitude = accel_magnitude;
	VectorCopy(gyro_offset, motion.gyro_offset);
}
