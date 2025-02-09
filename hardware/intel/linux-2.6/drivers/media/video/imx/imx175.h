#ifndef __IMX175_H__
#define __IMX175_H__

#define IMX_NAME	"imx175"
#define IMX_ID	0x0175

#define IMX_FOCAL_LENGTH_NUM	369	/*3.69mm*/
#define IMX_FOCAL_LENGTH_DEM	100
#define IMX_F_NUMBER_DEFAULT_NUM	22
#define IMX_F_NUMBER_DEM	10

#define IMX_BIN_FACTOR_MAX			4
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX_FOCAL_LENGTH_DEFAULT 0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX_F_NUMBER_DEFAULT 0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define IMX_F_NUMBER_RANGE 0x160a160a

enum imx_tok_type {
	IMX_8BIT  = 0x0001,
	IMX_16BIT = 0x0002,
	IMX_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	IMX_TOK_DELAY  = 0xfe00	/* delay token for reg list */
};

/**
 * struct imx_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct imx_reg {
	enum imx_tok_type type;
	u16 sreg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

struct imx_resolution {
	u8 *desc;
	const struct imx_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	bool used;
};
#define GROUPED_PARAMETER_HOLD_ENABLE  {IMX_8BIT, 0x0104, 0x1}
#define GROUPED_PARAMETER_HOLD_DISABLE  {IMX_8BIT, 0x0104, 0x0}

/************************** settings for imx *************************/

static struct imx_reg const imx_STILL_8M_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x0A},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x8C},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x0A},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x90},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x0C},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0xD0},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x09},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0xA0},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x00},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x09},
	{IMX_8BIT, 0x33D7, 0xA0},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_STILL_6M_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x0A},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x8C},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x0A},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x90},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x32},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x6D},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x0C},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0xD0},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x07},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x3C},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x00},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_STILL_2M_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x0A},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x8C},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x0A},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x90},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x68},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x04},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0xD0},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x01},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x68},
	{IMX_8BIT, 0x33D6, 0x04},
	{IMX_8BIT, 0x33D7, 0xD0},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_PREVIEW_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x03},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x34},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x02},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x68},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x02},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x03},
	{IMX_8BIT, 0x33D5, 0x34},
	{IMX_8BIT, 0x33D6, 0x02},
	{IMX_8BIT, 0x33D7, 0x68},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_WIDE_PREVIEW_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x14},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x8C},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x68},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x03},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0xBC},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x01},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x68},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0xBC},
	{IMX_TOK_TERM, 0, 0}
};

/*****************************video************************/
static struct imx_reg const imx_1080p_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xCC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x78},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0xD0},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x01},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xD8},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x02},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x42},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0A},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xF7},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x5D},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x09},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x20},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x05},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x1C},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x00},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x09},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x05},
	{IMX_8BIT, 0x33D7, 0x1C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_720p_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x48},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x64},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x87},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x3B},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x20},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x03},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x6C},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x01},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_WVGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0xD0},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0xCF},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x68},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x04},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x00},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x01},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x68},
	{IMX_8BIT, 0x33D6, 0x04},
	{IMX_8BIT, 0x33D7, 0x00},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_VGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x03},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x34},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x02},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x68},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x02},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x03},
	{IMX_8BIT, 0x33D5, 0x34},
	{IMX_8BIT, 0x33D6, 0x02},
	{IMX_8BIT, 0x33D7, 0x68},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_QVGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x03},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x38},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x02},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x68},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x09},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x97},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x37},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x01},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x98},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x01},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x34},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x02},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x01},
	{IMX_8BIT, 0x33D5, 0x98},
	{IMX_8BIT, 0x33D6, 0x01},
	{IMX_8BIT, 0x33D7, 0x34},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_QCIF_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x04},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xB8},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x03},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x70},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x08},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x17},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x06},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x2F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x00},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0xD8},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x00},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0xB0},  /*	y_output_size[7:0]	*/
	{IMX_8BIT, 0x0390, 0x02},
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x00},
	{IMX_8BIT, 0x33D5, 0xD8},
	{IMX_8BIT, 0x33D6, 0x00},
	{IMX_8BIT, 0x33D7, 0xB0},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_init_settings[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0103, 0x01},
	/* misc control */
	{IMX_8BIT, 0x3020, 0x10},
	{IMX_8BIT, 0x302D, 0x02},
	{IMX_8BIT, 0x302F, 0x80},
	{IMX_8BIT, 0x3032, 0xA3},
	{IMX_8BIT, 0x3033, 0x20},
	{IMX_8BIT, 0x3034, 0x24},
	{IMX_8BIT, 0x3041, 0x15},
	{IMX_8BIT, 0x3042, 0x87},
	{IMX_8BIT, 0x3050, 0x35},
	{IMX_8BIT, 0x3056, 0x57},
	{IMX_8BIT, 0x305D, 0x41},
	{IMX_8BIT, 0x3097, 0x69},
	{IMX_8BIT, 0x3109, 0x41},
	{IMX_8BIT, 0x3148, 0x3F},
	{IMX_8BIT, 0x330F, 0x07},
	/* csi & inck */
	{IMX_8BIT, 0x3364, 0x00},
	{IMX_8BIT, 0x3368, 0x13},
	{IMX_8BIT, 0x3369, 0x33},
	/* znr */
	{IMX_8BIT, 0x4100, 0x0E},
	{IMX_8BIT, 0x4104, 0x32},
	{IMX_8BIT, 0x4105, 0x32},
	{IMX_8BIT, 0x4108, 0x01},
	{IMX_8BIT, 0x4109, 0x7C},
	{IMX_8BIT, 0x410A, 0x00},
	{IMX_8BIT, 0x410B, 0x00},
	GROUPED_PARAMETER_HOLD_DISABLE,
	{IMX_TOK_TERM, 0, 0}
};
/* TODO settings of preview/still/video will be updated with new use case */
struct imx_resolution imx_res_preview[] = {
	{
		.desc = "VGA_strong_dvs_30fps",
		.regs = imx_VGA_strong_dvs_30fps,
		.width = 820	,
		.height = 616	,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 4,
		.bin_factor_y = 4,
		.used = 0	,
	},
	{
		.desc = "WIDE_PREVIEW_30fps",
		.regs = imx_WIDE_PREVIEW_30fps,
		.width = 1640	,
		.height = 956	,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0	,
	},
};

struct imx_resolution imx_res_still[] = {

	{
		.desc = "STILL_2M_15fps",
		.regs = imx_STILL_2M_15fps,
		.width = 1640,
		.height = 1232,
		.fps = 15,
		.pixels_per_line = 0x0A90, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
	},
	{
		.desc = "STILL_6M_15fps",
		.regs = imx_STILL_6M_15fps,
		.width = 3280,
		.height =	1852,
		.fps = 15,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0A42, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
	},
	{
		.desc = "STILL_8M_15fps",
		.regs = imx_STILL_8M_15fps,
		.width = 3280,
		.height = 2464,
		.fps = 15,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0A90, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
	},
};

struct imx_resolution imx_res_video[] = {
	{
		.desc = "QCIF_strong_dvs_30fps",
		.regs = imx_QCIF_strong_dvs_30fps,
		.width = 216,
		.height = 176,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 4,
		.bin_factor_y = 4,
		.used = 0,
	},
	{
		.desc =	"QVGA_strong_dvs_30fps",
		.regs = imx_QVGA_strong_dvs_30fps,
		.width =	408	,
		.height =	308	,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x =	4,
		.bin_factor_y =	4,
		.used = 0,
	},
	{
		.desc = "VGA_strong_dvs_30fps"	,
		.regs = imx_VGA_strong_dvs_30fps,
		.width = 820	,
		.height = 616	,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 4,
		.bin_factor_y = 4,
		.used = 0,
	},
	{
		.desc = "720p_strong_dvs_30fps",
		.regs = imx_720p_strong_dvs_30fps,
		.width = 1568,
		.height =	876,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
	},
	{
		.desc = "WVGA_strong_dvs_30fps",
		.regs = imx_WVGA_strong_dvs_30fps,
		.width = 1640,
		.height =	1024,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x0548, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
	},
	{
		.desc = "1080p_strong_dvs_30fps",
		.regs = imx_1080p_strong_dvs_30fps,
		.width = 2336,
		.height =	1308,
		.fps = 30,
		.pixels_per_line = 0x0D70, /* consistent with regs arrays */
		.lines_per_frame = 0x05D0, /* consistent with regs arrays */
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
	},
};

#define N_RES_PREVIEW (ARRAY_SIZE(imx_res_preview))
#define N_RES_STILL (ARRAY_SIZE(imx_res_still))
#define N_RES_VIDEO (ARRAY_SIZE(imx_res_video))

struct imx_resolution *imx_res = imx_res_preview;
static int N_RES = N_RES_PREVIEW;

#endif
