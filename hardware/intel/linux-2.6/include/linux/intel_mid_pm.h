/*
 * intel_mid_pm.h
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/errno.h>

#ifndef INTEL_MID_PM_H
#define INTEL_MID_PM_H

#include <asm/intel-mid.h>
#include <linux/init.h>
#include <linux/pci.h>


/* Chip ID of Intel Atom SOC*/
#define INTEL_ATOM_MRST 0x26
#define INTEL_ATOM_MFLD 0x27
#define INTEL_ATOM_CLV 0x35

/* Register Type definitions */
#define OSPM_REG_TYPE          0x0
#define APM_REG_TYPE           0x1
#define OSPM_MAX_POWER_ISLANDS 16
#define OSPM_ISLAND_UP         0x0
#define OSPM_ISLAND_DOWN       0x1

/* North complex power islands definitions for APM block*/
#define APM_GRAPHICS_ISLAND    0x1
#define APM_VIDEO_DEC_ISLAND   0x2
#define APM_VIDEO_ENC_ISLAND   0x4
#define APM_GL3_CACHE_ISLAND   0x8
#define APM_ISP_ISLAND         0x10
#define APM_IPH_ISLAND         0x20

/* North complex power islands definitions for OSPM block*/
#define OSPM_DISPLAY_A_ISLAND  0x2
#define OSPM_DISPLAY_B_ISLAND  0x80
#define OSPM_DISPLAY_C_ISLAND  0x100
#define OSPM_MIPI_ISLAND       0x200

#define C4_STATE_IDX	3
#define C6_STATE_IDX	4
#define S0I1_STATE_IDX  5
#define LPMP3_STATE_IDX 6
#define S0I3_STATE_IDX  7

#define C4_HINT	(0x30)
#define C6_HINT	(0x52)

#define CSTATE_EXIT_LATENCY_C1	 1
#define CSTATE_EXIT_LATENCY_C2	 20
#define CSTATE_EXIT_LATENCY_C4	 100
#define CSTATE_EXIT_LATENCY_C6	 140

/* Since entry latency is substantial
 * put exit_latency = entry+exit latency
 */
#define CSTATE_EXIT_LATENCY_LPMP3 1040
#define CSTATE_EXIT_LATENCY_S0i1 1040
#define CSTATE_EXIT_LATENCY_S0i3 2800

#ifdef CONFIG_ATOM_SOC_POWER
#define LOG_PMU_EVENTS

#define PMU1_MAX_DEVS   8
#define PMU2_MAX_DEVS   55
#define MAX_DEVICES	(PMU1_MAX_DEVS + PMU2_MAX_DEVS)
#define PMU_MAX_LSS_SHARE 4


/* Error codes for pmu */
#define	PMU_SUCCESS			0
#define PMU_FAILED			-1
#define PMU_BUSY_STATUS			0
#define PMU_MODE_ID			1
#define	SET_MODE			1
#define	SET_AOAC_S0i1			2
#define	SET_AOAC_S0i3			3
#define	SET_LPAUDIO			4
#define	SET_AOAC_S0i2			7

#define MID_S0I1_STATE         0x1
#define MID_LPMP3_STATE        0x3

#define MID_S0I3_STATE         0x7
#define MID_S0I2_STATE         0x7

#define MID_S0IX_STATE         0xf
#define MID_S3_STATE           0x1f

/* combinations */
#define MID_LPI1_STATE         0x1f
#define MID_LPI3_STATE         0x7f
#define MID_I1I3_STATE         0xff

#define REMOVE_LP_FROM_LPIX    4

/* Power number for MID_POWER */
#define C0_POWER_USAGE         450
#define C6_POWER_USAGE         200
#define LPMP3_POWER_USAGE      130
#define S0I1_POWER_USAGE       50
#define S0I3_POWER_USAGE       31

#ifdef CONFIG_PM_DEBUG
extern void pmu_s0ix_demotion_stat(int req_state, int grant_state);
#else
static inline void pmu_s0ix_demotion_stat(int req_state, int grant_state) { return; }
#endif
extern int get_target_platform_state(unsigned long *eax);
extern int mid_s0ix_enter(int);
extern int pmu_set_devices_in_d0i0(void);
extern int pmu_pci_set_power_state(struct pci_dev *pdev, pci_power_t state);
extern pci_power_t pmu_pci_choose_state(struct pci_dev *pdev);

extern void pmu_power_off(void);
extern void pmu_set_s0ix_complete(void);
extern bool pmu_is_s0ix_in_progress(void);
extern int pmu_nc_set_power_state
	(int islands, int state_type, int reg_type);
extern int pmu_nc_get_power_state(int island, int reg_type);
extern int pmu_set_emmc_to_d0i0_atomic(void);

#ifdef LOG_PMU_EVENTS
extern void pmu_log_ipc(u32 command);
extern void pmu_log_ipc_irq(void);
#else
static inline void pmu_log_ipc(u32 command) { return; };
static inline void pmu_log_ipc_irq(void) { return; };
#endif
extern void dump_nc_power_history(void);

extern bool mid_pmu_is_wake_source(u32 lss_number);
#else

/*
 * If CONFIG_ATOM_SOC_POWER is not defined
 * fall back to C6
 */
#define MID_S0I1_STATE         C6_HINT
#define MID_LPMP3_STATE        C6_HINT
#define MID_S0I3_STATE         C6_HINT

/* Power usage unknown if MID_POWER not defined */
#define C0_POWER_USAGE         0
#define C6_POWER_USAGE         0
#define LPMP3_POWER_USAGE      0
#define S0I1_POWER_USAGE       0
#define S0I3_POWER_USAGE       0

#define TEMP_DTS_ID     43

static inline int pmu_nc_set_power_state
	(int islands, int state_type, int reg_type) { return 0; }

static inline void pmu_set_s0ix_complete(void) { return; }
static inline bool pmu_is_s0ix_in_progress(void) { return false; };

/*returns function not implemented*/
static inline int pmu_set_devices_in_d0i0(void) { return 0; }
static inline void pmu_log_ipc(u32 command) { return; };
static inline void pmu_log_ipc_irq(void) { return; };
static inline int pmu_set_emmc_to_d0i0_atomic(void) { return -ENOSYS; }
static inline void pmu_power_off(void) { return; }
static inline bool mid_pmu_is_wake_source(u32 lss_number) { return false; }
#endif /* #ifdef CONFIG_ATOM_SOC_POWER */

#endif /* #ifndef INTEL_MID_PM_H */
