/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AP_Proximity_config.h"

#if AP_PROXIMITY_RANGEFINDER_ENABLED

#include "AP_Proximity_RangeFinder.h"

#include <AP_HAL/AP_HAL.h>
#include <ctype.h>
#include <stdio.h>
#include <AP_RangeFinder/AP_RangeFinder.h>
#include <AP_RangeFinder/AP_RangeFinder_Backend.h>

// update the state of the sensor
void AP_Proximity_RangeFinder::update(void)
{
    // exit immediately if no rangefinder object
    const RangeFinder *rngfnd = AP::rangefinder();
    if (rngfnd == nullptr) {
        set_status(AP_Proximity::Status::NoData);
        return;
    }

    uint32_t now = AP_HAL::millis();

    // look through all rangefinders
    for (uint8_t i=0; i < rngfnd->num_sensors(); i++) {
        AP_RangeFinder_Backend *sensor = rngfnd->get_backend(i);
        if (sensor == nullptr) {
            continue;
        }
        if (sensor->has_data()) {
            // check for horizontal range finders
            if (sensor->orientation() <= ROTATION_YAW_315) {
                const uint8_t sector = (uint8_t)sensor->orientation();
                const float angle = sector * 45;
                const AP_Proximity_Boundary_3D::Face face = frontend.boundary.get_face(angle);
                // distance in meters
                const float distance = sensor->distance();
                _distance_min = sensor->min_distance();
                _distance_max = sensor->max_distance();
                if ((distance <= _distance_max) && (distance >= _distance_min) && !ignore_reading(angle, distance, false)) {
                    frontend.boundary.set_face_attributes(face, angle, distance, state.instance);
                    // update OA database
                    database_push(angle, distance);
                } else {
                    frontend.boundary.reset_face(face, state.instance);
                }
                _last_update_ms = now;
            }
            // check upward facing range finder
            if (sensor->orientation() == ROTATION_PITCH_90) {
                const float distance_upward = sensor->distance();
                const float up_distance_min = sensor->min_distance();
                const float up_distance_max = sensor->max_distance();
                if ((distance_upward >= up_distance_min) && (distance_upward <= up_distance_max)) {
                    _distance_upward = distance_upward;
                } else {
                    _distance_upward = -1.0; // mark an valid reading
                }
                _last_upward_update_ms = now;
            }
        }
    }

    // check for timeout and set health status
    if ((_last_update_ms == 0 || (now - _last_update_ms > PROXIMITY_RANGEFIDER_TIMEOUT_MS)) &&
        (_last_upward_update_ms == 0 || (now - _last_upward_update_ms > PROXIMITY_RANGEFIDER_TIMEOUT_MS))) {
        set_status(AP_Proximity::Status::NoData);
    } else {
        set_status(AP_Proximity::Status::Good);
    }
}

// get distance upwards in meters. returns true on success
bool AP_Proximity_RangeFinder::get_upward_distance(float &distance) const
{
    if ((AP_HAL::millis() - _last_upward_update_ms <= PROXIMITY_RANGEFIDER_TIMEOUT_MS) &&
        is_positive(_distance_upward)) {
        distance = _distance_upward;
        return true;
    }
    return false;
}

#endif // AP_PROXIMITY_RANGEFINDER_ENABLED
