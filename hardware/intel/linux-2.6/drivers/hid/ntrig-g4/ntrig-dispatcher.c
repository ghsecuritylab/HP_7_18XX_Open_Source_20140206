/*
 *  HID driver for N-Trig touchscreens
 *
 *  Copyright (c) 2011 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */


#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"
#ifdef MT_REPORT_TYPE_B
	#include <linux/input/mt.h>
#endif
#include "ntrig-ncp-driver.h"
#include "ntrig-dispatcher-sysfs.h"
#include "ntrig-dispatcher-sys-depend.h"
#include "ntrig-direct-event-driver.h"

/******************************************************************************/

#define NTRIG_PLATFORM_PHYS	"ntrig_touch/input1"
#define NTRIG_PLATFORM_NAME	"ntrig_ts"

#define NTRIG_SINGLE_TOUCH_DEV_NAME	"Ntrig_Touch"
#define NTRIG_MULTI_TOUCH_DEV_NAME	"Ntrig_TouchMulti"

#ifndef ABS_MT_PRESSURE
	#define ABS_MT_PRESSURE         0x3A
#endif

#define MAX_MAJOR_TOUCH 0x00FF

#define MAX_BUS_ID_LENGTH 20
#define FINGER_INDEX_NOT_FOUND (-1)
/******************************************************************************/

/**
 * Local API
 */

static int ntrig_input_open(struct input_dev *dev);
static void ntrig_input_close(struct input_dev *dev);

#ifndef MT_REPORT_TYPE_B
static int ntrig_send_pen(struct input_dev *input_device,
	struct mr_message_types_s *pen);
static int ntrig_send_multi_touch(struct input_dev *input_device,
	struct mr_message_types_s *multi_touch);
#else
#ifdef SINGLE_PEN_DEV
static int ntrig_send_pen(struct input_dev *input_device,
	struct mr_message_types_s *pen);
#else
static int ntrig_send_pen_typeB(struct input_dev *input_device,
	struct mr_message_types_s *pen);
#endif
static int ntrig_send_multi_touch_typeB(struct input_dev *input,
	struct mr_message_types_s *multi_touch);
static void remove_slot_typeB(struct input_dev *input, int slot);
#endif	/* MT_REPORT_TYPE_B */
static int ntrig_send_button(struct input_dev *input_device,
	struct mr_message_types_s *buttons_msg);

static void memory_lock_touch_ev(unsigned long *flag);
static void memory_unlock_touch_ev(unsigned long *flag);
static void g4_init_synch_data_touch_ev(void);
static void reset_finger_map(void);

/******************************************************************************/

/* NOTE: Static variables and global variables are automatically initialized to
 * 0 by the compiler. The kernel style checker tool (checkpatch.pl) complains
 * if they are explicitly initialized to 0 (or NULL) in their definition.
 */

/* dispatcher configuration */
static __u8 g_num_sensors;
static __u8 g_next_read_screen_border_sensor_id = -1;

/** Finger map indices that should be freed - optimization for updating removed
 *  fingers in the finger map.
 */
static int gRemoveFromFingerMap[ABS_MT_TRACKING_ID_MAX];
static int gRemoveFromFingerMapCount;

#ifndef MT_REPORT_TYPE_B
static int lastFingerCount;
#else
static short gCurFrameIndex;
/* Bit masks for the slots that were used in the current and previous sensor
 * reports. Used for detecting fingers that were removed from the sensor but
 * whose "finger removed" event was missed. Bit 0 corresponds to slot 0, bit 1
 * to slot 1, etc.
 */
static int gCurrentFingers;
static int gPreviousFingers;
static bool g4_pen;
#endif	/* MT_REPORT_TYPE_B */

/******************************************************************************/

/* direct-events pointers */
message_callback g4_ntrig_push_to_direct_events;
/******************************************************************************/

/**
 * Interface API
 */
int g4_is_pen_in_range(__u8 flag) { return flag & 0x01; }
int g4_is_pen_tip_switch(__u8 flag) { return (flag >> 1) & 0x01; }
int g4_is_pen_barrel_switch(__u8 flag) { return (flag >> 2) & 0x01; }
int g4_is_pen_invert(__u8 flag) { return (flag >> 3) & 0x01; }
int g4_is_pen_eraser(__u8 flag) { return (flag >> 4) & 0x01; }

int g4_is_st_in_range(__u8 flag) { return flag & 0x01; }
int g4_is_st_tip_switch(__u8 flag) { return (flag >> 1) & 0x01; }
int g4_is_st_touch_valid(__u8 flag) { return (flag >> 2) & 0x01; }

void g4_set_st_touch_valid(__u8 *flag) { *flag = *flag | 0x04; }
void g4_set_st_touch_invalid(__u8 *flag) { *flag = *flag & 0xFA; }

/* get API */
int g4_is_mt_in_range(__u8 flag) { return flag & 0x01; }
int g4_is_mt_tip_switch(__u8 flag) { return (flag >> 1) & 0x01; }
int g4_is_mt_touch_valid(__u8 flag) { return (flag >> 2) & 0x01; }

/* set API */
void g4_set_mt_touch_valid(__u8 *flag) { *flag = *flag | 0x04; }
void g4_set_mt_touch_invalid(__u8 *flag) { *flag = *flag & 0xFA; }

/******************************************************************************/
/**
 *  Device info (obtained at device registration)
 */

/* Keep separate data structures for HID and RAW, to ease registration and
 * un-regitration of different device types, independantly
*/
/* Holds registered ntrig_bus_devices */
static struct _ntrig_dev_hid ntrig_devices_hid[MAX_NUMBER_OF_SENSORS] = {
	{{0}, -1, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0,
		NULL, NULL},
	{{0}, -1, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0,
		NULL, NULL}
};
/* Holds registered ntrig_bus_devices */
static struct _ntrig_dev_raw ntrig_devices_raw[MAX_NUMBER_OF_SENSORS] = {
	{{0}, NULL, NULL},
	{{0}, NULL, NULL}
};

static int ntrig_dev_hid_ref_count[MAX_NUMBER_OF_SENSORS] = {0, 0};
static int ntrig_dev_raw_ref_count[MAX_NUMBER_OF_SENSORS] = {0, 0};

/* ncp read */
static __u8 g_ncp_next_read_sensor_id = -1;
static __u8 g_next_read_dev_type  = -1;

/* get bus interface (separate sysfs file) */
static __u8 g_get_bus_interface_read_sensor_id = -1;


/******************************************************************************/

/**
 *  Threads protection on data
 */

/**
 *  Touch events data protection
 */
static spinlock_t	g_mem_lock_touch_ev;
/* check if mutext need to be initialized */
static u8		g_first_time_touch_ev = 1;

static void memory_lock_touch_ev(unsigned long *flag)
{
	spin_lock_irqsave(&g_mem_lock_touch_ev, *flag);
}

static void memory_unlock_touch_ev(unsigned long *flag)
{
	spin_unlock_irqrestore(&g_mem_lock_touch_ev, *flag);
}

static void g4_init_synch_data_touch_ev(void)
{
	if (g_first_time_touch_ev) {
		spin_lock_init(&g_mem_lock_touch_ev);
		g_first_time_touch_ev = 0;
	}
}

void g4_print_message(struct mr_message_types_s *msg)
{
	int i;
	struct device_pen_s *pen;
	struct finger_parse_s *finger;
	struct device_finger_s *fa;
	struct device_finger_s *f;
	if (msg->type == MSG_PEN_EVENTS) {
		printk(KERN_INFO "Pen: ");
		pen = &msg->msg.pen_event;
		printk(KERN_INFO
			"sensor=%d,x=%d,y=%d,pressure=%d,btn_code=%d,"
			"btn_removed=%d\n", (int)pen->sensor_id,
			(int)pen->x_coord, (int)pen->y_coord,
			(int)pen->pressure, (int)pen->btn_code,
			(int)pen->btn_removed);
	} else if (msg->type == MSG_FINGER_PARSE) {
		printk(KERN_INFO "Finger: ");
		finger = &msg->msg.fingers_event;
		printk(KERN_INFO "sensor=%d,num=%d,frameIndex=%d\n",
			(int)finger->sensor_id, (int)finger->num_of_fingers,
			(int)finger->frame_index);
		fa = finger->finger_array;
		for (i = 0; i < finger->num_of_fingers; i++) {
			f = fa + i;
			printk(KERN_INFO
				"\t%d: track=%d,x=%d,y=%d,dx=%d,dy=%d,"
				"removed=%d,palm=%d,generic=%d\n", i,
				(int)f->track_id, (int)f->x_coord,
				(int)f->y_coord, (int)f->dx, (int)f->dy,
				(int)f->removed, (int)f->palm, (int)f->generic);
		}
	} else
		printk(KERN_INFO "Other - %d\n", msg->type);
}

int g4_setup_direct_events(message_callback push_to_direct_events)
{
	g4_ntrig_push_to_direct_events = push_to_direct_events;
	return DTRG_NO_ERROR;
}

/******************************************************************************/
/**
 * NCP Char Device - access file /dev/ntrig
 */

/* Callback from user application - write data from ncp file to device */
int g4_write_ncp_to_device(void *buf)
{
	/* NCP message to dispatcher:
	 *	Fields Index	Fields Description
	 *      0           Sensor ID
	 *      1           NCP Command (RAW/ HID)
	 *      2-3         Length(Payload)
	 *      4           Payload
	*/
	char *in_buf = (char *) buf;
	__u8 sensor_id;
	__u8 command;
	short len;
	char *payload;
	int ret = DTRG_NO_ERROR;
	union len_u *pl = NULL;
	void *dev = NULL;

	if (!in_buf) {
		ntrig_err("ntrig inside %s Wrong parameter\n", __func__);
		return DTRG_FAILED;
	}

	sensor_id = in_buf[0];
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	command = in_buf[1];
	pl = (union len_u *) &in_buf[2];
	len = pl->msg_len;	/*in_buf[2];*/
	payload = &in_buf[NCP_HEADER_LEN];
	ntrig_dbg(
		"inside %s: sensor_id = %d, command = %d, len = %d, "
		"payload[0] = %0X payload[1]=%0X\n", __func__, sensor_id,
		command, len, payload[0], payload[1]);

	/* memory lock with semaphore is done in calling function
	 * (ncp_write in ncp driver) */

	switch (command) {
	case NCP_WRITE_RAW_COMMAND:
		if (ntrig_devices_raw[sensor_id].write_ncp == NULL) {
			ntrig_dbg("%s: no RAW ncp_write for sensor %d\n",
				__func__, sensor_id);
			ret = DTRG_FAILED;
			goto END;
		}
		dev = ntrig_devices_raw[sensor_id].dev;
		if (dev == NULL) {
			ntrig_dbg("%s: no RAW device for sensor %d\n", __func__,
				sensor_id);
			ret = DTRG_FAILED;
			goto END;
		}
		ret = ntrig_devices_raw[sensor_id].write_ncp(dev, payload, len);
			/* write to the raw device */
		break;
	case NCP_READ_RAW_COMMAND:
		ntrig_dbg("%s: Next command (%d) for sensor %d is READ\n",
			__func__, command, sensor_id);
		g_ncp_next_read_sensor_id = sensor_id;
			/* wait for "read from raw/hid device" command */
		g_next_read_dev_type = command;
		break;
	default:
		ntrig_err("%s: Wrong command (%d) for sensor %d\n", __func__,
			command, sensor_id);
		ret = DTRG_FAILED;
	}
END:
	return ret;
}

/* Send from device to user application (ncp file driver) */
int g4_read_ncp_from_device(void *buf, size_t count)
{
	int ret = DTRG_NO_ERROR;
	void *dev = NULL;
	union len_u *pl = NULL;
	char *out_buf = (char *)buf;
	struct _ntrig_dev_raw *dev_raw =
		&ntrig_devices_raw[g_ncp_next_read_sensor_id];

	pl = (union len_u *) &out_buf[2];

	if ((g_ncp_next_read_sensor_id < 0) ||
		(g_ncp_next_read_sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__,
			g_ncp_next_read_sensor_id);
		return DTRG_FAILED;
	}

	if ((g_next_read_dev_type < FIRST_DEV_TYPE) ||
		(g_ncp_next_read_sensor_id > LAST_DEV_TYPE)) {
		ntrig_err("%s: wrong dev_type: %d\n", __func__,
			g_ncp_next_read_sensor_id);
		return DTRG_FAILED;
	}

	/* memory lock with semaphore is done in calling function
	 * (ncp_read in ncp driver) */

	/* Assuming buffer is allocated in read function - will be deallocated
	 * in char driver read function */
	switch (g_next_read_dev_type) {
	case NCP_READ_RAW_COMMAND:
		if (dev_raw->read_ncp == NULL) {
			ntrig_err("%s: no RAW ncp_read for sensor %d\n",
				__func__, g_ncp_next_read_sensor_id);
			ret = DTRG_FAILED;
			goto END;
		}
		dev = dev_raw->dev;
		if (dev == NULL) {
			ntrig_err("%s: no RAW device for sensor %d\n",
				__func__, g_ncp_next_read_sensor_id);
			ret = DTRG_FAILED;
			goto END;
		}
		ret = dev_raw->read_ncp(dev, (char *)&out_buf[NCP_HEADER_LEN],
			count - NCP_HEADER_LEN); /* read from raw device */
		/* save space in buffer for header */
		ntrig_dbg("%s: NCP RAW read result = %d\n", __func__, ret);
		break;
	default:
		ret = DTRG_FAILED;
	}
END:
	if (ret >= 0) {
		out_buf[0] = g_ncp_next_read_sensor_id;
		out_buf[1] = g_next_read_dev_type;
		pl->msg_len = ret;  /* pl points to buf[2], 2 bytes lenth */
		ret += NCP_HEADER_LEN;
	}

	/* reset global read variables */
	g_ncp_next_read_sensor_id = -1;
	g_next_read_dev_type = -1;

	return ret;
}

int g4_setup_ncp(message_callback_count *read_from_ncp,
	message_callback *write_to_ncp)
{
	*read_from_ncp		= g4_read_ncp_from_device;
	*write_to_ncp		= g4_write_ncp_to_device;
	return DTRG_NO_ERROR;
}

/******************************************************************************/
/**
 * sysfs files - access files under /sys/bus/hid/devices
 */

int g4_write_dispatcher_configuration(void *buf,  int req_type)
{
	int retval = DTRG_FAILED;
	__u8 *data = (__u8 *) buf;

	switch (req_type) {
	case CONFIG_TOUCH_SCREEN_BORDER:
		if ((*data >= 0) && (*data < MAX_NUMBER_OF_SENSORS)) {
			g_next_read_screen_border_sensor_id = *data;
			ntrig_dbg(
				"Inside %s: CONFIG_TOUCH_SCREEN_BORDER=%d, "
				"data=%d\n", __func__,
				(int) g_next_read_screen_border_sensor_id,
				*data);
			retval = 1;
		} else {
			g_next_read_screen_border_sensor_id = -1;
			ntrig_err(
				"Inside %s: CONFIG_TOUCH_SCREEN_BORDER - "
				"wrong data=%d\n", __func__, *data);
		}
		break;
	case CONFIG_DEBUG_PRINT:
		g4_set_ntrig_debug_flag((char)*data);
		ntrig_dbg("Inside %s: CONFIG_DEBUG_PRINT, data=%d\n",
			__func__, (int)*data);
		retval = 1;
		break;
	default:
		ntrig_err("Inside %s: wrong req_type !!\n", __func__);
		break;
	}
	return retval;
}

int g4_read_dispatcher_configuration(void *buf,  int req_type)
{
	int retval = DTRG_FAILED;

	switch (req_type) {
	case CONFIG_TOUCH_SCREEN_BORDER:
		if ((g_next_read_screen_border_sensor_id >= 0) &&
			(g_next_read_screen_border_sensor_id <
				MAX_NUMBER_OF_SENSORS)) {
			struct _ntrig_dev_hid *dev = &ntrig_devices_hid[
				g_next_read_screen_border_sensor_id];
			retval = snprintf(buf, PAGE_SIZE, "%d %d %d %d",
				dev->x_min, dev->y_min, dev->x_max, dev->y_max);
			if (retval > 0)
				retval++;	/* for the terminating '\0' */
			ntrig_dbg("Inside %s: CONFIG_TOUCH_SCREEN_BORDER=%s\n",
				__func__, (char *)buf);
			g_next_read_screen_border_sensor_id = -1;
		} else {
			ntrig_err(
				"Inside %s: CONFIG_TOUCH_SCREEN_BORDER error"
				"- sensor_id not set correctly: %d\n", __func__,
				(int) g_next_read_screen_border_sensor_id);
		}
		break;
	case CONFIG_DRIVER_VERSION:
		retval = snprintf(buf, PAGE_SIZE, "%s\n", NTRIG_DRIVER_VERSION);
		if (retval > 0)
			retval++;	/* for the terminating '\0' */
		ntrig_dbg("Inside %s: CONFIG_DRIVER_VERSION=%s\n", __func__,
			(char *)buf);
		break;
	case CONFIG_DEBUG_PRINT:
		ntrig_dbg("Inside %s: CONFIG_DEBUG_PRINT=%d\n", __func__,
			g4_ntrig_debug_flag);
		retval = snprintf(buf, PAGE_SIZE, "%c",
			g4_get_ntrig_debug_flag_as_char());
		if (retval > 0)
			retval++;	/* for the terminating '\0' */
		break;
	default:
		ntrig_err("Inside %s: wrong req_type !!\n", __func__);
		break;
	}
	return retval;
}

int g4_setup_config_dispatcher(config_callback *read_config_dispatcher,
	config_callback *write_config_dispatcher)
{
	*read_config_dispatcher = g4_read_dispatcher_configuration;
	*write_config_dispatcher = g4_write_dispatcher_configuration;
	return DTRG_NO_ERROR;
}

/******************************************************************************/
/**
 * get bus interface sysfs file implementation
 */

int g4_write_get_bus_interface_impl(void *buf)
{
	/* Get bus interface write command (from GenericApi write to sysfs file)
	 * Fields Index   Fields Description
	 * 0              Sensor ID
	 * 1              Get bus interface command
	*/
	char *in_buf = (char *) buf;
	__u8 sensor_id;
	int cmd;
	int ret = DTRG_NO_ERROR;

	ntrig_dbg("inside %s in_buf size = %d\n", __func__,
		(int)sizeof(in_buf));

	if ((in_buf == NULL) || (sizeof(in_buf) < 2)) {
		ntrig_err("ntrig inside %s Wrong parameter\n", __func__);
		return DTRG_FAILED;
	}

	sensor_id = in_buf[0];
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	cmd = (int) in_buf[1];

	ntrig_dbg("%s: sensor_id = %d, command = %d\n", __func__, sensor_id,
		cmd);

	if ((cmd == REPORTID_GET_BUS_INTERFACE)) { /* next command is read */
		g_get_bus_interface_read_sensor_id = sensor_id;
		ntrig_dbg(
			"%s: sensor_id = %d, command = %d, next command is "
			"read\n", __func__, sensor_id, cmd);
	} else {
		ntrig_err("%s: invalid command %d\n", __func__, cmd);
		ret = DTRG_FAILED;
	}
	return ret;
}

int g4_read_get_bus_interface_impl(void *buf)
{
	char *out_buf = (char *)buf;

	ntrig_dbg("inside %s out_buf size = %d\n", __func__,
		(int)sizeof(out_buf));

	if ((g_get_bus_interface_read_sensor_id < 0) ||
		(g_get_bus_interface_read_sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__,
			g_get_bus_interface_read_sensor_id);
		return DTRG_FAILED;
	}

	out_buf[0] = g_get_bus_interface_read_sensor_id;

	switch (ntrig_devices_hid[
		g_get_bus_interface_read_sensor_id].bus_type) {
	case TYPE_BUS_USB_HID:
	case TYPE_BUS_USB_RAW:
		out_buf[1] = BUS_INTERFACE_USB;
		break;
	case TYPE_BUS_SPI_HID:
	case TYPE_BUS_SPI_RAW:
		out_buf[1] = BUS_INTERFACE_SPI;
		break;
	case TYPE_BUS_I2C_HID:
	case TYPE_BUS_I2C_RAW:
		out_buf[1] = BUS_INTERFACE_I2C;
		break;
	default:
		out_buf[1] = -1;
		break;
	}

	ntrig_dbg("%s: bus[0] = %d, bus[1] = %d\n", __func__, out_buf[0],
		out_buf[1]);

	/* reset global read variables */
	g_get_bus_interface_read_sensor_id = -1;
	ntrig_dbg("Leaving %s\n", __func__);

	return 2;
		/* additional byte containing sensor_id at the begining of
		 * the buffer */
}

int g4_setup_get_bus_interface(message_callback *read_get_bus_interface,
	message_callback *write_get_bus_interface)
{
	*read_get_bus_interface = g4_read_get_bus_interface_impl;
	*write_get_bus_interface = g4_write_get_bus_interface_impl;
	return DTRG_NO_ERROR;
}

/******************************************************************************/
/**
 * Dispatcher configuration get/set functions
 */

int g4_read_counters(struct _ntrig_counter **counters_list, int *length)
{
	if (ntrig_devices_hid[0].read_counters != NULL)
		return ntrig_devices_hid[0].read_counters(counters_list,
			length);
	return DTRG_FAILED;
}

void g4_reset_counters(void)
{
	if (ntrig_devices_hid[0].reset_counters != NULL)
		ntrig_devices_hid[0].reset_counters();
}

int g4_setup_config_counters(read_counters_callback *get_counters_local,
	reset_counters_callback *reset_counters_loacl)
{
	*get_counters_local = g4_read_counters;
	*reset_counters_loacl = g4_reset_counters;
	return DTRG_NO_ERROR;
}

/******************************************************************************/

int g4_allocate_device(struct _ntrig_bus_device **dev)
{
	*dev = kzalloc(sizeof(struct _ntrig_bus_device), GFP_KERNEL);
	if (!(*dev)) {
		ntrig_dbg("inside %s: error allocating device\n", __func__);
		return -ENOMEM;
	}
	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_allocate_device);

int g4_remove_device(struct _ntrig_bus_device **dev)
{
	kfree(*dev);
	*dev = NULL;
	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_remove_device);

void g4_set_ntrig_devices_hid_data(int index, int dev_type, char *bus_id,
	struct _ntrig_dev_ncp_func *ncp_func)
{
	ntrig_dbg("Inside %s\n", __func__);

	if (ncp_func) {
		if (ncp_func->read != NULL) {
			ntrig_devices_hid[index].read_ncp = ncp_func->read;
			ntrig_dbg("Inside %s - registering read_ncp function\n",
				__func__);
		}
		if (ncp_func->write != NULL) {
			ntrig_devices_hid[index].write_ncp = ncp_func->write;
			ntrig_dbg(
				"Inside %s - registering write_ncp "
				"function\n", __func__);
		}
		if (ncp_func->dev != NULL)
			ntrig_devices_hid[index].dev_ncp = ncp_func->dev;
		if (ncp_func->read_counters != NULL) {
			ntrig_devices_hid[index].read_counters =
				ncp_func->read_counters;
			ntrig_dbg(
				"Inside %s - registering read_cuonters "
				"function\n", __func__);
		}
		if (ncp_func->reset_counters != NULL) {
			ntrig_devices_hid[index].reset_counters =
				ncp_func->reset_counters;
			ntrig_dbg(
				"Inside %s - registering reset_counters "
				"function\n", __func__);
		}
	}

	strlcpy(&ntrig_devices_hid[index].bus_id[0], bus_id, MAX_BUS_ID_LENGTH);
		/* bus_id already set */
	ntrig_devices_hid[index].bus_type = (__u8)dev_type;

	ntrig_dev_hid_ref_count[index]++;
}

void g4_set_ntrig_devices_raw_data(int index, int dev_type, char *bus_id,
	struct _ntrig_dev_ncp_func *ncp_func)
{
	ntrig_dbg("Inside %s\n", __func__);
	if (ncp_func) {
		if (ncp_func->read != NULL) {
			ntrig_devices_raw[index].read_ncp = ncp_func->read;
			ntrig_dbg("Inside %s - registering read_ncp function\n",
				__func__);
		}
		if (ncp_func->write != NULL) {
			ntrig_devices_raw[index].write_ncp = ncp_func->write;
			ntrig_dbg(
				"Inside %s - registering write_ncp "
				"function\n", __func__);
		}
		strlcpy(&ntrig_devices_raw[index].bus_id[0], bus_id,
			MAX_BUS_ID_LENGTH);
			/* bus_id already set */
		if (ncp_func->dev != NULL)
			ntrig_devices_raw[index].dev = ncp_func->dev;
	}
	ntrig_dev_raw_ref_count[index]++;
}

static int is_same_bus_id(char *id1, char *id2)
{
	return (strncmp(id1, id2, MAX_BUS_ID_LENGTH) == 0);
}

/**
 * Register device with the dispatcher.
 * Dispatcher allocates sensor_id and saves pointers to ncp read/write HID/RAW
 * functions.
 * sensor_id is the index in the functions tables ntrig_devices_hid /
 * ntrig_devices_raw.
 */
__u8 g4_RegNtrigDispatcher(int dev_type, char *bus_id,
	struct _ntrig_dev_ncp_func *ncp_func)
{
	int i;
	int ret = DTRG_NO_ERROR;
	int sensor_id = -1;

	ntrig_dbg("inside %s\n", __func__);

	if (!bus_id) {
		ntrig_err(
			"inside %s - FAILED TO REGISTER -  missing bus_id!!\n",
			__func__);
		return DTRG_FAILED;
	}

	/* 1st path - search for registered devices */
	for (i = 0; i < MAX_NUMBER_OF_SENSORS; i++) {
		ntrig_dbg("inside %s - 1st pass - i=%d\n", __func__, i);
		/* same device - add function pointers */
		if (is_same_bus_id(ntrig_devices_hid[i].bus_id, bus_id) ||
			is_same_bus_id(ntrig_devices_raw[i].bus_id, bus_id)) {
			switch (dev_type) {
			case TYPE_BUS_USB_HID:
			case TYPE_BUS_SPI_HID:
			case TYPE_BUS_I2C_HID:
				g4_set_ntrig_devices_hid_data(i, dev_type, bus_id,
					ncp_func);
				sensor_id = i;
				break;
			case TYPE_BUS_USB_RAW:
			case TYPE_BUS_SPI_RAW:
			case TYPE_BUS_I2C_RAW:
				g4_set_ntrig_devices_raw_data(i, dev_type, bus_id,
					ncp_func);
				sensor_id = i;
				break;
			default:
				ntrig_err(
					"inside %s - FAILED TO REGISTER -  "
					"illegal dev type: %d\n", __func__,
					dev_type);
				ret = DTRG_FAILED;
			}

			ntrig_dbg("inside %s - updated sensor id = %d\n",
				__func__, sensor_id);

			if (sensor_id >= 0)
				break;

		}
	}

	if (sensor_id >= 0) {
		ntrig_dbg("Exiting %s - after 1st pass, sensor_id= %d\n",
			__func__, sensor_id);
		return (__u8) sensor_id;
	}

	/* 2nd pass - Add new _ntrig_bus_device in first free place in table */
	for (i = 0; i < MAX_NUMBER_OF_SENSORS; i++) {
		ntrig_dbg("inside %s - 2nd pass - i=%d\n", __func__, i);
		switch (dev_type) {
		case TYPE_BUS_USB_HID:
		case TYPE_BUS_SPI_HID:
		case TYPE_BUS_I2C_HID:
			if (ntrig_devices_hid[i].bus_id[0] == 0) /*empty place*/
				g4_set_ntrig_devices_hid_data(i, dev_type, bus_id,
					ncp_func);
			sensor_id = i;
			break;

		case TYPE_BUS_USB_RAW:
		case TYPE_BUS_SPI_RAW:
		case TYPE_BUS_I2C_RAW:
			if (ntrig_devices_raw[i].bus_id[0] == 0) /*empty place*/
				g4_set_ntrig_devices_raw_data(i, dev_type, bus_id,
					ncp_func);
			sensor_id = i;
			break;

		default:
			ntrig_err(
				"inside %s - FAILED TO REGISTER -  "
				"illegal dev type: %d\n", __func__, dev_type);
			return DTRG_FAILED;
		}

		if (sensor_id >= 0) {
			g_num_sensors++;
			break;
		}

		ntrig_dbg("inside %s - adding sensor id = %d\n", __func__,
			sensor_id);
	}

	/* No empty spot in table for new _ntrig_bus_device */
	if (i >= MAX_NUMBER_OF_SENSORS) {
		ntrig_err(
			"inside %s - FAILED TO REGISTER -  device for all "
			"sensors already exists !!\n", __func__);
		return DTRG_FAILED;
	}

	g4_init_synch_data_touch_ev();
	/* memory lock with semaphore is done in calling function
	 * (ncp_read in ncp driver) */

	ntrig_dbg("Leaving %s - sensor_id=%d\n", __func__, sensor_id);
	return (__u8) sensor_id;
}
EXPORT_SYMBOL_GPL(g4_RegNtrigDispatcher);

void g4_release_ntrig_devices_io(__u8 sensor_id, int dev_type, char *bus_id)
{
	ntrig_dbg("inside %s - for sensor %d\n", __func__, sensor_id);

	/* NOTE: single_touch_device & multi_touch_device are released by
	 * functions release_single_touch & release_multi_touch */
	switch (dev_type) {
	case TYPE_BUS_USB_HID:
	case TYPE_BUS_SPI_HID:
	case TYPE_BUS_I2C_HID:
		if (is_same_bus_id(ntrig_devices_hid[sensor_id].bus_id,
			bus_id)) {
			/* same device */
			ntrig_devices_hid[sensor_id].read_ncp = NULL;
			ntrig_devices_hid[sensor_id].write_ncp = NULL;
			ntrig_devices_hid[sensor_id].read_sensor = NULL;
			ntrig_devices_hid[sensor_id].write_sensor = NULL;
			ntrig_devices_hid[sensor_id].dev_ncp = NULL;
			ntrig_devices_hid[sensor_id].dev_hid = NULL;
			ntrig_dev_hid_ref_count[sensor_id]--;
			if (ntrig_dev_hid_ref_count[sensor_id] == 0) {
				memset(ntrig_devices_hid[sensor_id].bus_id, 0,
					sizeof(char) * MAX_BUS_ID_LENGTH);
				g_num_sensors--;
			}
		} else
			ntrig_dbg(
				"inside %s - no match for sensor_id %d, "
				"bus_id %s, dev_type %d\n", __func__, sensor_id,
				bus_id, dev_type);
		break;

	case TYPE_BUS_USB_RAW:
	case TYPE_BUS_SPI_RAW:
	case TYPE_BUS_I2C_RAW:
		if (is_same_bus_id(ntrig_devices_raw[sensor_id].bus_id,
			bus_id)) {
			/* same device */
			ntrig_devices_raw[sensor_id].read_ncp = NULL;
			ntrig_devices_raw[sensor_id].write_ncp = NULL;
			ntrig_devices_raw[sensor_id].dev = NULL;
			ntrig_dev_raw_ref_count[sensor_id]--;
			if (ntrig_dev_raw_ref_count[sensor_id] == 0) {
				memset(ntrig_devices_raw[sensor_id].bus_id, 0,
					sizeof(char) * MAX_BUS_ID_LENGTH);
				g_num_sensors--;
			}
		} else {
			ntrig_dbg(
				"inside %s - no match for sensor_id %d, "
				"bus_id %s, dev_type %d\n", __func__, sensor_id,
				bus_id, dev_type);
		}
		break;
	default:
		ntrig_err(
			"inside %s - FAILED TO UNREGISTER -  illegal dev "
			"type: %d\n", __func__, dev_type);
		break;
	}

	ntrig_dbg("Leaving %s\n", __func__);
}

void g4_UnregNtrigDispatcher(void *dev, __u8 sensor_id, int dev_type, char *bus_id)
{
	ntrig_dbg("inside %s\n", __func__);
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return;
	}

	if ((dev_type == TYPE_BUS_USB_HID) ||
		/* RAW drivers don't have input_device */
		(dev_type == TYPE_BUS_SPI_HID) ||
		(dev_type == TYPE_BUS_I2C_HID)) {
		g4_release_single_touch(sensor_id);
		g4_release_multi_touch(sensor_id);
	}
	g4_release_ntrig_devices_io(sensor_id, dev_type, bus_id);
}
EXPORT_SYMBOL_GPL(g4_UnregNtrigDispatcher);

int g4_send_message(void *buf)
{
	struct mr_message_types_s *message_packet =
		(struct mr_message_types_s *) buf;
	int ret = DTRG_NO_ERROR;
	int sensor_id = -1;
	data_send send_func = NULL;
	struct input_dev *input_device = NULL;

	/*ntrig_dbg("inside %s\n", __func__);*/

	switch (message_packet->type) {
	case MSG_PEN_EVENTS:
		sensor_id = (int) message_packet->msg.pen_event.sensor_id;
		/*ntrig_dbg("%s: MSG_PEN_EVENTS\n", __func__);*/
		if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
			ntrig_err("%s: MSG_PEN_EVENTS: wrong sensor_id: %d\n",
				__func__, sensor_id);
			ret = DTRG_FAILED;
			break;
		}
#if (defined MT_REPORT_TYPE_B) && (!defined SINGLE_PEN_DEV)
		input_device = (struct input_dev *)
			ntrig_devices_hid[sensor_id].multi_touch_device;
		send_func = (data_send) ntrig_send_pen_typeB;
#else
		input_device = (struct input_dev *)
			ntrig_devices_hid[sensor_id].single_touch_device;
		send_func = (data_send) ntrig_send_pen;
#endif
		break;
	case MSG_FINGER_PARSE:
		sensor_id = (int) message_packet->msg.fingers_event.sensor_id;
		/*ntrig_dbg("%s: MSG_FINGER_PARSE\n", __func__);*/
		if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
			ntrig_err("%s: MSG_FINGER_PARSE: wrong sensor_id: %d\n",
				__func__, sensor_id);
			ret = DTRG_FAILED;
			break;
		}
		input_device = (struct input_dev *)
			ntrig_devices_hid[sensor_id].multi_touch_device;
#ifdef MT_REPORT_TYPE_B
		send_func = (data_send) ntrig_send_multi_touch_typeB;
#else
		send_func = (data_send) ntrig_send_multi_touch;
#endif
		break;
	case MSG_BUTTON_EVENTS:
		sensor_id = (int) message_packet->msg.buttons_event.sensor_id;
		/*ntrig_dbg("%s: MSG_BUTTON_EVENTS\n", __func__);*/
		if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
			ntrig_err(
				"%s: MSG_BUTTON_EVENTS: wrong sensor_id: "
				"%d\n", __func__, sensor_id);
			ret = DTRG_FAILED;
			break;
		}
		input_device = (struct input_dev *)
			ntrig_devices_hid[sensor_id].single_touch_device;
		send_func = (data_send) ntrig_send_button;
		break;
	default:
		ret = DTRG_FAILED;
	}
	if ((input_device == NULL) || (ret == DTRG_FAILED)) {
		ntrig_err("ntrig inside %s, failed to send touch event\n",
			__func__);
		return DTRG_FAILED;
	}

	ret = send_func(input_device, message_packet);
	/*print_message(message_packet);*/
	g4_ntrig_push_to_direct_events(message_packet);
	return ret;
}

int g4_WriteHIDNTRIG(void *buf)
{
	int ret = DTRG_FAILED;
	unsigned long flag;

	if (!buf) {
		ntrig_err("ntrig inside %s Wrong parameter\n", __func__);
		return ret;
	}

	memory_lock_touch_ev(&flag);
	ntrig_dbg("ntrig inside %s send message directly to host events\n",
		__func__);
	ret = g4_send_message(buf);	  /* send directly to host events */
	memory_unlock_touch_ev(&flag);
	return ret;
}
EXPORT_SYMBOL_GPL(g4_WriteHIDNTRIG);

int g4_create_single_touch(struct _ntrig_bus_device *dev, __u8 sensor_id)
{
	struct input_dev *input_device = NULL;

	ntrig_dbg("inside %s\n", __func__);
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	/* if device was allocated before we need to release resource */
	if (g4_check_single_touch(sensor_id))
		g4_release_single_touch(sensor_id);

	input_device = input_allocate_device();
	if (!input_device) {
		ntrig_err(
			"ntrig inside %s ERROR! : cdev_alloc() error!!! "
			"no memory!!\n", __func__);
		return DTRG_FAILED;
	}

	input_device->name = NTRIG_SINGLE_TOUCH_DEV_NAME;
	input_device->phys = dev->phys;
	input_device->open = ntrig_input_open;
	input_device->close = ntrig_input_close;

	__set_bit(EV_KEY, input_device->evbit);
	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(ABS_X, input_device->absbit);
	__set_bit(ABS_Y, input_device->absbit);

	__set_bit(BTN_TOOL_PEN, input_device->keybit);
		/* makes Linux process it as a pen */
	__set_bit(BTN_TOUCH, input_device->keybit);

	/* support pen buttons */
	__set_bit(BTN_STYLUS, input_device->keybit);
	__set_bit(BTN_STYLUS2, input_device->keybit);

	input_set_abs_params(input_device, ABS_X,
		dev->logical_min_x + g4_get_touch_screen_border_pen_left(),
		dev->logical_max_x - g4_get_touch_screen_border_pen_right(), 0, 0);
	input_set_abs_params(input_device, ABS_Y,
		dev->logical_min_y + g4_get_touch_screen_border_pen_down(),
		dev->logical_max_y + g4_get_touch_screen_border_pen_up(), 0, 0);
	input_set_abs_params(input_device, ABS_PRESSURE, dev->pressure_min,
		dev->pressure_max, 0, 0);

	if (input_register_device(input_device)) {
		ntrig_err("ntrig inside %s input register device fail!!\n",
			__func__);
		input_free_device(input_device);
		return	DTRG_FAILED;
	}

	ntrig_devices_hid[sensor_id].single_touch_device = input_device;

	ntrig_dbg("ntrig inside %s : n-trig single touch event queue created\n",
		__func__);
	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_create_single_touch);

int g4_create_multi_touch(struct _ntrig_bus_device *dev, __u8 sensor_id)
{
	struct input_dev *input_device = NULL;

	ntrig_dbg("inside %s\n", __func__);
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	/* if device was allocated before we need to release resource */
	if (g4_check_multi_touch(sensor_id))
		g4_release_multi_touch(sensor_id);

	input_device = input_allocate_device();
	if (!input_device) {
		ntrig_err(
			"ntrig inside %s ERROR! : cdev_alloc() error!!! "
			"no memory!!\n", __func__);
		return DTRG_FAILED;
	}

	input_device->name = NTRIG_MULTI_TOUCH_DEV_NAME;
	input_device->phys = dev->phys;
	input_device->open = ntrig_input_open;
	input_device->close = ntrig_input_close;

	g4_config_multi_touch(dev, input_device);

#ifdef MT_REPORT_TYPE_B
#ifndef SINGLE_PEN_DEV
	/* added for pen over MT */
	__set_bit(EV_KEY, input_device->evbit);
	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(BTN_STYLUS, input_device->keybit);
	__set_bit(BTN_STYLUS2, input_device->keybit);
#endif
	if (input_mt_init_slots(input_device, ABS_MT_TRACKING_ID_MAX + 1) < 0) {
		printk(KERN_INFO "failed allocate slots\n");
		input_free_device(input_device);
		return DTRG_FAILED;
	}
#else
	/* set larger packet size to support newer sensors with 10 fingers */
	/* only for kernel versions > 2.6.35 (G4 always uses newer versions) */
	input_set_events_per_packet(input_device, 60);
#endif	/* MT_REPORT_TYPE_B */

	if (input_register_device(input_device))	{
		ntrig_err("ntrig inside %s input register device fail!!\n",
			__func__);
		input_free_device(input_device);
		return	DTRG_FAILED;
	}
	ntrig_devices_hid[sensor_id].x_min = dev->logical_min_x +
		g4_get_touch_screen_border_left();
	ntrig_devices_hid[sensor_id].x_max = dev->logical_max_x -
		g4_get_touch_screen_border_right();
	ntrig_devices_hid[sensor_id].y_min = dev->logical_min_y +
		g4_get_touch_screen_border_down();
	ntrig_devices_hid[sensor_id].y_max = dev->logical_max_y -
		g4_get_touch_screen_border_up();
	ntrig_devices_hid[sensor_id].multi_touch_device = input_device;

	/* reset the finger map in case we had left overs from previous
	 * disconnection */
	reset_finger_map();
	ntrig_dbg("ntrig inside %s : n-trig multi touch event queue created\n",
		__func__);

	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_create_multi_touch);

int g4_release_single_touch(__u8 sensor_id)
{
	struct _ntrig_dev_hid *dev = &ntrig_devices_hid[sensor_id];
	ntrig_dbg("inside %s disp-input = %ld\n", __func__,
		(long) dev->single_touch_device);

	/* if device was allocated before we need to release resource */
	if (dev->single_touch_device) {
		if (!dev->is_allocated_externally)
			input_unregister_device(dev->single_touch_device);
		dev->single_touch_device = NULL;
	}

	return DTRG_NO_ERROR;
}

int g4_release_multi_touch(__u8 sensor_id)
{
	struct _ntrig_dev_hid *dev = &ntrig_devices_hid[sensor_id];
	ntrig_dbg("inside %s, disp-input = %ld\n", __func__,
		(long) dev->multi_touch_device);

	/* if device was allocated before we need to release resource */
	if (dev->multi_touch_device) {
		if (!dev->is_allocated_externally)
			input_unregister_device(dev->multi_touch_device);
		dev->multi_touch_device = NULL;
	}

	return DTRG_NO_ERROR;
}


int g4_attach_single_touch(__u8 sensor_id, struct input_dev *input_device)
{
	ntrig_dbg("Inside %s\n", __func__);
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	/* if device was allocated before we need to release resource */
	ntrig_devices_hid[sensor_id].single_touch_device = input_device;
	ntrig_devices_hid[sensor_id].is_allocated_externally = 1;
	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_attach_single_touch);

int g4_attach_multi_touch(__u8 sensor_id, struct input_dev *input_device)
{
	ntrig_dbg("Inside %s\n", __func__);
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d\n", __func__, sensor_id);
		return DTRG_FAILED;
	}

	/* if device was allocated before we need to release resource */
	ntrig_devices_hid[sensor_id].multi_touch_device = input_device;
	ntrig_devices_hid[sensor_id].is_allocated_externally = 1;
	return DTRG_NO_ERROR;
}
EXPORT_SYMBOL_GPL(g4_attach_multi_touch);

bool g4_check_single_touch(__u8 sensor_id)
{
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d, FALSE\n", __func__,
			sensor_id);
		return false;
	}
	return (ntrig_devices_hid[sensor_id].single_touch_device) ?
		true : false;
}
EXPORT_SYMBOL_GPL(g4_check_single_touch);

bool g4_check_multi_touch(__u8 sensor_id)
{
	if ((sensor_id < 0) || (sensor_id >= MAX_NUMBER_OF_SENSORS)) {
		ntrig_err("%s: wrong sensor_id: %d, FALSE\n", __func__,
			sensor_id);
		return false;
	}
	return (ntrig_devices_hid[sensor_id].multi_touch_device) ? true : false;
}
EXPORT_SYMBOL_GPL(g4_check_multi_touch);

#ifdef MT_REPORT_TYPE_B
void g4_swipe_touch(u8 sensor_id)
{
	int i;
	struct _ntrig_dev_hid *dev = &ntrig_devices_hid[sensor_id];
	struct input_dev *input = dev->multi_touch_device;

	pr_info("Ntrig - %s: swiping touch\n", __func__);
	if (g4_pen) {
#ifdef SINGLE_PEN_DEV
		struct input_dev *input_pen = dev->single_touch_device;
		input_report_key(input_pen, BTN_TOOL_PEN, 0x00);
		input_report_key(input_pen, BTN_STYLUS, 0x00);
		input_report_key(input_pen, BTN_STYLUS2, 0x00);
		input_report_key(input_pen, BTN_TOUCH, 0x00);
		input_report_abs(input_pen, ABS_PRESSURE, 0);
		input_sync(input_pen);
#else
		input_mt_slot(input, 0);
		input_mt_report_slot_state(input, MT_TOOL_PEN, false);
		input_sync(input);
#endif
		g4_pen = false;
	}

	if (gCurrentFingers) {
		for (i = 0; gCurrentFingers; i++) {
			if (gCurrentFingers & 1) {
				input_mt_slot(input, i + 1);
				remove_slot_typeB(input, i);
			}
			gCurrentFingers >>= 1;
		}
		input_sync(input);
		gPreviousFingers = 0;
	}
}
#else
void g4_swipe_touch(u8 sensor_id)
{
	ntrig_dbg("ntrig %s: No touch event swipe for protocol A\n", __func__);
}
#endif

static int ntrig_input_open(struct input_dev *dev)
{
	ntrig_dbg("ntrig inside %s\n", __func__);
	return DTRG_NO_ERROR;
}

static void ntrig_input_close(struct input_dev *dev)
{
	ntrig_dbg("ntrig inside %s\n", __func__);
}

/** map from compressed tracking id (0..ABS_MT_TRACKING_ID-1)
 *  to TrackLib tracking id. -1 in a slot means it is free
 */
static int gFingerMap[ABS_MT_TRACKING_ID_MAX];
static int gNumLiveTracks;

static void reset_finger_map()
{
	int i;
	for (i = 0; i < ABS_MT_TRACKING_ID_MAX; i++)
		gFingerMap[i] = -1;
	gNumLiveTracks = 0;
}

/* Find the index of the specified track_id. If this is a new track, allocate
 * a new index if allocate_if_new is true.
 */
static int get_finger_index(int id, int allocate_if_new)
{
	static int first = 1;
	int i;

	if (first) {
		first = 0;
		reset_finger_map();
	}

	/* search for existing finger */
	for (i = 0; i < ABS_MT_TRACKING_ID_MAX; i++) {
		if (gFingerMap[i] == id)
			return i;
	}

	/* search for place for new finger */
	if (allocate_if_new) {
		for (i = 0; i < ABS_MT_TRACKING_ID_MAX; i++) {
			if (gFingerMap[i] < 0) {
				/* found */
				gFingerMap[i] = id;
				gNumLiveTracks++;
				return i;
			}
		}
		/* new finger, and all places are in use (should not happen) */
		ntrig_err(
			"%s: cannot find index for finger, all slots in "
			"use\n", __func__);
	}

	return FINGER_INDEX_NOT_FOUND;
}

static void free_finger_index(int index)
{
	gFingerMap[index] = -1;
	gNumLiveTracks--;
}

/* Handling missed "finger removed" events:
 * If the driver misses "finger removed" messages, the finger map will contain
 * stale entries and the framework may think that the removed fingers are still
 * touching.
 * Since the sensor always sends all the touching fingers, we can safely free
 * any tracks in the finger map that are not part of the current message.
 */


#ifndef MT_REPORT_TYPE_B
/*
 * This function takes a sensor MT message as parameter, finds stale entries in
 * the finger map, and adds their indices to gStaleFingerMapIndices.
 */

static int gStaleFingerMapIndices[ABS_MT_TRACKING_ID_MAX];
static int gStaleFingerMapIndicesCount;

static void find_stale_finger_map_entries(
	struct mr_message_types_s *multi_touch)
{
	int i, j, finger_found;
	int fingers_num = multi_touch->msg.fingers_event.num_of_fingers;
	struct device_finger_s *fingers =
		multi_touch->msg.fingers_event.finger_array;

	gStaleFingerMapIndicesCount = 0;
	for (i = 0; i < ABS_MT_TRACKING_ID_MAX; i++) {
		if (gFingerMap[i] < 0)
			continue;
		finger_found = 0;
		for (j = 0; j < fingers_num; j++) {
			if (fingers[j].track_id == gFingerMap[i]) {
				finger_found = 1;
				break;
			}
		}
		if (!finger_found) {
			/* No finger in current message matches this index */
			ntrig_err(
				"%s: Stale entry in finger map - index %d, "
				"track_id %d (possibly missed 'removed finger' "
				"event)\n", __func__, i, gFingerMap[i]);
			gStaleFingerMapIndices[gStaleFingerMapIndicesCount] = i;
			gStaleFingerMapIndicesCount++;
		}
	}
}
#endif	/* !MT_REPORT_TYPE_B */

static void mark_removed_finger_map_index(int index)
{
	gRemoveFromFingerMap[gRemoveFromFingerMapCount] = index;
	gRemoveFromFingerMapCount++;
}

static void remove_marked_indices_from_finger_map(void)
{
	int i;
	for (i = 0; i < gRemoveFromFingerMapCount; i++)
		free_finger_index(gRemoveFromFingerMap[i]);
	gRemoveFromFingerMapCount = 0;
}

#if (!defined MT_REPORT_TYPE_B) || (defined SINGLE_PEN_DEV)
static int ntrig_send_pen(struct input_dev *input,
	struct mr_message_types_s *pen)
{
	int btn_code;

	if (!input) {
		ntrig_err("%s No Input Queue\n", __func__);
		return DTRG_FAILED;
	}

	/*ntrig_dbg("%s, x=%d,y=%d,pressure=%d,btn_code=%d, btn_removed=%d\n",
	 *	__func__, pen->msg.pen_event.x_coord,
	 *	pen->msg.pen_event.y_coord, pen->msg.pen_event.pressure,
	 *	pen->msg.pen_event.btn_code, pen->msg.pen_event.btn_removed);
	 */

	/*
	 * button code algorithm:
	 * bit 0(in range) is always set
	 * bit 1(tip) is set when pen tip is touching the surface, except as
	 *	noted below
	 * bit 2(right click) is set when first button is pressed, clear when
	 *	not pressed
	 * the second button is somewhat tricky: when button is pressed and tip
	 *	is not pressed,
	 * bit 3+bit 0 will be set, but when second button is pressed and tip is
	 *	also pressed,
	 * bit 3+bit 4+bit 0 will be set (and bit 1, the tip will be clear in
	 *	this case!!)
	 */
	btn_code = pen->msg.pen_event.btn_code;
	/*ntrig_dbg("%s: btn_code = %d\n", __func__, btn_code);*/
	if (btn_code & EVENT_PEN_BIT_IN_RANGE) {
		g4_pen = true;
		/* handle the second button + tip first as it is special */
		if (btn_code & EVENT_PEN_BIT_ERASER) {
			/* second button + tip */
			input_report_key(input, BTN_TOOL_PEN, 0x00);
			input_report_key(input, BTN_STYLUS2, 0x01);
			input_report_key(input, BTN_TOUCH, 0x01);
		} else {
			/* normal handling of tip, stylus 2 */
			if (btn_code & EVENT_PEN_BIT_TIP) {
				/* stop hover */
				input_report_key(input, BTN_TOOL_PEN, 0x00);
				/* tip touch */
				input_report_key(input, BTN_TOUCH, 0x01);
			} else {
				/* start hover */
				input_report_key(input, BTN_TOOL_PEN, 0x01);
				/* no tip touch */
				input_report_key(input, BTN_TOUCH, 0x00);
			}
			input_report_key(input, BTN_STYLUS2,
				(btn_code & EVENT_PEN_BIT_INVERT) ? 1 : 0);
		}
		input_report_key(input, BTN_STYLUS,
			(btn_code & EVENT_PEN_BIT_RIGHT_CLICK) ? 1 : 0);
	} else
		g4_pen = false;

	input_report_abs(input, ABS_X, pen->msg.pen_event.x_coord);
	input_report_abs(input, ABS_Y, pen->msg.pen_event.y_coord);
	input_report_abs(input, ABS_PRESSURE, pen->msg.pen_event.pressure);
	input_sync(input);

	return DTRG_NO_ERROR;
}
#endif	/* MT_REPORT_TYPE_B */

#ifdef MT_REPORT_TYPE_B
static int ntrig_send_pen_typeB(struct input_dev *input,
	struct mr_message_types_s *pen)
{
	int btn_code;
	struct device_pen_s *pen_event;
	if (!input) {
		ntrig_err("%s No Input Queue\n", __func__);
		return DTRG_FAILED;
	}
	pen_event = &pen->msg.pen_event;
	/*
	 * button code algorithm:
	 * bit 0(in range) is always set
	 * bit 1(tip) is set when pen tip is touching the surface, except as
	 *	noted below
	 * bit 2(right click) is set when first button is pressed, clear when
	 *	not pressed
	 * the second button is somewhat tricky: when button is pressed and tip
	 *	is not pressed,
	 * bit 3+bit 0 will be set, but when second button is pressed and tip is
	 *	also pressed,
	 * bit 3+bit 4+bit 0 will be set (and bit 1, the tip will be clear in
	 *	this case!!)
	 */
	btn_code = pen_event->btn_code;
	/*ntrig_dbg("%s: btn_code = %d\n", __func__, btn_code);*/

	input_mt_slot(input, 0);
		/*pen is always at slot 0, fingers are reported as slots 1-10*/
	input_mt_report_slot_state(input, MT_TOOL_PEN,
		btn_code & EVENT_PEN_BIT_IN_RANGE);

	if (btn_code & EVENT_PEN_BIT_IN_RANGE) {
		g4_pen = true;
		/* handle the second button + tip first as it is special */
		if (btn_code & EVENT_PEN_BIT_ERASER) {
			input_report_key(input, BTN_STYLUS2, 0x01);
		} else {
			/* normal handling of tip, stylus 2 */
			input_report_key(input, BTN_STYLUS2,
				(btn_code & EVENT_PEN_BIT_INVERT) ? 1 : 0);
		}
		input_report_key(input, BTN_STYLUS,
			(btn_code & EVENT_PEN_BIT_RIGHT_CLICK) ? 1 : 0);
		input_report_abs(input, ABS_MT_POSITION_X, pen_event->x_coord);
		input_report_abs(input, ABS_MT_POSITION_Y, pen_event->y_coord);
		input_report_abs(input, ABS_MT_PRESSURE, pen_event->pressure);
	} else {
		g4_pen = false;
	}
	input_sync(input);

	return DTRG_NO_ERROR;
}

static void remove_slot_typeB(struct input_dev *input, int slot)
{
	input_report_abs(input, ABS_MT_PRESSURE, 0);
	input_mt_report_slot_state(input, MT_TOOL_FINGER, 0);
	mark_removed_finger_map_index(slot);
}

static void markBit(int *map, int index)
{
	*map = *map | (1 << index);
}

static void unMarkBit(int *map, int index)
{
	*map = *map & (~(1 << index));
}

/*
 * This function checks for missed "removed fingers" events from the sensor.
 * If we have at least one finger in the previous report that doesn't exist in
 * the current, it means that we missed a "removed finger" event. We should
 * find those fingers and report them as removed.
 */
static void check_for_missed_remove_fingers(struct input_dev *input)
{
	int j;
	/* Find the fingers in the previous report that are not in part of the
	 * current report.
	 */
	int missed = gPreviousFingers & (~gCurrentFingers);
	if (missed) {
		ntrig_err(
			"ERROR: in %s, found fingers that were in the "
			"previous report and do not exist in the current report"
			", missed finger mask is %d\n", __func__, missed);
		for (j = 0; missed; j++) {
			if (missed & 1) {
				input_mt_slot(input, j + 1);
				remove_slot_typeB(input, j);
			}
			missed >>= 1;
		}
	}
	gPreviousFingers = gCurrentFingers;
	gCurrentFingers = 0;
}

static int ntrig_send_multi_touch_typeB(struct input_dev *input,
	struct mr_message_types_s *multi_touch)
{
	int i;
	int fingers_num = multi_touch->msg.fingers_event.num_of_fingers;
	struct device_finger_s *fingers =
		multi_touch->msg.fingers_event.finger_array;
	int dx, dy, major, minor, orientation;
	int frame_index = multi_touch->msg.fingers_event.frame_index;

	ntrig_dbg("In %s:\n", __func__);
	/* If we missed some 'finger removed' events, free stale tracks */
	if (frame_index != gCurFrameIndex) {
		ntrig_dbg("cur frame index: %hd, got frame index: %hd\n",
			gCurFrameIndex, frame_index);
		check_for_missed_remove_fingers(input);
		gCurFrameIndex = frame_index;
	}

	for (i = 0; i < fingers_num; i++) {
		/* Find finger index, allocate one if finger is new
		 * (unless removed) */
		int slot = get_finger_index(fingers[i].track_id,
			(!fingers[i].removed));
		if (slot < 0) {
			ntrig_err(
				"%s: Can't find/allocate finger index of "
				"finger %d with track_id %d!\n", __func__, i,
				(int)fingers[i].track_id);
			continue;
		}

		input_mt_slot(input, slot + 1);
			/*we use slots 1-20 for fingers, slot 0 for pen*/
		if (fingers[i].removed) {
			ntrig_dbg(
				"finger removed: number %d, track_id %d, "
				"slot %d\n", i, fingers[i].track_id, slot);
			unMarkBit(&gPreviousFingers, slot);
			remove_slot_typeB(input, slot);
		} else {
			ntrig_dbg(
				"finger NOT removed: number %d, track_id %d, "
				"slot %d\n", i, fingers[i].track_id, slot);
			markBit(&gCurrentFingers, slot);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, 1);

			/* WIDTH_MAJOR ,WIDTH_MINOR, ORIENTATION implementation:
			WIDTH_MAJOR = max(dx, dy): major axis of contact ellipse
			WIDTH_MINOR = min(dx, dy): minor axis of contact ellipse
			ORIENTATION = 0 if dy>dx (ellipse aligned on y axis),
				1 otherwise
			*/
			dx = fingers[i].dx;
			dy = fingers[i].dy;
			if (dy >= dx) {
				major = dy;
				minor = dx;
				orientation = 0;
			} else {
				major = dx;
				minor = dy;
				orientation = 1;
			}

			input_report_abs(input, ABS_MT_WIDTH_MAJOR, major);
			input_report_abs(input, ABS_MT_WIDTH_MINOR, minor);
			input_report_abs(input, ABS_MT_ORIENTATION,
				orientation);

			input_report_abs(input, ABS_MT_POSITION_X,
				fingers[i].x_coord);
			input_report_abs(input, ABS_MT_POSITION_Y,
				fingers[i].y_coord);
			input_report_abs(input, ABS_MT_PRESSURE, 25);
		}
	}
	input_sync(input);

	/* Free the removed fingers in the finger map */
	remove_marked_indices_from_finger_map();

	return DTRG_NO_ERROR;
}
#endif /* MT_REPORT_TYPE_B */

#ifndef MT_REPORT_TYPE_B
static int ntrig_send_multi_touch(struct input_dev *input,
	struct mr_message_types_s *multi_touch)
{
	int i = 0;
	int fingers_num = multi_touch->msg.fingers_event.num_of_fingers;
	struct device_finger_s *fingers =
		multi_touch->msg.fingers_event.finger_array;
	int index, first;
	int fingerCount = 0;
	int dx, dy, major, minor, orientation;

	ntrig_dbg("Inside %s\n", __func__);
	if (!input) {
		ntrig_err("%s No Input Queue\n", __func__);
		return DTRG_FAILED;
	}

	/* If we missed some 'finger removed' events, free stale tracks */
	if (gNumLiveTracks > fingers_num) {
		find_stale_finger_map_entries(multi_touch);
		for (i = 0; i < gStaleFingerMapIndicesCount; i++) {
			int index = gStaleFingerMapIndices[i];
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0x0);
			/* ntrig_dbg("%s: sent ABS_MT_TOUCH_MAJOR, 0x0\n",
			 *	__func__); */
			input_report_abs(input, ABS_MT_TRACKING_ID, index + 1);
				/* fingers use slots 1-20, pen uses slot 0 */
			input_mt_sync(input);
			mark_removed_finger_map_index(index);
		}
		gStaleFingerMapIndicesCount = 0;
	}

	/**
	*   [48/0x30] - ABS_MT_TOUCH_MAJOR  0 .. 255
	*   [50/0x32] - ABS_MT_WIDTH_MAJOR  0 .. 30
	*   [53/0x35] - ABS_MT_POSITION_X   0 .. 1023
	*   [54/0x36] - ABS_MT_POSITION_Y   0.. 599
	*   ABS_MT_POSITION_Y =
	*/

	/*ntrig_dbg("%s, !!!!!Number Of Fingers=%d\n", __func__, fingers_num);*/

	for (i = 0; i < fingers_num; i++) {
		if (!fingers[i].removed)
			++fingerCount;
	}

	first = 1;
	for (i = 0; i < fingers_num; i++) {
		/* Find finger index, allocate one if finger is new
		 * (unless removed) */
		index = get_finger_index(fingers[i].track_id,
			(!fingers[i].removed));
		if (index < 0) {
			ntrig_err(
				"%s: Can't find/allocate finger index of "
				"finger %d with track_id %d!\n", __func__, i,
				(int)fingers[i].track_id);
			/* This should not ever happen, but if it happens, it's
			 * better to clear the finger map so we can at least
			 * continue working.
			 * *** NOTE: At this stage we're not supposed to support
			 * type-A any longer - all products switched to type-B.
			 */
			reset_finger_map();
			continue;
		}

		/* TODO: should be removed in Ubuntu 11.04 */
		/* temporary patch for Ubuntu multi-touch support (single touch
		 * simulation + gestures) */
		if (first) {
			ntrig_simulate_single_touch(input, fingers);
			first = 0;
		}

		ntrig_dbg(
			"%s: fingers[%d].removed=%d, lastFingerCount=%d, "
			"fingerCount = %d\n", __func__, i,
			fingers[i].removed,  lastFingerCount, fingerCount);

		if (fingers[i].removed) {
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0x0);
			/* ntrig_dbg("%s: sent  ABS_MT_TOUCH_MAJOR,0x0\n",
			 *	__func__); */
			mark_removed_finger_map_index(index);
		} else {
			/* report a fixed TOUCH_MAJOR to simulate fixed pressure
			 * for fingers */
			input_report_abs(input, ABS_MT_TOUCH_MAJOR,
				ABS_MT_TOUCH_MAJOR_VAL);
			/* ntrig_dbg("%s: sent ABS_MT_TOUCH_MAJOR,0x32\n",
			 *	__func__); */

			/* WIDTH_MAJOR ,WIDTH_MINOR, ORIENTATION implementation:
			WIDTH_MAJOR = max(dx, dy): major axis of contact ellipse
			WIDTH_MINOR = min(dx, dy): minor axis of contact ellipse
			ORIENTATION = 0 if dy>dx (ellipse aligned on y axis),
				1 otherwise
			*/
			dx = fingers[i].dx;
			dy = fingers[i].dy;
			if (dy >= dx) {
				major = dy;
				minor = dx;
				orientation = 0;
			} else {
				major = dx;
				minor = dy;
				orientation = 1;
			}

			input_report_abs(input, ABS_MT_WIDTH_MAJOR, major);
			input_report_abs(input, ABS_MT_WIDTH_MINOR, minor);
			input_report_abs(input, ABS_MT_ORIENTATION,
				orientation);

			input_report_abs(input, ABS_MT_POSITION_X,
				fingers[i].x_coord);
			input_report_abs(input, ABS_MT_POSITION_Y,
				fingers[i].y_coord);
		}
		/* TRACKING_ID: use index */
		input_report_abs(input, ABS_MT_TRACKING_ID, index + 1);
			/*fingers use indexes 1-20, pen uses index 0*/
		input_mt_sync(input);
	}
	input_sync(input);

	lastFingerCount = fingerCount;

	/* Free the removed fingers in the finger map */
	remove_marked_indices_from_finger_map();

	return DTRG_NO_ERROR;
}
#endif /* ! MT_REPORT_TYPE_B */

static int ntrig_send_button(struct input_dev *input,
	struct mr_message_types_s *buttons_msg)
{
	int buttons_num	= buttons_msg->msg.buttons_event.num_of_buttons;
	struct capacitive_button_s *buttons =
		buttons_msg->msg.buttons_event.buttons_array;
	int i;

	if (!input) {
		ntrig_err("%s No Input Queue\n", __func__);
		return DTRG_FAILED;
	}

	/*ntrig_dbg("%s, set_button=%d, remove_button=%d\n", __func__,
	 *	buttons_msg->msg.button_event.btn_set_code,
	 *	buttons_msg->msg.button_event.btn_rmv_code);
	 */

	for (i = 0; i < buttons_num; ++i)
		input_report_key(input, buttons[i].btn_code,
			buttons[i].btn_mode);
	input_sync(input);

	return DTRG_NO_ERROR;
}

/******************************************************************************/

static int __init ntrig_dispatcher_init(void)
{
	printk(KERN_INFO "Dispatcher Driver Version %s\n",
		NTRIG_DRIVER_VERSION);
		/*DEBUG Use until we stablize the version*/
	g4_ntrig_dispatcher_sysfs_init();
	g4_bus_init_ncp();
	g4_bus_init_direct_events();
	return 0;
}

static void __exit ntrig_dispatcher_exit(void)
{
	g4_bus_exit_direct_events();
	g4_bus_exit_ncp();
	g4_ntrig_dispathcer_sysfs_exit();
}


module_init(ntrig_dispatcher_init);
module_exit(ntrig_dispatcher_exit);

MODULE_LICENSE("GPL");
