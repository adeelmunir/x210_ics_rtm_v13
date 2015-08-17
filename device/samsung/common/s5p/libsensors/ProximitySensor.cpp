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

#include "ProximitySensor.h"

/*****************************************************************************/

#define FIRST_GOOD_EVENT    5
#define PROX_SYSFS_PATH  "/sys/bus/iio/devices/device0/proxim_ir"

/* return the current time in nanoseconds */
extern int64_t now_ns(void);

ProximitySensor::ProximitySensor()
    : SensorBase(NULL, NULL),
      mEnabled(0),
      mHasPendingEvent(false),
      mAlready_warned(false),
      mLast_value(-1)
{
    delay_time = 200000000LL;
}

ProximitySensor::~ProximitySensor() {
}

int ProximitySensor::setDelay(int32_t handle, int64_t ns) {
    delay_time = ns;
    return 0;
}

int64_t ProximitySensor::getDelay() const {
    return delay_time;
}

int ProximitySensor::enable(int32_t handle, int en) {
    if (en != 0) {
        mLast_value = -1;
        mEnabled = true;
    }
    else
       mEnabled = false;
    return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    if(mEnabled)
        return true;
    else
        return false;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count) {
    static int log_count = 0;
    float value = -1.0f;

    if (count < 1 || data == NULL || !mEnabled)
        return 0;

    data_fd = open(PROX_SYSFS_PATH, O_RDONLY);
    if (data_fd < 0)
        return 0;

    char buffer[20] = {0};
    int amt = read(data_fd, buffer, sizeof(buffer));
    close(data_fd);

    if (amt <= 0 && mAlready_warned == false) {
        LOGE("ProximitySensor: read from %s failed", PROX_SYSFS_PATH);
    mAlready_warned = true;
        return 0;
    }
    value = atof(buffer);
    if (value == mLast_value)
       return 0;
    (*data).version = sizeof(sensors_event_t);
    (*data).sensor = ID_P;
    (*data).type = SENSOR_TYPE_PROXIMITY;
    (*data).distance =  value > 5000 ? 0 : 1; /* 0 = near; 1 = far. Based on how android expects */
    (*data).timestamp = now_ns();
    mLast_value = value;
    LOGD("ProximitySensor: value is %i", (int)value );
    return 1;
}

int ProximitySensor::getFd() const {
    return -1;
}
