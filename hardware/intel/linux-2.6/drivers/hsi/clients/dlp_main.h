/*
 * dlp_main.h
 *
 * Intel Mobile Communication modem protocol driver for DLP
 * (Data Link Protocl (LTE)). This driver is implementing a 5-channel HSI
 * protocol consisting of:
 * - An internal communication control channel;
 * - A multiplexed channel exporting a TTY interface;
 * - Three dedicated high speed channels exporting each a network interface.
 * All channels are using fixed-length pdus, although of different sizes.
 *
 * Copyright (C) 2010-2011 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _DLP_MAIN_H_
#define _DLP_MAIN_H_

#include <linux/hsi/hsi.h>
#include <linux/hsi/hsi_dlp.h>
#include <linux/tty.h>
#include <linux/netdevice.h>
#include <linux/wait.h>

#define DRVNAME				"hsi-dlp"

/* Delays for powering up/resetting the modem */
#define DLP_ON1_DURATION   60 /* ON1 pulse duration (usec) */
#define DLP_ON1_DELAY     200 /* ON1 wait duration (usec) */

#define DLP_COLD_RST_DELAY       200 /* Delay for RESET_BB_N (usec) */
#define DLP_COLD_REG_DELAY        20 /* Delay for CHIPCNTRL (msec) */

#define DLP_WARM_RST_DURATION        60 /* RESET_BB_N pulse delay (usec) */
#define DLP_WARM_RST_FLASHING_DELAY  30 /* RESET_BB_N wait duration (msec) */

#define DLP_MODEM_READY_DELAY  60 /* Modem readiness wait duration (sec) */

/* Defaut TX timeout delay (in microseconds) */
#define DLP_HANGUP_DELAY	1000000

/* Defaut HSI TX delay (in microseconds) */
#define DLP_HSI_TX_DELAY		10000

/* Defaut HSI RX delay (in microseconds) */
#define DLP_HSI_RX_DELAY		10000

/* Maximal number of pdu allocation failure prior firing an error message */
#define DLP_PDU_ALLOC_RETRY_MAX_CNT	10

/* Compute the TX and RX, FIFO depth from the buffering requirements */
/* For optimal performances the DLP_HSI_TX_CTRL_FIFO size shall be set to 2 at
 * least to allow back-to-back transfers. */
#define DLP_HSI_TX_CTRL_FIFO	2
#define DLP_HSI_RX_CTRL_FIFO	9

#define DLP_HSI_TX_WAIT_FIFO	15
#define DLP_HSI_RX_WAIT_FIFO	8

/* PDU size for TTY channel */
#define DLP_TTY_TX_PDU_SIZE		4096	/* 4 KBytes */
#define DLP_TTY_RX_PDU_SIZE		4096	/* 4 KBytes */
#define DLP_TTY_HEADER_LENGTH	16
#define DLP_TTY_PAYLOAD_LENGTH	(DLP_TTY_TX_PDU_SIZE - DLP_TTY_HEADER_LENGTH)

/* PDU size for NET channels */
#define DLP_NET_RX_PDU_SIZE	8192	/* 8 KBytes */
#define DLP_NET_TX_PDU_SIZE	6656	/* 6.5 KBytes */

/* PDU size for CTRL channel */
#define DLP_CTRL_TX_PDU_SIZE	4	/* 4 Bytes */
#define DLP_CTRL_RX_PDU_SIZE	4	/* 4 Bytes */

/* PDU size for Flashing channel */
#define DLP_FLASH_TX_PDU_SIZE	4	/* 4 Bytes */
#define DLP_FLASH_RX_PDU_SIZE	4	/* 4 Bytes */

/* PDU size for Trace channel */
#define DLP_TRACE_TX_PDU_SIZE	8192	/* 8 KBytes */
#define DLP_TRACE_RX_PDU_SIZE	8192	/* 8 KBytes */

/* Alignment params */
#define DLP_PACKET_ALIGN_AP		16
#define DLP_PACKET_ALIGN_CP		16

/* Header space params */
#define DLP_HDR_SPACE_AP		16
#define DLP_HDR_SPACE_CP		16

/* Header signature */
#define DLP_HEADER_SIGNATURE	0xF9A80000

/* header fields */
#define DLP_HDR_DATA_SIZE(sz)	((sz) & 0x3FFFF)
#define DLP_HDR_MORE_DESC		(0x1 << 31)
#define DLP_HDR_NO_MORE_DESC	(0x0 << 31)

#define DLP_HDR_MIDDLE_OF_PACKET (0x0 << 29)
#define DLP_HDR_END_OF_PACKET	 (0x1 << 29)
#define DLP_HDR_START_OF_PACKET	 (0x2 << 29)
#define DLP_HDR_COMPLETE_PACKET	 (0x3 << 29)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Debug macro */
#ifdef DEBUG
#define EDLP_TTY_RX_DATA_LEN_REPORT  (dlp_drv.debug & 0x001)
#define EDLP_TTY_RX_DATA_REPORT      (dlp_drv.debug & 0x002)
#define EDLP_TTY_TX_DATA_LEN_REPORT  (dlp_drv.debug & 0x004)
#define EDLP_TTY_TX_DATA_REPORT      (dlp_drv.debug & 0x008)

#define EDLP_NET_RX_DATA_LEN_REPORT  (dlp_drv.debug & 0x010)
#define EDLP_NET_RX_DATA_REPORT      (dlp_drv.debug & 0x020)
#define EDLP_NET_TX_DATA_LEN_REPORT  (dlp_drv.debug & 0x040)
#define EDLP_NET_TX_DATA_REPORT      (dlp_drv.debug & 0x080)

#define EDLP_CTRL_RX_DATA_REPORT     (dlp_drv.debug & 0x100)
#define EDLP_CTRL_TX_DATA_REPORT     (dlp_drv.debug & 0x200)
#else
#define EDLP_TTY_RX_DATA_LEN_REPORT  0
#define EDLP_TTY_RX_DATA_REPORT      0
#define EDLP_TTY_TX_DATA_LEN_REPORT  0
#define EDLP_TTY_TX_DATA_REPORT      0

#define EDLP_NET_RX_DATA_LEN_REPORT  0
#define EDLP_NET_RX_DATA_REPORT      0
#define EDLP_NET_TX_DATA_LEN_REPORT  0
#define EDLP_NET_TX_DATA_REPORT      0

#define EDLP_CTRL_RX_DATA_REPORT     0
#define EDLP_CTRL_TX_DATA_REPORT     0
#endif

/*
 * Number of /dev/tty exported for IPC
 * Currently only one is used (/dev/ttyIFX0)
 */
#define DLP_TTY_DEV_NUM 1


/*
 * Get a ref to the given channel context
 */
#define DLP_CHANNEL_CTX(hsi_ch) (dlp_drv.channels[hsi_ch])

/* RX and TX state machine definitions */
enum {
	IDLE,
	READY,
	ACTIVE
};

/* DLP contexts */
enum {
	DLP_CHANNEL_CTRL,	/* HSI Channel 0 */
	DLP_CHANNEL_TTY,	/* HSI Channel 1 */
	DLP_CHANNEL_NET1,	/* HSI Channel 2 */
	DLP_CHANNEL_NET2,	/* HSI Channel 3 */
	DLP_CHANNEL_NET3,	/* HSI Channel 4 */
	DLP_CHANNEL_TRACE,	/* HSI Channel 4 */

	DLP_CHANNEL_FLASH,	/* HSI Channel 0 */
	DLP_CHANNEL_COUNT
};

/* DLP channel state */
enum {
	DLP_CH_STATE_CLOSED,	/* Default initial state (channel closed) */
	DLP_CH_STATE_CLOSING,	/* Closing ... (waiting for CLOSE_CONN resp) */
	DLP_CH_STATE_OPENING,	/* Opening ... (waiting for OPEN_CONN resp) */
	DLP_CH_STATE_OPENED,	/* Channel correctly opened (OPEN_CONN + ACK) */
	DLP_CH_STATE_NONE		/* No channel state set yet */
};

/* eDLP error code */
enum {
	EDLP_ERR_NONE,
	EDLP_ERR_CH_ALREADY_OPENED,
	EDLP_ERR_CH_ALREADY_CLOSED,
	EDLP_ERR_CH_RX_ALREADY_CLOSED,
	EDLP_ERR_CH_TX_ALREADY_CLOSED,
	EDLP_ERR_CH_OUT_OF_RANGE,
	EDLP_ERR_CH_NO_FLOW_CONTROL,
	EDLP_ERR_WRONG_PDU_SIZE,
	EDLP_ERR_RESOURCE_NOT_AVAILABLE,
	EDLP_ERR_DATA_IN_PROCESS,
	EDLP_ERR_UNKNOWN = 0xFF
};

/*  */
#define DLP_GLOBAL_STATE_SZ		2
#define DLP_GLOBAL_STATE_MASK	((1<<DLP_GLOBAL_STATE_SZ)-1)

/*
 * HSI transfer complete callback prototype
 */
typedef void (*xfer_complete_cb) (struct hsi_msg *pdu);

/*
 * HSI client start/stop RX callback prototype
 */
typedef void (*hsi_client_cb) (struct hsi_client *cl);

/**
 * struct dlp_xfer_ctx - TX/RX transfer context
 * @pdu_size: the xfer pdu size
 * @wait_pdus: head of the FIFO of TX/RX waiting pdus
 * @wait_len: current length of the TX/RX waiting pdus FIFO
 * @wait_max: maximal length of the TX/RX waiting pdus FIFO
 * @recycled_pdus: head of the FIFO of TX/RX recycled pdus
 * @ctrl_max: maximal count of outstanding TX/RX pdus in the controller
 * @ctrl_len: current count of TX/RX outstanding pdus in the controller
 * @buffered: number of bytes currently buffered in the wait FIFO
 * @room: room available in the wait FIFO with a byte granularity
 * @timer: context of the TX active timeout/RX TTY insert retry timer
 * @lock: spinlock to serialise access to the TX/RX context information
 * @delay: nb of jiffies for the TX active timeout/RX TTY insert retry timer
 * @state: current TX/RX state (global and internal one)
 * @channel: reference to the channel context
 * @ch_num: HSI channel number
 * @payload_len: the fixed (maximal) size of a pdu payload in bytes
 * @all_len: total count of TX/RX pdus (including recycled ones)
 * @config: current updated HSI configuration
 * @complete_cb: xfer complete callback
 * @ttype: xfer type (RX/TX)
 * @seq_num: The current xfer index
 * @cmd_xfer_done: Used to wait for CTRL command completion (RX/TX)
 * @tty_stats: TTY stats
 */
struct dlp_xfer_ctx {
	unsigned int pdu_size;
	struct list_head wait_pdus;
	unsigned int wait_len;
	unsigned int wait_max;

	struct list_head recycled_pdus;

	unsigned int ctrl_max;
	unsigned int ctrl_len;

	int buffered;
	int room;

	struct timer_list timer;
	rwlock_t lock;
	unsigned long delay;
	unsigned int state;

	struct dlp_channel *channel;
	unsigned int payload_len;
	unsigned int all_len;

	struct hsi_config config;

	xfer_complete_cb complete_cb;
	unsigned int ttype;

	u16 seq_num;
	struct completion cmd_xfer_done;

#ifdef CONFIG_HSI_DLP_TTY_STATS
	struct hsi_dlp_stats tty_stats;
#endif
};

/**
 * struct dlp_hangup_ctx - Hangup management context
 * @cause: Current cause of the hangup
 * @last_cause: Previous cause of the hangup
 * @timer: TX timeout timner
 */
struct dlp_hangup_ctx {
	unsigned int cause;
	unsigned int last_cause;
	struct timer_list timer;
};

/**
 * struct dlp_channel - HSI channel context
 * @client: reference to this HSI client
 * @credits: credits value (nb of pdus that can be sent to the modem)
 * @credits_lock: credits lock
 * @use_flow_ctrl: specify if the flow control (CREDITS) is enabled
 * @state: the current channel stated (Opened, Close, ...)
 * @stop_tx_w: Work for making synchronous TX start request
 * @stop_tx: Work for making synchronous TX stop request
 * @controller: reference to the controller bound to this context
 * @hsi_channel: the HSI channel number
 * @credits: the credits value
 * @tx_empty_event: TX empty check event
 * @tx: current TX context
 * @rx: current RX context
 */
struct dlp_channel {
	unsigned int hsi_channel;
	unsigned int credits;
	spinlock_t	 lock;
	unsigned int state;
	unsigned int use_flow_ctrl;
	struct work_struct start_tx_w;
	struct work_struct stop_tx_w;

	/* TX/RX contexts */
	wait_queue_head_t tx_empty_event;
	struct dlp_xfer_ctx tx;
	struct dlp_xfer_ctx rx;

	/* Hangup management */
	struct dlp_hangup_ctx hangup;

	/* Reset, TX Timeout & Coredump callbacks */
	void (*modem_coredump_cb) (struct dlp_channel *ch_ctx);
	void (*modem_reset_cb) (struct dlp_channel *ch_ctx);
	void (*modem_tx_timeout_cb) (struct dlp_channel *ch_ctx);

	/* Credits callback */
	void (*credits_available_cb)(struct dlp_channel *ch_ctx);

	/* Called to push any needed RX pdu */
	int (*push_rx_pdus) (struct dlp_channel *ch_ctx);

	/* Debug */
	void (*dump_state)(struct dlp_channel *ch_ctx, struct seq_file *m);

	/* Channel sepecific data */
	void *ch_data;
};

/**
 * struct dlp_driver - fixed pdu length protocol on HSI driver data
 * @channels: array of DLP Channel contex references
 * @is_dma_capable: a flag to check if the ctrl supports the DMA
 * @controller: a reference to the HSI controller
 * @channels: a reference to the HSI client
 * @recycle_wq: Workqueue for submitting pdu-recycling background tasks
 * @tx_hangup_wq: Workqueue for submitting tx timeout hangup background tasks
 * @modem_ready: The modem is up & running
 * @lock: Used for modem ready flag lock
 * @ipc_tx_cfg: HSI client configuration (Used for IPC TX)
 * @ipc_xx_cfg: HSI client configuration (Used for IPC RX)
 * @flash_tx_cfg: HSI client configuration (Used for Boot/Flashing TX)
 * @flash_rx_cfg: HSI client configuration (Used for Boot/Flashing RX)
 * @start_rx_cb: HSI client start RX callback
 * @stop_rx_cb: HSI client stop RX callback
 * @debug: Dynamic debug variable
 * @debug_dir: Debugfs directy entry (for debugging)
 */
struct dlp_driver {
	struct dlp_channel *channels[DLP_CHANNEL_COUNT];

	unsigned int is_dma_capable;
	struct hsi_client *client;
	struct device *controller;

	/* Workqueue for tty buffer forwarding */
	struct workqueue_struct *rx_wq;
	struct workqueue_struct *tx_wq;
	struct workqueue_struct *hangup_wq;

	/* Modem readiness */
	int modem_ready;
	spinlock_t lock;

	/* RX start/stop callbacks */
	hsi_client_cb start_rx_cb;
	hsi_client_cb stop_rx_cb;

	/* Modem boot/flashing */
	struct hsi_config ipc_tx_cfg;
	struct hsi_config ipc_rx_cfg;

	struct hsi_config flash_tx_cfg;
	struct hsi_config flash_rx_cfg;

	int flow_ctrl;

#ifdef DEBUG
	/* Debug purpose */
	long debug;
	struct dentry *debug_dir;
#endif
};

/*
 * Context alloc/free function proptype
 */
typedef struct dlp_channel *(*dlp_context_create) (unsigned int index,
						   struct device *dev);

typedef int (*dlp_context_delete) (struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * IPC/FLASH switching
 *
 ***************************************************************************/
int dlp_set_flashing_mode(int on_off);

/****************************************************************************
 *
 * PDU handling
 *
 ***************************************************************************/
int dlp_allocate_pdus_pool(struct dlp_channel *ch_ctx,
						struct dlp_xfer_ctx *xfer_ctx);

void *dlp_buffer_alloc(unsigned int buff_size, dma_addr_t *dma_addr);

void dlp_buffer_free(void *buff, dma_addr_t dma_addr, unsigned int buff_size);

struct hsi_msg *dlp_pdu_alloc(unsigned int hsi_channel,
			      int ttype,
			      int buffer_size,
			      int nb_entries,
			      void *user_data,
			      xfer_complete_cb complete_cb,
			      xfer_complete_cb destruct_cb);

void dlp_pdu_free(struct hsi_msg *pdu, int hsi_channel);

void dlp_pdu_delete(struct dlp_xfer_ctx *xfer_ctx, struct hsi_msg *pdu);

void dlp_pdu_recycle(struct dlp_xfer_ctx *xfer_ctx, struct hsi_msg *pdu);

void dlp_pdu_update(struct dlp_channel *ch_ctx, struct hsi_msg *pdu);

inline void dlp_pdu_reset(struct dlp_xfer_ctx *xfer_ctx,
			  struct hsi_msg *pdu, unsigned int length);

int dlp_pdu_header_check(struct dlp_xfer_ctx *xfer_ctx,
		struct hsi_msg *pdu);

inline void dlp_pdu_set_length(struct hsi_msg *pdu, u32 sz);

unsigned int dlp_pdu_get_offset(struct hsi_msg *pdu);

inline unsigned int dlp_pdu_room_in(struct hsi_msg *pdu);

inline __attribute_const__
unsigned char *dlp_pdu_data_ptr(struct hsi_msg *pdu, unsigned int offset);

/****************************************************************************
 *
 * State handling
 *
 ***************************************************************************/
void dlp_dump_channel_state(struct dlp_channel *ch_ctx, struct seq_file *m);

inline __must_check
unsigned int dlp_ctx_get_state(struct dlp_xfer_ctx *xfer_ctx);

inline void dlp_ctx_set_state(struct dlp_xfer_ctx *xfer_ctx,
			      unsigned int state);

inline __must_check int dlp_ctx_has_flag(struct dlp_xfer_ctx *xfer_ctx,
					 unsigned int flag);

inline void dlp_ctx_set_flag(struct dlp_xfer_ctx *xfer_ctx, unsigned int flag);

inline void dlp_ctx_clear_flag(struct dlp_xfer_ctx *xfer_ctx,
			       unsigned int flag);

inline int dlp_ctx_is_empty(struct dlp_xfer_ctx *xfer_ctx);

int dlp_ctx_have_credits(struct dlp_xfer_ctx *xfer_ctx,
			 struct dlp_channel *ch_ctx);

int dlp_ctx_is_empty_safe(struct dlp_xfer_ctx *xfer_ctx);

void dlp_ctx_update_status(struct dlp_xfer_ctx *xfer_ctx);

/****************************************************************************
 *
 * Generic FIFO handling
 *
 ***************************************************************************/
inline __must_check struct hsi_msg *dlp_fifo_tail(struct list_head *fifo);

inline void _dlp_fifo_pdu_push(struct hsi_msg *pdu, struct list_head *fifo);

inline void _dlp_fifo_pdu_push_back(struct hsi_msg *pdu,
				    struct list_head *fifo);

/****************************************************************************
 *
 * Wait FIFO handling
 *
 ***************************************************************************/
struct hsi_msg *dlp_fifo_wait_pop(struct dlp_xfer_ctx *xfer_ctx);

inline void dlp_fifo_wait_push(struct dlp_xfer_ctx *xfer_ctx,
			       struct hsi_msg *pdu);

inline void dlp_fifo_wait_push_back(struct dlp_xfer_ctx *xfer_ctx,
				    struct hsi_msg *pdu);

void dlp_pop_wait_push_ctrl(struct dlp_xfer_ctx *xfer_ctx);

/****************************************************************************
 *
 * Frame recycling handling
 *
 ***************************************************************************/
inline struct hsi_msg *dlp_fifo_recycled_pop(struct dlp_xfer_ctx *xfer_ctx);

__must_check int dlp_pop_recycled_push_ctrl(struct dlp_xfer_ctx *xfer_ctx);

/****************************************************************************
 *
 * HSI Controller
 *
 ***************************************************************************/
int dlp_push_rx_pdus(void);

inline void dlp_hsi_controller_pop(struct dlp_xfer_ctx *xfer_ctx);

int dlp_hsi_controller_push(struct dlp_xfer_ctx *xfer_ctx,
		struct hsi_msg *pdu);

void dlp_do_start_tx(struct work_struct *work);
void dlp_do_stop_tx(struct work_struct *work);

void dlp_start_tx(struct dlp_xfer_ctx *xfer_ctx);
void dlp_stop_tx(struct dlp_xfer_ctx *xfer_ctx);

inline void dlp_stop_rx(struct dlp_xfer_ctx *xfer_ctx,
			struct dlp_channel *ch_ctx);

int dlp_hsi_port_claim(void);

inline void dlp_hsi_port_unclaim(void);

void dlp_save_rx_callbacks(hsi_client_cb *start_rx_cb,
		hsi_client_cb *stop_rx_cb);

void dlp_restore_rx_callbacks(hsi_client_cb *start_rx_cb,
		hsi_client_cb *stop_rx_cb);

/****************************************************************************
 *
 * RX/TX xfer contexts
 *
 ***************************************************************************/
void dlp_xfer_ctx_init(struct dlp_channel *ch_ctx,
		       unsigned int pdu_size,
		       unsigned int delay,
		       unsigned int wait_max,
		       unsigned int ctrl_max,
		       xfer_complete_cb complete_cb, unsigned int ttype);

void dlp_xfer_ctx_clear(struct dlp_xfer_ctx *xfer_ctx);

/****************************************************************************
 *
 * Time handling
 *
 ***************************************************************************/
inline unsigned long from_usecs(const unsigned long delay);

/****************************************************************************
 *
 * CONTROL channel exported functions
 *
 ***************************************************************************/
struct dlp_channel *dlp_ctrl_ctx_create(unsigned int index,
		struct device *dev);

int dlp_ctrl_ctx_delete(struct dlp_channel *ch_ctx);

int dlp_ctrl_cold_boot(struct dlp_channel *ch_ctx);

int dlp_ctrl_cold_reset(struct dlp_channel *ch_ctx);

int dlp_ctrl_normal_warm_reset(struct dlp_channel *ch_ctx);

int dlp_ctrl_flashing_warm_reset(struct dlp_channel *ch_ctx);

inline int dlp_ctrl_get_reset_ongoing(void);

inline void dlp_ctrl_set_reset_ongoing(int ongoing);

inline int dlp_ctrl_get_hangup_reasons(void);

inline void dlp_ctrl_set_hangup_reasons(unsigned int hsi_channel, int reason);

inline unsigned int dlp_ctrl_modem_is_ready(void);

int dlp_ctrl_open_channel(struct dlp_channel *ch_ctx);

int dlp_ctrl_close_channel(struct dlp_channel *ch_ctx);

int dlp_ctrl_send_nop(struct dlp_channel *ch_ctx);

inline void
dlp_ctrl_set_channel_state(struct dlp_channel *, unsigned char);

inline unsigned char
dlp_ctrl_get_channel_state(struct dlp_channel *ch_ctx);

void dlp_ctrl_hangup_ctx_init(struct dlp_channel *ch_ctx,
		void (*timeout_func)(struct dlp_channel *ch_ctx));

void dlp_ctrl_hangup_ctx_deinit(struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * TTY channel exported functions
 *
 ***************************************************************************/
struct dlp_channel *dlp_tty_ctx_create(unsigned int index, struct device *dev);

int dlp_tty_ctx_delete(struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * NETWORK channels exported functions
 *
 ***************************************************************************/
struct dlp_channel *dlp_net_ctx_create(unsigned int index, struct device *dev);

int dlp_net_ctx_delete(struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * Boot/Flashing channel exported functions
 *
 ***************************************************************************/
struct dlp_channel *dlp_flash_ctx_create(unsigned int index,
		struct device *dev);

int dlp_flash_ctx_delete(struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * Modem Trace channel exported functions
 *
 ***************************************************************************/
struct dlp_channel *dlp_trace_ctx_create(unsigned int index,
		struct device *dev);

int dlp_trace_ctx_delete(struct dlp_channel *ch_ctx);

/****************************************************************************
 *
 * Global variables
 *
 ***************************************************************************/
extern struct dlp_driver dlp_drv;

#ifdef CONFIG_ATOM_SOC_POWER
extern unsigned int enable_standby;
#endif

#endif /* _DLP_MAIN_H_ */
