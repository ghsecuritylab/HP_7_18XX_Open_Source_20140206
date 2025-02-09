/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics Inc.  Cedar Park, TX., USA.
 * All Rights Reserved.
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
 **************************************************************************/

#ifndef _PSB_DRM_H_
#define _PSB_DRM_H_

#if defined(__linux__) && !defined(__KERNEL__)
#include<stdint.h>
#include <linux/types.h>
#include "drm_mode.h"
#endif

#include "psb_ttm_fence_user.h"
#include "psb_ttm_placement_user.h"

#define PSB_FIXED_SHIFT 16

#define PSB_NUM_PIPE 3

/*
 * Public memory types.
 */

#define DRM_PSB_MEM_MMU TTM_PL_PRIV1
#define DRM_PSB_FLAG_MEM_MMU TTM_PL_FLAG_PRIV1

#define TTM_PL_CI               TTM_PL_PRIV0
#define TTM_PL_FLAG_CI          TTM_PL_FLAG_PRIV0

#define TTM_PL_RAR               TTM_PL_PRIV2
#define TTM_PL_FLAG_RAR          TTM_PL_FLAG_PRIV2

/*
typedef int32_t psb_fixed;
typedef uint32_t psb_ufixed;
*/
static inline int32_t psb_int_to_fixed(int a)
{
	return a * (1 << PSB_FIXED_SHIFT);
}

static inline uint32_t psb_unsigned_to_ufixed(unsigned int a)
{
	return a << PSB_FIXED_SHIFT;
}

/*Status of the command sent to the gfx device.*/
enum drm_cmd_status {
	DRM_CMD_SUCCESS,
	DRM_CMD_FAILED,
	DRM_CMD_HANG
};

struct drm_psb_scanout {
	uint32_t buffer_id;	/* DRM buffer object ID */
	uint32_t rotation;	/* Rotation as in RR_rotation definitions */
	uint32_t stride;	/* Buffer stride in bytes */
	uint32_t depth;		/* Buffer depth in bits (NOT) bpp */
	uint32_t width;		/* Buffer width in pixels */
	uint32_t height;	/* Buffer height in lines */
	int32_t transform[3][3];	/* Buffer composite transform */
	/* (scaling, rot, reflect) */
};

#define DRM_PSB_SAREA_OWNERS 16
#define DRM_PSB_SAREA_OWNER_2D 0
#define DRM_PSB_SAREA_OWNER_3D 1

#define DRM_PSB_SAREA_SCANOUTS 3

struct drm_psb_sarea {
	/* Track changes of this data structure */

	uint32_t major;
	uint32_t minor;

	/* Last context to touch part of hw */
	uint32_t ctx_owners[DRM_PSB_SAREA_OWNERS];

	/* Definition of front- and rotated buffers */
	uint32_t num_scanouts;
	struct drm_psb_scanout scanouts[DRM_PSB_SAREA_SCANOUTS];

	int planeA_x;
	int planeA_y;
	int planeA_w;
	int planeA_h;
	int planeB_x;
	int planeB_y;
	int planeB_w;
	int planeB_h;
	/* Number of active scanouts */
	uint32_t num_active_scanouts;
};

#define PSB_RELOC_MAGIC         0x67676767
#define PSB_RELOC_SHIFT_MASK    0x0000FFFF
#define PSB_RELOC_SHIFT_SHIFT   0
#define PSB_RELOC_ALSHIFT_MASK  0xFFFF0000
#define PSB_RELOC_ALSHIFT_SHIFT 16

#define PSB_RELOC_OP_OFFSET     0	/* Offset of the indicated
					 * buffer
					 */

struct drm_psb_reloc {
	uint32_t reloc_op;
	uint32_t where;		/* offset in destination buffer */
	uint32_t buffer;	/* Buffer reloc applies to */
	uint32_t mask;		/* Destination format: */
	uint32_t shift;		/* Destination format: */
	uint32_t pre_add;	/* Destination format: */
	uint32_t background;	/* Destination add */
	uint32_t dst_buffer;	/* Destination buffer. Index into buffer_list */
	uint32_t arg0;		/* Reloc-op dependant */
	uint32_t arg1;
};

#define PSB_GPU_ACCESS_READ         (1ULL << 32)
#define PSB_GPU_ACCESS_WRITE        (1ULL << 33)
#define PSB_GPU_ACCESS_MASK         (PSB_GPU_ACCESS_READ | PSB_GPU_ACCESS_WRITE)

#define PSB_BO_FLAG_COMMAND         (1ULL << 52)

#define PSB_ENGINE_2D 2
#define PSB_ENGINE_DECODE 0
#define PSB_ENGINE_VIDEO 0
#define LNC_ENGINE_ENCODE 1
#define VSP_ENGINE_VPP 6

/*
 * For this fence class we have a couple of
 * fence types.
 */

#define _PSB_FENCE_EXE_SHIFT           0
#define _PSB_FENCE_FEEDBACK_SHIFT      4

#define _PSB_FENCE_TYPE_EXE         (1 << _PSB_FENCE_EXE_SHIFT)
#define _PSB_FENCE_TYPE_FEEDBACK    (1 << _PSB_FENCE_FEEDBACK_SHIFT)

#define PSB_NUM_ENGINES 7

#define PSB_FEEDBACK_OP_VISTEST (1 << 0)

struct drm_psb_extension_rep {
	int32_t exists;
	uint32_t driver_ioctl_offset;
	uint32_t sarea_offset;
	uint32_t major;
	uint32_t minor;
	uint32_t pl;
};

#define DRM_PSB_EXT_NAME_LEN 128

union drm_psb_extension_arg {
	char extension[DRM_PSB_EXT_NAME_LEN];
	struct drm_psb_extension_rep rep;
};

struct psb_validate_req {
	uint64_t set_flags;
	uint64_t clear_flags;
	uint64_t next;
	uint64_t presumed_gpu_offset;
	uint32_t buffer_handle;
	uint32_t presumed_flags;
	uint32_t group;
	uint32_t pad64;
};

struct psb_validate_rep {
	uint64_t gpu_offset;
	uint32_t placement;
	uint32_t fence_type_mask;
};

#define PSB_USE_PRESUMED     (1 << 0)

struct psb_validate_arg {
	int handled;
	int ret;
	union {
		struct psb_validate_req req;
		struct psb_validate_rep rep;
	} d;
};

#define DRM_PSB_FENCE_NO_USER        (1 << 0)

struct psb_ttm_fence_rep {
	uint32_t handle;
	uint32_t fence_class;
	uint32_t fence_type;
	uint32_t signaled_types;
	uint32_t error;
};

typedef struct drm_psb_cmdbuf_arg {
	uint64_t buffer_list;	/* List of buffers to validate */
	uint64_t fence_arg;

	uint32_t cmdbuf_handle;	/* 2D Command buffer object or, */
	uint32_t cmdbuf_offset;	/* rasterizer reg-value pairs */
	uint32_t cmdbuf_size;

	uint32_t reloc_handle;	/* Reloc buffer object */
	uint32_t reloc_offset;
	uint32_t num_relocs;

	/* Not implemented yet */
	uint32_t fence_flags;
	uint32_t engine;

} drm_psb_cmdbuf_arg_t;

struct drm_psb_pageflip_arg_t {
	uint32_t flip_offset;
	uint32_t stride;
};

enum lnc_getparam_key {
	LNC_VIDEO_DEVICE_INFO,
	LNC_VIDEO_GETPARAM_RAR_INFO,
	LNC_VIDEO_GETPARAM_CI_INFO,
	LNC_VIDEO_FRAME_SKIP,
	IMG_VIDEO_DECODE_STATUS,
	IMG_VIDEO_NEW_CONTEXT,
	IMG_VIDEO_RM_CONTEXT,
	IMG_VIDEO_UPDATE_CONTEXT,
	IMG_VIDEO_MB_ERROR,
	IMG_VIDEO_SET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_HDMI_STATE,
	IMG_VIDEO_SET_HDMI_STATE,
	PNW_VIDEO_QUERY_ENTRY,
	IMG_DISPLAY_SET_WIDI_EXT_STATE,
	IMG_VIDEO_IED_STATE
};

struct drm_lnc_video_getparam_arg {
	enum lnc_getparam_key key;
	uint64_t arg;		/* argument pointer */
	uint64_t value;		/* feed back pointer */
};

struct drm_video_displaying_frameinfo {
	uint32_t buf_handle;
	uint32_t width;
	uint32_t height;
	uint32_t size;		/* buffer size */
	uint32_t format;	/* fourcc */
	uint32_t luma_stride;	/* luma stride */
	uint32_t chroma_u_stride;	/* chroma stride */
	uint32_t chroma_v_stride;
	/* luma offset from the beginning of the memory */
	uint32_t luma_offset;
	/* UV offset from the beginning of the memory */
	uint32_t chroma_u_offset;
	uint32_t chroma_v_offset;
	uint32_t reserved;
};

/*
 * Feedback components:
 */

/*
 * Vistest component. The number of these in the feedback buffer
 * equals the number of vistest breakpoints + 1.
 * This is currently the only feedback component.
 */

struct drm_psb_vistest {
	uint32_t vt[8];
};

struct drm_psb_sizes_arg {
	uint32_t ta_mem_size;
	uint32_t mmu_size;
	uint32_t pds_size;
	uint32_t rastgeom_size;
	uint32_t tt_size;
	uint32_t vram_size;
};

struct drm_psb_hist_status_arg {
	uint32_t buf[32];
};

struct drm_psb_dpst_lut_arg {
	uint8_t lut[256];
	int output_id;
};

struct mrst_timing_info {
	uint16_t pixel_clock;
	uint8_t hactive_lo;
	uint8_t hblank_lo;
	uint8_t hblank_hi:4;
	uint8_t hactive_hi:4;
	uint8_t vactive_lo;
	uint8_t vblank_lo;
	uint8_t vblank_hi:4;
	uint8_t vactive_hi:4;
	uint8_t hsync_offset_lo;
	uint8_t hsync_pulse_width_lo;
	uint8_t vsync_pulse_width_lo:4;
	uint8_t vsync_offset_lo:4;
	uint8_t vsync_pulse_width_hi:2;
	uint8_t vsync_offset_hi:2;
	uint8_t hsync_pulse_width_hi:2;
	uint8_t hsync_offset_hi:2;
	uint8_t width_mm_lo;
	uint8_t height_mm_lo;
	uint8_t height_mm_hi:4;
	uint8_t width_mm_hi:4;
	uint8_t hborder;
	uint8_t vborder;
	uint8_t unknown0:1;
	uint8_t hsync_positive:1;
	uint8_t vsync_positive:1;
	uint8_t separate_sync:2;
	uint8_t stereo:1;
	uint8_t unknown6:1;
	uint8_t interlaced:1;
} __packed;

struct gct_r10_timing_info {
	uint16_t pixel_clock;
	uint32_t hactive_lo:8;
	uint32_t hactive_hi:4;
	uint32_t hblank_lo:8;
	uint32_t hblank_hi:4;
	uint32_t hsync_offset_lo:8;
	uint16_t hsync_offset_hi:2;
	uint16_t hsync_pulse_width_lo:8;
	uint16_t hsync_pulse_width_hi:2;
	uint16_t hsync_positive:1;
	uint16_t rsvd_1:3;
	uint8_t vactive_lo:8;
	uint16_t vactive_hi:4;
	uint16_t vblank_lo:8;
	uint16_t vblank_hi:4;
	uint16_t vsync_offset_lo:4;
	uint16_t vsync_offset_hi:2;
	uint16_t vsync_pulse_width_lo:4;
	uint16_t vsync_pulse_width_hi:2;
	uint16_t vsync_positive:1;
	uint16_t rsvd_2:3;
} __packed;

struct mrst_panel_descriptor_v1 {
	uint32_t Panel_Port_Control;	/* 1 dword, Register 0x61180 if LVDS */
	/* 0x61190 if MIPI */
	uint32_t Panel_Power_On_Sequencing;	/*1 dword,Register 0x61208, */
	uint32_t Panel_Power_Off_Sequencing;	/*1 dword,Register 0x6120C, */
	uint32_t Panel_Power_Cycle_Delay_and_Reference_Divisor;	/* 1 dword */
	/* Register 0x61210 */
	struct mrst_timing_info DTD;	/*18 bytes, Standard definition */
	/* 16 bits, as follows */
	uint16_t Panel_Backlight_Inverter_Descriptor;
	/* Bit 0, Frequency, 15 bits,0 - 32767Hz */
	/* Bit 15, Polarity, 1 bit, 0: Normal, 1: Inverted */
	uint16_t Panel_MIPI_Display_Descriptor;
	/*16 bits, Defined as follows: */
	/* if MIPI, 0x0000 if LVDS */
	/* Bit 0, Type, 2 bits, */
	/* 0: Type-1, */
	/* 1: Type-2, */
	/* 2: Type-3, */
	/* 3: Type-4 */
	/* Bit 2, Pixel Format, 4 bits */
	/* Bit0: 16bpp (not supported in LNC), */
	/* Bit1: 18bpp loosely packed, */
	/* Bit2: 18bpp packed, */
	/* Bit3: 24bpp */
	/* Bit 6, Reserved, 2 bits, 00b */
	/* Bit 8, Minimum Supported Frame Rate, 6 bits, 0 - 63Hz */
	/* Bit 14, Reserved, 2 bits, 00b */
} __packed;

struct mrst_panel_descriptor_v2 {
	uint32_t Panel_Port_Control;	/* 1 dword, Register 0x61180 if LVDS */
	/* 0x61190 if MIPI */
	uint32_t Panel_Power_On_Sequencing;	/*1 dword,Register 0x61208, */
	uint32_t Panel_Power_Off_Sequencing;	/*1 dword,Register 0x6120C, */
	uint8_t Panel_Power_Cycle_Delay_and_Reference_Divisor;	/* 1 byte */
	/* Register 0x61210 */
	struct mrst_timing_info DTD;	/*18 bytes, Standard definition */
	uint16_t Panel_Backlight_Inverter_Descriptor;	/*16 bits, as follows */
	/*Bit 0, Frequency, 16 bits, 0 - 32767Hz */
	uint8_t Panel_Initial_Brightness;	/* [7:0] 0 - 100% */
	/*Bit 7, Polarity, 1 bit,0: Normal, 1: Inverted */
	uint16_t Panel_MIPI_Display_Descriptor;
	/*16 bits, Defined as follows: */
	/* if MIPI, 0x0000 if LVDS */
	/* Bit 0, Type, 2 bits, */
	/* 0: Type-1, */
	/* 1: Type-2, */
	/* 2: Type-3, */
	/* 3: Type-4 */
	/* Bit 2, Pixel Format, 4 bits */
	/* Bit0: 16bpp (not supported in LNC), */
	/* Bit1: 18bpp loosely packed, */
	/* Bit2: 18bpp packed, */
	/* Bit3: 24bpp */
	/* Bit 6, Reserved, 2 bits, 00b */
	/* Bit 8, Minimum Supported Frame Rate, 6 bits, 0 - 63Hz */
	/* Bit 14, Reserved, 2 bits, 00b */
} __packed;

union mrst_panel_rx {
	struct {
		/*Num of Lanes, 2 bits,0 = 1 lane, */
		uint16_t NumberOfLanes:2;
		/* 1 = 2 lanes, 2 = 3 lanes, 3 = 4 lanes. */
		uint16_t MaxLaneFreq:3;	/* 0: 100MHz, 1: 200MHz, 2: 300MHz, */
		/*3: 400MHz, 4: 500MHz, 5: 600MHz, 6: 700MHz, 7: 800MHz. */
		uint16_t SupportedVideoTransferMode:2;	/*0: Non-burst only */
		/* 1: Burst and non-burst */
		/* 2/3: Reserved */
		/*0: Continuous, 1: Non-continuous */
		uint16_t HSClkBehavior:1;
		uint16_t DuoDisplaySupport:1;	/*1 bit,0: No, 1: Yes */
		uint16_t ECC_ChecksumCapabilities:1;	/*1 bit,0: No, 1: Yes */
		uint16_t BidirectionalCommunication:1;	/*1 bit,0: No, 1: Yes */
		uint16_t Rsvd:5;	/*5 bits,00000b */
	} panelrx;
	uint16_t panel_receiver;
} __packed;

struct gct_ioctl_arg {
	/* boot panel index, number of panel used during boot */
	uint8_t bpi;
	uint8_t pt;		/* panel type, 4 bit field, 0=lvds, 1=mipi */
	struct mrst_timing_info DTD;	/* timing info for the selected panel */
	uint32_t Panel_Port_Control;
	uint32_t PP_On_Sequencing;	/*1 dword,Register 0x61208, */
	uint32_t PP_Off_Sequencing;	/*1 dword,Register 0x6120C, */
	uint32_t PP_Cycle_Delay;
	uint16_t Panel_Backlight_Inverter_Descriptor;
	uint16_t Panel_MIPI_Display_Descriptor;
} __packed;

struct mrst_vbt {
	char Signature[4];	/*4 bytes,"$GCT" */
	uint8_t Revision;	/*1 byte */
	uint8_t Size;		/*1 byte */
	uint8_t Checksum;	/*1 byte,Calculated */
	void *mrst_gct;
} __packed;

struct mrst_gct_v1 {	/* expect this table to change per customer request */
	union {			/*8 bits,Defined as follows: */
		struct {
			uint8_t PanelType:4;/*4 bits, Bit field for panels */
			/* 0 - 3: 0 = LVDS, 1 = MIPI */
			/*2 bits,Specifies which of the */
			uint8_t BootPanelIndex:2;
			/* 4 panels to use by default */
			uint8_t BootMIPI_DSI_RxIndex:2;	/*Specifies which of */
			/* the 4 MIPI DSI receivers to use */
		} PD;
		uint8_t PanelDescriptor;
	};
	struct mrst_panel_descriptor_v1 panel[4];/*panel descrs,38 bytes each */
	union mrst_panel_rx panelrx[4];	/* panel receivers */
} __packed;

struct mrst_gct_v2 {	/* expect this table to change per customer request */
	union {			/*8 bits,Defined as follows: */
		struct {
			uint8_t PanelType:4;/*4 bits, Bit field for panels */
			/* 0 - 3: 0 = LVDS, 1 = MIPI */
			/*2 bits,Specifies which of the */
			uint8_t BootPanelIndex:2;
			/* 4 panels to use by default */
			uint8_t BootMIPI_DSI_RxIndex:2;	/*Specifies which of */
			/* the 4 MIPI DSI receivers to use */
		} PD;
		uint8_t PanelDescriptor;
	};
	struct mrst_panel_descriptor_v2 panel[4];/*panel descrs,38 bytes each */
	union mrst_panel_rx panelrx[4];	/* panel receivers */
} __packed;

#define PSB_DC_CRTC_SAVE 0x01
#define PSB_DC_CRTC_RESTORE 0x02
#define PSB_DC_OUTPUT_SAVE 0x04
#define PSB_DC_OUTPUT_RESTORE 0x08
#define PSB_DC_CRTC_MASK 0x03
#define PSB_DC_OUTPUT_MASK 0x0C

struct drm_psb_dc_state_arg {
	uint32_t flags;
	uint32_t obj_id;
};

struct drm_psb_mode_operation_arg {
	uint32_t obj_id;
	uint16_t operation;
	struct drm_mode_modeinfo mode;
	void *data;
};

struct drm_psb_stolen_memory_arg {
	uint32_t base;
	uint32_t size;
};

/*Display Register Bits*/
#define REGRWBITS_PFIT_CONTROLS			(1 << 0)
#define REGRWBITS_PFIT_AUTOSCALE_RATIOS		(1 << 1)
#define REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS	(1 << 2)
#define REGRWBITS_PIPEASRC			(1 << 3)
#define REGRWBITS_PIPEBSRC			(1 << 4)
#define REGRWBITS_VTOTAL_A			(1 << 5)
#define REGRWBITS_VTOTAL_B			(1 << 6)
#define REGRWBITS_DSPACNTR	(1 << 8)
#define REGRWBITS_DSPBCNTR	(1 << 9)
#define REGRWBITS_DSPCCNTR	(1 << 10)

/*Overlay Register Bits*/
#define OV_REGRWBITS_OVADD			(1 << 0)
#define OV_REGRWBITS_OGAM_ALL			(1 << 1)

#define OVC_REGRWBITS_OVADD                  (1 << 2)
#define OVC_REGRWBITS_OGAM_ALL			(1 << 3)

struct drm_psb_register_rw_arg {
	uint32_t b_force_hw_on;

	uint32_t display_read_mask;
	uint32_t display_write_mask;

	struct {
		uint32_t pfit_controls;
		uint32_t pfit_autoscale_ratios;
		uint32_t pfit_programmed_scale_ratios;
		uint32_t pipeasrc;
		uint32_t pipebsrc;
		uint32_t vtotal_a;
		uint32_t vtotal_b;
		uint32_t dspcntr_a;
		uint32_t dspcntr_b;
	} display;

	uint32_t overlay_read_mask;
	uint32_t overlay_write_mask;

	struct {
		uint32_t OVADD;
		uint32_t OGAMC0;
		uint32_t OGAMC1;
		uint32_t OGAMC2;
		uint32_t OGAMC3;
		uint32_t OGAMC4;
		uint32_t OGAMC5;
		uint32_t IEP_ENABLED;
		uint32_t IEP_BLE_MINMAX;
		uint32_t IEP_BSSCC_CONTROL;
		uint32_t b_wait_vblank;
		uint32_t b_wms;
		uint32_t buffer_handle;
	} overlay;

	uint32_t vsync_operation_mask;

	struct {
		uint32_t pipe;
		int vsync_pipe;
		uint64_t timestamp;
	} vsync;

	uint32_t sprite_enable_mask;
	uint32_t sprite_disable_mask;

	struct {
		uint32_t dspa_control;
		uint32_t dspa_key_value;
		uint32_t dspa_key_mask;
		uint32_t dspc_control;
		uint32_t dspc_stride;
		uint32_t dspc_position;
		uint32_t dspc_linear_offset;
		uint32_t dspc_size;
		uint32_t dspc_surface;
	} sprite;

	uint32_t subpicture_enable_mask;
	uint32_t subpicture_disable_mask;
};

struct psb_gtt_mapping_arg {
	void *hKernelMemInfo;
	uint32_t offset_pages;
	uint32_t page_align;
};

struct drm_psb_getpageaddrs_arg {
	uint32_t handle;
	unsigned long *page_addrs;
	unsigned long gtt_offset;
};

#define MAX_SLICES_PER_PICTURE 72
struct drm_psb_msvdx_decode_status_t {
	uint32_t fw_status;
	uint32_t num_error_slice;
	int32_t start_error_mb_list[MAX_SLICES_PER_PICTURE];
	int32_t end_error_mb_list[MAX_SLICES_PER_PICTURE];
	int32_t slice_missing_or_error[MAX_SLICES_PER_PICTURE];
};

/* Controlling the kernel modesetting buffers */

#define DRM_PSB_KMS_OFF		0x00
#define DRM_PSB_KMS_ON		0x01
#define DRM_PSB_VT_LEAVE        0x02
#define DRM_PSB_VT_ENTER        0x03
#define DRM_PSB_EXTENSION       0x06
#define DRM_PSB_SIZES           0x07
#define DRM_PSB_FUSE_REG	0x08
#define DRM_PSB_VBT		0x09
#define DRM_PSB_DC_STATE	0x0A
#define DRM_PSB_ADB		0x0B
#define DRM_PSB_MODE_OPERATION	0x0C
#define DRM_PSB_STOLEN_MEMORY	0x0D
#define DRM_PSB_REGISTER_RW	0x0E
#define DRM_PSB_GTT_MAP         0x0F
#define DRM_PSB_GTT_UNMAP       0x10
#define DRM_PSB_GETPAGEADDRS	0x11
/**
 * NOTE: Add new commands here, but increment
 * the values below and increment their
 * corresponding defines where they're
 * defined elsewhere.
 */
#define DRM_PVR_RESERVED1	0x12
#define DRM_PVR_RESERVED2	0x13
#define DRM_PVR_RESERVED3	0x14
#define DRM_PVR_RESERVED4	0x15
#define DRM_PVR_RESERVED5	0x16

#define DRM_PSB_HIST_ENABLE	0x17
#define DRM_PSB_HIST_STATUS	0x18
#define DRM_PSB_UPDATE_GUARD	0x19
#define DRM_PSB_INIT_COMM	0x1A
#define DRM_PSB_DPST		0x1B
#define DRM_PSB_GAMMA		0x1C
#define DRM_PSB_DPST_BL		0x1D

#define DRM_PVR_RESERVED6	0x1E

#define DRM_PSB_GET_PIPE_FROM_CRTC_ID 0x1F
#define DRM_PSB_DPU_QUERY 0x20
#define DRM_PSB_DPU_DSR_ON 0x21
#define DRM_PSB_DPU_DSR_OFF 0x22
#define DRM_PSB_HDMI_FB_CMD 0x23

/* HDCP IOCTLs */
#define DRM_PSB_QUERY_HDCP		0x24
#define DRM_PSB_VALIDATE_HDCP_KSV	0x25
#define DRM_PSB_GET_HDCP_STATUS		0x26
#define DRM_PSB_ENABLE_HDCP		0x27
#define DRM_PSB_DISABLE_HDCP		0x28
#define DRM_PSB_CSC_GAMMA_SETTING	0x29
#define DRM_PSB_ENABLE_HDCP_REPEATER	0x2c
#define DRM_PSB_DISABLE_HDCP_REPEATER	0x2d
#define DRM_PSB_HDCP_REPEATER_PRESENT	0x2e

/* S3D IOCTLs */
/*
#define DRM_PSB_S3D_QUERY               0x2A
#define DRM_PSB_S3D_PREMODESET          0x2B
#define DRM_PSB_S3D_ENABLE              0x2C
*/

/* CSC IOCTLS */
#define DRM_PSB_SET_CSC			0x2A
#define DRM_PSB_GET_HDCP_LINK_STATUS	0x2b

    /*****************************
     *  BEGIN S3D OVERLAY
     ****************************/

    /*****************************
     *  S3D OVERLAY IOCTLs
     *
     *      DRM_OVL_S3D_ACQUIRE
     *          Gain access to the driver allowing use of the overlay
     *          s3d features  for video display.  Returns a  non-zero
     *          identifier in the value pointed to by 'data' and zero
     *          on success or a negative error code.
     *
     *      DRM_OVL_S3D_RELEASE
     *          When passed a valid non-zero identifier, releases the
     *          exclusive access to the overlay s3d features. Returns
     *          zero on success or a negative error.
     *
     *      DRM_OVL_S3D_UPDATE
     *          Perform an update in accordance with a populated ovl_
     *          s3d_update_t  record.  Returns  zero on success  or a
     *          negative error.
     *
     *      DRM_OVL_S3D_CONFIG
     *          Accepts a configuration record describing the desired
     *          s3d overlay setup,  and  performs the setup.  Returns
     *          zero on success or a negative error code.
     */
#define DRM_OVL_S3D_ACQUIRE             0x2E
#define DRM_OVL_S3D_RELEASE             0x2F
#define DRM_OVL_S3D_UPDATE              0x30
#define DRM_OVL_S3D_CONFIG              0x31

    /*****************************
     *  This is for debugging and troubleshooting on the overlay reg-
     *      isters; it may go away someday.  Individual registers may
     *      be read and/or written.
     */
#define DRM_OVL_S3D_OVLREG              0x32

    /*****************************
     *  These  are all possible  S3D surface modes that a  3D display
     *      could recognize.
     */
enum ovlmode_t {
	OM_ERROR = -1,		/*  error occured               */
	OM_NONE,		/*  no mode (or restore)        */
	OM_LINEINTER,		/*  full line interleaved       */
	OM_HALFLINEINTER,	/*  half line interleaved       */
	OM_PIXINTER,		/*  full pixel interleaved      */
	OM_HALFPIXINTER,	/*  half pixel interleaved      */
	OM_SIDEBYSIDE,		/*  full side-by-side           */
	OM_HALFSIDEBYSIDE,	/*  half side-by-side           */
	OM_TOPBOTTOM,		/*  top-bottom mode             */
	OM_HALFTOPBOTTOM	/*  half top-bottom mode        */
};

    /*****************************
     *  These  definitions follow  similar pixel formats  for HDMI in
     *      'psb_intel_hdmi.h'.
     */
enum ovlpixel_t {
	OX_NONE,		/*  analagous to HDMI RGB       */
	OX_NV12,		/*  12-bit planar format        */
	OX_YUV422,		/*  16-bit packaed format       */
	OX_YUV420,		/*  12-bit planar format        */
	OX_FUTURE		/*  for future expansion        */
};

    /*****************************
     *  These definitions  determine the destination pipe to use with
     *      the s3d overlay mode.
     */
enum ovlpipe_t {
	OP_NONE,		/*  No pipe selection given     */
	OP_PIPEA,		/*  select pipe A               */
	OP_PIPEB,		/*  select pipe B               */
	OP_PIPEC,		/*  select pipe C               */
	OP_RESERVED		/*  future expansion            */
};

    /*****************************
     *  These are bits  set to determine  which of the fields  in the
     *      following record are valid.
     */
#define OV_GEOMETRY 0x00000001	/*  sw, sh (input)              */
#define OV_MODE     0x00000002	/*  mode (input)                */
#define OV_PIXEL    0x00000004	/*  pixel (input)               */
#define OV_PIPE     0x00000008	/*  pipe (input)                */
#define OV_MMINFO   0x00000010	/*  mmap fields (input)         */
#define OV_ALLITEMS 0x0000001F	/*  mask isolating all bits     */

    /*****************************
     *  ovl_s3d_config_t  -  record of a caller's request for overlay
     *      S3D setup.
     */
typedef struct tagOVLS3DCONFIGURATION {
	unsigned int valid;	/*  valid fields bitmask        */
	int id,			/*  acquisition identifier      */
	 sw, sh;		/*  source geometry             */
	enum ovlmode_t mode;		/*  s3d overlay mode to set     */
	enum ovlpixel_t pixel;	/*  surface pixel format        */
	enum ovlpipe_t pipe;		/*  destination pipe            */
	unsigned int mmlen,	/*  buffer size in bytes        */
	 stride,		/*  horizontal stride in bytes  */
	 uvstride,		/*  UV stride in planar modes   */
	 yoffset,		/*  Y planar location           */
	 uoffset,		/*  U planar location           */
	 voffset;		/*  V planar location           */
} ovl_s3d_config_t, *ovl_s3d_config_p;

    /*****************************
     *  These are bits  set to determine  which of the fields  in the
     *      following record are valid.
     */
#define OU_SURFACE  0x00000001	/*  'lbuf' and 'rbuf'           */
#define OU_PAN      0x00000002	/*  'sx' and 'sy'               */
#define OU_GEOMETRY 0x00000004	/*  'x', 'y', 'w' and 'h'       */
#define OU_ENABLE   0x00000008	/*  'enable'                    */
#define OU_HCOEFF   0x00000010	/*  'hcoef'                     */

    /*****************************
     *  ovl_s3d_update_t - record used to "flip" the overlay surfaces
     *      to a fresh set (right and left).
     */
typedef struct tagOVLS3DUPDATE {
	unsigned int valid;	/*  ORed OU_xx flags            */
	int id,			/*  acquisition identifier      */
	 sx, sy,		/*  new pan origin              */
	 x, y,			/*  new position                */
	 w, h,			/*  new geometry                */
	 enable,		/*  enable the overlays         */
	 vwait;			/*  wait for vertical blanking  */
	uint32_t lbuf,		/*  left buffer                 */
	 rbuf,			/*  right buffer                */
	*hcoef;			/*  64 user space coefficients  */
} ovl_s3d_update_t, *ovl_s3d_update_p;

#define OM_READ     1		/*  read an overlay register    */
#define OM_WRITE    2		/*  write an overlay register   */

    /*****************************
     *  Record to be passed  as an argument to the DRM_OVL_S3D_OVLREG
     *      ioctl.
     */
typedef struct tagOVLS3DREGREADWRITE {
	unsigned int reg,	/*  register offset             */
	 data;			/*  input/output value          */
	int ovc,		/*  0 -> overlay A              */
	 mode;			/*  OR'ed combo of OM_xx flags  */
} ovl_s3d_reg_t, *ovl_s3d_reg_p;

    /*****************************
     *  END S3D OVERLAY
     ****************************/

#define DRM_PSB_DSR_ENABLE	0xfffffffe
#define DRM_PSB_DSR_DISABLE	0xffffffff

/*
 * TTM execbuf extension.
 */
#define DRM_PSB_TTM_START      0x50
#define DRM_PSB_TTM_END        0x5F
#if defined(PDUMP)
	#define DRM_PSB_CMDBUF		  (PVR_DRM_DBGDRV_CMD + 1)
#else
	#define DRM_PSB_CMDBUF		  (DRM_PSB_TTM_START)
#endif
#define DRM_PSB_SCENE_UNREF	  (DRM_PSB_CMDBUF + 1)
#define DRM_PSB_PLACEMENT_OFFSET   (DRM_PSB_SCENE_UNREF + 1)

struct drm_psb_csc_matrix {
	int pipe;
	int64_t matrix[9];
};

struct psb_drm_dpu_rect {
	int x, y;
	int width, height;
};

struct drm_psb_drv_dsr_off_arg {
	int screen;
	struct psb_drm_dpu_rect damage_rect;
};

struct drm_psb_dev_info_arg {
	uint32_t num_use_attribute_registers;
};
#define DRM_PSB_DEVINFO         0x01

#define PSB_MODE_OPERATION_MODE_VALID	0x01
#define PSB_MODE_OPERATION_SET_DC_BASE  0x02

struct drm_psb_get_pipe_from_crtc_id_arg {
	/** ID of CRTC being requested **/
	uint32_t crtc_id;

	/** pipe of requested CRTC **/
	uint32_t pipe;
};
#define DRM_PSB_DISP_SAVE_HDMI_FB_HANDLE 1
#define DRM_PSB_DISP_GET_HDMI_FB_HANDLE 2
#define DRM_PSB_DISP_DEQUEUE_BUFFER 3
#define DRM_PSB_DISP_PLANEB_DISABLE  4
#define DRM_PSB_DISP_PLANEB_ENABLE  5
#define DRM_PSB_HDMI_OSPM_ISLAND_DOWN 6

struct drm_psb_disp_ctrl {
	uint32_t cmd;
	uint32_t data;
};

#define S3D_MIPIA_DISPLAY 0
#define S3D_HDMI_DISPLAY 1
#define S3D_MIPIC_DISPLAY 2
#define S3D_WIDI_DISPLAY 0xFF

struct drm_psb_s3d_query {
	uint32_t s3d_display_type;
	uint32_t is_s3d_supported;
	uint32_t s3d_format;
	uint32_t mode_resolution_x;
	uint32_t mode_resolution_y;
	uint32_t mode_refresh_rate;
	uint32_t is_interleaving;
};

struct drm_psb_s3d_premodeset {
	uint32_t s3d_buffer_format;
};
#endif
