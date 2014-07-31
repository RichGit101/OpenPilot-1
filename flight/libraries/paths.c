/**
 ******************************************************************************
 *
 * @file       paths.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      Library path manipulation
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <pios.h>
#include <pios_math.h>
#include <mathmisc.h>

#include "paths.h"

#include "uavobjectmanager.h" // <--.
#include "pathdesired.h" // <-- needed only for correct ENUM macro usage with path modes (PATHDESIRED_MODE_xxx,
// no direct UAVObject usage allowed in this file

// private functions
static void path_endpoint(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool mode);
static void path_vector(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool mode);
static void path_circle(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool clockwise);

/**
 * @brief Compute progress along path and deviation from it
 * @param[in] start_point Starting point
 * @param[in] end_point Ending point
 * @param[in] cur_point Current location
 * @param[in] mode Path following mode
 * @param[out] status Structure containing progress along path and deviation
 */
void path_progress(float *start_point, float *end_point, float *cur_point, struct path_status *status, uint8_t mode)
{
    switch (mode) {
    case PATHDESIRED_MODE_FLYVECTOR:
        return path_vector(start_point, end_point, cur_point, status, true);

	break;
    case PATHDESIRED_MODE_DRIVEVECTOR:
        return path_vector(start_point, end_point, cur_point, status, false);

        break;
    case PATHDESIRED_MODE_FLYCIRCLERIGHT:
    case PATHDESIRED_MODE_DRIVECIRCLERIGHT:
        return path_circle(start_point, end_point, cur_point, status, 1);

        break;
    case PATHDESIRED_MODE_FLYCIRCLELEFT:
    case PATHDESIRED_MODE_DRIVECIRCLELEFT:
        return path_circle(start_point, end_point, cur_point, status, 0);

        break;
    case PATHDESIRED_MODE_FLYENDPOINT:
        return path_endpoint(start_point, end_point, cur_point, status, true);

        break;
    case PATHDESIRED_MODE_DRIVEENDPOINT:
    default:
        // use the endpoint as default failsafe if called in unknown modes
        return path_endpoint(start_point, end_point, cur_point, status, false);

        break;
    }
}

/**
 * @brief Compute progress towards endpoint. Deviation equals distance
 * @param[in] start_point Starting point
 * @param[in] end_point Ending point
 * @param[in] cur_point Current location
 * @param[out] status Structure containing progress along path and deviation
 * @param[in] mode3D set true to include altitude in distance and progress calculation
 */
static void path_endpoint(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool mode3D)
{
    float path[3], diff[3];
    float dist_path, dist_diff;

    // we do not correct in this mode
    status->correction_direction[0] = status->correction_direction[1] = status->correction_direction[2] = 0;

    // Distance to go
    path[0] = end_point[0] - start_point[0];
    path[1] = end_point[1] - start_point[1];
    path[2] = mode3D ? end_point[2] - start_point[2] : 0;

    // Current progress location relative to end
    diff[0] = end_point[0] - cur_point[0];
    diff[1] = end_point[1] - cur_point[1];
    diff[2] = mode3D ? end_point[2] - cur_point[2] : 0;

    dist_diff  = sqrtf(diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]);
    dist_path  = sqrtf(path[0]*path[0] + path[1]*path[1] + path[2]*path[2]);

    if (dist_diff < 1e-6f) {
        status->fractional_progress = 1;
        status->error = 0;
        status->path_direction[0] = status->path_direction[1] = 0;
        status->path_direction[2] = 1;
        return;
    }

    if (dist_path + 1 > dist_diff) {
        status->fractional_progress = 1 - dist_diff / (1 + dist_path);
    } else {
        status->fractional_progress = 0; // we don't want fractional_progress to become negative
    }
    status->error = dist_diff;

    // Compute direction to travel
    status->path_direction[0] = diff[0] / dist_diff;
    status->path_direction[1] = diff[1] / dist_diff;
    status->path_direction[2] = diff[2] / dist_diff;
}

/**
 * @brief Compute progress along path and deviation from it
 * @param[in] start_point Starting point
 * @param[in] end_point Ending point
 * @param[in] cur_point Current location
 * @param[out] status Structure containing progress along path and deviation
 * @param[in] mode3D set true to include altitude in distance and progress calculation
 */
static void path_vector(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool mode3D)
{
    float path[3], diff[3];
    float dist_path;
    float dot;
    float track_point[3];

    // Distance to go
    path[0] = end_point[0] - start_point[0];
    path[1] = end_point[1] - start_point[1];
    path[2] = mode3D ? end_point[2] - start_point[2] : 0;

    // Current progress location relative to start
    diff[0] = cur_point[0] - start_point[0];
    diff[1] = cur_point[1] - start_point[1];
    diff[2] = mode3D ? cur_point[2] - start_point[2]: 0;

    dot = path[0] * diff[0] + path[1] * diff[1] + path[2] * diff[2];
    dist_path  = sqrtf(path[0] * path[0] + path[1] * path[1] + path[2] * path[2]);

    if (dist_path > 1e-6f) {
        // Compute direction to travel & progress
        status->path_direction[0] = path[0] / dist_path;
        status->path_direction[1] = path[1] / dist_path;
        status->path_direction[2] = path[2] / dist_path;
        status->fractional_progress = dot / (dist_path * dist_path);
    } else {
        // if the path is too short, we cannot determine vector direction.
        // Assume progress=1 and zero-length path.
        status->path_direction[0] = 0;
        status->path_direction[1] = 0;
        status->path_direction[2] = 0;
        status->fractional_progress = 1;
    }

    // Compute point on track that is closest to our current position.
    // Limiting fractional_progress makes sure the resulting point is also
    // limited to be between start and endpoint.
    status->fractional_progress = boundf(status->fractional_progress, 0, 1);

    track_point[0] = status->fractional_progress * path[0] + start_point[0];
    track_point[1] = status->fractional_progress * path[1] + start_point[1];
    track_point[2] = status->fractional_progress * path[2] + start_point[2];

    status->correction_direction[0] = track_point[0] - cur_point[0];
    status->correction_direction[1] = track_point[1] - cur_point[1];
    status->correction_direction[2] = track_point[2] - cur_point[2];

    status->error = sqrt(status->correction_direction[0] * status->correction_direction[0] +
                         status->correction_direction[1] * status->correction_direction[1] +
                         status->correction_direction[2] * status->correction_direction[2]);

    // Normalize correction_direction but avoid division by zero
    if (status->error > 1e-6f) {
        status->correction_direction[0] /= status->error;
        status->correction_direction[1] /= status->error;
        status->correction_direction[2] /= status->error;
    } else {
        status->correction_direction[0] = 0;
        status->correction_direction[1] = 0;
        status->correction_direction[2] = 1;
    }
}

/**
 * @brief Compute progress along circular path and deviation from it
 * @param[in] start_point Starting point
 * @param[in] end_point Center point
 * @param[in] cur_point Current location
 * @param[out] status Structure containing progress along path and deviation
 */
static void path_circle(float *start_point, float *end_point, float *cur_point, struct path_status *status, bool clockwise)
{
    float radius_north, radius_east, diff_north, diff_east;
    float radius, cradius;
    float normal[2];
    float progress;
    float a_diff, a_radius;

    // Radius
    radius_north = end_point[0] - start_point[0];
    radius_east  = end_point[1] - start_point[1];

    // Current location relative to center
    diff_north   = cur_point[0] - end_point[0];
    diff_east    = cur_point[1] - end_point[1];

    radius  = sqrtf(powf(radius_north, 2) + powf(radius_east, 2));
    cradius = sqrtf(powf(diff_north, 2) + powf(diff_east, 2));

    if (cradius < 1e-6f) {
        // cradius is zero, just fly somewhere and make sure correction is still a normal
        status->fractional_progress     = 1;
        status->error = radius;
        status->correction_direction[0] = 0;
        status->correction_direction[1] = 1;
        status->correction_direction[2] = 0;
        status->path_direction[0] = 1;
        status->path_direction[1] = 0;
        status->path_direction[2] = 0;
        return;
    }

    if (clockwise) {
        // Compute the normal to the radius clockwise
        normal[0] = -diff_east / cradius;
        normal[1] = diff_north / cradius;
    } else {
        // Compute the normal to the radius counter clockwise
        normal[0] = diff_east / cradius;
        normal[1] = -diff_north / cradius;
    }

    // normalize progress to 0..1
    a_diff   = atan2f(diff_north, diff_east);
    a_radius = atan2f(radius_north, radius_east);

    if (a_diff < 0) {
        a_diff += 2.0f * M_PI_F;
    }
    if (a_radius < 0) {
        a_radius += 2.0f * M_PI_F;
    }

    progress = (a_diff - a_radius + M_PI_F) / (2.0f * M_PI_F);

    if (progress < 0) {
        progress += 1.0f;
    } else if (progress >= 1.0f) {
        progress -= 1.0f;
    }

    if (clockwise) {
        progress = 1 - progress;
    }

    status->fractional_progress = progress;

    // error is current radius minus wanted radius - positive if too close
    status->error = radius - cradius;

    // Compute direction to correct error
    status->correction_direction[0] = (status->error > 0 ? 1 : -1) * diff_north / cradius;
    status->correction_direction[1] = (status->error > 0 ? 1 : -1) * diff_east / cradius;
    status->correction_direction[2] = 0;

    // Compute direction to travel
    status->path_direction[0] = normal[0];
    status->path_direction[1] = normal[1];
    status->path_direction[2] = 0;

    status->error = fabs(status->error);
}
