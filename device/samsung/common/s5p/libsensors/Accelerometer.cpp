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

#include "Accelerometer.h"

#define EVENT_RATE_CODE_25HZ    40
#define EVENT_RATE_CODE_50HZ    20
#define EVENT_RATE_CODE_100HZ   10
#define EVENT_RATE_CODE_200HZ   5

#define G_SCALE					(GRAVITY_EARTH * 4 / 4096)
#define RATE_SYSFS_PATH			"/sys/devices/platform/s3c2440-i2c.0/i2c-0/0-000f/rate"

/*****************************************************************************/

/* return the current time in nanoseconds */
extern int64_t now_ns(void);

Accelerometer::Accelerometer() :
	SensorBase(NULL, NULL), mEnabled(0), mHasPendingEvent(false), mInputReader(
			32) {
	data_name = "gsensor";
	data_fd = openInput("gsensor");

	mPendingEvent.version = sizeof(sensors_event_t);
	mPendingEvent.sensor = ID_A;
	mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
	mPendingEvent.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

	delay_time = -1LL;
}

Accelerometer::~Accelerometer() {
	if (data_fd >= 0)
		close( data_fd);
}

int Accelerometer::setDelay(int32_t handle, int64_t ns) {
	int sys_fd = -1;
	unsigned int code = EVENT_RATE_CODE_25HZ;
	char new_value[3];

	/*
	 * Mapping of data rate according to the delay
	 *          FASTEST      GAME        UI         NORMAL
	 * <--200HZ-->|<--100HZ-->|<--50HZ-->|<----25HZ---|----
	 *
	 * Autosleep is disabled when events rate required
	 * is fast, for games. If autosleep is active accelerometer
	 * takes time to wake and response delivered is not
	 * good. For UI events, such as screen orientation
	 * autosleep can be enabled which will reduce power consumption.
	 */
	if (ns <= SENSOR_DELAY_FASTEST) {
		code = EVENT_RATE_CODE_200HZ;
	} else if (ns <= SENSOR_DELAY_GAME) {
		code = EVENT_RATE_CODE_100HZ;
	} else if (ns <= SENSOR_DELAY_UI) {
		code = EVENT_RATE_CODE_50HZ;
	}

	/* Change data rate through sysfs entry */
	sys_fd = open(RATE_SYSFS_PATH, O_WRONLY);
	if (sys_fd < 0)
		return 0;

	sprintf(new_value, "%d", code);
	write(sys_fd, new_value, sizeof(new_value));
	close(sys_fd);

	return 0;
}

int64_t Accelerometer::getDelay() const {
	return delay_time;
}

int Accelerometer::enable(int32_t handle, int en) {
	if ((en != 0) && (data_fd >= 0)) {
		mEnabled = true;
	} else
		mEnabled = false;
	return 0;
}

bool Accelerometer::hasPendingEvents() const {
	return false;
}

int Accelerometer::readEvents(sensors_event_t* data, int count) {
	if (count < 1)
		return -EINVAL;

	ssize_t n = mInputReader.fill(data_fd);
	if (n < 0)
		return n;

	int numEventReceived = 0;
	input_event const* event;

	while (count && mInputReader.readEvent(&event)) {
		int type = event->type;
		if (type == EV_ABS) {
			processEvent(event->code, event->value);
		} else if (type == EV_SYN) {
			int64_t time = timevalToNano(event->time);
			mPendingEvent.timestamp = time;
			if (mEnabled) {
				*data++ = mPendingEvent;
				count--;
				numEventReceived++;
				/* FOR DEBUG
				LOGE("Accelerometer: x = %f, y = %f, z = %f",
						mPendingEvent.acceleration.x,
						mPendingEvent.acceleration.y,
						mPendingEvent.acceleration.z);
				*/
			}
		} else {
			LOGE("Accelerometer: unknown event (type=%d, code=%d)", type,
					event->code);
		}
		mInputReader.next();
	}
	return numEventReceived;
}

void Accelerometer::processEvent(int code, int value) {
	switch (code) {
	case ABS_X:
		mPendingEvent.acceleration.x = value * G_SCALE;
		break;
	case ABS_Y:
		mPendingEvent.acceleration.y = value * G_SCALE;
		break;
	case ABS_Z:
		mPendingEvent.acceleration.z = value * G_SCALE;
		break;
	}
}

int Accelerometer::getFd() const {
	LOGE("Accelerometer: getFd returning %d", data_fd);
	return data_fd;
}
