#ifndef __INTEL_MID_THERMAL_H__
#define __INTEL_MID_THERMAL_H__

#include <linux/thermal.h>

#define BPTHERM_NAME	 "bptherm"
#define SKIN0_NAME	 "skin0"
#define SKIN1_NAME	 "skin1"
#define MSIC_DIE_NAME	 "msicdie"
#define MSIC_SYS_NAME	 "sys"
#define SYSTHERM1_NAME	 "systherm1"


/**
 * struct intel_mid_thermal_sensor - Intel mid thermal sensor information
 * @name:		name of the sensor
 * @index:		index number of sensor
 * @slope:		slope used for temp calculation
 * @intercept:		intercept used for temp calculation
 * @adc_channel:	adc channel id|flags
 * @direct:		If true then direct conversion is used.
 * @priv:		private sensor data
 * @temp_correlation:	temp correlation function
 */

struct intel_mid_thermal_sensor {
	char name[THERMAL_NAME_LENGTH];
	int index;
	long slope;
	long intercept;
	int adc_channel;
	bool direct;
	void *priv;
	int (*temp_correlation)(void *info, long temp, long *res);
};

/**
 * struct intel_mid_thermal_platform_data - Platform data for
 *		intel mid thermal driver
 *
 * @num_sensors:	Maximum number of sensors supported
 * @sensors:		sensor info
 * @soc_cooling:	True or false
 */
struct intel_mid_thermal_platform_data {
	int num_sensors;
	struct intel_mid_thermal_sensor *sensors;
	bool soc_cooling;
};

/**
 * struct skin1_private_info - skin1 sensor private data
 *
 * @dependent:		dependency on other sensors
			0   - no dependency,
			> 0 - depends on other sensors
 * @sensors:		dependent sensor address.
 */

struct skin1_private_info {
	int dependent;
	struct intel_mid_thermal_sensor **sensors;
};


/* skin0 sensor temperature correlation function*/
int skin0_temp_correlation(void *info,  long temp,  long *res);
/* skin1 sensor temperature correlation function*/
int skin1_temp_correlation(void *info,  long temp,  long *res);
/* bptherm sensor temperature correlation function*/
int bptherm_temp_correlation(void *info,  long temp,  long *res);
#endif
