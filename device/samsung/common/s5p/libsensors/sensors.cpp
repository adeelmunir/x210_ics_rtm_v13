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

#define LOG_TAG "Sensors"
#define FUNC_LOG LOGV("%s", __PRETTY_FUNCTION__)

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <linux/input.h>
#include <utils/Atomic.h>
#include <utils/Log.h>

#include "sensors.h"
#include "LightSensor.h"
#include "ProximitySensor.h"
#include "Accelerometer.h"
#include "TemperatureMonitor.h"

/*****************************************************************************/

#define DELAY_OUT_TIME 0x7FFFFFFF

#define SENSORS_ACCELERATION     (1<<ID_A)
#define SENSORS_LIGHT            (1<<ID_L)
#define SENSORS_PROXIMITY        (1<<ID_P)
#define SENSORS_TEMPERATURE      (1<<ID_T)

#define SENSORS_ACCELERATION_HANDLE     0
#define SENSORS_LIGHT_HANDLE            1
#define SENSORS_PROXIMITY_HANDLE        2
#define SENSORS_TEMPERATURE_HANDLE      3

/*****************************************************************************/

/* The SENSORS Module */
static const struct sensor_t sSensorList[] = {
	{
		"3-axis Accelerometer", "Analog Devices", 1,
		SENSORS_ACCELERATION_HANDLE, SENSOR_TYPE_ACCELEROMETER, RANGE_A,
		RESOLUTION_A, 0.23f, 20000, { }
	},
	{
		"Intersil isl29018 Ambient Light Sensor", "Intersil", 1,
		SENSORS_LIGHT_HANDLE, SENSOR_TYPE_LIGHT, 1000.0f, 1.0f, 1.0f, 0, { }
	},
	{
		"Intersil isl29018 Proximity sensor", "Intersil", 1,
		SENSORS_PROXIMITY_HANDLE, SENSOR_TYPE_PROXIMITY, 1.0f, 1.0f,
		1.0f, 0, { }
	},
	{
		"ADT7461 Temperature Monitor",
		"Analog Devices", 1, SENSORS_TEMPERATURE_HANDLE,
		SENSOR_TYPE_TEMPERATURE, 150.0f, 1.0f, 1.0f, 0, { }
	},
};

static int open_sensors(const struct hw_module_t* module, const char* id,
		struct hw_device_t** device);

static int sensors__get_sensors_list(struct sensors_module_t* module,
		struct sensor_t const** list) {
	*list = sSensorList;
	return ARRAY_SIZE(sSensorList);
}

static struct hw_module_methods_t sensors_module_methods = {
	open: open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: SENSORS_HARDWARE_MODULE_ID,
		name: "Android Sensors Module",
		author: "jerryjianjun",
		methods: &sensors_module_methods,
	},
	get_sensors_list: sensors__get_sensors_list,
};

struct sensors_poll_context_t {
	struct sensors_poll_device_t device; // must be first

	sensors_poll_context_t();
	~sensors_poll_context_t();
	int activate(int handle, int enabled);
	void update_delay();
	int setDelay(int handle, int64_t ns);
	int pollEvents(sensors_event_t* data, int count);

private:
	enum {
		accelerometer = 0,
		light = 1,
		proximity = 2,
		temperature = 3,
		numSensorDrivers,
		numFds,
	};

	static const size_t wake = numFds - 1;
	static const char WAKE_MESSAGE = 'W';
	struct pollfd mPollFds[numFds];
	int mWritePipeFd;
	uint32_t mEnabled;
	uint64_t mTimeout;
	SensorBase* mSensors[numSensorDrivers];

	int handleToDriver(int handle) const {
		switch (handle) {
		case ID_A:
			return accelerometer;
		case ID_L:
			return light;
		case ID_P:
			return proximity;
		case ID_T:
			return temperature;
		}
		return -EINVAL;
	}
};

/*****************************************************************************/

sensors_poll_context_t::sensors_poll_context_t() {
	FUNC_LOG;

	mEnabled = 0;
	mTimeout = -1LLU;

	mSensors[accelerometer] = new Accelerometer();
	mPollFds[accelerometer].fd = mSensors[accelerometer]->getFd();
	mPollFds[accelerometer].events = POLLIN;
	mPollFds[accelerometer].revents = 0;

	mSensors[light] = new LightSensor();
	mPollFds[light].fd = -1;

	mSensors[proximity] = new ProximitySensor();
	mPollFds[proximity].fd = -1;

	mSensors[temperature] = new TemperatureMonitor();
	mPollFds[temperature].fd = -1;

	int wakeFds[2];
	int result = pipe(wakeFds);
	LOGE_IF(result < 0, "error creating wake pipe (%s)", strerror(errno));
	fcntl(wakeFds[0], F_SETFL, O_NONBLOCK);
	fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);
	mWritePipeFd = wakeFds[1];

	mPollFds[wake].fd = wakeFds[0];
	mPollFds[wake].events = POLLIN;
	mPollFds[wake].revents = 0;
}

sensors_poll_context_t::~sensors_poll_context_t() {
	FUNC_LOG;
	for (int i = 0; i < numSensorDrivers; i++) {
		delete mSensors[i];
	}
	close(mPollFds[wake].fd);
	close(mWritePipeFd);
}

int sensors_poll_context_t::activate(int handle, int enabled) {
	FUNC_LOG;

	int index = handleToDriver(handle);
	if (index < 0)
		return index;
	int err = mSensors[index]->enable(handle, enabled);
	if (!err) {
		int newState = enabled ? 1 : 0;
		if (((uint32_t(newState) << index) != (mEnabled & (1 << index))) && ((1
				<< index) != (1 << accelerometer))) {
			mEnabled &= ~(1 << index);
			mEnabled |= (uint32_t(newState) << index);
			update_delay();
		}
	}

	if (enabled && !err) {
		const char wakeMessage(WAKE_MESSAGE);
		int result = write(mWritePipeFd, &wakeMessage, 1);
		LOGE_IF(result < 0, "error sending wake message (%s)", strerror(errno));
	}
	return err;
}

void sensors_poll_context_t::update_delay() {
	uint64_t time = -1LLU;

	if (mEnabled) {
		for (int i = 0; i < numSensorDrivers; i++) {
			if (mEnabled & (1 << i)) {
				uint64_t temp = (uint64_t) mSensors[i]->getDelay();
				time = time < temp ? time : temp;
			}
		}
	}
	mTimeout = time;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) {
	FUNC_LOG;
	int ret;
	int index = handleToDriver(handle);

	if (index < 0)
		return index;

	if (ns < 0)
		return -EINVAL;

	ret = mSensors[index]->setDelay(handle, ns);
	if (ret >= 0)
		update_delay();
	return ret;
}

int sensors_poll_context_t::pollEvents(sensors_event_t* data, int count) {
	FUNC_LOG;
	int nbEvents = 0;
	int readCount = 0;
	int n = 0;
	sensors_event_t* p = data;

	do {
		// see if we have some leftover from the last poll()
		readCount = count > numSensorDrivers ? count / numSensorDrivers : 1;
		for (int i = 0; count && i < numSensorDrivers; i++) {
			SensorBase* const sensor(mSensors[i]);
			if (sensor->hasPendingEvents() || (mPollFds[i].revents & POLLIN)) {
				int nb = sensor->readEvents(data, readCount);
				if (nb < count) {
					// no more data for this sensor
					mPollFds[i].revents = 0;
				}
				count -= nb;
				nbEvents += nb;
				data += nb;
			}
		}

		if (count > 0) {
			// we still have some room, so try to see if we can get
			// some events immediately or just wait if we don't have
			// anything to return
			int delay = (mTimeout == -1LLU) ? -1 : (int) (mTimeout / 1000000);
			n = poll(mPollFds, numFds, nbEvents ? 0 : delay);
			if (n < 0) {
				LOGE("poll() failed (%s)", strerror(errno));
				return -errno;
			}
			if (mPollFds[wake].revents & POLLIN) {
				char msg;
				int result = read(mPollFds[wake].fd, &msg, 1);
				LOGE_IF(result < 0, "error reading from wake pipe (%s)",
						strerror(errno));
				LOGE_IF(msg != WAKE_MESSAGE,
						"unknown message on wake queue (0x%02x) %c", int(msg),
						msg);
				mPollFds[wake].revents = 0;
			}
		}
		// if we have events and space, go read them
	} while (n > 0 && count > 0);
	return nbEvents;
}

/*****************************************************************************/

static int poll__close(struct hw_device_t *dev) {
	FUNC_LOG;
	sensors_poll_context_t *ctx = (sensors_poll_context_t *) dev;
	LOGE("poll__close is called");
	if (ctx) {
		delete ctx;
	}
	return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev, int handle,
		int enabled) {
	FUNC_LOG;
	sensors_poll_context_t *ctx = (sensors_poll_context_t *) dev;
	return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev, int handle,
		int64_t ns) {
	FUNC_LOG;
	sensors_poll_context_t *ctx = (sensors_poll_context_t *) dev;
	return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev, sensors_event_t* data,
		int count) {
	FUNC_LOG;
	sensors_poll_context_t *ctx = (sensors_poll_context_t *) dev;
	return ctx->pollEvents(data, count);
}

/*****************************************************************************/

/** Open a new instance of a sensor device using name */
static int open_sensors(const struct hw_module_t* module, const char* id,
		struct hw_device_t** device) {
	FUNC_LOG;
	int status = -EINVAL;
	sensors_poll_context_t *dev = new sensors_poll_context_t();

	memset(&dev->device, 0, sizeof(sensors_poll_device_t));

	LOGE("********************* x210 gsensor driver *********************");
	LOGE("Web: www.9tripod.com");
	LOGE("BBS: bbs.9tripod.com");
	LOGE("TEL: 0755-29650886");
	LOGE("ADDRESS: C-604,Block B,Shangchuan Road(East),Fanshen Village,");
	LOGE("         Xin'an Sub-district,Bao'an District,Shenzhen City");
	LOGE("***************************************************************");

	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version = 0;
	dev->device.common.module = const_cast<hw_module_t*> (module);
	dev->device.common.close = poll__close;
	dev->device.activate = poll__activate;
	dev->device.setDelay = poll__setDelay;
	dev->device.poll = poll__poll;

	*device = &dev->device.common;
	status = 0;

	return status;
}

