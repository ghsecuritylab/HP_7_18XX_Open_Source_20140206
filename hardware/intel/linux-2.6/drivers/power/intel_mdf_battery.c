/*
 * intel_mdf_battery.c - Intel Medfield MSIC Internal charger and Battery Driver
 *
 * Copyright (C) 2010 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ananth Krishna <ananth.krishna.r@intel.com>,
 *         Anantha Narayanan <anantha.narayanan@intel.com>
 *         Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/ipc_device.h>
#include <linux/power_supply.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <linux/sfi.h>
#include <linux/wakelock.h>
#include <linux/async.h>
#include <linux/reboot.h>

#include <asm/intel_scu_ipc.h>
#include <linux/usb/penwell_otg.h>
#include <linux/power/intel_mdf_battery.h>
#include <asm/intel_mid_gpadc.h>

#include "intel_mdf_charger.h"
#include "linux/sfi.h"

#define DRIVER_NAME "intel_mdf_battery"
#define CHARGER_PS_NAME "msic_charger"

#define SFI_SIG_OEM0        "OEM0"
#define IRQ_KFIFO_ELEMENT	1

#define MODE_SWITCH_VOLT_OFF 50 /* 25mV*/

static void *otg_handle;
static struct device *msic_dev;
static struct power_supply *fg_psy;

static char *msic_power_supplied_to[] = {
			"msic_battery",
#ifdef CONFIG_BATTERY_MAX17042
			"max170xx_battery",
			"max17042_battery",
			"max17047_battery",
			"max17050_battery",
#else
			"bq27425_battery-0",
#endif
};

static unsigned long long adc_ttl;
static int adc_sensor_vals[MSIC_BATT_SENSORS];

/*
 * This array represents the Battery Pack thermistor
 * temperature and corresponding ADC value limits
 */
static int const therm_curve_data_P2[THERM_CURVE_MAX_SAMPLES]
	[THERM_CURVE_MAX_VALUES] = {
	/* {temp_max, temp_min, adc_max, adc_min} */
#ifdef BATTERY_TEMP_CURV_P701T
	{-15, -20, 977, 961},
	{-10, -15, 961, 941},
	{-5, -10, 941, 917},
	{0, -5, 917, 887},
	{5, 0, 887, 853},
	{10, 5, 853, 813},
	{15, 10, 813, 769},
	{20, 15, 769, 720},
	{25, 20, 720, 669},
	{30, 25, 669, 615},
	{35, 30, 615, 561},
	{40, 35, 561, 508},
	{45, 40, 508, 456},
	{50, 45, 456, 407},
	{55, 50, 407, 357},
	{60, 55, 357, 315},
	{65, 60, 315, 277},
	{70, 65, 277, 243},
	{75, 70, 243, 212},
	{80, 75, 212, 186},
	{85, 80, 186, 162},
	{90, 85, 162, 140},
	{100, 90, 140, 107},
#else
	{-15, -20, 1020, 980},
	{-10, -15, 980, 952},
	{-5, -10, 952, 905},
	{0, -5, 905, 876},
	{5, 0, 876, 838},
	{10, 5, 838, 808},
	{15, 10, 808, 730},
	{20, 15, 730, 685},
	{25, 20, 685, 662},
	{30, 25, 662, 621},
	{35, 30, 621, 540},
	{40, 35, 540, 478},
	{45, 40, 478, 434},
	{50, 45, 434, 379},
	{55, 50, 379, 320},
	{60, 55, 320, 277},
	{65, 60, 277, 228},
	{70, 65, 228, 197},
	{75, 70, 197, 164},
	{80, 75, 164, 140},
	{85, 80, 140, 127},
	{90, 85, 127, 107},
	{100, 90, 107, 87},
#endif
};

static int const therm_curve_data_P1[THERM_CURVE_MAX_SAMPLES]
	[THERM_CURVE_MAX_VALUES] = {
	/* {temp_max, temp_min, adc_max, adc_min} */
#ifdef BATTERY_TEMP_CURV_P701T
	{-15, -20, 977, 961},
	{-10, -15, 961, 941},
	{-5, -10, 941, 917},
	{0, -5, 917, 887},
	{5, 0, 887, 853},
	{10, 5, 853, 813},
	{15, 10, 813, 769},
	{20, 15, 769, 720},
	{25, 20, 720, 669},
	{30, 25, 669, 615},
	{35, 30, 615, 561},
	{40, 35, 561, 508},
	{45, 40, 508, 456},
	{50, 45, 456, 407},
	{55, 50, 407, 357},
	{60, 55, 357, 315},
	{65, 60, 315, 277},
	{70, 65, 277, 243},
	{75, 70, 243, 212},
	{80, 75, 212, 186},
	{85, 80, 186, 162},
	{90, 85, 162, 140},
	{100, 90, 140, 107},
#else
	{-15, -20, 1020, 980},
	{-10, -15, 980, 952},
	{-5, -10, 952, 905},
	{0, -5, 905, 876},
	{5, 0, 876, 838},
	{10, 5, 838, 808},
	{15, 10, 808, 730},
	{20, 15, 730, 685},
	{25, 20, 685, 662},
	{30, 25, 662, 621},
	{35, 30, 621, 540},
	{40, 35, 540, 478},
	{45, 40, 478, 434},
	{50, 45, 434, 379},
	{55, 50, 379, 320},
	{60, 55, 320, 277},
	{65, 60, 277, 228},
	{70, 65, 228, 197},
	{75, 70, 197, 164},
	{80, 75, 164, 140},
	{85, 80, 140, 127},
	{90, 85, 127, 107},
	{100, 90, 107, 87},
#endif
};

static int (*therm_curve_data)[THERM_CURVE_MAX_VALUES];

static int BATT_TEMP_MAX_DETA_P1 = 0;
static int BATT_TEMP_MIN_DETA_P1 = 0;
static int BATT_TEMP_MAX_DETA_P2 = 0;
static int BATT_TEMP_MIN_DETA_P2 = 0;

static int BATT_TEMP_MAX_DETA = 0;
static int BATT_TEMP_MIN_DETA = 0;

static struct batt_safety_thresholds *batt_thrshlds;


static struct msic_batt_sfi_prop *sfi_table;

static int error_count;


/*
 * All interrupt request are queued from interrupt
 * handler and processed in the bottom half
 */
static DEFINE_KFIFO(irq_fifo, u32, IRQ_FIFO_MAX);


/* Sysfs Entry for enable or disable Charging from user space */
static ssize_t set_chrg_enable(struct device *device,
			       struct device_attribute *attr, const char *buf,
			       size_t count);
static ssize_t get_chrg_enable(struct device *device,
			       struct device_attribute *attr, char *buf);
static DEVICE_ATTR(charge_enable, S_IRUGO | S_IWUSR, get_chrg_enable,
		   set_chrg_enable);

/* Sysfs Entry to show if lab power supply is used */
static ssize_t get_is_power_supply_conn(struct device *device,
			struct device_attribute *attr, char *buf);
static DEVICE_ATTR(power_supply_conn, S_IRUGO, get_is_power_supply_conn, NULL);

/* Sysfs Entry for disabling/enabling Charger Safety Timer */
static ssize_t set_disable_safety_timer(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t get_disable_safety_timer(struct device *device,
		struct device_attribute *attr, char *buf);
static DEVICE_ATTR(disable_safety_timer, S_IRUGO | S_IWUSR,
		get_disable_safety_timer, set_disable_safety_timer);

static int msic_event_handler(void *arg, int event, struct otg_bc_cap *cap);
static int check_charger_conn(struct msic_power_module_info *mbi);
/*
 * msic usb properties
 */
static enum power_supply_property msic_usb_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int battery_reboot_notifier_callback(struct notifier_block *notifier,
		unsigned long event, void *data);

static void notify_fg_batt_in( struct msic_power_module_info *mbi );
static struct notifier_block battery_reboot_notifier = {
	.notifier_call = battery_reboot_notifier_callback,
};

/**
 * check_batt_psy -check for whether power supply type is battery
 * @dev : Power Supply dev structure
 * @data : Power Supply Driver Data
 * Context: can sleep
 *
 * Return true if power supply type is battery
 *
 */
static int check_batt_psy(struct device *dev, void *data)
{
	struct power_supply *psy = dev_get_drvdata(dev);

	/* check for whether power supply type is battery */
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY) {
		dev_info(msic_dev, "fg chip found:%s\n", psy->name);
		fg_psy = psy;
		return true;
	}

	return false;
}

/**
 * get_fg_chip_psy - identify the Fuel Gauge Power Supply device
 * Context: can sleep
 *
 * Return Fuel Gauge power supply structure
 */
static struct power_supply *get_fg_chip_psy(void)
{
	if (fg_psy)
		return fg_psy;

	/* loop through power supply class */
	class_for_each_device(power_supply_class, NULL, NULL,
				check_batt_psy);
	return fg_psy;
}

/**
 * fg_chip_get_property - read a power supply property from Fuel Gauge driver
 * @psp : Power Supply property
 *
 * Return power supply property value
 *
 */
static int fg_chip_get_property(enum power_supply_property psp)
{
	union power_supply_propval val;
	int ret = -ENODEV;

	if (!fg_psy)
		fg_psy = get_fg_chip_psy();
	if (fg_psy) {
		ret = fg_psy->get_property(fg_psy, psp, &val);
		if (!ret)
			return val.intval;
	}

	return ret;
}

/* Exported Functions to use with Fuel Gauge driver */
bool intel_msic_is_capacity_shutdown_en(void)
{
	if (batt_thrshlds &&
		(batt_thrshlds->fpo1 & FPO1_CAPACITY_SHUTDOWN))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(intel_msic_is_capacity_shutdown_en);

bool intel_msic_is_volt_shutdown_en(void)
{
	if (batt_thrshlds &&
		(batt_thrshlds->fpo1 & FPO1_VOLTAGE_SHUTDOWN))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(intel_msic_is_volt_shutdown_en);

bool intel_msic_is_lowbatt_shutdown_en(void)
{
	if (batt_thrshlds &&
		(batt_thrshlds->fpo1 & FPO1_LOWBATTINT_SHUTDOWN))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(intel_msic_is_lowbatt_shutdown_en);

int intel_msic_get_vsys_min(void)
{
	/* Power Supply subsystem expects voltage in micro volts */
	if (batt_thrshlds && batt_thrshlds->vbatt_sh_min)
		return batt_thrshlds->vbatt_sh_min * 1000;
	else
		return MSIC_BATT_VMIN_THRESHOLD * 1000;
}
EXPORT_SYMBOL(intel_msic_get_vsys_min);

int intel_msic_is_current_sense_enabled(void)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(ipcdev == NULL)
		return 0;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(mbi == NULL)
		return 0;

	return mbi->is_batt_valid;
}
EXPORT_SYMBOL(intel_msic_is_current_sense_enabled);

int intel_msic_check_battery_present(void)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(!ipcdev)
		return MSIC_BATT_NOT_PRESENT;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(!mbi)
		return MSIC_BATT_NOT_PRESENT;

	int val;
	
	mutex_lock(&mbi->batt_lock);
	val = mbi->batt_props.present;
	mutex_unlock(&mbi->batt_lock);

	return val;
}
EXPORT_SYMBOL(intel_msic_check_battery_present);

int intel_msic_check_battery_health(void)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(ipcdev == NULL)
		return 0;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(mbi == NULL)
		return 0;

	int val;

	mutex_lock(&mbi->batt_lock);
	val = mbi->batt_props.health;
	mutex_unlock(&mbi->batt_lock);

	return val;
}
EXPORT_SYMBOL(intel_msic_check_battery_health);

int intel_msic_check_battery_status(void)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(!ipcdev)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(!mbi)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	int val;

	mutex_lock(&mbi->batt_lock);
	val = mbi->batt_props.status;
	mutex_unlock(&mbi->batt_lock);

	return val;
}
EXPORT_SYMBOL(intel_msic_check_battery_status);

static int is_protected_regs(u16 addr)
{
	/* in unsigned kernel mode write to some registers are blocked */
	if (addr == MSIC_BATT_CHR_CHRCVOLTAGE_ADDR ||
		addr == MSIC_BATT_CHR_CHRCCURRENT_ADDR ||
		addr == MSIC_BATT_CHR_PWRSRCLMT_ADDR ||
		addr == MSIC_BATT_CHR_CHRCTRL1_ADDR)
		return true;
	else
		return false;
}

/**
 * handle_ipc_rw_status - handle msic ipc read/write status
 * @error_val : ipc read/write status
 * @address   : msic register address
 * @rw	      : read/write access
 *
 * If the error count is more than MAX_IPC_ERROR_COUNT, report
 * charger and battery health as POWER_SUPPLY_HEALTH_UNSPEC_FAILURE
 *
 * returns error value in case of error else return 0
 */

static inline int handle_ipc_rw_status(int error_val,
		const u16 address, char rw)
{

	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(ipcdev == NULL)
		return 0;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);

	if(mbi == NULL)
		return 0;

	/*
	* Write to protected registers in unsigned kernel
	* mode will return -EIO
	*/
	/* Overwriting result value when failure is not a timeout, to support
	 * properly safety charging : we must ensure that when using
	 * an unsigned kernel, the failing access to protected registers
	 * (expected behaviour, returning -EIO)) will not block the accesses
	 * to the non protected registers.
	 * */
	if ((is_protected_regs(address)) && (rw == MSIC_IPC_WRITE) &&
			(error_val == -EIO))
		return 0;

	if (error_count < MAX_IPC_ERROR_COUNT) {
		error_count++;
		dev_warn(msic_dev, "MSIC IPC %s access to %x failed",
			(rw == MSIC_IPC_WRITE ? "write" : "read"), address);
	} else {

		dev_crit(msic_dev, "MSIC IPC %s access to %x failed",
			(rw == MSIC_IPC_WRITE ? "write" : "read"), address);

		mutex_lock(&mbi->batt_lock);
		/* set battery health */
		if (mbi->batt_props.health == POWER_SUPPLY_HEALTH_GOOD) {
			mbi->batt_props.health =
				POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		}
		mutex_unlock(&mbi->batt_lock);
		power_supply_changed(&mbi->usb);
	}

	return error_val;
}

/**
 * get_batt_fg_curve_index - get the fg curve ID from umip
 * Returns FG curve ID
 */
static int get_batt_fg_curve_index(void)
{
	int mip_offset, i, ret;
	u8 batt_id[BATTID_STR_LEN + 1];
	u8 num_tbls = 0;

	/* get the no.of tables from mip */
	ret = intel_scu_ipc_read_mip((u8 *)&num_tbls, 1,
					UMIP_NO_OF_CFG_TBLS, 0);
	if (ret) {
		dev_warn(msic_dev, "%s: umip read failed\n", __func__);
		goto get_idx_failed;
	}

	/* compare the batt ID provided by SFI table and FG table in mip */
	mip_offset = UMIP_BATT_FG_TABLE_OFFSET + BATT_FG_TBL_BATTID;
	for (i = 0; i < num_tbls; i++) {
		ret = intel_scu_ipc_read_mip(batt_id, BATTID_STR_LEN,
							mip_offset, 0);
		if (ret) {
			dev_warn(msic_dev, "%s: umip read failed\n", __func__);
			goto get_idx_failed;
		}
		dev_info(msic_dev, "[umip] tbl:%d, batt_id:%s\n", i, batt_id);

		if (!strncmp(batt_id, sfi_table->batt_id, BATTID_STR_LEN))
			break;

		mip_offset += UMIP_FG_TBL_SIZE;
		memset(batt_id, 0x0, BATTID_STR_LEN);
	}

	if (i < num_tbls)
		ret = i;
	else
		ret = -ENXIO;

get_idx_failed:
	return ret;
}

/**
 * intel_msic_store_referenced_table - store data to
 * referenced table from preconfigured battery data
 */
static int intel_msic_store_refrenced_table(void)
{
	int mip_offset, ret, batt_index;
	void *data;
	u8 batt_id[BATTID_STR_LEN];

	dev_info(msic_dev, "[sfi->batt_id]:%s\n", sfi_table->batt_id);

	mip_offset = UMIP_REF_FG_TBL + BATT_FG_TBL_BATTID;
	ret = intel_scu_ipc_read_mip(batt_id, BATTID_STR_LEN,
						mip_offset, 0);
	if (ret) {
		dev_warn(msic_dev, "%s: umip read failed\n", __func__);
		goto store_err;
	}

	/*if correct table is already in place then don't do anything*/
	if (!strncmp(batt_id, sfi_table->batt_id, BATTID_STR_LEN)) {
		dev_info(msic_dev,
		 "%s: match found in ref tbl already\n", __func__);
		return 0;
	}

	batt_index = get_batt_fg_curve_index();
	if (batt_index < 0) {
		dev_err(msic_dev,
			"can't find fg battery index\n");
		return batt_index;
	}

	data = kmalloc(UMIP_FG_TBL_SIZE, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto store_err;
	}

	/* read the fg data from batt_index */
	mip_offset = UMIP_BATT_FG_TABLE_OFFSET + UMIP_FG_TBL_SIZE * batt_index;
	ret = intel_scu_ipc_read_mip((u8 *)data, UMIP_FG_TBL_SIZE,
							mip_offset, 0);
	if (ret) {
		dev_warn(msic_dev, "%s: umip read failed\n", __func__);
		kfree(data);
		goto store_err;
	}
	/* write the data to ref table */
	mip_offset = UMIP_REF_FG_TBL;
	ret = intel_scu_ipc_write_umip((u8 *)data, UMIP_FG_TBL_SIZE,
					mip_offset);
	if (ret) {
		dev_warn(msic_dev, "%s: umip read failed\n", __func__);
		kfree(data);
		goto store_err;
	}

	kfree(data);

store_err:
	return ret;
}

/**
 * intel_msic_restore_config_data - restore config data
 * @name : Power Supply name
 * @data : config data output pointer
 * @len : length of config data
 *
 */
int intel_msic_restore_config_data(const char *name, void *data, int len)
{
	int mip_offset, ret;

	/* check if msic charger is ready */
	if (!power_supply_get_by_name(CHARGER_PS_NAME)){
		dev_err(msic_dev, "%s msic charger is not ready", __func__);
		return -EAGAIN;
	}

	ret = intel_msic_store_refrenced_table();
	if (ret < 0) {
		dev_err("%s failed to read fg data\n", __func__);
		return ret;
	}

	/* Read the fuel gauge config data from umip */
	mip_offset = UMIP_REF_FG_TBL + BATT_FG_TBL_BODY;
	ret = intel_scu_ipc_read_mip((u8 *)data, len, mip_offset, 0);
	if (ret)
		dev_warn(msic_dev, "%s: umip read failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(intel_msic_restore_config_data);

/**
 * intel_msic_save_config_data - save config data
 * @name : Power Supply name
 * @data : config data input pointer
 * @len : length of config data
 *
 */
int intel_msic_save_config_data(const char *name, void *data, int len)
{
	int mip_offset, ret;

	/* check if msic charger is ready */
	if (!power_supply_get_by_name("msic_charger"))
		return -EAGAIN;

	/* write the fuel gauge config data to umip */
	mip_offset = UMIP_REF_FG_TBL + BATT_FG_TBL_BODY;
	ret = intel_scu_ipc_write_umip((u8 *)data, len, mip_offset);
	if (ret)
		dev_warn(msic_dev, "%s: umip write failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(intel_msic_save_config_data);

/* Check for valid Temp ADC range */
static bool is_valid_temp_adc(int adc_val)
{
	if (adc_val >= MSIC_BTP_ADC_MIN && adc_val <= MSIC_BTP_ADC_MAX)
		return true;
	else
		return false;
}

/* Temperature conversion Macros */
static int conv_adc_temp(int adc_val, int adc_max, int adc_diff, int temp_diff)
{
	int ret;

	ret = (adc_max - adc_val) * temp_diff;
	return ret / adc_diff;
}

/* Check if the adc value is in the curve sample range */
static bool is_valid_temp_adc_range(int val, int min, int max)
{
	if (val > min && val <= max)
		return true;
	else
		return false;
}

/**
 * adc_to_temp - convert ADC code to temperature
 * @adc_val : ADC sensor reading
 *
 * Returns temperature in Degree Celsius
 */
static int adc_to_temp(uint16_t adc_val)
{
	int temp = 0;
	int i;

	if (!is_valid_temp_adc(adc_val)) {
		dev_warn(msic_dev, "Temperature out of Range: %u\n", adc_val);
		return -ERANGE;
	}
	/*BYD avoid adc overflow*/
	if (adc_val >= therm_curve_data[0][2]) 
		adc_val = therm_curve_data[0][2];
 
	for (i = 0; i < THERM_CURVE_MAX_SAMPLES; i++) {
		/* linear approximation for battery pack temperature */
		if (is_valid_temp_adc_range(adc_val, therm_curve_data[i][3],
					    therm_curve_data[i][2])) {

			temp = conv_adc_temp(adc_val, therm_curve_data[i][2],
					     therm_curve_data[i][2] -
					     therm_curve_data[i][3],
					     therm_curve_data[i][0] -
					     therm_curve_data[i][1]);

			temp += therm_curve_data[i][1];
			break;
		}
	}

	if (i >= THERM_CURVE_MAX_SAMPLES){
		dev_warn(msic_dev, "Invalid temp adc range\n");
		return -ERANGE;
	}

	return temp;
}

static inline bool is_ttl_valid(u64 ttl)
{
	return time_before64(get_jiffies_64(), ttl);
}

/**
 * mdf_multi_read_adc_regs - read multiple ADC sensors
 * @mbi :  msic battery info pointer
 * @sample_count: do sample serveral times and get the average value.
 * @...: sensor numbers
 *
 * Returns 0 if success
 */
static int mdf_multi_read_adc_regs(struct msic_power_module_info *mbi,
				   int sample_count, int channel_count, ...)
{
	va_list args;
	int ret = 0, i, sensor, tmp;
	int *adc_val;
	int temp_adc_val[MSIC_BATT_SENSORS];

	mutex_lock(&mbi->adc_val_lock);
	if (!is_ttl_valid(adc_ttl) || (sample_count > 1)) {
		ret =
		    intel_mid_gpadc_sample(mbi->adc_handle, sample_count,
					   &adc_sensor_vals[MSIC_ADC_VOL_IDX],
					   &adc_sensor_vals[MSIC_ADC_TEMP_IDX],
					   &adc_sensor_vals
						[MSIC_ADC_USB_VOL_IDX],
					   &adc_sensor_vals
						[MSIC_ADC_BATTID_IDX]);
		if (ret) {
			dev_err(&mbi->ipcdev->dev,
				"adc driver api returned error(%d)\n", ret);
			mutex_unlock(&mbi->adc_val_lock);
			goto adc_multi_exit;
		}

		adc_ttl = get_jiffies_64() + ADC_TIME_TO_LIVE;
	}
	memcpy(temp_adc_val, adc_sensor_vals, sizeof(temp_adc_val));
	mutex_unlock(&mbi->adc_val_lock);

	va_start(args, channel_count);
	for (i = 0; i < channel_count; i++) {
		/* get sensor number */
		sensor = va_arg(args, int);
		/* get value pointer */
		adc_val = va_arg(args, int *);
		if (adc_val == NULL) {
			ret = -EINVAL;
			goto adc_multi_exit;
		}
		*adc_val = temp_adc_val[sensor];
		switch (sensor) {
		case MSIC_ADC_VOL_IDX:
			tmp = MSIC_ADC_TO_VOL(*adc_val);
			break;
		case MSIC_ADC_TEMP_IDX:
			tmp = adc_to_temp(*adc_val);
			dev_info(&mbi->ipcdev->dev, "Temp adc value is %d, Temp value is: %d.", *adc_val, tmp);
			if(tmp == -ERANGE){
				ret = -ERANGE;
				goto adc_multi_exit;
			}
			break;
		case MSIC_ADC_USB_VOL_IDX:
			tmp = MSIC_ADC_TO_VBUS_VOL(*adc_val);
			break;
		case MSIC_ADC_BATTID_IDX:
			tmp = *adc_val;
			break;
		default:
			dev_err(&mbi->ipcdev->dev, "invalid sensor%d", sensor);
			return -EINVAL;
		}
		*adc_val = tmp;
	}
	va_end(args);

adc_multi_exit:
	return ret;
}

static int mdf_read_adc_regs(int sensor, int *sensor_val,
		struct msic_power_module_info *mbi)
{
	int ret;
	ret = mdf_multi_read_adc_regs(mbi, 1, 1, sensor, sensor_val);

	if (ret)
		dev_err(&mbi->ipcdev->dev, "%s:mdf_multi_read_adc_regs failed",
			__func__);
	return ret;
}

int intel_msic_get_battery_pack_temp(int *temp)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(!ipcdev)
		return -EINVAL;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(!mbi)
		return -EINVAL;

	/* check if msic charger is ready */
	if (!power_supply_get_by_name(CHARGER_PS_NAME))
		return -EAGAIN;

	if (!mbi->is_batt_valid)
		return -ENODEV;

	return mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, temp, mbi);
}
EXPORT_SYMBOL(intel_msic_get_battery_pack_temp);

/* Avoid MSIC-reads when debugging not enabled */
#if defined(DEBUG) || defined(CONFIG_DYNAMIC_DEBUG)
static void dump_registers(int dump_mask)
{
	int i, retval = 0;
	uint8_t reg_val;
	uint16_t chk_reg_addr;
	uint16_t reg_addr_boot[] = {MSIC_BATT_RESETIRQ1_ADDR,
		MSIC_BATT_RESETIRQ2_ADDR, MSIC_BATT_CHR_LOWBATTDET_ADDR,
		MSIC_BATT_CHR_SPCHARGER_ADDR, MSIC_BATT_CHR_CHRTTIME_ADDR,
		MSIC_BATT_CHR_CHRCTRL1_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR,
		MSIC_BATT_CHR_CHRSAFELMT_ADDR};
	char *reg_str_boot[] = {"rirq1", "rirq2", "lowdet",
				"spchr", "chrtime", "chrctrl1",
				"chrgwdt", "safelmt"};
	uint16_t reg_addr_evt[] = {MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_CHRCVOLTAGE_ADDR, MSIC_BATT_CHR_CHRCCURRENT_ADDR,
		MSIC_BATT_CHR_SPWRSRCINT_ADDR, MSIC_BATT_CHR_SPWRSRCINT1_ADDR,
		CHR_STATUS_FAULT_REG};
	char *reg_str_evt[] = {"chrctrl", "chrcv", "chrcc",
				"spwrsrcint", "sprwsrcint1", "chrflt"};


	if (dump_mask & MSIC_CHRG_REG_DUMP_BOOT) {
		for (i = 0; i < ARRAY_SIZE(reg_addr_boot); i++) {
			retval = intel_scu_ipc_ioread8(reg_addr_boot[i],
								&reg_val);
			if (retval) {
				chk_reg_addr = reg_addr_boot[i];
				goto ipcread_err;
			}
			dev_info(msic_dev, "%s val: %x\n", reg_str_boot[i],
								reg_val);
		}
	}
	if (dump_mask & MSIC_CHRG_REG_DUMP_EVENT) {
		for (i = 0; i < ARRAY_SIZE(reg_addr_evt); i++) {
			retval = intel_scu_ipc_ioread8(reg_addr_evt[i],
								&reg_val);
			if (retval) {
				chk_reg_addr = reg_addr_evt[i];
				goto ipcread_err;
			}
			dev_info(msic_dev, "%s val: %x\n", reg_str_evt[i],
								reg_val);
		}
	}

	return;

ipcread_err:
	handle_ipc_rw_status(retval, chk_reg_addr, MSIC_IPC_READ);
}
#else
static void dump_registers(int dump_mask)
{
}
#endif

static bool is_charger_fault(void)
{
	uint8_t fault_reg, chrctrl_reg, stat, spwrsrcint_reg;
	int chr_mode, i, retval = 0;
	int adc_temp, adc_usb_volt, batt_volt;
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	if(ipcdev == NULL)
		return 0;

	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	if(mbi == NULL)
		return 0;

	mutex_lock(&mbi->event_lock);
	chr_mode = mbi->charging_mode;
	mutex_unlock(&mbi->event_lock);
	
	/* Get low battery setting, for DEBUG */
	uint8_t lowbatt_seting;
	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_LOWBATTDET_ADDR,
					&lowbatt_seting);
	if (retval) {
		retval = handle_ipc_rw_status(retval,
						MSIC_BATT_CHR_LOWBATTDET_ADDR, MSIC_IPC_READ);
		if (retval)
			return false;
	}
	dev_info(msic_dev, "address = 0x%x, value = 0x%x\n", MSIC_BATT_CHR_LOWBATTDET_ADDR, lowbatt_seting);
						

	/* if charger is disconnected then report false */
	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR,
		&spwrsrcint_reg);
	if (retval) {
		retval = handle_ipc_rw_status(retval,
			MSIC_BATT_CHR_SPWRSRCINT_ADDR, MSIC_IPC_READ);
		if (retval)
			return false;
	}

	if (!(spwrsrcint_reg & MSIC_BATT_CHR_USBDET_MASK)) {
		/* This should be triggered irrespective of charging_mode,
		 * as a rare sequence of CONNECT, OTG_DISCONNECT events
		 * should be handled
		 */
		dev_err(msic_dev, "USBDET bit not set\n");
		/*
		 * USBDET bit is not set, this means
		 * OTG has missed unplug event and we
		 * should stop charge cycle.
		 */
		dev_warn(msic_dev, "sending disconnect event\n");
		schedule_delayed_work(&mbi->force_disconn_dwrk, 0);
		return false;
	}

	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_CHRCTRL_ADDR,
			&chrctrl_reg);
	if (retval) {
		retval = handle_ipc_rw_status(retval,
				MSIC_BATT_CHR_CHRCTRL_ADDR, MSIC_IPC_READ);
		if (retval)
			return false;
	}
	/* if charger is disabled report false */
	if (chrctrl_reg & CHRCNTL_CHRG_DISABLE)
		return false;

	/* due to MSIC HW bug, the fault register is not getting updated
	 * immediately after the charging is enabled. As a SW WA the
	 * driver will retry reading the fault registers for 3 times
	 * with delay of 1 mSec.
	 */
	for (i = 0; i < CHR_READ_RETRY_CNT; i++) {
		retval = intel_scu_ipc_ioread8(CHR_STATUS_FAULT_REG,
								&fault_reg);
		if (retval) {
			retval = handle_ipc_rw_status(retval,
					CHR_STATUS_FAULT_REG, MSIC_IPC_READ);
			if (retval)
				return retval;
		}

		stat = (fault_reg & CHR_STATUS_BIT_MASK) >> CHR_STATUS_BIT_POS;
		if (stat == CHR_STATUS_BIT_READY) {
			dev_info(msic_dev, "retry reading Fault Reg:0x%x\n",
								fault_reg);
			mdelay(30);
			continue;
		}
		break;
	}

	/* if charger is enabled and STAT(0:1) shows charging
	* progress or charging done then we report false
	*/
	if (stat == CHR_STATUS_BIT_PROGRESS ||
		stat == CHR_STATUS_BIT_CYCLE_DONE)
		return false;

fault_detected:
	dev_info(msic_dev, "Charger fault occured CHRGFLT=%d\n", fault_reg);
	retval = mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, &adc_temp, mbi);
	if (retval >= 0)
		dev_info(msic_dev, "[CHRG FLT] Temp:%d", adc_temp);

	retval = mdf_read_adc_regs(MSIC_ADC_USB_VOL_IDX, &adc_usb_volt, mbi);
	if (retval >= 0)
		dev_info(msic_dev, "[CHRG FLT] char_volt:%d", adc_usb_volt);

	batt_volt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (batt_volt > 0)
		dev_info(msic_dev, "[CHRG FLT] batt_volt:%d", batt_volt/1000);

	return true;
}

/**
 * msic_usb_get_property - usb power source get property
 * @psy: usb power supply context
 * @psp: usb power source property
 * @val: usb power source property value
 * Context: can sleep
 *
 * MSIC usb power source property needs to be provided to power_supply
 * subsystem for it to provide the information to users.
 */
static int msic_usb_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct msic_power_module_info *mbi =
	    container_of(psy, struct msic_power_module_info, usb);
	int retval = 0;
	int err_event;

	mutex_lock(&mbi->event_lock);
	err_event = mbi->msic_chr_err;
	mutex_unlock(&mbi->event_lock);

	if (system_state != SYSTEM_RUNNING) {
		if (!mutex_trylock(&mbi->usb_chrg_lock))
			return -EBUSY;
	} else
		mutex_lock(&mbi->usb_chrg_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = mbi->usb_chrg_props.charger_present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (mbi->batt_props.status != POWER_SUPPLY_STATUS_NOT_CHARGING
				|| err_event != MSIC_CHRG_ERROR_WEAKVIN)
			val->intval = mbi->usb_chrg_props.charger_present;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = mbi->usb_chrg_props.charger_health;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		retval = mdf_read_adc_regs(MSIC_ADC_USB_VOL_IDX,
				    &mbi->usb_chrg_props.vbus_vol, mbi);
		if (retval)
			break;
		val->intval = mbi->usb_chrg_props.vbus_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = mbi->usb_chrg_props.charger_model;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = mbi->usb_chrg_props.charger_vender;
		break;
	default:
		mutex_unlock(&mbi->usb_chrg_lock);
		return -EINVAL;
	}

	mutex_unlock(&mbi->usb_chrg_lock);
	return retval;
}

/**
 * msic_log_exception_event - log battery events
 * @event: msic event to be logged
 * Context: can sleep
 *
 * There are multiple battery and internal charger events
 * which may be of interest to users.
 * this battery function logs the different events onto the
 * kernel log messages.
 */
static void msic_log_exception_event(enum msic_event event)
{
	switch (event) {
	case MSIC_EVENT_BATTOCP_EXCPT:
		dev_warn(msic_dev,
			 "over battery charge current condition detected\n");
		break;
	case MSIC_EVENT_BATTOTP_EXCPT:
		dev_warn(msic_dev,
			 "battery out of acceptable temp range condition detected\n");
		break;
	case MSIC_EVENT_LOWBATT_EXCPT:
		dev_warn(msic_dev, "Low battery voltage condition detected\n");
		break;
	case MSIC_EVENT_BATTOVP_EXCPT:
		dev_warn(msic_dev, "battery over voltage condition detected\n");
		break;
	case MSIC_EVENT_CHROTP_EXCPT:
		dev_warn(msic_dev,
			 "charger high temperature condition detected\n");
		break;
	case MSIC_EVENT_USBOVP_EXCPT:
		dev_warn(msic_dev, "USB over voltage condition detected\n");
		break;
	case MSIC_EVENT_USB_VINREG_EXCPT:
		dev_warn(msic_dev, "USB Input voltage regulation "
			 "condition detected\n");
		break;
	case MSIC_EVENT_WEAKVIN_EXCPT:
		dev_warn(msic_dev, "USB Weak VBUS voltage "
			 "condition detected\n");
		break;
	case MSIC_EVENT_TIMEEXP_EXCPT:
		dev_warn(msic_dev, "Charger Total Time Expiration "
			 "condition detected\n");
		break;
	case MSIC_EVENT_WDTIMEEXP_EXCPT:
		dev_warn(msic_dev, "Watchdog Time Expiration "
			 "condition detected\n");
		break;
	default:
		dev_warn(msic_dev, "unknown error %u detected\n", event);
		break;
	}
}

/**
 * msic_handle_exception - handle any exception scenario
 * @mbi: device info structure to update the information
 * Context: can sleep
 *
 */

static void msic_handle_exception(struct msic_power_module_info *mbi,
				  uint8_t CHRINT_reg_value,
				  uint8_t CHRINT1_reg_value)
{
	enum msic_event exception;
	int temp, retval;
	int msic_vbatt, fg_vbatt, fg_curr;
	unsigned int health = POWER_SUPPLY_HEALTH_GOOD;

	/* Battery Events */
	if (CHRINT_reg_value & MSIC_BATT_CHR_BATTOCP_MASK) {
		exception = MSIC_EVENT_BATTOCP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT_reg_value & MSIC_BATT_CHR_BATTOTP_MASK) {
		retval = mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, &temp, mbi);
		if (retval) {
			dev_err(msic_dev, "%s(): Error in reading"
			       " temperature. Setting health as OVERHEAT\n",
			       __func__);
		}
		if (retval || (temp > batt_thrshlds->temp_high) ||
			(temp <= batt_thrshlds->temp_low))
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
		exception = MSIC_EVENT_BATTOTP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT_reg_value & MSIC_BATT_CHR_LOWBATT_MASK) {
		if (!mbi->usb_chrg_props.charger_present)
			health = POWER_SUPPLY_HEALTH_DEAD;
		exception = MSIC_EVENT_LOWBATT_EXCPT;

		/* log Vbatt voltage from msic */
		retval = mdf_read_adc_regs(MSIC_ADC_VOL_IDX, &msic_vbatt, mbi);
		if (retval < 0)
			dev_warn(msic_dev,
				"[Low Batt]msic vbatt read error:%d\n", retval);
		else
			dev_info(msic_dev,
				"[Low Batt] msic vbatt:%dmV\n", msic_vbatt);

		/* read ocv voltage from fuel gauge */
		fg_vbatt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_OCV);
		if (fg_vbatt < 0)
			dev_warn(msic_dev,
				"[Low Bat]Can't read voltage ocv from FG\n");
		else
			dev_info(msic_dev,
			"[Low Batt]fg vbatt ocv:%dmV\n", fg_vbatt/1000);

		/* read avg voltage from fuel gauge */
		fg_vbatt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_AVG);
		if (fg_vbatt < 0)
			dev_warn(msic_dev,
				"[Low Bat]Can't read voltage avg from FG\n");
		else
			dev_info(msic_dev,
			"[Low Batt]fg vbatt avg:%dmV\n", fg_vbatt/1000);

		/* read inst voltage from fuel gauge */
		fg_vbatt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
		if (fg_vbatt < 0)
			dev_warn(msic_dev,
				"[Low Bat]Can't read voltage now from FG\n");
		else
			dev_info(msic_dev,
			"[Low Batt]fg vbatt now:%dmV\n", fg_vbatt/1000);

		/* read current from fuel gauge */
		fg_curr = fg_chip_get_property(POWER_SUPPLY_PROP_CURRENT_NOW);
		if (fg_curr == -ENODEV || fg_curr == -EINVAL)
			dev_warn(msic_dev, "Can't read current from FG\n");
		else
			dev_info(msic_dev,
				"[Low Batt] fg current:%dmA\n", fg_curr/1000);

		msic_log_exception_event(exception);
	}
	if (CHRINT_reg_value & MSIC_BATT_CHR_TIMEEXP_MASK) {
		exception = MSIC_EVENT_TIMEEXP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT_reg_value & MSIC_BATT_CHR_WDTIMEEXP_MASK) {
		exception = MSIC_EVENT_WDTIMEEXP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT1_reg_value & MSIC_BATT_CHR_BATTOVP_MASK) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		exception = MSIC_EVENT_BATTOVP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (health != POWER_SUPPLY_HEALTH_GOOD) {
		mutex_lock(&mbi->batt_lock);
		mbi->batt_props.health = health;
		mutex_unlock(&mbi->batt_lock);
		health = POWER_SUPPLY_HEALTH_GOOD;
	}

	/* Charger Events */
	if (CHRINT1_reg_value & MSIC_BATT_CHR_CHROTP_MASK) {
		exception = MSIC_EVENT_CHROTP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT1_reg_value & MSIC_BATT_CHR_USBOVP_MASK) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		exception = MSIC_EVENT_USBOVP_EXCPT;
		msic_log_exception_event(exception);
	}
	if (CHRINT1_reg_value & MSIC_BATT_CHR_WKVINDET_MASK) {
		health = POWER_SUPPLY_HEALTH_DEAD;
		exception = MSIC_EVENT_WEAKVIN_EXCPT;
		msic_log_exception_event(exception);
	}

	if (health != POWER_SUPPLY_HEALTH_GOOD) {
		mutex_lock(&mbi->usb_chrg_lock);
		mbi->usb_chrg_props.charger_health = health;
		mutex_unlock(&mbi->usb_chrg_lock);
		health = POWER_SUPPLY_HEALTH_GOOD;
	}

	mutex_lock(&mbi->event_lock);
	if (CHRINT1_reg_value & MSIC_BATT_CHR_VINREGMINT_MASK) {
		/* change input current limit to 500mA
		 * to recover from VINREG condition
		 */
		mbi->in_cur_lmt = CHRCNTL_VINLMT_500;
		mbi->refresh_charger = 1;
		exception = MSIC_EVENT_USB_VINREG_EXCPT;
		msic_log_exception_event(exception);
	}
	mutex_unlock(&mbi->event_lock);
}

/**
 *	msic_chr_write_multi	-	multi-write helper
 *	@mbi: msic power module
 *	@address: addresses of IPC writes.
		  should only be given in pairs, WDTWRITE address followed
		  by other charger register address
 *	@data: data for IPC writes
 *	@n: size of write table
 *
 *	Write a series of values to the SCU while respecting the ipc_rw_lock
 *	across the entire sequence. Handle any error reporting and pass back
 *	error codes on failure
 */
static int msic_chr_write_multi(struct msic_power_module_info *mbi,
			    const u16 *address, const u8 *data, int n)
{
	int retval = 0, i;
	int rep_count = 0;
	u8 read_data;

	/* All writes to charger-registers should accompany WDTWRITE address */
	if (n%2) {
		dev_warn(msic_dev, "Invalid no of register-arguments to %s\n",
				__func__);

		return -EINVAL;
	}

	mutex_lock(&mbi->ipc_rw_lock);
	/* Two writes are issued at once, first one to unlock WDTWRITE
	   and second one with given charger-register value */
	for (i = 0; i < n; i += 2) {
		/* Once written, the register is read back to check,
		   and re-written if required. This is repeated 3 times */
		for (rep_count = 0; rep_count < CHR_WRITE_RETRY_CNT;
				rep_count++) {
			retval = intel_scu_ipc_writev(address+i, data+i, 2);
			if (retval) {
				handle_ipc_rw_status(retval,
						*(address+i+1),
						MSIC_IPC_WRITE);
				break;
			} else {
				retval = intel_scu_ipc_ioread8(*(address+i+1),
						&read_data);
				if (retval) {
					handle_ipc_rw_status(retval,
							*(address+i+1),
							MSIC_IPC_READ);
					break;
				}

 dev_info(msic_dev, "address = 0x%x, read_data = 0x%x, *(data+i+1) = 0x%x\n", *(address+i+1), read_data, *(data+i+1));
				if (read_data == *(data+i+1))
					break;
				else {
					dev_warn(msic_dev, "MSIC-Register RW "
							"mismatch on try %d\n",
							rep_count+1);
					if (rep_count == CHR_WRITE_RETRY_CNT-1)
						dev_err(msic_dev, "Error in MSIC-Register RW\n");
				}
			}
		}
	}
	mutex_unlock(&mbi->ipc_rw_lock);

	return retval;
}

/**
 *	ipc_read_modify_chr_param_reg - read and modify charger registers
 *	@mbi: msic power module
 *	@address:  charger register address
 *	@data: value to be set/reset
 *	@n: set or reset
 *
 */
static int ipc_read_modify_chr_param_reg(struct msic_power_module_info *mbi,
					 uint16_t addr, uint8_t val, int set)
{
	int ret = 0;
	static u16 address[2] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, 0
	};
	static u8 data[2] = {
		WDTWRITE_UNLOCK_VALUE, 0
	};

	address[1] = addr;

	/* Unlock Charge parameter registers before reading */
	ret = intel_scu_ipc_iowrite8(address[0], data[0]);
	if (ret) {
		ret = handle_ipc_rw_status(ret,
				address[0], MSIC_IPC_WRITE);
		if (ret)
			return ret;
	}

	ret = intel_scu_ipc_ioread8(address[1], &data[1]);
	if (ret) {
		ret = handle_ipc_rw_status(ret, address[1], MSIC_IPC_READ);
		if (ret)
			return ret;
	}

	if (set)
		data[1] |= val;
	else
		data[1] &= (~val);

	return msic_chr_write_multi(mbi, address, data, 2);
}

/**
 *	ipc_read_modify__reg - read and modify MSIC registers
 *	@mbi: msic power module
 *	@address:  charger register address
 *	@data: value to be set/reset
 *	@n: set or reset
 *
 */
static int ipc_read_modify_reg(struct msic_power_module_info *mbi,
			       uint16_t addr, uint8_t val, int set)
{
	int ret;
	u8 data;

	ret = intel_scu_ipc_ioread8(addr, &data);
	if (ret) {
		ret = handle_ipc_rw_status(ret, addr, MSIC_IPC_READ);
		if (ret)
			return ret;
	}

	if (set)
		data |= val;
	else
		data &= (~val);

	ret = intel_scu_ipc_iowrite8(addr, data);
	if (ret)
		ret = handle_ipc_rw_status(ret, addr, MSIC_IPC_WRITE);

	return ret;
}

/**
 * update_usb_ps_info - update usb power supply parameters
 * @mbi: msic power module structure
 * @cap: charger capability structure
 * @event: USB OTG events
 * Context: can sleep
 *
 * Updates the USB power supply parameters based on otg event.
 */
static void update_usb_ps_info(struct msic_power_module_info *mbi,
				struct otg_bc_cap *cap, int event)
{
	mutex_lock(&mbi->usb_chrg_lock);
	switch (event) {
	case USBCHRG_EVENT_DISCONN:
		mbi->usb.type = POWER_SUPPLY_TYPE_USB;
	case USBCHRG_EVENT_SUSPEND:
		dev_dbg(msic_dev, "Charger Disconnected or Suspended\n");
		mbi->usb_chrg_props.charger_health =
					POWER_SUPPLY_HEALTH_UNKNOWN;
		memcpy(mbi->usb_chrg_props.charger_model, "Unknown",
							sizeof("Unknown"));
		memcpy(mbi->usb_chrg_props.charger_vender, "Unknown",
							sizeof("Unknown"));
		if (event == USBCHRG_EVENT_SUSPEND)
			mbi->usb_chrg_props.charger_present =
						MSIC_USB_CHARGER_PRESENT;
		else
			mbi->usb_chrg_props.charger_present =
						MSIC_USB_CHARGER_NOT_PRESENT;
		break;
	case USBCHRG_EVENT_CONNECT:
	case USBCHRG_EVENT_UPDATE:
	case USBCHRG_EVENT_RESUME:
		dev_info(msic_dev, "Charger Connected or Updated\n");
		if (cap->chrg_type == CHRG_CDP) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB_CDP;
			dev_info(msic_dev, "Charger type: CDP, "
					"current-val: %d", cap->mA);
		} else if (cap->chrg_type == CHRG_DCP) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
			dev_info(msic_dev, "Charger type: DCP, "
					"current-val: %d", cap->mA);
		} else if (cap->chrg_type == CHRG_SE1) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
			dev_info(msic_dev, "Charger type: SE1, "
					"current-val: %d", cap->mA);
		} else if (cap->chrg_type == CHRG_ACA) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB_ACA;
			dev_info(msic_dev, "Charger type: ACA, "
					"current-val: %d", cap->mA);
		} else if (cap->chrg_type == CHRG_SDP) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB;
			dev_info(msic_dev, "Charger type: SDP, "
					"current negotiated: %d", cap->mA);
		} else if (cap->chrg_type == CHRG_SDP_INVAL) {
			mbi->usb.type = POWER_SUPPLY_TYPE_USB;
			dev_info(msic_dev, "Charger type: SDP_INVAL, "\
					"current negotiated: %d", cap->mA);
		} else {
			/* CHRG_UNKNOWN */
			dev_warn(msic_dev, "Charger type:%d unknown\n",
					cap->chrg_type);
			mbi->usb.type = POWER_SUPPLY_TYPE_USB;
			goto update_usb_ps_exit;
		}
		mbi->usb_chrg_props.charger_present = MSIC_USB_CHARGER_PRESENT;
		mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_GOOD;
		memcpy(mbi->usb_chrg_props.charger_model, "msic",
							sizeof("msic"));
		memcpy(mbi->usb_chrg_props.charger_vender, "Intel",
							sizeof("Intel"));
		break;
	default:
		dev_warn(msic_dev, "Invalid OTG event\n");
	}

update_usb_ps_exit:
	mutex_unlock(&mbi->usb_chrg_lock);

	power_supply_changed(&mbi->usb);
}

static int msic_batt_stop_charging(struct msic_power_module_info *mbi)
{
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR,
		MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR,
		MSIC_BATT_CHR_CHRSTWDT_ADDR,
	};
	static const u8 data[] = {
		WDTWRITE_UNLOCK_VALUE,	/* Unlock chrg params */
		/*Disable Charging,Enable Low Power Mode */
		CHRCNTL_CHRG_DISABLE | CHRCNTL_CHRG_LOW_PWR_ENBL,
		WDTWRITE_UNLOCK_VALUE,	/* Unlock chrg params */
		CHR_WDT_DISABLE,	/*  Disable WDT Timer */
	};

	/*
	 * Charger connect handler delayed work also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied by
	 * msic_chr_write_multi
	 */
	dev_info(msic_dev, "%s:Charger Disabled\n", __func__);
	return msic_chr_write_multi(mbi, address, data, 4);
}

/**
 * msic_batt_do_charging - set battery charger
 * @mbi: device info structure
 * @chrg: charge mode to set battery charger in
 * Context: can sleep
 *
 * MsIC battery charger needs to be enabled based on the charger
 * capabilities connected to the platform.
 */
static int msic_batt_do_charging(struct msic_power_module_info *mbi,
				 struct charge_params *params,
				 int is_maint_mode)
{
	int retval;
	u8 val;
	static u8 data[] = {
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_SET_60SEC,
		WDTWRITE_UNLOCK_VALUE, 0
	};
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCCURRENT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCVOLTAGE_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_SPCHARGER_ADDR
	};

	data[1] = params->ccur;
	data[3] = params->cvol;	/* charge voltage 4.14V */
	data[5] = params->vinilmt;
	data[9] = params->weakvin;

	/*
	 * Charger disconnect handler also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied
	 */
	retval = msic_chr_write_multi(mbi, address, data, 10);
	if (retval < 0) {
		dev_warn(msic_dev, "ipc multi write failed:%s\n", __func__);
		return retval;
	}

	/* prevent system from entering s3 while charger is connected */
	if (!wake_lock_active(&mbi->wakelock))
		wake_lock(&mbi->wakelock);

	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_CHRCTRL_ADDR, &val);
	if (retval) {
		handle_ipc_rw_status(retval, MSIC_BATT_CHR_CHRCTRL_ADDR,
								MSIC_IPC_READ);
		return retval;
	}

	/* Check if charging enabled or not */
	if (val & CHRCNTL_CHRG_DISABLE) {
		dev_warn(msic_dev, "Charging not Enabled by MSIC!!!\n");
		return -EIO;
	}

	return 0;
}

static void reset_wdt_timer(struct msic_power_module_info *mbi)
{
	static const u16 address[2] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR
	};
	static const u8 data[2] = {
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_SET_60SEC
	};

	/*
	 * Charger disconnect handler also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied
	 */
	msic_chr_write_multi(mbi, address, data, 2);
}

/**
 * check_charge_full -  check battery is full or not
 * @mbi: device info structure
 * @vref: battery voltage
 * @temp_idx : temperature range index
 * Context: can sleep
 *
 * Return true if full
 *
 */
static int check_charge_full(struct msic_power_module_info *mbi,
			     int vref, int temp_idx)
{
	static int volt_prev;
	int is_full = false;
	int volt_now;
	int cur_avg;

	/* Read voltage and current from FG driver */
	volt_now = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_OCV);
	if (volt_now == -ENODEV || volt_now == -EINVAL) {
		dev_warn(msic_dev, "%s, Can't read voltage from FG\n", __func__);
		return false;
	}
	/* convert to milli volts */
	volt_now /= 1000;

	/* Using Current-avg instead of Current-now to take care of
	 * instantaneous spike or dip */
	cur_avg = fg_chip_get_property(POWER_SUPPLY_PROP_CURRENT_AVG);
	if (cur_avg == -ENODEV || cur_avg == -EINVAL) {
		dev_warn(msic_dev, "Can't read current-avg from FG\n");
		return false;
	}
	/* convert to milli amps */
	cur_avg /= 1000;
	dev_info(msic_dev, "\n%s() volt_prev=%d volt_now=%d\n",__func__, volt_prev, volt_now);
	dev_info(msic_dev, "\n%s() cur_avg=%d\n",__func__, cur_avg);
	/*
	 * charge full is detected (1)when Vbatt > Vfull and could
	 * be discharging, this can happen  when temperature zone
	 * transition happens. (2) when Vbatt is close to Vfull and
	 * the charge current is with in the termination range.
	 */
	if ((volt_now > vref) && (volt_prev > vref) &&
			(cur_avg <= mbi->term_curr)) {
		is_full = true;
	} else if ((volt_now > (vref - VBATT_FULL_DET_MARGIN)) &&
		(volt_prev > (vref - VBATT_FULL_DET_MARGIN))) {
		if (cur_avg >= FULL_CURRENT_AVG_LOW  &&
				cur_avg <= mbi->term_curr)
			is_full = true;
		else
			is_full = false;
	} else {
		is_full = false;
	}

/*	
	if (is_full)
		if (fg_chip_get_property(POWER_SUPPLY_PROP_CAPACITY) == 100)
			is_full = true;
		else
			is_full = false;
*/
	if (is_full) {
		dev_info(msic_dev, "Charge full detected\n");
		dev_info(msic_dev, "volt_now:%d, volt_prev:%d, "
				"volt_ref:%d, cur_avg:%d\n",
				volt_now, volt_prev, vref, cur_avg);
		/* Disable Charging */
		msic_batt_stop_charging(mbi);
	}

	volt_prev = volt_now;

	return is_full;
}
static void get_batt_temp_thresholds(short int *temp_high, short int *temp_low)
{
	int i, max_range;
	*temp_high = *temp_low = 0;

	if (sfi_table->temp_mon_ranges < SFI_TEMP_NR_RNG)
		max_range = sfi_table->temp_mon_ranges;
	else
		max_range = SFI_TEMP_NR_RNG;

	for (i = 0; i < max_range; i++) {
		if (*temp_high < sfi_table->temp_mon_range[i].temp_up_lim)
			*temp_high = sfi_table->temp_mon_range[i].temp_up_lim;
	}

	for (i = 0; i < max_range; i++) {
		if (*temp_low > sfi_table->temp_mon_range[i].temp_low_lim)
			*temp_low = sfi_table->temp_mon_range[i].temp_low_lim;
	}
}

#ifdef DEBUG_SHOW_SFI_TEMP_INFO
static void sfi_table_dump(void)
{
	int i;
	struct temp_mon_table tmt;

	dev_info(msic_dev, "SFI battery id %s\n", sfi_table->batt_id);
	dev_info(msic_dev, "SFI voltage max %d\n", sfi_table->voltage_max);
	dev_info(msic_dev, "SFI capacity %d\n", sfi_table->capacity);
	dev_info(msic_dev, "SFI battery type %d\n", sfi_table->battery_type);
	dev_info(msic_dev, "SFI temp_mon_ranges %d\n", sfi_table->temp_mon_ranges);

	for(i=0; i<sfi_table->temp_mon_ranges; i++)
	{
		tmt = sfi_table->temp_mon_range[i];
		dev_info(msic_dev, "SFI temp_up_lim %d\n", tmt.temp_up_lim);
		dev_info(msic_dev, "SFI temp_low_lim %d\n", tmt.temp_low_lim);
		dev_info(msic_dev, "SFI rbatt %d\n", tmt.rbatt);
		dev_info(msic_dev, "SFI full_chrg_vol %d\n", tmt.full_chrg_vol);
		dev_info(msic_dev, "SFI full_chrg_cur %d\n", tmt.full_chrg_cur);
		dev_info(msic_dev, "SFI maint_chrg_vol_ll %d\n", tmt.maint_chrg_vol_ll);
		dev_info(msic_dev, "SFI maint_chrg_vol_ul %d\n", tmt.maint_chrg_vol_ul);
		dev_info(msic_dev, "SFI maint_chrg_cur %d\n", tmt.maint_chrg_cur);
	}
}
#else
#define sfi_table_dump() 
#endif

/**
* sfi_temp_range_lookup - lookup SFI table to find the temperature range index
* @adc_temp : temperature in Degree Celcius
*
* Returns temperature range index
*/
static unsigned int sfi_temp_range_lookup(int adc_temp)
{
	int i;
	int max_range;

	if (sfi_table->temp_mon_ranges < SFI_TEMP_NR_RNG)
		max_range = sfi_table->temp_mon_ranges;
	else
		max_range = SFI_TEMP_NR_RNG;

	for (i = 0; i < max_range; i++) {
		if (adc_temp <= sfi_table->temp_mon_range[i].temp_up_lim &&
		    adc_temp > sfi_table->temp_mon_range[i].temp_low_lim) {
			dev_dbg(msic_dev, "Temp Range %d\n", i);
			break;
		}
	}

	return i;
}

static bool weak_vbus_detect(int vbus_volt)
{
	static int vbus_volt_prev = -1;
	bool flag = false;

	/*
	 * set weak vin condition only if the
	 * vbus voltage goes below the WEAKVIN
	 * threshold twice in a row.
	 */
	if ((vbus_volt_prev > 0) &&
		(vbus_volt_prev < WEAKVIN_VOLTAGE_LEVEL) &&
		(vbus_volt < WEAKVIN_VOLTAGE_LEVEL)) {
		dev_warn(msic_dev,
			"[weak vbus detected]vbus_volt:%d vbus_volt_prev:%d\n",
						vbus_volt, vbus_volt_prev);
		flag = true;
	}

	vbus_volt_prev = vbus_volt;
	return flag;
}

/**
* msic_batt_temp_charging - manages the charging based on temperature
* @charge_param: charging parameter
* @sfi_table: SFI table structure
*
* To manage the charging based on the
* temperature of the battery
*/
static void msic_batt_temp_charging(struct work_struct *work)
{
	int ret, i, is_maint_chrg = false, is_lowchrg_enbl, is_chrg_flt;
	static int iprev = -1, is_chrg_enbl;
	short int cv = 0, cc = 0, vinlimit = 0, cvref;
	int adc_temp, adc_vol;
	int vbus_voltage;
	struct charge_params charge_param;
	struct msic_power_module_info *mbi =
	    container_of(work, struct msic_power_module_info,
			 connect_handler.work);
	struct temp_mon_table *temp_mon = NULL;
	bool is_weakvin;

	memset(&charge_param, 0x0, sizeof(struct charge_params));
	charge_param.vinilmt = mbi->ch_params.vinilmt;
	charge_param.chrg_type = mbi->ch_params.chrg_type;

	mutex_lock(&mbi->event_lock);
	if (mbi->refresh_charger) {
		/*
		 * If the charger type is unknown or None
		 * better start the charging again and compute
		 * the properties again.
		 */
		mbi->refresh_charger = 0;
		iprev = -1;
		is_chrg_enbl = false;
	}
	mutex_unlock(&mbi->event_lock);

	if (mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, &adc_temp, mbi)) {
		dev_err(msic_dev, "Error in reading temperature\n");
		goto lbl_sched_work;
	}

	/* find the temperature range */
	i = sfi_temp_range_lookup(adc_temp);

	/* get charger status */
	is_chrg_flt = is_charger_fault();

	if (mdf_read_adc_regs(MSIC_ADC_USB_VOL_IDX, &vbus_voltage, mbi)) {
		dev_warn(msic_dev, "Error in reading charger"
					" voltage:%s\n", __func__);
		goto lbl_sched_work;
	}
	/* determine weak vbus condition */
	is_weakvin = weak_vbus_detect(vbus_voltage);

	/* change to fix buffer overflow issue */
	if (i >= ((sfi_table->temp_mon_ranges < SFI_TEMP_NR_RNG) ?
			sfi_table->temp_mon_ranges : SFI_TEMP_NR_RNG) ||
						is_chrg_flt || is_weakvin) {

		if ((adc_temp > batt_thrshlds->temp_high) ||
			(adc_temp <= batt_thrshlds->temp_low)) {
			dev_warn(msic_dev,
					"TEMP RANGE DOES NOT EXIST FOR %d\n",
						adc_temp);
			mutex_lock(&mbi->batt_lock);
			mbi->batt_props.health = POWER_SUPPLY_HEALTH_OVERHEAT;
			mutex_unlock(&mbi->batt_lock);

			mutex_lock(&mbi->event_lock);
			mbi->msic_chr_err = MSIC_CHRG_ERROR_OVERHEAT;
			mutex_unlock(&mbi->event_lock);
		}
		/* set battery charging status */
		if (mbi->batt_props.status !=
				POWER_SUPPLY_STATUS_NOT_CHARGING) {
			mutex_lock(&mbi->batt_lock);
			mbi->batt_props.status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
			mutex_unlock(&mbi->batt_lock);
		}

		if (is_weakvin) {
			dev_warn(msic_dev, "vbus_volt:%d less than"
					"WEAKVIN threshold", vbus_voltage);
			mutex_lock(&mbi->usb_chrg_lock);
				mbi->usb_chrg_props.charger_health =
						POWER_SUPPLY_HEALTH_DEAD;
			mutex_unlock(&mbi->usb_chrg_lock);

			mutex_lock(&mbi->event_lock);
			mbi->msic_chr_err = MSIC_CHRG_ERROR_WEAKVIN;
			mutex_unlock(&mbi->event_lock);
		}

		iprev = -1;
		dump_registers(MSIC_CHRG_REG_DUMP_EVENT);
		/*
		 * If we are in middle of charge cycle is safer to Reset WDT
		 * Timer Register.Because battery temperature and charge
		 * status register are not related.
		 */
		reset_wdt_timer(mbi);
		power_supply_changed(&mbi->usb);
		dev_dbg(msic_dev, "Charger Watchdog timer reset for 60sec\n");
		goto lbl_sched_work;
	}

	/* Set charger parameters */
	cv = sfi_table->temp_mon_range[i].full_chrg_vol;
	cc = sfi_table->temp_mon_range[i].full_chrg_cur;
	cvref = cv;
	dev_info(msic_dev, "cc:%d  cv:%d\n", cc, cv);

	mutex_lock(&mbi->event_lock);
	/*
	 * Check on user setting for charge current.
	 * MFLD doesn't support VSYS alone and at the same
	 * time 100mA charging doesn't really charge the battery.
	 * So for USER_SET_CHRG_LMT1, USER_SET_CHRG_LMT2 and
	 * USER_SET_CHRG_LMT3,we limit the charging to 500mA.
	 * For MFLD,USER_SET_CHRG_LMT4 and USER_SET_CHRG_LMT3 are
	 * almost same(1A). It does not make any sense charging at
	 * full rate in warning state so limiting the current to 500mA
	 * even in USER_SET_CHRG_LMT3.
	 */
	if ((mbi->usr_chrg_enbl == USER_SET_CHRG_LMT1) ||
		(mbi->usr_chrg_enbl == USER_SET_CHRG_LMT2) ||
		(mbi->usr_chrg_enbl == USER_SET_CHRG_LMT3)) {
		vinlimit = CHRCNTL_VINLMT_500;	/* VINILMT set to 500mA */
	} else {
		/* D7,D6 bits of CHRCNTL will set the VINILMT */
		if (charge_param.vinilmt > 950)
			vinlimit = CHRCNTL_VINLMT_NOLMT;
		else if (charge_param.vinilmt > 500)
			vinlimit = CHRCNTL_VINLMT_950;
		else if (charge_param.vinilmt > 100)
			vinlimit = CHRCNTL_VINLMT_500;
		else
			vinlimit = CHRCNTL_VINLMT_100;
	}

	/* input current limit can be changed
	 * due to VINREG or weakVIN conditions
	 */
	if (mbi->in_cur_lmt < vinlimit)
		vinlimit = mbi->in_cur_lmt;
	/*
	 * Check for Charge full condition and set the battery
	 * properties accordingly. Also check for charging mode
	 * whether it is normal or maintenance mode.
	 */
	if (mbi->charging_mode == BATT_CHARGING_MODE_MAINTENANCE) {
		cvref = sfi_table->temp_mon_range[i].maint_chrg_vol_ul;
		is_maint_chrg = true;
	}
	mutex_unlock(&mbi->event_lock);

	/* Check full detection only if we are charging */
	if (is_chrg_enbl)
		ret = check_charge_full(mbi, cvref, i);
	else
		ret = is_chrg_enbl;

	if (ret) {
		is_chrg_enbl = false;
		if (!is_maint_chrg) {
			dev_dbg(msic_dev, "Going to Maintenance CHRG Mode\n");

			mutex_lock(&mbi->event_lock);
			mbi->charging_mode = BATT_CHARGING_MODE_MAINTENANCE;
			mutex_unlock(&mbi->event_lock);

			mutex_lock(&mbi->batt_lock);
			mbi->batt_props.status = POWER_SUPPLY_STATUS_FULL;
			mutex_unlock(&mbi->batt_lock);

			is_maint_chrg = true;
			power_supply_changed(&mbi->usb);
		}
	}else {
		if(fg_chip_get_property(POWER_SUPPLY_PROP_CAPACITY)==100){
			if(!is_maint_chrg){
				dev_dbg(msic_dev, "Going to Maintenance CHRG Mode\n");

				mutex_lock(&mbi->event_lock);
				mbi->charging_mode = BATT_CHARGING_MODE_MAINTENANCE;
				mutex_unlock(&mbi->event_lock);

				mutex_lock(&mbi->batt_lock);
				mbi->batt_props.status = POWER_SUPPLY_STATUS_FULL;
				mutex_unlock(&mbi->batt_lock);

				is_maint_chrg = true;
				power_supply_changed(&mbi->usb);
			}
		}else{
			if(is_maint_chrg){
				dev_dbg(msic_dev, "Going to CHRG Mode\n");
			
				mutex_lock(&mbi->event_lock);
				mbi->charging_mode = BATT_CHARGING_MODE_NORMAL;
				mutex_unlock(&mbi->event_lock);
			
				mutex_lock(&mbi->batt_lock);
				mbi->batt_props.status = POWER_SUPPLY_STATUS_CHARGING;
				mutex_unlock(&mbi->batt_lock);
			
				is_maint_chrg = false;
				power_supply_changed(&mbi->usb);
			}
		}
	}

	/*
	 * If we are in same Temperature range check for the
	 * maintenance charging mode and enable the charging depending
	 * on the voltage.If Temperature range is changed then anyways
	 * we need to set charging parameters and enable charging.
	 */
	if (i == iprev) {
		if (!is_maint_chrg) {
			/*Reset WDT Timer reset for 60 sec */
			reset_wdt_timer(mbi);
			dev_dbg(msic_dev, "Charger Watchdog timer reset"
					" for 60sec\n");
			goto lbl_sched_work;
		}

		/*
		 * Check if the voltage falls below lower threshold
		 * if we are in maintenance mode charging.
		 */
		temp_mon = &sfi_table->temp_mon_range[i];
		/* Read battery Voltage */
		adc_vol = fg_chip_get_property(
				POWER_SUPPLY_PROP_VOLTAGE_OCV);
		if (adc_vol == -ENODEV || adc_vol == -EINVAL) {
			dev_warn(msic_dev, "%s, Can't read voltage from FG\n", __func__);
			goto lbl_sched_work;
		}
		/* convert to milli volts */
		adc_vol /= 1000;
		/*
		 * Switch to normal mode charging, if the voltage drops
		 * much below the lower threshold range.
		 */
		if (is_chrg_enbl && adc_vol < (temp_mon->maint_chrg_vol_ll -
				MODE_SWITCH_VOLT_OFF)) {
			dev_info(msic_dev, "Drop in voltage"
					"switch to normal mode"
					"charging vocv_vol:%dmv\n",
					adc_vol);
			is_maint_chrg = false;
			mutex_lock(&mbi->event_lock);
			mbi->charging_mode = BATT_CHARGING_MODE_NORMAL;
			mutex_unlock(&mbi->event_lock);
			cv = temp_mon->full_chrg_vol;
		} else if (!is_chrg_enbl && (adc_vol <=
					temp_mon->maint_chrg_vol_ll)) {
			dev_info(msic_dev, "restart maint charging"
					"vocv_vol:%dmv\n",
					adc_vol);
			cv = temp_mon->maint_chrg_vol_ul;
		} else {
			dev_dbg(msic_dev, "vbat is more than ll\n");
			reset_wdt_timer(mbi);

			goto lbl_sched_work;
		}
	} else {
		temp_mon = &sfi_table->temp_mon_range[i];
		dev_info(msic_dev, "Current Temp zone is %d, "
				"it's parameters are:\n", i);
		dev_info(msic_dev, "full_volt:%d, full_cur:%d\n",
				temp_mon->full_chrg_vol,
				temp_mon->full_chrg_cur);
		dev_info(msic_dev, "maint_vol_ll:%d, maint_vol_ul:%d, "
				"maint_cur:%d\n", temp_mon->maint_chrg_vol_ll,
				temp_mon->maint_chrg_vol_ul,
				temp_mon->maint_chrg_cur);
	}

	iprev = i;
	mbi->ch_params.cvol = cv;
	charge_param.cvol = CONV_VOL_DEC_MSICREG(cv);
	/* CHRCC_MIN_CURRENT is th lowet value */
	cc = cc - CHRCC_MIN_CURRENT;
	/*
	 * If charge current parameter is less than 550mA we should
	 * enable LOW CHARGE mode which will limit the charge current to 325mA.
	 */
	if (cc <= 0) {
		dev_dbg(msic_dev, "LOW CHRG mode enabled\n");
		cc = 0;
		is_lowchrg_enbl = BIT_SET;
	} else {
		dev_dbg(msic_dev, "LOW CHRG mode NOT enabled\n");
		cc = cc / 100;
		is_lowchrg_enbl = BIT_RESET;
	}
	cc = cc << 3;

	charge_param.ccur = cc;
	charge_param.vinilmt = vinlimit;

	dev_info(msic_dev, "params  vol: %x  cur:%x vinilmt:%x\n",
		charge_param.cvol, charge_param.ccur, charge_param.vinilmt);

	if (cv > WEAKVIN_VOLTAGE_LEVEL)
		charge_param.weakvin = CHR_SPCHRGER_WEAKVIN_LVL1;
	else
		charge_param.weakvin = CHR_SPCHRGER_WEAKVIN_LVL2;

	if (is_lowchrg_enbl)
		charge_param.weakvin |= CHR_SPCHRGER_LOWCHR_ENABLE;

	/* enable charging here */
	dev_info(msic_dev, "Enable Charging\n");
	ret = msic_batt_do_charging(mbi, &charge_param, is_maint_chrg);
	/* update battery status */
	mutex_lock(&mbi->batt_lock);
	if (ret < 0 && !is_chrg_enbl) {
		dev_warn(msic_dev, "msic_batt_do_charging failed\n");
		mbi->batt_props.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		if (is_maint_chrg) {
			if (mbi->batt_props.status ==
					POWER_SUPPLY_STATUS_NOT_CHARGING) {
				mbi->charging_mode = BATT_CHARGING_MODE_NORMAL;
				is_maint_chrg = false;
				mbi->batt_props.status =
					 POWER_SUPPLY_STATUS_CHARGING;
			} else {
				mbi->batt_props.status =
					POWER_SUPPLY_STATUS_FULL;
			}
		} else {
			mbi->batt_props.status = POWER_SUPPLY_STATUS_CHARGING;
		}
		is_chrg_enbl = true;
	}
	mutex_unlock(&mbi->batt_lock);

	dump_registers(MSIC_CHRG_REG_DUMP_EVENT);
	power_supply_changed(&mbi->usb);

lbl_sched_work:
	/* Schedule the work after 30 Seconds */
	schedule_delayed_work(&mbi->connect_handler, TEMP_CHARGE_DELAY_JIFFIES);
}
static void update_charger_health(struct msic_power_module_info *mbi)
{
	int vbus_volt;
	unsigned char dummy_val;

	/* We don't get an interrupt once charger returns from
	  error state. So check current status by reading voltage
	  and report health as good if recovered from error state */


	/* Read charger data from ADC channels */
	if (mdf_read_adc_regs(MSIC_ADC_USB_VOL_IDX, &vbus_volt, mbi)) {
		dev_warn(msic_dev, "Error in reading charger"
				" voltage:%s\n", __func__);
		return;
	}
	dev_info(msic_dev, "vbus_volt:%d\n", vbus_volt);

	/* Compute Charger health */
	mutex_lock(&mbi->usb_chrg_lock);
	/* check system recovered from overvoltage and dead conditions */
	if ((mbi->usb_chrg_props.charger_health ==
			POWER_SUPPLY_HEALTH_OVERVOLTAGE ||
		mbi->usb_chrg_props.charger_health
			== POWER_SUPPLY_HEALTH_DEAD) &&
	    vbus_volt >= MSIC_VBUS_LOW_VOLTAGE &&
	    vbus_volt <= MSIC_VBUS_OVER_VOLTAGE) {

		mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_GOOD;

		mutex_lock(&mbi->event_lock);
		if (mbi->msic_chr_err == MSIC_CHRG_ERROR_WEAKVIN)
			mbi->msic_chr_err = MSIC_CHRG_ERROR_NONE;
		mutex_unlock(&mbi->event_lock);

	}
	mutex_unlock(&mbi->usb_chrg_lock);
}

static void battery_presence_detect(struct msic_power_module_info *mbi, int ntries)
{	
	unsigned char data;
	int retval;
	int itries;

	for (itries = 0; itries < ntries; itries++) {
		/* read specific to determine the status */
		retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR, &data);
		if (retval)
			retval = handle_ipc_rw_status(retval, MSIC_BATT_CHR_SPWRSRCINT_ADDR,
					MSIC_IPC_READ);

		//if register failed to be read, set battery is not present directly.
		//then we can retry more times.
		if (retval)
			data = 0;

		/* determine battery presence */
		if (data & MSIC_BATT_CHR_BATTDET_MASK) {
			dev_info(msic_dev, "battery is present.");
			break;
		} else {
			dev_warn(msic_dev, "battery is not present, try NO.%d time(s).\n", itries);
			mdelay(10);
		}
	}

	mutex_lock(&mbi->batt_lock);
	if (data & MSIC_BATT_CHR_BATTDET_MASK) {
		mbi->batt_props.not_present_times = 0;
	} else {
		mbi->batt_props.not_present_times ++;
	}

	 if (mbi->batt_props.not_present_times >= mbi->batt_props.not_present_max_times) {
		mbi->batt_props.present = MSIC_BATT_NOT_PRESENT;
		mbi->batt_props.not_present_times = mbi->batt_props.not_present_max_times; 
	 } else {
		mbi->batt_props.present = MSIC_BATT_PRESENT;
	 }
	mutex_unlock(&mbi->batt_lock);
}

static void update_battery_health(struct msic_power_module_info *mbi)
{
	int temp, curr, volt, chr_mode, max_volt, max_volt_hyst;
	unsigned char dummy_val;

	mutex_lock(&mbi->event_lock);
	chr_mode = mbi->charging_mode;
	mutex_unlock(&mbi->event_lock);

	/* determine battery Presence */
	battery_presence_detect(mbi, 3);

	/* We don't get an interrupt once battery returns from
	  error state. So check current status by reading voltagei and
	  temperature and report health as good if recovered from error state */

	volt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (volt == -ENODEV || volt == -EINVAL) {
		dev_warn(msic_dev, "%s, Can't read voltage from FG.\n", __func__);
		return;
	}
	/* convert to milli volts */
	volt /= 1000;

	temp = fg_chip_get_property(POWER_SUPPLY_PROP_TEMP);
	if (temp == -ENODEV || temp == -EINVAL) {
		dev_warn(msic_dev, "%s, Can't read temp from FG\n", __func__);
		return;
	}
	/*convert to degree Celcius from tenths of degree Celsius */
	temp = temp / 10;

	curr = fg_chip_get_property(POWER_SUPPLY_PROP_CURRENT_NOW);

	if (curr == -ENODEV || curr == -EINVAL) {
		dev_warn(msic_dev, "%s, Can't read curr from FG\n", __func__);
		return;
	}
	/* convert to milli ampere */
	curr /= 1000;

	dev_info(msic_dev, "vbatt:%d curr:%d temp:%d\n", volt, curr, temp);

	if (chr_mode != BATT_CHARGING_MODE_NONE) {
		max_volt = (mbi->ch_params.cvol * OVP_VAL_MULT_FACTOR) / 10;
		/* hysteresis is 10% of CV */
		max_volt_hyst = (mbi->ch_params.cvol) / 10;
	} else {
		max_volt = BATT_OVERVOLTAGE_CUTOFF_VOLT;
		max_volt_hyst = BATT_OVERVOLTAGE_CUTOFF_VOLT_HYST;
	}

	/* Check for fault and update health */
	mutex_lock(&mbi->batt_lock);
	/*
	 * Valid temperature window is 0 to 60 Degrees
	 * and thermistor has 2 degree hysteresis and considering
	 * 2 degree adc error, fault revert temperature will
	 * be 4 to 56 degrees.
	 */
	if (mbi->batt_props.health == POWER_SUPPLY_HEALTH_GOOD) {
		/* if BATTOTP is masked check temperature and
		 * update BATT HEALTH */
		if (mbi->chrint_mask & MSIC_BATT_CHR_BATTOTP_MASK) {
			if ((temp > (batt_thrshlds->temp_high)) ||
					(temp <= (batt_thrshlds->temp_low))) {
				/* We report OVERHEAT in both cases*/
				mbi->batt_props.health =
					POWER_SUPPLY_HEALTH_OVERHEAT;
				dev_dbg(msic_dev, "Battery pack temp: %d, "
						"too hot or too cold\n", temp);
			}
		}
	} else {
		/* check for battery overvoltage,overheat and dead  health*/
		if ((mbi->batt_props.health !=
				      POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) &&
			(mbi->batt_props.health != POWER_SUPPLY_HEALTH_DEAD) &&
			(temp <= (batt_thrshlds->temp_high -
				 MSIC_TEMP_HYST_ERR)) &&
			(temp >= (batt_thrshlds->temp_low
				  + MSIC_TEMP_HYST_ERR)) &&
			(volt >= batt_thrshlds->vbatt_crit) &&
			(volt <= (max_volt - max_volt_hyst))) {

			mbi->batt_props.health = POWER_SUPPLY_HEALTH_GOOD;
			dev_dbg(msic_dev, "Setting battery-health, power-supply good");

			mutex_lock(&mbi->event_lock);
			if (mbi->msic_chr_err == MSIC_CHRG_ERROR_OVERHEAT)
				mbi->msic_chr_err = MSIC_CHRG_ERROR_NONE;
			mutex_unlock(&mbi->event_lock);
		} else if (mbi->batt_props.health ==
				POWER_SUPPLY_HEALTH_UNSPEC_FAILURE){

			/* do a dummy IPC read to see IPC recovered from
			 * IPC error. If recovered reset error_count*/

			if (!intel_scu_ipc_ioread8(MSIC_BATT_CHR_CHRCTRL_ADDR,
						&dummy_val)) {
				error_count = 0;
				mbi->batt_props.health =
					POWER_SUPPLY_HEALTH_GOOD;

			}
		}
	}

	mutex_unlock(&mbi->batt_lock);

}

static void msic_batt_disconn(struct work_struct *work)
{
	int ret, event;
	struct msic_power_module_info *mbi =
	    container_of(work, struct msic_power_module_info,
			 disconn_handler.work);
	int err_event;

	mutex_lock(&mbi->event_lock);
	event = mbi->batt_event;
	err_event = mbi->msic_chr_err;
	mutex_unlock(&mbi->event_lock);

	if (event != USBCHRG_EVENT_SUSPEND &&
		event != USBCHRG_EVENT_DISCONN) {
		dev_warn(msic_dev, "%s:Not a Disconn or Suspend event\n",
							__func__);
		return ;
	}

	switch (err_event) {
	case MSIC_CHRG_ERROR_NONE:
		dev_info(msic_dev, "Stopping charging due to "
				"charger event: %s\n",
				(event == USBCHRG_EVENT_SUSPEND) ? "SUSPEND" :
				"DISCONNECT");
		break;
	case MSIC_CHRG_ERROR_CHRTMR_EXPIRY:
		dev_info(msic_dev, "Stopping charging due to "
				"charge-timer expiry");
		break;
	case MSIC_CHRG_ERROR_USER_DISABLE:
		dev_info(msic_dev, "Stopping charging due to "
				"user-disable event");
		break;
	default:
		dev_warn(msic_dev, "Stopping charging due to "
				"unknown error event:%d\n", err_event);
	}

	dump_registers(MSIC_CHRG_REG_DUMP_EVENT);
	ret = msic_batt_stop_charging(mbi);
	if (ret) {
		dev_warn(msic_dev, "%s: failed\n", __func__);
		return;
	}

	mutex_lock(&mbi->batt_lock);
	if (event == USBCHRG_EVENT_SUSPEND)
		mbi->batt_props.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		mbi->batt_props.status = POWER_SUPPLY_STATUS_DISCHARGING;
	mutex_unlock(&mbi->batt_lock);

	/* release the wake lock when charger is unplugged */
	if (wake_lock_active(&mbi->wakelock))
		wake_unlock(&mbi->wakelock);

	power_supply_changed(&mbi->usb);
}

/**
* msic_event_handler - msic event handler
* @arg : private  data pointer
* @event: MSIC event
* @cap : otg capabilities
*
*/
static int msic_event_handler(void *arg, int event, struct otg_bc_cap *cap)
{
	struct msic_power_module_info *mbi =
	    (struct msic_power_module_info *)arg;

	/* cancel the worker of opposite events
	 * i.e cancel the connect handler worker
	 * upon disconnect or suspend event and
	 * cancel the disconnect handler worker
	 * upon connect/update/resume event. This
	 * ensures that the status or health params
	 * won't be modified due to contineous flood
	 * of OTG events.
	 */
	switch (event) {
	case USBCHRG_EVENT_CONNECT:
	case USBCHRG_EVENT_RESUME:
	case USBCHRG_EVENT_UPDATE:
		cancel_delayed_work_sync(&mbi->disconn_handler);
		break;
	case USBCHRG_EVENT_DISCONN:
	case USBCHRG_EVENT_SUSPEND:
		cancel_delayed_work_sync(&mbi->connect_handler);
		break;
	default:
		dev_warn(msic_dev, "Invalid OTG Event:%s\n", __func__);
	}

	/* Update USB power supply info */
	update_usb_ps_info(mbi, cap, event);

	dev_dbg(msic_dev, "Battery present: %d, battery valid: %s, battery event: %d, charge enable: %s", 
			mbi->batt_props.present, mbi->is_batt_valid?"true":"false", mbi->batt_event, mbi->usr_chrg_enbl?"true":"false");
	/* check for valid battery condition */
	if (!mbi->is_batt_valid)
		return 0;

	mutex_lock(&mbi->event_lock);
	if ((mbi->batt_event == event && event != USBCHRG_EVENT_UPDATE) ||
	    (!mbi->usr_chrg_enbl)) {
		mutex_unlock(&mbi->event_lock);
		return 0;
	}
	mbi->batt_event = event;
	mutex_unlock(&mbi->event_lock);

	switch (event) {
	case USBCHRG_EVENT_CONNECT:
		pm_runtime_get_sync(&mbi->ipcdev->dev);
	case USBCHRG_EVENT_RESUME:
	case USBCHRG_EVENT_UPDATE:
		/*
		 * If previous event CONNECT and current is event is
		 * UPDATE, we have already queued the work.
		 * Its better to dequeue the previous work
		 * and add the new work to the queue.
		 */
		cancel_delayed_work_sync(&mbi->connect_handler);
		mbi->ch_params.vinilmt = cap->mA;
		mbi->in_cur_lmt = CHRCNTL_VINLMT_NOLMT;
		mbi->ch_params.chrg_type = cap->chrg_type;
		dev_info(msic_dev, "CHRG TYPE:%d %d\n", cap->chrg_type, cap->mA);
		mutex_lock(&mbi->event_lock);
		mbi->refresh_charger = 1;
		if (mbi->charging_mode == BATT_CHARGING_MODE_NONE)
			mbi->charging_mode = BATT_CHARGING_MODE_NORMAL;
		mutex_unlock(&mbi->event_lock);

		/* Enable charger LED */
		ipc_read_modify_chr_param_reg(mbi, MSIC_CHRG_LED_CNTL_REG,
					      MSIC_CHRG_LED_ENBL, 1);
		/*Disable charger  LOW Power Mode */
		ipc_read_modify_chr_param_reg(mbi, MSIC_BATT_CHR_CHRCTRL_ADDR,
					      CHRCNTL_CHRG_LOW_PWR_ENBL, 0);

		schedule_delayed_work(&mbi->connect_handler, 0);
		break;
	case USBCHRG_EVENT_DISCONN:
		pm_runtime_put_sync(&mbi->ipcdev->dev);
		mutex_lock(&mbi->event_lock);
		/* Reset the error value on DISCONNECT */
		mbi->msic_chr_err = MSIC_CHRG_ERROR_NONE;
		mutex_unlock(&mbi->event_lock);
	case USBCHRG_EVENT_SUSPEND:
		schedule_delayed_work(&mbi->disconn_handler, 0);

		mutex_lock(&mbi->event_lock);
		mbi->refresh_charger = 0;
		mbi->charging_mode = BATT_CHARGING_MODE_NONE;
		mutex_unlock(&mbi->event_lock);

		/* Disable charger LED */
		ipc_read_modify_chr_param_reg(mbi, MSIC_CHRG_LED_CNTL_REG,
					      MSIC_CHRG_LED_ENBL, 0);
		/*Enable charger LOW Power Mode */
		ipc_read_modify_chr_param_reg(mbi, MSIC_BATT_CHR_CHRCTRL_ADDR,
					      CHRCNTL_CHRG_LOW_PWR_ENBL, 1);
		break;
	default:
		dev_warn(msic_dev, "Invalid OTG Event:%s\n", __func__);
	}
	return 0;
}

static void msic_chrg_callback_worker(struct work_struct *work)
{
	struct otg_bc_cap cap;
	struct msic_power_module_info *mbi =
	    container_of(work, struct msic_power_module_info,
			 chrg_callback_dwrk.work);
	penwell_otg_query_charging_cap(&cap);
	msic_event_handler(mbi, cap.current_event, &cap);
}

static void msic_chrg_force_disconn_worker(struct work_struct *work)
{
	struct msic_power_module_info *mbi =
	    container_of(work, struct msic_power_module_info,
			 force_disconn_dwrk.work);
	msic_event_handler(mbi, USBCHRG_EVENT_DISCONN, NULL);
}

/*
 * msic_charger_callback - callback for USB OTG
 * @arg: device info structure
 * @event: USB event
 * @cap: charging capabilities
 * Context: Interrupt Context can not sleep
 *
 * Will be called from the OTG driver.Depending on the event
 * schedules a bottom half to enable or disable the charging.
 */
static int msic_charger_callback(void *arg, int event, struct otg_bc_cap *cap)
{
	struct msic_power_module_info *mbi =
	    (struct msic_power_module_info *)arg;

	schedule_delayed_work(&mbi->chrg_callback_dwrk, 0);
	return 0;
}

/**
 * msic_status_monitor - worker function to monitor status
 * @work: delayed work handler structure
 * Context: Can sleep
 *
 * Will be called from the threaded IRQ function.
 * Monitors status of the charge register and temperature.
 */
static void msic_status_monitor(struct work_struct *work)
{
	unsigned int delay = CHARGE_STATUS_DELAY_JIFFIES;
	struct msic_power_module_info *mbi =
	    container_of(work, struct msic_power_module_info,
			 chr_status_monitor.work);

	pm_runtime_get_sync(&mbi->ipcdev->dev);

	/*update charger and battery health */
	update_charger_health(mbi);
	update_battery_health(mbi);

	power_supply_changed(&mbi->usb);
	schedule_delayed_work(&mbi->chr_status_monitor, delay);

	pm_runtime_put_sync(&mbi->ipcdev->dev);
}

/**
 * msic_battery_interrupt_handler - msic battery interrupt handler
 * Context: interrupt context
 *
 * MSIC battery interrupt handler which will be called on insertion
 * of valid power source to charge the battery or an exception
 * condition occurs.
 */
static irqreturn_t msic_battery_interrupt_handler(int id, void *dev)
{
	struct msic_power_module_info *mbi = dev;
	u32 reg_int_val;

	/* We have only one concurrent fifo reader
	 * and only one concurrent writer, so we are not
	 * using any lock to protect fifo.
	 */
	if (unlikely(kfifo_is_full(&irq_fifo))) {
		dev_warn(&mbi->ipcdev->dev, "KFIFO Full\n");
		return IRQ_WAKE_THREAD;
	}
	/* Copy Interrupt registers locally */
	reg_int_val = readl(mbi->msic_intr_iomap);
	/* Add the Interrupt regs to  FIFO */
	kfifo_in(&irq_fifo, &reg_int_val, IRQ_KFIFO_ELEMENT);

	return IRQ_WAKE_THREAD;
}

/**
 * msic_battery_thread_handler - msic battery threaded IRQ function
 * Context: can sleep
 *
 * MSIC battery needs to either update the battery status as full
 * if it detects battery full condition caused the interrupt or needs
 * to enable battery charger if it detects usb and battery detect
 * caused the source of interrupt.
 */
static irqreturn_t msic_battery_thread_handler(int id, void *dev)
{
	int ret;
	unsigned char data[2];
	struct msic_power_module_info *mbi = dev;
	u32 tmp;
	unsigned char intr_stat;
	u32 log_intr;
	bool disable_chr_tmr;

	/* We have only one concurrent fifo reader
	 * and only one concurrent writer, we are not
	 * using any lock to protect fifo.
	 */
	if (unlikely(kfifo_is_empty(&irq_fifo))) {
		dev_warn(msic_dev, "KFIFO Empty\n");
		return IRQ_NONE;
	}
	/* Get the Interrupt regs state from FIFO */
	ret = kfifo_out(&irq_fifo, &tmp, IRQ_KFIFO_ELEMENT);
	if (ret != IRQ_KFIFO_ELEMENT) {
		dev_warn(msic_dev, "KFIFO underflow\n");
		return IRQ_NONE;
	}

	/* Even though some second level charger interrupts are masked by SCU
	 * the flags for these interrupts will be set in second level interrupt
	 * status register when SCU forwards the unmasked interrupts. Kernel
	 * should ignore the status for masked registers.*/

	/* CHRINT Register */
	data[0] = ((tmp & 0x00ff0000) >> 16) & ~(mbi->chrint_mask);
	/* CHRINT1 Register */
	data[1] = (tmp & 0xff000000) >> 24 & ~(mbi->chrint1_mask);

	dev_info(msic_dev, "PWRSRC Int %x %x\n", tmp & 0xff,
		(tmp & 0xff00) >> 8);
	dev_info(msic_dev, "CHR Int %x %x\n", data[0], data[1]);

	/* Saving the read interrupt value for printing later */
	log_intr = tmp;

	mutex_lock(&mbi->event_lock);
	tmp = mbi->charging_mode;
	disable_chr_tmr = mbi->disable_safety_tmr;
	mutex_unlock(&mbi->event_lock);

	dump_registers(MSIC_CHRG_REG_DUMP_EVENT);

	/* Check if charge complete */
	if (data[1] & MSIC_BATT_CHR_CHRCMPLT_MASK)
		dev_dbg(msic_dev, "CHRG COMPLT\n");

#if 0
	/*ignore Charger-timer expiry event in Yukka Beach board*/
	if ((data[0] & MSIC_BATT_CHR_TIMEEXP_MASK) &&
			(tmp == BATT_CHARGING_MODE_NORMAL) &&
			!disable_chr_tmr) {
		dev_dbg(msic_dev, "force suspend event\n");

		/* Note the error-event, so that we don't restart charging */
		mutex_lock(&mbi->event_lock);
		mbi->msic_chr_err = MSIC_CHRG_ERROR_CHRTMR_EXPIRY;
		mutex_unlock(&mbi->event_lock);

		msic_event_handler(mbi, USBCHRG_EVENT_SUSPEND, NULL);
	}
#endif

	if (data[1] & MSIC_BATT_CHR_WKVINDET_MASK) {
		dev_dbg(msic_dev, "CHRG WeakVIN Detected\n");

		/* Sometimes for USB unplug we are receiving WeakVIN
		 * interrupts,So as SW work around we will check the
		 * SPWRSRCINT SUSBDET bit to know the USB connection.
		 */
		ret = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR,
					    &intr_stat);
		if (ret)
			handle_ipc_rw_status(ret, MSIC_BATT_CHR_SPWRSRCINT_ADDR,
					MSIC_IPC_READ);
		else if (!(intr_stat & MSIC_BATT_CHR_USBDET_MASK) &&
		    (tmp != BATT_CHARGING_MODE_NONE)) {
			data[1] &= ~MSIC_BATT_CHR_WKVINDET_MASK;
			dev_info(msic_dev, "force disconnect event\n");
			msic_event_handler(mbi, USBCHRG_EVENT_DISCONN, NULL);
		}
	}

	/* Check if an exception occurred */
	if (data[0] || (data[1] & ~(MSIC_BATT_CHR_CHRCMPLT_MASK)))
		msic_handle_exception(mbi, data[0], data[1]);

	/*
	 * Mask LOWBATT INT once after recieving the first
	 * LOWBATT INT because the platfrom will shutdown
	 * on LOWBATT INT, So no need to service LOWBATT INT
	 * afterwards and increase the load on CPU.
	 */
	if ((data[0] & MSIC_BATT_CHR_LOWBATT_MASK) &&
			!mbi->usb_chrg_props.charger_present) {
		dev_warn(msic_dev, "Masking LOWBATTINT\n");
		mbi->chrint_mask |= MSIC_BATT_CHR_LOWBATT_MASK;
		ret = intel_scu_ipc_iowrite8(MSIC_BATT_CHR_MCHRINT_ADDR,
						mbi->chrint_mask);
		if (ret)
			handle_ipc_rw_status(ret,
				MSIC_BATT_CHR_MCHRINT_ADDR, MSIC_IPC_WRITE);
	}

	/* Check charger Status bits */
	if ((data[0] & ~(MSIC_BATT_CHR_TIMEEXP_MASK)) ||
		(data[1] & ~(MSIC_BATT_CHR_CHRCMPLT_MASK))) {
		dev_info(msic_dev, "PWRSRC Int %x %x\n", log_intr & 0xff,
				(log_intr & 0xff00) >> 8);
		dev_info(msic_dev, "CHR Int %x %x\n", data[0], data[1]);
	}

	power_supply_changed(&mbi->usb);
	return IRQ_HANDLED;
}

/**
 * check_charger_conn - check charger connection and handle the state
 * @mbi : charger device info
 * Context: can sleep
 *
 */
static int check_charger_conn(struct msic_power_module_info *mbi)
{
	int retval;
	struct otg_bc_cap cap;
	unsigned char data;

	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR,
			&data);
	if (retval) {
		retval = handle_ipc_rw_status(retval,
				MSIC_BATT_CHR_SPWRSRCINT_ADDR, MSIC_IPC_READ);
		if (retval)
			goto disable_chrg_block;
	}

	if (data & MSIC_BATT_CHR_USBDET_MASK) {
		retval = penwell_otg_query_charging_cap(&cap);
		if (retval) {
			dev_warn(msic_dev, "%s(): usb otg power query "
				 "failed with error code %d\n", __func__,
				 retval);
			goto disable_chrg_block;
		}
		/* Enable charging only if vinilmt is >= 100mA */
		if (cap.mA >= 100) {
			msic_event_handler(mbi, USBCHRG_EVENT_CONNECT, &cap);
			return retval;
		}
	}

disable_chrg_block:
	/* Disable the charging block */
	dev_info(msic_dev, "%s\n", __func__);
	msic_batt_stop_charging(mbi);
	/*Putting the charger in LOW Power Mode */
	ipc_read_modify_chr_param_reg(mbi, MSIC_BATT_CHR_CHRCTRL_ADDR,
				      CHRCNTL_CHRG_LOW_PWR_ENBL, 1);
	return retval;
}

/**
 * set_chrg_enable - sysfs set api for charge_enable attribute
 * Parameter as define by sysfs interface
 * Context: can sleep
 *
 */
static ssize_t set_chrg_enable(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct ipc_device *ipcdev =
	    container_of(dev, struct ipc_device, dev);
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	long value;
	int retval, chr_mode;

	if (kstrtol(buf, 10, &value))
		return -EINVAL;

	/* Allow only 0 to 4 for writing */
	if (value < USER_SET_CHRG_DISABLE || value > USER_SET_CHRG_NOLMT)
		return -EINVAL;

	mutex_lock(&mbi->event_lock);

	/*if same value is given, no neeid to do anything. Not an error  */
	if (value == mbi->usr_chrg_enbl) {
		mutex_unlock(&mbi->event_lock);
		return count;
	}
	/* No need to process if same value given
	 * or charging stopped due to an error */
	if (mbi->msic_chr_err == MSIC_CHRG_ERROR_CHRTMR_EXPIRY) {
		mutex_unlock(&mbi->event_lock);
		return -EIO;
	}

	chr_mode = mbi->charging_mode;
	mutex_unlock(&mbi->event_lock);

	if (!value && (chr_mode != BATT_CHARGING_MODE_NONE)) {
		dev_dbg(msic_dev, "User App charger disable !\n");
		mutex_lock(&mbi->event_lock);
		mbi->msic_chr_err = MSIC_CHRG_ERROR_USER_DISABLE;
		mutex_unlock(&mbi->event_lock);

		/* Disable charger before setting the usr_chrg_enbl */
		msic_event_handler(mbi, USBCHRG_EVENT_SUSPEND, NULL);

		mutex_lock(&mbi->event_lock);
		mbi->usr_chrg_enbl = value;
		mutex_unlock(&mbi->event_lock);

	} else if (value && (chr_mode == BATT_CHARGING_MODE_NONE)) {
		dev_dbg(msic_dev, "User App charger enable!\n");

		/* enable usr_chrg_enbl before checking charger connection */
		mutex_lock(&mbi->event_lock);
		mbi->msic_chr_err = MSIC_CHRG_ERROR_NONE;
		mbi->usr_chrg_enbl = value;
		mutex_unlock(&mbi->event_lock);

		retval = check_charger_conn(mbi);
		if (retval)
			dev_warn(msic_dev, "check_charger_conn failed\n");
	} else {
		mutex_lock(&mbi->event_lock);
		mbi->refresh_charger = 1;
		mbi->usr_chrg_enbl = value;
		mutex_unlock(&mbi->event_lock);
	}

	return count;
}

/**
 * get_chrg_enable - sysfs get api for charge_enable attribute
 * Parameter as define by sysfs interface
 * Context: can sleep
 *
 */
static ssize_t get_chrg_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ipc_device *ipcdev =
	    container_of(dev, struct ipc_device, dev);
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	int val;

	mutex_lock(&mbi->event_lock);
	val = mbi->usr_chrg_enbl;
	mutex_unlock(&mbi->event_lock);

	return sprintf(buf, "%d\n", val);
}

/* get_is_power_supply_conn - sysfs get api for power_supply_conn attribute
 * Parameter as defined by sysfs interface
 * Context: can sleep
 *
 */
static ssize_t get_is_power_supply_conn(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", !(intel_msic_is_current_sense_enabled()));
}

/*
 * set_disable_safety_timer - sysfs set api for disable_safety_tmr
 * Parameter as defined by sysfs interface
 * Context: can sleep
 */
static ssize_t set_disable_safety_timer(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev =
	    container_of(dev, struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned long value;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	/* Allow only 0 or 1 for writing */
	if (value > 1)
		return -EINVAL;

	mutex_lock(&mbi->event_lock);
	if (value)
		mbi->disable_safety_tmr = true;
	else
		mbi->disable_safety_tmr = false;
	mutex_unlock(&mbi->event_lock);

	return count;
}

/*
 * get_disable_safety_timer - sysfs get api for disable_safety_tmr
 * Parameter as defined by sysfs interface
 * Context: can sleep
 */
static ssize_t get_disable_safety_timer(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev =
	    container_of(dev, struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned int val;

	mutex_lock(&mbi->event_lock);
	val = mbi->disable_safety_tmr;
	mutex_unlock(&mbi->event_lock);

	return sprintf(buf, "%d\n", val);
}

/**
 * sfi_table_invalid_batt - default battery SFI table values  to be
 * used in case of invalid battery
 *
 * @sfi_table : sfi table pointer
 * Context: can sleep
 *
 */
static void sfi_table_invalid_batt(struct msic_batt_sfi_prop *sfi_table)
{

	/*
	 * In case of invalid battery we manually set
	 * the SFI parameters and limit the battery from
	 * charging, so platform will be in discharging mode
	 */
	memcpy(sfi_table->batt_id, "UNKNOWN", sizeof("UNKNOWN"));
	sfi_table->voltage_max = CHR_CHRVOLTAGE_SET_DEF;
	sfi_table->capacity = DEFAULT_MAX_CAPACITY;
	sfi_table->battery_type = POWER_SUPPLY_TECHNOLOGY_LION;
	sfi_table->temp_mon_ranges = 0;

}
/**
* mfld_umip_read_termination_current - reads the termination current data from umip using IPC.
* @term_curr : termination current read from umip.
*/
static void  mfld_umip_read_termination_current(u32 *term_curr)
{
	int mip_offset, ret;
	/* Read 2bytes of termination current data from the umip */
	mip_offset = UMIP_REF_FG_TBL + UMIP_BATT_FG_TERMINATION_CURRENT;
	ret = intel_scu_ipc_read_mip((u8 *)term_curr, 2, mip_offset, 0);
	if (ret) {
		dev_warn(msic_dev, "Reading umip for termination_current failed. setting to default");
		*term_curr = FULL_CURRENT_AVG_HIGH;
	} else {
		 /* multiply the current with maxim current conversion factor*/
		 *term_curr *= TERMINATION_CUR_CONV_FACTOR;
		 /* convert in to mili amps */
		 *term_curr /= 1000;
	}
	dev_info(msic_dev, "termination_current read from umip: %dmA\n",
			*term_curr);
}


/**
 * sfi_table_populate - Simple Firmware Interface table Populate
 * @sfi_table: Simple Firmware Interface table structure
 *
 * SFI table has entries for the temperature limits
 * which is populated in a local structure
 */
static int __init sfi_table_populate(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct msic_batt_sfi_prop *pentry;
	struct ipc_device *ipcdev =
	    container_of(msic_dev, struct ipc_device, dev);
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	int totentrs = 0, totlen = 0;

	sb = (struct sfi_table_simple *)table;
	if (!sb) {
		dev_warn(msic_dev, "SFI: Unable to map BATT signature\n");
		mbi->is_batt_valid = false;
		return -ENODEV;
	}

	totentrs = SFI_GET_NUM_ENTRIES(sb, struct msic_batt_sfi_prop);
	if (totentrs) {
		pentry = (struct msic_batt_sfi_prop *)sb->pentry;
		totlen = totentrs * sizeof(*pentry);
		memcpy(sfi_table, pentry, totlen);
		mbi->is_batt_valid = true;
		if (sfi_table->temp_mon_ranges != SFI_TEMP_NR_RNG)
			dev_warn(msic_dev, "SFI: temperature monitoring range"
				"doesn't match with its Array elements size\n");
	} else {
		dev_warn(msic_dev, "Invalid battery detected\n");
		sfi_table_invalid_batt(sfi_table);
		mbi->is_batt_valid = false;
	}

	return 0;
}

static void notify_fg_batt_in( struct msic_power_module_info *mbi )
{
#ifdef CONFIG_BATTERY_MAX17042
#else
	int ret;
	int gpio_value;

	mbi->gpio_battery_insert = get_gpio_by_name("batt_in");
	if (mbi->gpio_battery_insert == -1) {
		dev_err(msic_dev, "Unable to get batt_in pin\n");	
		return;
	}
	
	ret = gpio_request(mbi->gpio_battery_insert, "batt_in");
	if (ret) {
		dev_err(msic_dev, "Unable to request batt_in pin\n");	
		return;
	}

	gpio_value = 0;

	ret = gpio_direction_output(mbi->gpio_battery_insert, gpio_value);	
	if (ret) {
		dev_err(msic_dev, "failed to set gpio(pin %d) direction\n", mbi->gpio_battery_insert);	
		gpio_free(mbi->gpio_battery_insert);
	} else {
		dev_info(msic_dev, "FG is notified for battery is %s.\n", gpio_value == 0 ? "INSERT" : "REMOVED");	
	}
#endif

	return;
}

/**
 * init_batt_props - initialize battery properties
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_batt_props function initializes the
 * MSIC battery properties.
 */
static void init_batt_props(struct msic_power_module_info *mbi)
{
	int retval;

	mbi->batt_event = USBCHRG_EVENT_DISCONN;
	mbi->charging_mode = BATT_CHARGING_MODE_NONE;
	mbi->usr_chrg_enbl = USER_SET_CHRG_NOLMT;
	mbi->in_cur_lmt = CHRCNTL_VINLMT_NOLMT;
	mbi->msic_chr_err = MSIC_CHRG_ERROR_NONE;

	mbi->batt_props.status = POWER_SUPPLY_STATUS_DISCHARGING;
	mbi->batt_props.health = POWER_SUPPLY_HEALTH_GOOD;
	mbi->batt_props.present = MSIC_BATT_NOT_PRESENT;
	mbi->batt_props.not_present_times = 0;
	mbi->batt_props.not_present_max_times = 3;
	/*initialize the termination current*/
	mfld_umip_read_termination_current(&mbi->term_curr);

	/* detect battery presence */
	battery_presence_detect(mbi, 5);
	notify_fg_batt_in( mbi );

	/* Enable Status Register */
	retval = intel_scu_ipc_iowrite8(CHR_STATUS_FAULT_REG,
					CHR_STATUS_TMR_RST |
					CHR_STATUS_STAT_ENBL);
	if (retval)
		handle_ipc_rw_status(retval,
				CHR_STATUS_FAULT_REG, MSIC_IPC_WRITE);

	/* Disable charger LED */
	ipc_read_modify_chr_param_reg(mbi, MSIC_CHRG_LED_CNTL_REG,
				      MSIC_CHRG_LED_ENBL, 0);

	/*
	 * Mask Battery Detect Interrupt, we are not
	 * handling battery jarring in the driver.
	 */
	ipc_read_modify_reg(mbi, MSIC_BATT_CHR_MPWRSRCINT_ADDR,
			    MSIC_MPWRSRCINT_BATTDET, 1);
}

static u8 compute_pwrsrc_lmt_reg_val(int temp_high, int temp_low)
{
	u8 data = 0;
	if (temp_high >= 60)
		data |= CHR_PWRSRCLMT_TMPH_60;
	else if (temp_high >= 55)
		data |= CHR_PWRSRCLMT_TMPH_55;
	else if (temp_high >= 50)
		data |= CHR_PWRSRCLMT_TMPH_50;
	else
		data |= CHR_PWRSRCLMT_TMPH_45;

	if (temp_low >= 15)
		data |= CHR_PWRSRCLMT_TMPL_15;
	else if (temp_low >= 10)
		data |= CHR_PWRSRCLMT_TMPL_10;
	else if (temp_low >= 5)
		data |= CHR_PWRSRCLMT_TMPL_05;
	else
		data |= CHR_PWRSRCLMT_TMPL_0;

	return data;

}

/**
 * init_batt_thresholds - initialize battery thresholds
 * @mbi: msic module device structure
 * Context: can sleep
 */
static void init_batt_thresholds(struct msic_power_module_info *mbi)
{
	int ret;
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_PWRSRCLMT_ADDR,
	};
	static u8 data[2];

	batt_thrshlds->vbatt_sh_min = MSIC_BATT_VMIN_THRESHOLD;
	batt_thrshlds->vbatt_crit = BATT_CRIT_CUTOFF_VOLT;
	batt_thrshlds->temp_high = MSIC_BATT_TEMP_MAX;
	batt_thrshlds->temp_low = MSIC_BATT_TEMP_MIN;

	/* Read the threshold data from SMIP */
	dev_dbg(msic_dev, "[SMIP Read] offset: %x\n", BATT_SMIP_BASE_OFFSET);
	ret = intel_scu_ipc_read_mip((u8 *) batt_thrshlds,
			  sizeof(struct batt_safety_thresholds),
			  BATT_SMIP_BASE_OFFSET, 1);
	if (ret)
		dev_warn(msic_dev, "%s: smip read failed\n", __func__);

	if (mbi->is_batt_valid)
		get_batt_temp_thresholds(&batt_thrshlds->temp_high,
				&batt_thrshlds->temp_low);

	batt_thrshlds->temp_high += BATT_TEMP_MAX_DETA;
	batt_thrshlds->temp_low  += BATT_TEMP_MIN_DETA;

	data[0] = WDTWRITE_UNLOCK_VALUE;
	data[1] = compute_pwrsrc_lmt_reg_val(batt_thrshlds->temp_high,
			batt_thrshlds->temp_low);
	if (msic_chr_write_multi(mbi, address, data, 2))
		dev_err(msic_dev, "Error in programming PWRSRCLMT reg\n");

	dev_info(msic_dev, "vbatt shutdown: %d\n", batt_thrshlds->vbatt_sh_min);
	dev_info(msic_dev, "vbatt_crit: %d\n", batt_thrshlds->vbatt_crit);
	dev_info(msic_dev, "Temp High Lmt: %d\n", batt_thrshlds->temp_high);
	dev_info(msic_dev, "Temp Low Lmt: %d\n", batt_thrshlds->temp_low);
}

/**
 * init_charger_props - initialize charger properties
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_charger_props function initializes the
 * MSIC usb charger properties.
 */
static void init_charger_props(struct msic_power_module_info *mbi)
{
	mbi->usb_chrg_props.charger_present = MSIC_USB_CHARGER_NOT_PRESENT;
	mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	memcpy(mbi->usb_chrg_props.charger_model, "Unknown", sizeof("Unknown"));
	memcpy(mbi->usb_chrg_props.charger_vender, "Unknown",
	       sizeof("Unknown"));
}

/**
 * init_msic_regs - initialize msic registers
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_msic_regs function initializes the
 * MSIC registers like CV,Power Source LMT,etc..
 */
static int init_msic_regs(struct msic_power_module_info *mbi)
{
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSAFELMT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCVOLTAGE_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRTTIME_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_SPCHARGER_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCTRL1_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_VBUSDET_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_LOWBATTDET_ADDR,
	};
	static u8 data[] = {
		WDTWRITE_UNLOCK_VALUE, MSIC_BATT_CHR_CHRSAFELMT,
		WDTWRITE_UNLOCK_VALUE,
		CONV_VOL_DEC_MSICREG(CHR_CHRVOLTAGE_SET_DEF),
		WDTWRITE_UNLOCK_VALUE, CHR_CHRTIME_SET_13HRS,
		WDTWRITE_UNLOCK_VALUE,
		(~CHR_SPCHRGER_LOWCHR_ENABLE & CHR_SPCHRGER_WEAKVIN_LVL1),
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_DISABLE,
		WDTWRITE_UNLOCK_VALUE, MSIC_CHRG_EXTCHRDIS,
		WDTWRITE_UNLOCK_VALUE, MSIC_BATT_CHR_VBUSDET_SET_MIN,
		WDTWRITE_UNLOCK_VALUE, MSIC_BATT_CHR_SET_LOWBATTREG,
	};

	dump_registers(MSIC_CHRG_REG_DUMP_BOOT | MSIC_CHRG_REG_DUMP_EVENT);

	return msic_chr_write_multi(mbi, address, data, 16);
}

/**
 * msic_battery_probe - msic battery initialize
 * @ipcdev: msic battery ipc device structure
 * Context: can sleep
 *
 * MSIC battery initializes its internal data structure and other
 * infrastructure components for it to work as expected.
 */
static int msic_battery_probe(struct ipc_device *ipcdev)
{
	int retval, read_temp;
	uint8_t data;
	struct msic_power_module_info *mbi = NULL;

	mbi = kzalloc(sizeof(struct msic_power_module_info), GFP_KERNEL);
	if (!mbi) {
		dev_err(&ipcdev->dev, "%s(): memory allocation failed\n",
			__func__);
		return -ENOMEM;
	}

	sfi_table = kzalloc(sizeof(struct msic_batt_sfi_prop), GFP_KERNEL);
	if (!sfi_table) {
		dev_err(&ipcdev->dev, "%s(): memory allocation failed\n",
			__func__);
		kfree(mbi);
		return -ENOMEM;
	}
	batt_thrshlds = kzalloc(sizeof(struct batt_safety_thresholds),
				GFP_KERNEL);
	if (!batt_thrshlds) {
		dev_err(&ipcdev->dev, "%s(): memory allocation failed\n",
			__func__);
		kfree(sfi_table);
		kfree(mbi);
		return -ENOMEM;
	}

	mbi->ipcdev = ipcdev;
	mbi->irq = ipc_get_irq(ipcdev, 0);
	ipc_set_drvdata(ipcdev, mbi);
	msic_dev = &ipcdev->dev;

	/* initialize all required framework before enabling interrupts */

	/* OTG Disconnect is being called from IRQ context
	 * so calling ipc function is not appropriate from otg callback
	 */
	INIT_DELAYED_WORK(&mbi->disconn_handler, msic_batt_disconn);
	INIT_DELAYED_WORK(&mbi->connect_handler, msic_batt_temp_charging);
	INIT_DELAYED_WORK_DEFERRABLE(&mbi->chr_status_monitor,
				     msic_status_monitor);
	INIT_DELAYED_WORK(&mbi->chrg_callback_dwrk, msic_chrg_callback_worker);
	INIT_DELAYED_WORK(&mbi->force_disconn_dwrk,
			msic_chrg_force_disconn_worker);
	wake_lock_init(&mbi->wakelock, WAKE_LOCK_SUSPEND,
		       "msicbattery_wakelock");

	/* Initialize mutex locks */
	mutex_init(&mbi->usb_chrg_lock);
	mutex_init(&mbi->batt_lock);
	mutex_init(&mbi->ipc_rw_lock);
	mutex_init(&mbi->event_lock);
	mutex_init(&mbi->adc_val_lock);

	/* Allocate ADC Channels */
	mbi->adc_handle =
	    intel_mid_gpadc_alloc(MSIC_BATT_SENSORS,
				  MSIC_BATT_PACK_VOL | CH_NEED_VCALIB,
				  MSIC_BATT_PACK_TEMP | CH_NEED_VCALIB |
				  CH_NEED_VREF,
				  MSIC_USB_VOLTAGE | CH_NEED_VCALIB,
				  MSIC_BATTID | CH_NEED_VREF | CH_NEED_VCALIB);
	if (mbi->adc_handle == NULL)
		dev_err(&ipcdev->dev, "ADC allocation failed\n");

	/* check for valid SFI table entry for OEM0 table */
	retval = sfi_table_parse(SFI_SIG_OEM0, NULL, NULL, sfi_table_populate);
	if (retval) {
		dev_warn(&ipcdev->dev, "call sfi_table_parse error, retcode: %d", retval);
		sfi_table_invalid_batt(sfi_table);
		mbi->is_batt_valid = false;
	}
	sfi_table_dump();

	/* Initialize battery and charger Properties */
	init_batt_props(mbi);
	init_charger_props(mbi);
	init_batt_thresholds(mbi);

	/* Re Map Phy address space for MSIC regs */
	mbi->msic_intr_iomap = ioremap_nocache(MSIC_SRAM_INTR_ADDR, 8);
	if (!mbi->msic_intr_iomap) {
		dev_err(&ipcdev->dev, "battery: ioremap Failed\n");
		retval = -ENOMEM;
		goto ioremap_intr_failed;
	}

	/* Init MSIC Registers */
	retval = init_msic_regs(mbi);
	if (retval < 0)
		dev_err(&ipcdev->dev, "MSIC registers init failed\n");

	/* register msic-usb with power supply subsystem */
	mbi->usb.name = CHARGER_PS_NAME;
	mbi->usb.type = POWER_SUPPLY_TYPE_USB;
	mbi->usb.supplied_to = msic_power_supplied_to;
	mbi->usb.num_supplicants = ARRAY_SIZE(msic_power_supplied_to);
	mbi->usb.properties = msic_usb_props;
	mbi->usb.num_properties = ARRAY_SIZE(msic_usb_props);
	mbi->usb.get_property = msic_usb_get_property;
	mbi->disable_safety_tmr = true;
	retval = power_supply_register(&ipcdev->dev, &mbi->usb);
	if (retval) {
		dev_err(&ipcdev->dev, "%s(): failed to register msic usb "
			"device with power supply subsystem\n", __func__);
		goto power_reg_failed_usb;
	}

	retval = device_create_file(&ipcdev->dev, &dev_attr_charge_enable);
	if (retval)
		goto  sysfs1_create_failed;
	retval = device_create_file(&ipcdev->dev, &dev_attr_power_supply_conn);
	if (retval)
		goto sysfs2_create_failed;
	retval = device_create_file(&ipcdev->dev,
			&dev_attr_disable_safety_timer);
	if (retval)
		goto  sysfs3_create_failed;

	/* Register with OTG */
	otg_handle = penwell_otg_register_bc_callback(msic_charger_callback,
						      (void *)mbi);
	if (!otg_handle) {
		dev_err(&ipcdev->dev, "battery: OTG Registration failed\n");
		retval = -EBUSY;
		goto otg_failed;
	}

	/* Init Runtime PM State */
	pm_runtime_put_noidle(&mbi->ipcdev->dev);
	pm_schedule_suspend(&mbi->ipcdev->dev, MSEC_PER_SEC);

	/* Check if already exist a Charger connection */
	retval = check_charger_conn(mbi);
	if (retval)
		dev_err(&ipcdev->dev, "check charger Conn failed\n");

	mbi->chrint_mask = CHRINT_MASK;
	mbi->chrint1_mask = CHRINT1_MASK;

	retval = intel_scu_ipc_iowrite8(MSIC_BATT_CHR_MCHRINT_ADDR,
				       mbi->chrint_mask);
	if (retval)
		handle_ipc_rw_status(retval,
			       MSIC_BATT_CHR_MCHRINT_ADDR, MSIC_IPC_WRITE);

	retval = intel_scu_ipc_iowrite8(MSIC_BATT_CHR_MCHRINT1_ADDR,
				       mbi->chrint1_mask);
	if (retval)
		handle_ipc_rw_status(retval,
				MSIC_BATT_CHR_MCHRINT1_ADDR, MSIC_IPC_WRITE);

	/* register interrupt */
	retval = request_threaded_irq(mbi->irq, msic_battery_interrupt_handler,
				      msic_battery_thread_handler,
				      IRQF_NO_SUSPEND, DRIVER_NAME, mbi);
	if (retval) {
		dev_err(&ipcdev->dev, "%s(): cannot get IRQ\n", __func__);
		goto requestirq_failed;
	}

	/*
	 * When no battery is present and the board is operated from
	 * a lab power supply, the battery thermistor is absent.
	 * In this case, the MSIC reports emergency temperature warnings,
	 * which must be ignored, to avoid a rain of interrupts
	 * (KFIFO_FULL messages)
	 * By reading the thermistor value on BPTHERM1 during driver probe
	 * it is possible to detect operation without a battery and
	 * mask the undesired MSIC interrupt in this case
	 *
	 */
	mdf_multi_read_adc_regs(mbi, HYSTR_SAMPLE_MAX, 1,
				MSIC_ADC_TEMP_IDX, &read_temp);

	if (read_temp == -ERANGE) {
		dev_warn(msic_dev,
			 "Temp read out of range:"
				"disabling BATTOTP interrupts");

		retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_MCHRINT_ADDR,
					       &data);
		if (retval) {
			retval = handle_ipc_rw_status(retval,
				MSIC_BATT_CHR_MCHRINT_ADDR, MSIC_IPC_READ);
			if (retval)
				return retval;
		}

		/* Applying BATTOTP INT mask */
		data |= MSIC_BATT_CHR_BATTOTP_MASK;
		retval = intel_scu_ipc_iowrite8(MSIC_BATT_CHR_MCHRINT_ADDR,
						data);
		if (retval) {
			retval = handle_ipc_rw_status(retval,
			       MSIC_BATT_CHR_MCHRINT_ADDR, MSIC_IPC_WRITE);
			return retval;
		}
	}

	/* Start the status monitoring worker */
	schedule_delayed_work(&mbi->chr_status_monitor, 0);
	return retval;

requestirq_failed:
	penwell_otg_unregister_bc_callback(otg_handle);
otg_failed:
	device_remove_file(&ipcdev->dev, &dev_attr_disable_safety_timer);
sysfs3_create_failed:
	device_remove_file(&ipcdev->dev, &dev_attr_power_supply_conn);
sysfs2_create_failed:
	device_remove_file(&ipcdev->dev, &dev_attr_charge_enable);
sysfs1_create_failed:
	power_supply_unregister(&mbi->usb);
power_reg_failed_usb:
	iounmap(mbi->msic_intr_iomap);
ioremap_intr_failed:
	kfree(batt_thrshlds);
	kfree(sfi_table);
	kfree(mbi);

	return retval;
}

static void do_exit_ops(struct msic_power_module_info *mbi)
{
	/* disable MSIC Charger */
	mutex_lock(&mbi->batt_lock);
	if (mbi->batt_props.status != POWER_SUPPLY_STATUS_DISCHARGING)
		msic_batt_stop_charging(mbi);
	mutex_unlock(&mbi->batt_lock);
}

/**
 * msic_battery_remove - msic battery finalize
 * @ipcdev: msic battery ipc device structure
 * Context: can sleep
 *
 * MSIC battery finalizes its internal data structure and other
 * infrastructure components that it initialized in
 * msic_battery_probe.
 */
static int msic_battery_remove(struct ipc_device *ipcdev)
{
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);

	if (mbi) {
		penwell_otg_unregister_bc_callback(otg_handle);
		flush_scheduled_work();
		intel_mid_gpadc_free(mbi->adc_handle);
		if (gpio_is_valid(mbi->gpio_battery_insert))
			gpio_free(mbi->gpio_battery_insert);
		free_irq(mbi->irq, mbi);
		pm_runtime_get_noresume(&mbi->ipcdev->dev);
		do_exit_ops(mbi);
		if (mbi->msic_intr_iomap != NULL)
			iounmap(mbi->msic_intr_iomap);
		device_remove_file(&ipcdev->dev, &dev_attr_charge_enable);
		device_remove_file(&ipcdev->dev, &dev_attr_power_supply_conn);
		device_remove_file(&ipcdev->dev,
				&dev_attr_disable_safety_timer);
		power_supply_unregister(&mbi->usb);
		wake_lock_destroy(&mbi->wakelock);

		kfree(batt_thrshlds);
		kfree(sfi_table);
		kfree(mbi);
	}

	return 0;
}

static int battery_reboot_notifier_callback(struct notifier_block *notifier,
		unsigned long event, void *data)
{
	struct ipc_device *ipcdev = container_of(msic_dev,
					struct ipc_device, dev);
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);

	if (mbi)
		do_exit_ops(mbi);

	return NOTIFY_OK;
}

#ifdef CONFIG_PM
static int msic_battery_prepare(struct device *dev)
{
	struct msic_power_module_info *mbi = dev_get_drvdata(dev);
	int event;

	mutex_lock(&mbi->event_lock);
	event = mbi->batt_event;
	mutex_unlock(&mbi->event_lock);

	if (event == USBCHRG_EVENT_CONNECT ||
	    event == USBCHRG_EVENT_UPDATE || event == USBCHRG_EVENT_RESUME) {

		msic_event_handler(mbi, USBCHRG_EVENT_SUSPEND, NULL);
		dev_dbg(&mbi->ipcdev->dev, "Forced suspend\n");
	}

	cancel_delayed_work_sync(&mbi->chr_status_monitor);

	return 0;
}

static void msic_battery_complete(struct device *dev)
{
	int retval = 0;
	struct msic_power_module_info *mbi = dev_get_drvdata(dev);
	int event;

	mutex_lock(&mbi->event_lock);
	event = mbi->batt_event;
	mutex_unlock(&mbi->event_lock);

	if ((event == USBCHRG_EVENT_SUSPEND || event == USBCHRG_EVENT_DISCONN)
			&& (mbi->msic_chr_err == MSIC_CHRG_ERROR_NONE)) {
		/* Check if already exist a Charger connection */
		retval = check_charger_conn(mbi);
		if (retval)
			dev_warn(msic_dev, "check_charger_conn failed\n");
	}

	schedule_delayed_work(&mbi->chr_status_monitor, msecs_to_jiffies(1000));
	return;
}
#else
#define msic_battery_prepare	NULL
#define msic_battery_complete	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int msic_runtime_suspend(struct device *dev)
{

	/* ToDo: Check for MSIC Power rails */
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int msic_runtime_resume(struct device *dev)
{
	/* ToDo: Check for MSIC Power rails */
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int msic_runtime_idle(struct device *dev)
{
	struct ipc_device *ipcdev =
	    container_of(dev, struct ipc_device, dev);
	struct msic_power_module_info *mbi = ipc_get_drvdata(ipcdev);
	int event;

	dev_dbg(dev, "%s called\n", __func__);

	mutex_lock(&mbi->event_lock);
	event = mbi->batt_event;
	mutex_unlock(&mbi->event_lock);

	if (event == USBCHRG_EVENT_CONNECT ||
	    event == USBCHRG_EVENT_UPDATE || event == USBCHRG_EVENT_RESUME) {

		dev_warn(&mbi->ipcdev->dev, "%s: device busy\n", __func__);

		return -EBUSY;
	}

	return 0;
}
#else
#define msic_runtime_suspend	NULL
#define msic_runtime_resume	NULL
#define msic_runtime_idle	NULL
#endif
/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct ipc_device_id battery_id_table[] = {
	{"msic_battery", 1},
};

static const struct dev_pm_ops msic_batt_pm_ops = {
	.prepare = msic_battery_prepare,
	.complete = msic_battery_complete,
	.runtime_suspend = msic_runtime_suspend,
	.runtime_resume = msic_runtime_resume,
	.runtime_idle = msic_runtime_idle,
};

static struct ipc_driver msic_battery_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .pm = &msic_batt_pm_ops,
		   },
	.probe = msic_battery_probe,
	.remove = __devexit_p(msic_battery_remove),
	.id_table = battery_id_table,
};

static int __init msic_battery_module_init(void)
{
	int ret;
	char hw_version[] = "P1";//temp used only --dfu

	if(strcmp(hw_version, "P1") == 0){
		therm_curve_data = therm_curve_data_P1;
		BATT_TEMP_MAX_DETA = BATT_TEMP_MAX_DETA_P1;
		BATT_TEMP_MIN_DETA = BATT_TEMP_MIN_DETA_P1;
	} else 	{
		therm_curve_data = therm_curve_data_P1;
		BATT_TEMP_MAX_DETA = BATT_TEMP_MAX_DETA_P1;
		BATT_TEMP_MIN_DETA = BATT_TEMP_MIN_DETA_P1;
	}

	ret = ipc_driver_register(&msic_battery_driver);
	if (ret)
		dev_err(msic_dev, "driver_register failed");

	if (register_reboot_notifier(&battery_reboot_notifier))
		dev_warn(msic_dev, "Battery: Unable to register reboot notifier");

	return ret;
}

static void __exit msic_battery_module_exit(void)
{
	unregister_reboot_notifier(&battery_reboot_notifier);
	ipc_driver_unregister(&msic_battery_driver);
}

late_initcall_async(msic_battery_module_init);
module_exit(msic_battery_module_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_AUTHOR("Anantha Narayanan <anantha.narayanan@intel.com>");
MODULE_AUTHOR("Ananth Krishna <ananth.krishna.r@intel.com>");
MODULE_DESCRIPTION("Intel Medfield MSIC Battery Driver");
MODULE_LICENSE("GPL");
