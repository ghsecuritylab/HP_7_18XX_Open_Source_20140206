/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef	__ATOMISP_CMD_H__
#define	__ATOMISP_CMD_H__


#include <linux/interrupt.h>
#include <linux/delay.h>
#include "sh_css_types.h"
#include "atomisp_internal.h"

#include <linux/atomisp.h>

#define MSI_ENABLE_BIT		16
#define INTR_DISABLE_BIT	10
#define BUS_MASTER_ENABLE	2
#define MEMORY_SPACE_ENABLE	1
#define INTR_IER		24
#define INTR_IIR		16

/*
 * Helper function
 */
void dump_sp_dmem(unsigned int addr, unsigned int size);
struct camera_mipi_info *atomisp_to_sensor_mipi_info(struct v4l2_subdev *sd);
struct atomisp_video_pipe *atomisp_to_video_pipe(struct video_device *dev);
int atomisp_reset(struct atomisp_device *isp);
void atomisp_flush_bufs_and_wakeup(struct atomisp_device *isp);
int atomisp_alloc_css_stat_bufs(struct atomisp_device *isp, int count);

/* TODO:should be here instead of atomisp_helper.h
extern void __iomem *atomisp_io_base;

static inline void __iomem *atomisp_get_io_virt_addr(unsigned int address)
{
	void __iomem *ret = atomisp_io_base + (address & 0x003FFFFF);
	return ret;
}
*/
void *atomisp_kernel_malloc(size_t bytes);
void atomisp_kernel_free(void *ptr);

/*
 * Interrupt functions
 */
void atomisp_msi_irq_init(struct atomisp_device *isp, struct pci_dev *dev);
void atomisp_msi_irq_uninit(struct atomisp_device *isp, struct pci_dev *dev);
void atomisp_wdt_work(struct work_struct *work);
void atomisp_wdt(unsigned long isp_addr);
void atomisp_setup_flash(struct atomisp_device *isp);
irqreturn_t atomisp_isr(int irq, void *dev);
irqreturn_t atomisp_isr_thread(int irq, void *isp_ptr);
int atomisp_get_frame_pgnr(const struct sh_css_frame *frame, u32 * p_pgnr);

/*
 * CSI-2 receiver configuration
 */
void atomisp_set_term_en_count(struct atomisp_device *isp);

/*
 * Get internal fmt according to V4L2 fmt
 */

int atomisp_is_pixelformat_supported(u32 pixelformat);
bool atomisp_is_viewfinder_support(struct atomisp_device *isp);

/*
 * ISP features control function
 */

/*
 * Function to enable/disable lens geometry distortion correction (GDC) and
 * chromatic aberration correction (CAC)
 */
int atomisp_gdc_cac(struct atomisp_device *isp, int flag, __s32 * value);

/*
 * Function to enable/disable low light mode (including ANR)
 */
int atomisp_low_light(struct atomisp_device *isp, int flag, __s32 * value);

/*
 * Function to enable/disable extra noise reduction (XNR) in low light
 * condition
 */
int atomisp_xnr(struct atomisp_device *isp, int flag, int *arg);

/*
 * Function to configure noise reduction
 */
int atomisp_nr(struct atomisp_device *isp, int flag,
	       struct atomisp_nr_config *config);

/*
 * Function to configure temporal noise reduction (TNR)
 */
int atomisp_tnr(struct atomisp_device *isp, int flag,
		struct atomisp_tnr_config *config);

/*
 * Function to get histogram data for image frame
 */
int atomisp_histogram(struct atomisp_device *isp,
	int flag, void *config);

/*
 * Function to configure black level compensation
 */
int atomisp_black_level(struct atomisp_device *isp, int flag,
			struct atomisp_ob_config *config);

/*
 * Function to configure edge enhancement
 */
int atomisp_ee(struct atomisp_device *isp, int flag,
	       struct atomisp_ee_config *config);

/*
 * Function to update Gamma table for gamma, brightness and contrast config
 */
int atomisp_gamma(struct atomisp_device *isp, int flag,
		  struct atomisp_gamma_table *config);
/*
 * Function to update Ctc table for Chroma Enhancement
 */
int atomisp_ctc(struct atomisp_device *isp, int flag,
		struct atomisp_ctc_table *config);

/*
 * Function to update gamma correction parameters
 */
int atomisp_gamma_correction(struct atomisp_device *isp, int flag,
	struct atomisp_gc_config *config);

/*
 * Function to update Gdc table for gdc
 */
int atomisp_gdc_cac_table(struct atomisp_device *isp, int flag,
			  struct atomisp_morph_table *config);

/*
 * Function to update table for macc
 */
int atomisp_macc_table(struct atomisp_device *isp, int flag,
		       struct atomisp_macc_config *config);
/*
 * Function to get DIS statistics.
 */
int atomisp_get_dis_stat(struct atomisp_device *isp,
			 struct atomisp_dis_statistics *stats);

/*
 * Function to set the DIS coefficients.
 */
int atomisp_set_dis_coefs(struct atomisp_device *isp,
			  struct atomisp_dis_coefficients *coefs);

/*
 * Function to set the DIS motion vector.
 */
int atomisp_set_dis_vector(struct atomisp_device *isp,
			   struct atomisp_dis_vector *vector);

/*
 * Function to set/get 3A stat from isp
 */
int atomisp_3a_stat(struct atomisp_device *isp, int flag,
		    struct atomisp_3a_statistics *config);

int atomisp_set_parameters(struct atomisp_device *isp,
		struct atomisp_parameters *arg);

/*
 * Function to set/get isp parameters to isp
 */
int atomisp_param(struct atomisp_device *isp, int flag,
		  struct atomisp_parm *config);

/*
 * Function to configure color effect of the image
 */
int atomisp_color_effect(struct atomisp_device *isp, int flag, __s32 *effect);

/*
 * Function to configure bad pixel correction
 */
int atomisp_bad_pixel(struct atomisp_device *isp, int flag, __s32 *value);

/*
 * Function to configure bad pixel correction params
 */
int atomisp_bad_pixel_param(struct atomisp_device *isp, int flag,
			    struct atomisp_dp_config *config);

/*
 * Function to enable/disable video image stablization
 */
int atomisp_video_stable(struct atomisp_device *isp, int flag, __s32 * value);

/*
 * Function to configure fixed pattern noise
 */
int atomisp_fixed_pattern(struct atomisp_device *isp, int flag, __s32 * value);

/*
 * Function to configure fixed pattern noise table
 */
int atomisp_fixed_pattern_table(struct atomisp_device *isp,
				struct v4l2_framebuffer *config);

/*
 * Function to configure vf overlay image
 */
int atomisp_vf_overlay(struct atomisp_device *isp, int flag,
		       struct atomisp_overlay *overlay);

/*
 * Function to configure false color correction
 */
int atomisp_false_color(struct atomisp_device *isp, int flag, __s32 * value);

/*
 * Function to configure false color correction params
 */
int atomisp_false_color_param(struct atomisp_device *isp, int flag,
			      struct atomisp_de_config *config);

/*
 * Function to configure white balance params
 */
int atomisp_white_balance_param(struct atomisp_device *isp, int flag,
				struct atomisp_wb_config *config);

int atomisp_3a_config_param(struct atomisp_device *isp, int flag,
			    struct atomisp_3a_config *config);

/*
 * Function to enable/disable lens shading correction
 */
int atomisp_shading_correction(struct atomisp_device *isp, int flag,
				       __s32 *value);

/*
 * Function to setup digital zoom
 */
int atomisp_digital_zoom(struct atomisp_device *isp, int flag, __s32 * value);

int atomisp_get_sensor_mode_data(struct atomisp_device *isp,
				 struct atomisp_sensor_mode_data *config);

int atomisp_get_fmt(struct video_device *vdev, struct v4l2_format *f);


/* This function looks up the closest available resolution. */
int atomisp_try_fmt(struct video_device *vdev, struct v4l2_format *f,
						bool *res_overflow);

int atomisp_set_fmt(struct video_device *vdev, struct v4l2_format *f);
int atomisp_set_fmt_file(struct video_device *vdev, struct v4l2_format *f);

void atomisp_free_all_shading_tables(struct atomisp_device *isp);
int atomisp_set_shading_table(struct atomisp_device *isp,
			      struct atomisp_shading_table *shading_table);

int atomisp_save_iunit_reg(struct atomisp_device *isp);
int atomisp_restore_iunit_reg(struct atomisp_device *isp);

int atomisp_ospm_dphy_down(struct atomisp_device *isp);
int atomisp_ospm_dphy_up(struct atomisp_device *isp);
int atomisp_exif_makernote(struct atomisp_device *isp,
			   struct atomisp_makernote_info *config);

void atomisp_free_internal_buffers(struct atomisp_device *isp);
void atomisp_free_3a_dis_buffers(struct atomisp_device *isp);

int  atomisp_flash_enable(struct atomisp_device *isp, int num_frames);

#endif
