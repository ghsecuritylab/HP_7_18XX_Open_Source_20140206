/*
 * coretemp.c - Linux kernel module for hardware monitoring
 *
 * Copyright (C) 2007 Rudolf Marek <r.marek@assembler.cz>
 *
 * Inspired from many hwmon drivers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/moduleparam.h>
#include <asm/msr.h>
#include <asm/processor.h>
#include <linux/intel_mid_pm.h>

#define DRVNAME	"coretemp"

/*
 * TjMax is the critical temperature of the CPU. When the CPU reaches
 * this temperature, a reliable operation of the silicon is not guaranteed.
 * This TjMax is model specific and varies from CPU to CPU.
 *
 * force_tjmax only matters when TjMax can't be read from the CPU itself.
 * When set, it replaces the driver's suboptimal heuristic.
 */
static int force_tjmax;
module_param_named(tjmax, force_tjmax, int, 0444);
MODULE_PARM_DESC(tjmax, "TjMax value in degrees Celsius");

#define BASE_SYSFS_ATTR_NO	2	/* Sysfs Base attr no for coretemp */
#define NUM_REAL_CORES		16	/* Number of Real cores per cpu */
#define CORETEMP_NAME_LENGTH	33	/* String Length of attrs */
#define MAX_CORE_ATTRS		5	/* Maximum no of basic attrs */
#define MAX_THRESH_ATTRS	4	/* Maximum no of threshold attrs */
#define TOTAL_ATTRS		(MAX_CORE_ATTRS + MAX_THRESH_ATTRS)
#define MAX_CORE_DATA		(NUM_REAL_CORES + BASE_SYSFS_ATTR_NO)

#ifdef CONFIG_SMP
#define TO_PHYS_ID(cpu)		cpu_data(cpu).phys_proc_id
#define TO_CORE_ID(cpu)		cpu_data(cpu).cpu_core_id
#define for_each_sibling(i, cpu)	for_each_cpu(i, cpu_sibling_mask(cpu))
#else
#define TO_PHYS_ID(cpu)		(cpu)
#define TO_CORE_ID(cpu)		(cpu)
#define for_each_sibling(i, cpu)	for (i = 0; false; )
#endif
#define TO_ATTR_NO(cpu)		(TO_CORE_ID(cpu) + BASE_SYSFS_ATTR_NO)

/*
 * SOC DTS Registers:
 * These registers/values are Documented in the Penwell Thermal
 * Management HAS.
 */
#define PUNIT_PORT		0x04
#define PUNIT_TEMP_REG		0xB1
#define SOC_TJMAX		90
#define SOC_CALIB_TEMP		84

/*
 * Per-Core Temperature Data
 * @last_updated: The time when the current temperature value was updated
 *		earlier (in jiffies).
 * @cpu_core_id: The CPU Core from which temperature values should be read
 *		This value is passed as "id" field to rdmsr/wrmsr functions.
 * @status_reg: One of IA32_THERM_STATUS or IA32_PACKAGE_THERM_STATUS,
 *		from where the temperature values should be read.
 * @intrpt_reg: One of IA32_THERM_INTERRUPT or IA32_PACKAGE_THERM_INTERRUPT,
 *		from where the thresholds are read.
 * @attr_size:  Total number of pre-core attrs displayed in the sysfs.
 * @is_pkg_data: If this is 1, the temp_data holds pkgtemp data.
 *		Otherwise, temp_data holds coretemp data.
 * @valid: If this is 1, the current temperature is valid.
 */
struct temp_data {
	int temp;
	int ttarget;
	int tjmax;
	unsigned long last_updated;
	unsigned int cpu;
	u32 cpu_core_id;
	u32 status_reg;
	u32 intrpt_reg;
	int attr_size;
	bool is_pkg_data;
	bool valid;
	struct sensor_device_attribute sd_attrs[TOTAL_ATTRS];
	char attr_name[TOTAL_ATTRS][CORETEMP_NAME_LENGTH];
	struct mutex update_lock;
};

/* Platform Data per Physical CPU */
struct platform_data {
	struct device *hwmon_dev;
	u16 phys_proc_id;
	struct temp_data *core_data[MAX_CORE_DATA];
	struct device_attribute name_attr;
	struct device_attribute soc_temp_attr;
};

struct pdev_entry {
	struct list_head list;
	struct platform_device *pdev;
	u16 phys_proc_id;
};

static LIST_HEAD(pdev_list);
static DEFINE_MUTEX(pdev_list_mutex);

static ssize_t show_soc_temp(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	int temp;
	u32 soc_temp_offset;

	/* Read 32 bits of 0xB1 register */
	soc_temp_offset = intel_mid_msgbus_read32(PUNIT_PORT, PUNIT_TEMP_REG);

	/* Lower 8 bits denote the temperature offset */
	soc_temp_offset &= 0xFF;

	/* Calculate the temperature from the offset */
	temp = SOC_TJMAX + SOC_CALIB_TEMP - soc_temp_offset;

	/* Show the temperature in milli degree celsius */
	return sprintf(buf, "%d\n", temp * 1000);
}

static ssize_t show_name(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	return sprintf(buf, "%s\n", DRVNAME);
}

static ssize_t show_label(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct temp_data *tdata = pdata->core_data[attr->index];

	if (tdata->is_pkg_data)
		return sprintf(buf, "Physical id %u\n", pdata->phys_proc_id);

	return sprintf(buf, "Core %u\n", tdata->cpu_core_id);
}

static ssize_t show_crit_alarm(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	u32 eax, edx;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct temp_data *tdata = pdata->core_data[attr->index];

	rdmsr_on_cpu(tdata->cpu, tdata->status_reg, &eax, &edx);

	return sprintf(buf, "%d\n", (eax >> 5) & 1);
}

static ssize_t show_tx_triggered(struct device *dev,
				 struct device_attribute *devattr, char *buf,
				 u32 mask)
{
	u32 eax, edx;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct temp_data *tdata = pdata->core_data[attr->index];

	rdmsr_on_cpu(tdata->cpu, tdata->status_reg, &eax, &edx);

	return sprintf(buf, "%d\n", !!(eax & mask));
}

static ssize_t show_t0_triggered(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	return show_tx_triggered(dev, devattr, buf, THERM_STATUS_THRESHOLD0);
}

static ssize_t show_t1_triggered(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	return show_tx_triggered(dev, devattr, buf, THERM_STATUS_THRESHOLD1);
}

static ssize_t show_tjmax(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", pdata->core_data[attr->index]->tjmax);
}

static ssize_t show_ttarget(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", pdata->core_data[attr->index]->ttarget);
}

static ssize_t show_tx(struct device *dev,
		       struct device_attribute *devattr, char *buf,
		       u32 mask, int shift)
{
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct temp_data *tdata = pdata->core_data[attr->index];
	u32 eax, edx;
	int t;

	rdmsr_on_cpu(tdata->cpu, tdata->intrpt_reg, &eax, &edx);
	t = tdata->tjmax - ((eax & mask) >> shift) * 1000;
	return sprintf(buf, "%d\n", t);
}

static ssize_t store_tx(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count,
			u32 mask, int shift)
{
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct temp_data *tdata = pdata->core_data[attr->index];
	u32 eax, edx;
	unsigned long val;
	int diff;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	/*
	 * Thermal threshold mask is 7 bits wide. Values are entered in terms
	 * of milli degree celsius. Hence don't accept val > (127 * 1000)
	 */
	if (val > tdata->tjmax || val > 127000)
		return -EINVAL;

	diff = (tdata->tjmax - val) / 1000;

	mutex_lock(&tdata->update_lock);
	rdmsr_on_cpu(tdata->cpu, tdata->intrpt_reg, &eax, &edx);
	eax = (eax & ~mask) | (diff << shift);
	wrmsr_on_cpu(tdata->cpu, tdata->intrpt_reg, eax, edx);
	mutex_unlock(&tdata->update_lock);

	return count;
}

static ssize_t show_t0(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	return show_tx(dev, devattr, buf, THERM_MASK_THRESHOLD0,
		       THERM_SHIFT_THRESHOLD0);
}

static ssize_t store_t0(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	return store_tx(dev, devattr, buf, count, THERM_MASK_THRESHOLD0,
			THERM_SHIFT_THRESHOLD0);
}

static ssize_t show_t1(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	return show_tx(dev, devattr, buf, THERM_MASK_THRESHOLD1,
		       THERM_SHIFT_THRESHOLD1);
}

static ssize_t store_t1(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	return store_tx(dev, devattr, buf, count, THERM_MASK_THRESHOLD1,
			THERM_SHIFT_THRESHOLD1);
}

static ssize_t show_temp(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	u32 eax, edx;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct platform_data *pdata = dev_get_drvdata(dev);
	struct temp_data *tdata = pdata->core_data[attr->index];

	mutex_lock(&tdata->update_lock);

	/* Check whether the time interval has elapsed */
	if (!tdata->valid || time_after(jiffies, tdata->last_updated + HZ)) {
		rdmsr_on_cpu(tdata->cpu, tdata->status_reg, &eax, &edx);
		tdata->valid = 0;
		/* Check whether the data is valid */
		if (eax & 0x80000000) {
			tdata->temp = tdata->tjmax -
					((eax >> 16) & 0x7f) * 1000;
			tdata->valid = 1;
		}
		tdata->last_updated = jiffies;
	}

	mutex_unlock(&tdata->update_lock);
	return tdata->valid ? sprintf(buf, "%d\n", tdata->temp) : -EAGAIN;
}

static int adjust_tjmax(struct cpuinfo_x86 *c, u32 id, struct device *dev)
{
	/* The 100C is default for both mobile and non mobile CPUs */

	int tjmax = 100000;
	int tjmax_ee = 85000;
	int usemsr_ee = 1;
	int err;
	u32 eax, edx;
	struct pci_dev *host_bridge;

	/* Early chips have no MSR for TjMax */

	if (c->x86_model == 0xf && c->x86_mask < 4)
		usemsr_ee = 0;

	/* Atom CPUs */
	/* Model '0x27 or 0x35' for Atom based Penwell CPU */
	if (c->x86_model == 0x1c || c->x86_model == 0x27
				|| c->x86_model == 0x35) {
		usemsr_ee = 0;

		host_bridge = pci_get_bus_and_slot(0, PCI_DEVFN(0, 0));

		if (host_bridge && host_bridge->vendor == PCI_VENDOR_ID_INTEL
		    && (host_bridge->device == 0xa000	/* NM10 based nettop */
		    || host_bridge->device == 0xa010))	/* NM10 based netbook */
			tjmax = 100000;
		else
			tjmax = 90000;

		pci_dev_put(host_bridge);
	}

	if (c->x86_model > 0xe && usemsr_ee) {
		u8 platform_id;

		/*
		 * Now we can detect the mobile CPU using Intel provided table
		 * http://softwarecommunity.intel.com/Wiki/Mobility/720.htm
		 * For Core2 cores, check MSR 0x17, bit 28 1 = Mobile CPU
		 */
		err = rdmsr_safe_on_cpu(id, 0x17, &eax, &edx);
		if (err) {
			dev_warn(dev,
				 "Unable to access MSR 0x17, assuming desktop"
				 " CPU\n");
			usemsr_ee = 0;
		} else if (c->x86_model < 0x17 && !(eax & 0x10000000)) {
			/*
			 * Trust bit 28 up to Penryn, I could not find any
			 * documentation on that; if you happen to know
			 * someone at Intel please ask
			 */
			usemsr_ee = 0;
		} else {
			/* Platform ID bits 52:50 (EDX starts at bit 32) */
			platform_id = (edx >> 18) & 0x7;

			/*
			 * Mobile Penryn CPU seems to be platform ID 7 or 5
			 * (guesswork)
			 */
			if (c->x86_model == 0x17 &&
			    (platform_id == 5 || platform_id == 7)) {
				/*
				 * If MSR EE bit is set, set it to 90 degrees C,
				 * otherwise 105 degrees C
				 */
				tjmax_ee = 90000;
				tjmax = 105000;
			}
		}
	}

	if (usemsr_ee) {
		err = rdmsr_safe_on_cpu(id, 0xee, &eax, &edx);
		if (err) {
			dev_warn(dev,
				 "Unable to access MSR 0xEE, for Tjmax, left"
				 " at default\n");
		} else if (eax & 0x40000000) {
			tjmax = tjmax_ee;
		}
	} else if (tjmax == 100000) {
		/*
		 * If we don't use msr EE it means we are desktop CPU
		 * (with exeception of Atom)
		 */
		dev_warn(dev, "Using relative temperature scale!\n");
	}

	return tjmax;
}

static int get_tjmax(struct cpuinfo_x86 *c, u32 id, struct device *dev)
{
	int err;
	u32 eax, edx;
	u32 val;

	/*
	 * A new feature of current Intel(R) processors, the
	 * IA32_TEMPERATURE_TARGET contains the TjMax value
	 */
	err = rdmsr_safe_on_cpu(id, MSR_IA32_TEMPERATURE_TARGET, &eax, &edx);
	if (err) {
		if (c->x86_model > 0xe && c->x86_model != 0x1c &&
				c->x86_model != 0x27 && c->x86_model != 0x35) {
			dev_warn(dev, "Unable to read TjMax from CPU %u\n", id);
		}
	} else {
		val = (eax >> 16) & 0xff;
		/*
		 * If the TjMax is not plausible, an assumption
		 * will be used
		 */
		if (val) {
			dev_dbg(dev, "TjMax is %d degrees C\n", val);
			return val * 1000;
		}
	}

	if (force_tjmax) {
		dev_notice(dev, "TjMax forced to %d degrees C by user\n",
			   force_tjmax);
		return force_tjmax * 1000;
	}

	/*
	 * An assumption is made for early CPUs and unreadable MSR.
	 * NOTE: the calculated value may not be correct.
	 */
	return adjust_tjmax(c, id, dev);
}

static int create_name_attr(struct platform_data *pdata, struct device *dev)
{
	sysfs_attr_init(&pdata->name_attr.attr);
	pdata->name_attr.attr.name = "name";
	pdata->name_attr.attr.mode = S_IRUGO;
	pdata->name_attr.show = show_name;
	return device_create_file(dev, &pdata->name_attr);
}

static int create_soc_temp_attr(struct platform_data *pdata, struct device *dev)
{
	sysfs_attr_init(&pdata->soc_temp_attr.attr);
	pdata->soc_temp_attr.attr.name = "soc_temp_input";
	pdata->soc_temp_attr.attr.mode = S_IRUGO;
	pdata->soc_temp_attr.show = show_soc_temp;
	return device_create_file(dev, &pdata->soc_temp_attr);
}

static int create_core_attrs(struct temp_data *tdata, struct device *dev,
				int attr_no, bool have_ttarget)
{
	int err, i;
	static ssize_t (*const rd_ptr[TOTAL_ATTRS]) (struct device *dev,
			struct device_attribute *devattr, char *buf) = {
			show_label, show_crit_alarm, show_temp, show_tjmax,
			show_ttarget, show_t0, show_t0_triggered,
			show_t1, show_t1_triggered };
	static ssize_t (*rw_ptr[TOTAL_ATTRS]) (struct device *dev,
			struct device_attribute *devattr, const char *buf,
			size_t count) = { NULL, NULL, NULL, NULL, NULL,
					store_t0, NULL, store_t1, NULL };
	static const char *const names[TOTAL_ATTRS] = {
					"temp%d_label", "temp%d_crit_alarm",
					"temp%d_input", "temp%d_crit",
					"temp%d_max",
					"temp%d_threshold1",
					"temp%d_threshold1_triggered",
					"temp%d_threshold2",
					"temp%d_threshold2_triggered" };

	for (i = 0; i < tdata->attr_size; i++) {
		if (rd_ptr[i] == show_ttarget && !have_ttarget)
			continue;
		snprintf(tdata->attr_name[i], CORETEMP_NAME_LENGTH, names[i],
			attr_no);
		sysfs_attr_init(&tdata->sd_attrs[i].dev_attr.attr);
		tdata->sd_attrs[i].dev_attr.attr.name = tdata->attr_name[i];
		tdata->sd_attrs[i].dev_attr.attr.mode = S_IRUGO;
		if (rw_ptr[i]) {
			tdata->sd_attrs[i].dev_attr.attr.mode |= S_IWUSR;
			tdata->sd_attrs[i].dev_attr.store = rw_ptr[i];
		}
		tdata->sd_attrs[i].dev_attr.show = rd_ptr[i];
		tdata->sd_attrs[i].index = attr_no;
		err = device_create_file(dev, &tdata->sd_attrs[i].dev_attr);
		if (err)
			goto exit_free;
	}
	return 0;

exit_free:
	while (--i >= 0) {
		if (!tdata->sd_attrs[i].dev_attr.attr.name)
			continue;
		device_remove_file(dev, &tdata->sd_attrs[i].dev_attr);
	}
	return err;
}

static void __devinit get_ucode_rev_on_cpu(void *edx)
{
	u32 eax;

	wrmsr(MSR_IA32_UCODE_REV, 0, 0);
	sync_core();
	rdmsr(MSR_IA32_UCODE_REV, eax, *(u32 *)edx);
}

static int __cpuinit chk_ucode_version(unsigned int cpu)
{
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	int err;
	u32 edx;

	/*
	 * Check if we have problem with errata AE18 of Core processors:
	 * Readings might stop update when processor visited too deep sleep,
	 * fixed for stepping D0 (6EC).
	 */
	if (c->x86_model == 0xe && c->x86_mask < 0xc) {
		/* check for microcode update */
		err = smp_call_function_single(cpu, get_ucode_rev_on_cpu,
					       &edx, 1);
		if (err) {
			pr_err("Cannot determine microcode revision of "
			       "CPU#%u (%d)!\n", cpu, err);
			return -ENODEV;
		} else if (edx < 0x39) {
			pr_err("Errata AE18 not fixed, update BIOS or "
			       "microcode of the CPU!\n");
			return -ENODEV;
		}
	}
	return 0;
}

static struct platform_device *coretemp_get_pdev(unsigned int cpu)
{
	u16 phys_proc_id = TO_PHYS_ID(cpu);
	struct pdev_entry *p;

	mutex_lock(&pdev_list_mutex);

	list_for_each_entry(p, &pdev_list, list)
		if (p->phys_proc_id == phys_proc_id) {
			mutex_unlock(&pdev_list_mutex);
			return p->pdev;
		}

	mutex_unlock(&pdev_list_mutex);
	return NULL;
}

static struct temp_data *init_temp_data(unsigned int cpu, int pkg_flag)
{
	struct temp_data *tdata;

	tdata = kzalloc(sizeof(struct temp_data), GFP_KERNEL);
	if (!tdata)
		return NULL;

	tdata->status_reg = pkg_flag ? MSR_IA32_PACKAGE_THERM_STATUS :
							MSR_IA32_THERM_STATUS;
	tdata->intrpt_reg = pkg_flag ? MSR_IA32_PACKAGE_THERM_INTERRUPT :
						MSR_IA32_THERM_INTERRUPT;
	tdata->is_pkg_data = pkg_flag;
	tdata->cpu = cpu;
	tdata->cpu_core_id = TO_CORE_ID(cpu);
	tdata->attr_size = MAX_CORE_ATTRS;
	mutex_init(&tdata->update_lock);
	return tdata;
}

static int create_core_data(struct platform_device *pdev,
				unsigned int cpu, int pkg_flag)
{
	struct temp_data *tdata;
	struct platform_data *pdata = platform_get_drvdata(pdev);
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	u32 eax, edx;
	int err, attr_no;
	bool have_ttarget = false;

	/*
	 * Find attr number for sysfs:
	 * We map the attr number to core id of the CPU
	 * The attr number is always core id + 2
	 * The Pkgtemp will always show up as temp1_*, if available
	 */
	attr_no = pkg_flag ? 1 : TO_ATTR_NO(cpu);

	if (attr_no > MAX_CORE_DATA - 1)
		return -ERANGE;

	/*
	 * Provide a single set of attributes for all HT siblings of a core
	 * to avoid duplicate sensors (the processor ID and core ID of all
	 * HT siblings of a core are the same).
	 * Skip if a HT sibling of this core is already registered.
	 * This is not an error.
	 */
	if (pdata->core_data[attr_no] != NULL)
		return 0;

	tdata = init_temp_data(cpu, pkg_flag);
	if (!tdata)
		return -ENOMEM;

	/* Test if we can access the status register */
	err = rdmsr_safe_on_cpu(cpu, tdata->status_reg, &eax, &edx);
	if (err)
		goto exit_free;

	/* We can access status register. Get Critical Temperature */
	tdata->tjmax = get_tjmax(c, cpu, &pdev->dev);

	/*
	 * Read the still undocumented bits 8:15 of IA32_TEMPERATURE_TARGET.
	 * The target temperature is available on older CPUs but not in this
	 * register. Atoms don't have the register at all.
	 */
	if (c->x86_model > 0xe && c->x86_model != 0x1c) {
		err = rdmsr_safe_on_cpu(cpu, MSR_IA32_TEMPERATURE_TARGET,
					&eax, &edx);
		if (!err) {
			tdata->ttarget
			  = tdata->tjmax - ((eax >> 8) & 0xff) * 1000;
			have_ttarget = true;
		}
	}

	/*
	 * Test if we can access the intrpt register. If so, increase
	 * 'size' enough to support t0 and t1 attributes.
	 */
	err = rdmsr_safe_on_cpu(cpu, tdata->intrpt_reg, &eax, &edx);
	if (!err)
		tdata->attr_size += MAX_THRESH_ATTRS;

	pdata->core_data[attr_no] = tdata;

	/* Create sysfs interfaces */
	err = create_core_attrs(tdata, &pdev->dev, attr_no, have_ttarget);
	if (err)
		goto exit_free;

	return 0;
exit_free:
	pdata->core_data[attr_no] = NULL;
	kfree(tdata);
	return err;
}

static void coretemp_add_core(unsigned int cpu, int pkg_flag)
{
	struct platform_device *pdev = coretemp_get_pdev(cpu);
	int err;

	if (!pdev)
		return;

	err = create_core_data(pdev, cpu, pkg_flag);
	if (err)
		dev_err(&pdev->dev, "Adding Core %u failed\n", cpu);
}

static void coretemp_remove_core(struct platform_data *pdata,
				struct device *dev, int indx)
{
	int i;
	struct temp_data *tdata = pdata->core_data[indx];

	/* Remove the sysfs attributes */
	for (i = 0; i < tdata->attr_size; i++) {
		if (!tdata->sd_attrs[i].dev_attr.attr.name)
			continue;
		device_remove_file(dev, &tdata->sd_attrs[i].dev_attr);
	}

	kfree(pdata->core_data[indx]);
	pdata->core_data[indx] = NULL;
}

static int __devinit coretemp_probe(struct platform_device *pdev)
{
	struct platform_data *pdata;
	int err;

	/* Initialize the per-package data structures */
	pdata = kzalloc(sizeof(struct platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	err = create_name_attr(pdata, &pdev->dev);
	if (err)
		goto exit_free;

	err = create_soc_temp_attr(pdata, &pdev->dev);
	if (err)
		goto exit_name;

	pdata->phys_proc_id = pdev->id;
	platform_set_drvdata(pdev, pdata);

	pdata->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(pdata->hwmon_dev)) {
		err = PTR_ERR(pdata->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto exit_soc_temp;
	}
	return 0;

exit_soc_temp:
	device_remove_file(&pdev->dev, &pdata->soc_temp_attr);
exit_name:
	device_remove_file(&pdev->dev, &pdata->name_attr);
	platform_set_drvdata(pdev, NULL);
exit_free:
	kfree(pdata);
	return err;
}

static int __devexit coretemp_remove(struct platform_device *pdev)
{
	struct platform_data *pdata = platform_get_drvdata(pdev);
	int i;

	for (i = MAX_CORE_DATA - 1; i >= 0; --i)
		if (pdata->core_data[i])
			coretemp_remove_core(pdata, &pdev->dev, i);

	device_remove_file(&pdev->dev, &pdata->name_attr);
	hwmon_device_unregister(pdata->hwmon_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(pdata);
	return 0;
}

static struct platform_driver coretemp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRVNAME,
	},
	.probe = coretemp_probe,
	.remove = __devexit_p(coretemp_remove),
};

static int __cpuinit coretemp_device_add(unsigned int cpu)
{
	int err;
	struct platform_device *pdev;
	struct pdev_entry *pdev_entry;

	mutex_lock(&pdev_list_mutex);

	pdev = platform_device_alloc(DRVNAME, TO_PHYS_ID(cpu));
	if (!pdev) {
		err = -ENOMEM;
		pr_err("Device allocation failed\n");
		goto exit;
	}

	pdev_entry = kzalloc(sizeof(struct pdev_entry), GFP_KERNEL);
	if (!pdev_entry) {
		err = -ENOMEM;
		goto exit_device_put;
	}

	err = platform_device_add(pdev);
	if (err) {
		pr_err("Device addition failed (%d)\n", err);
		goto exit_device_free;
	}

	pdev_entry->pdev = pdev;
	pdev_entry->phys_proc_id = pdev->id;

	list_add_tail(&pdev_entry->list, &pdev_list);
	mutex_unlock(&pdev_list_mutex);

	return 0;

exit_device_free:
	kfree(pdev_entry);
exit_device_put:
	platform_device_put(pdev);
exit:
	mutex_unlock(&pdev_list_mutex);
	return err;
}

static void coretemp_device_remove(unsigned int cpu)
{
	struct pdev_entry *p, *n;
	u16 phys_proc_id = TO_PHYS_ID(cpu);

	mutex_lock(&pdev_list_mutex);
	list_for_each_entry_safe(p, n, &pdev_list, list) {
		if (p->phys_proc_id != phys_proc_id)
			continue;
		platform_device_unregister(p->pdev);
		list_del(&p->list);
		kfree(p);
	}
	mutex_unlock(&pdev_list_mutex);
}

static bool is_any_core_online(struct platform_data *pdata)
{
	int i;

	/* Find online cores, except pkgtemp data */
	for (i = MAX_CORE_DATA - 1; i >= 0; --i) {
		if (pdata->core_data[i] &&
			!pdata->core_data[i]->is_pkg_data) {
			return true;
		}
	}
	return false;
}

static void __cpuinit get_core_online(unsigned int cpu)
{
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	struct platform_device *pdev = coretemp_get_pdev(cpu);
	int err;

	/*
	 * CPUID.06H.EAX[0] indicates whether the CPU has thermal
	 * sensors. We check this bit only, all the early CPUs
	 * without thermal sensors will be filtered out.
	 */
	if (!cpu_has(c, X86_FEATURE_DTS))
		return;

	if (!pdev) {
		/* Check the microcode version of the CPU */
		if (chk_ucode_version(cpu))
			return;

		/*
		 * Alright, we have DTS support.
		 * We are bringing the _first_ core in this pkg
		 * online. So, initialize per-pkg data structures and
		 * then bring this core online.
		 */
		err = coretemp_device_add(cpu);
		if (err)
			return;
		/*
		 * Check whether pkgtemp support is available.
		 * If so, add interfaces for pkgtemp.
		 */
		if (cpu_has(c, X86_FEATURE_PTS))
			coretemp_add_core(cpu, 1);
	}
	/*
	 * Physical CPU device already exists.
	 * So, just add interfaces for this core.
	 */
	coretemp_add_core(cpu, 0);
}

static void __cpuinit put_core_offline(unsigned int cpu)
{
	int i, indx;
	struct platform_data *pdata;
	struct platform_device *pdev = coretemp_get_pdev(cpu);

	/* If the physical CPU device does not exist, just return */
	if (!pdev)
		return;

	pdata = platform_get_drvdata(pdev);
	if (!pdata)
		return;

	indx = TO_ATTR_NO(cpu);

	/* The core id is too big, just return */
	if (indx > MAX_CORE_DATA - 1)
		return;

	if (pdata->core_data[indx] && pdata->core_data[indx]->cpu == cpu)
		coretemp_remove_core(pdata, &pdev->dev, indx);

	/*
	 * If a HT sibling of a core is taken offline, but another HT sibling
	 * of the same core is still online, register the alternate sibling.
	 * This ensures that exactly one set of attributes is provided as long
	 * as at least one HT sibling of a core is online.
	 */
	for_each_sibling(i, cpu) {
		if (i != cpu) {
			get_core_online(i);
			/*
			 * Display temperature sensor data for one HT sibling
			 * per core only, so abort the loop after one such
			 * sibling has been found.
			 */
			break;
		}
	}
	/*
	 * If all cores in this pkg are offline, remove the device.
	 * coretemp_device_remove calls unregister_platform_device,
	 * which in turn calls coretemp_remove. This removes the
	 * pkgtemp entry and does other clean ups.
	 */
	if (!is_any_core_online(pdata))
		coretemp_device_remove(cpu);
}

static int __cpuinit coretemp_cpu_callback(struct notifier_block *nfb,
				 unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long) hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_DOWN_FAILED:
		get_core_online(cpu);
		break;
	case CPU_DOWN_PREPARE:
		put_core_offline(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block coretemp_cpu_notifier __refdata = {
	.notifier_call = coretemp_cpu_callback,
};

static int __init coretemp_init(void)
{
	int i, err = -ENODEV;

	/* quick check if we run Intel */
	if (cpu_data(0).x86_vendor != X86_VENDOR_INTEL)
		goto exit;

	err = platform_driver_register(&coretemp_driver);
	if (err)
		goto exit;

	for_each_online_cpu(i)
		get_core_online(i);

#ifndef CONFIG_HOTPLUG_CPU
	if (list_empty(&pdev_list)) {
		err = -ENODEV;
		goto exit_driver_unreg;
	}
#endif

	register_hotcpu_notifier(&coretemp_cpu_notifier);
	return 0;

#ifndef CONFIG_HOTPLUG_CPU
exit_driver_unreg:
	platform_driver_unregister(&coretemp_driver);
#endif
exit:
	return err;
}

static void __exit coretemp_exit(void)
{
	struct pdev_entry *p, *n;

	unregister_hotcpu_notifier(&coretemp_cpu_notifier);
	mutex_lock(&pdev_list_mutex);
	list_for_each_entry_safe(p, n, &pdev_list, list) {
		platform_device_unregister(p->pdev);
		list_del(&p->list);
		kfree(p);
	}
	mutex_unlock(&pdev_list_mutex);
	platform_driver_unregister(&coretemp_driver);
}

MODULE_AUTHOR("Rudolf Marek <r.marek@assembler.cz>");
MODULE_DESCRIPTION("Intel Core temperature monitor");
MODULE_LICENSE("GPL");

module_init(coretemp_init)
module_exit(coretemp_exit)
