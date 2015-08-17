/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <cutils/log.h>
#include <stdlib.h>

#include "LightSensor.h"

/*****************************************************************************/

#define FIRST_GOOD_EVENT    5
#define LUX_SYSFS_PATH  "/sys/bus/iio/devices/device0/lux"

/* return the current time in nanoseconds */
int64_t now_ns(void)
{
    //FUNC_LOG;
    struct timespec  ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    //LOGV("Time %lld", (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

LightSensor::LightSensor()
    : SensorBase(NULL, NULL),
      mEnabled(0),
      mHasPendingEvent(false),
      mAlready_warned(false),
      mLast_value(-1)
{
    delay_time = 200000000LL;
}

LightSensor::~LightSensor() {
}

int LightSensor::setDelay(int32_t handle, int64_t ns) {
    delay_time = ns;
    return 0;
}

int64_t LightSensor::getDelay() const {
    return delay_time;
}

int LightSensor::enable(int32_t handle, int en) {
    if (en != 0)
        mEnabled = true;
    else
        mEnabled = false;
    return 0;
}

bool LightSensor::hasPendingEvents() const {
    if(mEnabled)
        return true;
    else
        return false;
}

int LightSensor::readEvents(sensors_event_t* data, int count) {
    static int log_count = 0;
    static float value = -1.0f;
    sensors_event_t evt;

    if (count < 1 || data == NULL || !mEnabled) {
        LOGE("Will not work on zero count(%i) or null pointer\n", count);
        return 0;
    }

    /* The opening of the file handle is placed here
     * as work around that if it is opened in the constructor
     * read on the file handle fails.
     */
    data_fd = open(LUX_SYSFS_PATH, O_RDONLY);
    if (data_fd < 0)
        return 0;

    int numEventReceived = 0;
    input_event const* event;
    char buffer[20] = {0};

    int amt = read(data_fd, buffer, sizeof(buffer));
    close(data_fd);

    if (amt <= 0 && mAlready_warned == false) {
        LOGE("LightSensor: read from %s failed", LUX_SYSFS_PATH);
        mAlready_warned = false;
        return 0;
    }
    value = atof(buffer);
    if (value == mLast_value)
       return 0;
    evt.version = sizeof(sensors_event_t);
    evt.sensor = ID_L;
    evt.type = SENSOR_TYPE_LIGHT;
    evt.light = value;
    evt.timestamp = now_ns();
    *data = evt;
    mLast_value = value;
    LOGE("LightSensor: value is %i", (int)value );
    return 1;
}

int LightSensor::getFd() const {
    return -1;
}
