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

#define LOG_TAG "bluedroid"
#define LOG_NDEBUG 0

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cutils/log.h>
#include <cutils/misc.h>
#include <cutils/properties.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <bluedroid/bluetooth.h>

#include <fcntl.h>
#include <sys/mman.h> 

#ifndef HCI_DEV_ID
#define HCI_DEV_ID 0
#endif

#define HCID_START_DELAY_SEC   3
#define HCID_STOP_DELAY_USEC 500000

#define UR_PATCH_RDA58XX_BT

#define MIN(x,y) (((x)<(y))?(x):(y))

#if defined(UR_PATCH_RDA58XX_BT)
#define X210_BT_DEVICE_PATH			"/dev/ut_bt_dev"
#define BT_DEV_MAJOR_NUM 			234
#define IOCTL_BT_DEV_POWER   		_IO(BT_DEV_MAJOR_NUM, 100)
#define IOCTL_BT_DEV_SPECIFIC		_IO(BT_DEV_MAJOR_NUM, 101)

#define BT_DEV_ON					12
#define BT_DEV_OFF					0

#define CSR_MODULE                  0x12
#define BRCM_MODULE                 0x34
#define RDA_MODULE                  0x56

static int rfkill_id = -1;
static char *rfkill_state_path = NULL;

static int bt_on_off_flag = 0;

typedef struct 
{
	int module;  // 0x12:CSR, 0x34:Broadcom //ox56:RDA 
	int resume_flg;
	int temp;
} x210_bt_info_t;

static int check_bluetooth_power() {
	return bt_on_off_flag;
}

static int set_bluetooth_power(int on) {
	int fd = -1;
	int bt_on_off;
	x210_bt_info_t bt_info;

	fd = open(X210_BT_DEVICE_PATH, O_RDWR);
	if( fd < 0 ) {
		LOGE("## set_bluetooth_power open [%s] open error[%d]", X210_BT_DEVICE_PATH, fd);
		return -1;
	} else {
		bt_on_off = on;		
		ioctl(fd, IOCTL_BT_DEV_POWER, &bt_on_off);		
	}

	
	bt_on_off_flag = on;

	LOGI("### %s(); bt_on_off_flag = %d", __func__, bt_on_off_flag);

	if(on) {
		bt_info.module = 0;
		ioctl(fd, IOCTL_BT_DEV_SPECIFIC, &bt_info);

		LOGI("## %s(); BT_INFO module[%d]", __func__, bt_info.module);

		if(bt_info.module == CSR_MODULE) {
 
		} else if(bt_info.module == BRCM_MODULE) {
			
		} else if(bt_info.module == RDA_MODULE) {
			
		} else {
			LOGE("# ERROR!!!!! Unknown Module!!!!!");
			
		}

	}

	close( fd );

	return 0;

}
#else // usb BT dongle
static int rfkill_id = -1;
static char *rfkill_state_path = NULL;


static int init_rfkill() {
    char path[64];
    char buf[16];
    int fd;
    int sz;
    int id;

    LOGW("####%s(),####\n", __func__);	
#if 1//
    for (id = 0; id<20; id++) {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            LOGW("open(%s) failed: %s (%d)\n", path, strerror(errno), errno);
            //return -1;
            continue;
        }
        sz = read(fd, &buf, sizeof(buf));
        close(fd);
        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0) {
            rfkill_id = id;
            break;
        }
    }
    if(id==20) {
	return -1;
    }
	
#else
    for (id = 0; ; id++) {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            LOGW("open(%s) failed: %s (%d)\n", path, strerror(errno), errno);
            return -1;
        }
        sz = read(fd, &buf, sizeof(buf));
        close(fd);
        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0) {
            rfkill_id = id;
            break;
        }
    }
#endif


    asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkill_id);
    LOGW("###The blue dongle rfkill_state_path=[%s]\n", rfkill_state_path);

    return 0;
}

static int check_bluetooth_power() {
    int sz;
    int fd = -1;
    int ret = -1;
    char buffer;

    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_RDONLY);
    if (fd < 0) {
        LOGE("open(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    sz = read(fd, &buffer, 1);
    if (sz != 1) {
        LOGE("read(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }

    switch (buffer) {
    case '1':
        ret = 1;
        break;
    case '0':
        ret = 0;
        break;
    }

out:
    if (fd >= 0) close(fd);
    return ret;
}

static int set_bluetooth_power(int on) {
    int sz;
    int fd = -1;
    int ret = -1;
    const char buffer = (on ? '1' : '0');

    LOGD("###%s(%d);\n", __func__, on);
	
    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_WRONLY);
    if (fd < 0) {
        LOGE("open(%s) for write failed: %s (%d)", rfkill_state_path,
             strerror(errno), errno);
        goto out;
    }
    sz = write(fd, &buffer, 1);
    if (sz < 0) {
        LOGE("write(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    ret = 0;

out:
    if (fd >= 0) close(fd);
    return ret;
}
#endif

static inline int create_hci_sock() {
    int sk = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (sk < 0) {
        LOGE("Failed to create bluetooth hci socket: %s (%d)",
             strerror(errno), errno);
    }
    return sk;
}

int bt_enable() {
    LOGV(__FUNCTION__);

    int ret = -1;
    int hci_sock = -1;
    int attempt;
	
    LOGD(__FUNCTION__);

    if (set_bluetooth_power(1) < 0) goto out;
    LOGD("Starting hciattach daemon");
    if (property_set("ctl.start", "hciattach") < 0) {
        LOGE("##Failed to start hciattach");
        set_bluetooth_power(0);
        goto out;
    }

    LOGD("Starting hciattach daemon A");

    // Try for 10 seconds, this can only succeed once hciattach has sent the
    // firmware and then turned on hci device via HCIUARTSETPROTO ioctl
    #define TRY_TIME 1000
    for (attempt = TRY_TIME; attempt > 0;  attempt--) {
        hci_sock = create_hci_sock();
        if (hci_sock < 0) {
           goto out;
        }
		
        ret = ioctl(hci_sock, HCIDEVUP, HCI_DEV_ID);
        if (!ret) {
            break;
        }else{
	        if (1==attempt)
	             LOGE("ioctl error: %s (%d)", strerror(errno), errno);
        }

        close(hci_sock);
        usleep(10000);  // 10 ms retry delay
    }
    if (attempt == 0) {
        LOGE("%s: Timeout waiting for HCI device to come up [### ERROR ###]", __FUNCTION__);
        if (property_set("ctl.stop", "hciattach") < 0) {
            LOGE("Error stopping hciattach");
        }
        set_bluetooth_power(0);
        goto out;
    }

    LOGI("%s: %d.%2d seconds\n", __FUNCTION__,(TRY_TIME - attempt)/100,(TRY_TIME - attempt)%100  );
 
    LOGI("Starting bluetoothd deamon");
    if (property_set("ctl.start", "bluetoothd") < 0) {
        LOGE("Failed to start bluetoothd");
        set_bluetooth_power(0);
        goto out;
    }

    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int bt_disable() {
    LOGV(__FUNCTION__);

    int ret = -1;
    int hci_sock = -1;

    LOGI("Stopping bluetoothd deamon");
    if (property_set("ctl.stop", "bluetoothd") < 0) {
        LOGE("Error stopping bluetoothd");
        goto out;
    }
    usleep(HCID_STOP_DELAY_USEC);
	
    hci_sock = create_hci_sock();
    if (hci_sock < 0) goto out;
    ioctl(hci_sock, HCIDEVDOWN, HCI_DEV_ID);

    LOGI("Stopping hciattach deamon");
    if (property_set("ctl.stop", "hciattach") < 0) {
        LOGE("Error stopping hciattach");
        goto out;
    }

    if (set_bluetooth_power(0) < 0) {
        goto out;
    }
    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int bt_is_enabled() {
    LOGV(__FUNCTION__);

    int hci_sock = -1;
    int ret = -1;
    struct hci_dev_info dev_info;


    // Check power first
    ret = check_bluetooth_power();
    LOGI("# %s();   check ret = %d", __func__, ret);	
    if (ret == -1 || ret == 0) goto out;

    ret = -1;

    // Power is on, now check if the HCI interface is up
    hci_sock = create_hci_sock();
    if (hci_sock < 0) goto out;

    dev_info.dev_id = HCI_DEV_ID;
    if (ioctl(hci_sock, HCIGETDEVINFO, (void *)&dev_info) < 0) {
        ret = 0;
        goto out;
    }

    if (dev_info.flags & (1 << (HCI_UP & 31))) {
        ret = 1;
    } else {
        ret = 0;
    }

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int ba2str(const bdaddr_t *ba, char *str) {
    return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);
}

int str2ba(const char *str, bdaddr_t *ba) {
    int i;
	char *ptr = (char*)str;
    for (i = 5; i >= 0; i--) {
        ba->b[i] = (uint8_t) strtoul(ptr, &ptr, 16);
        str++;
    }
    return 0;
}
