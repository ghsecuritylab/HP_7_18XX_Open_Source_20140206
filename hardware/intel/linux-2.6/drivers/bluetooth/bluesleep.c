/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28  Motorola         The kernel module for running the Bluetooth(R)
                                 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.
   2009-Aug-10  Motorola         Changed "add_timer" to "mod_timer" to solve
                                 race when flurry of queued work comes in.
*/

#include <linux/module.h>       /* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/delay.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
//#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/serial_core.h>
//#include <mach/serial.h>
//#include <mach/msm_serial_hs.h>
//#include <linux/serial_mfd.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"

//add by lwl 
#include <linux/pm_runtime.h>
#include <linux/serial_mfd.h>
#include <asm/intel_mid_hsu.h>
#include <linux/mfd.h>
//end

/*lgh here we configure BT_WAKE and HOST_WAKE to active_low, you should match setting in brcm_patchram_plus.c*/

#define BLUESLEEP_DBG 1

#ifdef BLUESLEEP_DBG
#define BT_SLEEP_DBG(fmt, arg...)  printk(KERN_ERR "%s: " fmt "\n" , __func__ , ## arg)
#else
#define BT_SLEEP_DBG(fmt, arg...) 
#endif

/*
 * Defines
 */

#define VERSION         "1.1"
#define PROC_DIR        "bluetooth/sleep"

//extern unsigned int serial_hsu_tx_empty(struct uart_port *port);
//extern void serial_hsu_set_mctrl(struct uart_port *port, unsigned int mctrl);
struct bluesleep_info {
        unsigned host_wake;
        unsigned ext_wake;
        unsigned host_wake_irq;
        struct uart_port *uport;
        struct wake_lock wake_lock;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);
//add by lwl
static void start_work(struct work_struct *work);
static void stop_work(struct work_struct *work);
//add ene

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);
//add by lwl
DECLARE_DELAYED_WORK(start_workqueue, start_work);
DECLARE_DELAYED_WORK(stop_workqueue, stop_work);
//add end

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
//add by lwl 
#define bluesleep_start_work()	 schedule_delayed_work(&start_workqueue, 0)
#define bluesleep_stop_work()	schedule_delayed_work(&stop_workqueue, 0)
//add end

/* 1 second timeout */
#define TX_TIMER_INTERVAL      10 

/* state variable names and bit positions */
#define BT_PROTO        0x01
#define BT_TXDATA       0x02
#define BT_ASLEEP       0x04

/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */

static int bluesleep_hci_event(struct notifier_block *this,
                            unsigned long event, void *data);
static int bluesleep_start(void);
static void bluesleep_stop(void);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Workqueue to respond to change in hostwake line */
struct work_struct hostwake_work;
struct workqueue_struct *hostwake_work_queue;

/** Transmission timer */
static void bluesleep_tx_timer_expire(unsigned long data);
static DEFINE_TIMER(tx_timer, bluesleep_tx_timer_expire, 0, 0);

/** Lock for state transitions */
static spinlock_t rw_lock;

/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
        .notifier_call = bluesleep_hci_event,
};

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */
 

static void hsuart_power(int on)
{
	if (on) {
		//msm_hs_request_clock_on(bsi->uport);
		//serial_hsu_set_mctrl(bsi->uport, TIOCM_RTS);
		bsi->uport->ops->set_mctrl(bsi->uport, TIOCM_RTS);
	} else {
		//serial_hsu_set_mctrl(bsi->uport, 0);
		bsi->uport->ops->set_mctrl(bsi->uport, 0);
		//msm_hs_request_clock_off(bsi->uport);
	}
}


/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
int bluesleep_can_sleep(void)
{
        /* check if WAKE_BT_GPIO and BT_WAKE_GPIO are both deasserted */
		//add by lwl for debug
		printk("bluesleep_can_sleep come in\n");
		if(gpio_get_value(bsi->ext_wake))
			printk("bluesleep_can_sleep come in***** bsi->ext_wake value ==1\n");
		if(gpio_get_value(bsi->host_wake))
			printk("bluesleep_can_sleep come in***** bsi->host_wake value ==1\n");
		if(bsi->uport != NULL)
			printk("bluesleep_can_sleep come in***** bsi->uport != NULL\n");
		//add end
        return gpio_get_value(bsi->ext_wake) &&
                gpio_get_value(bsi->host_wake) &&
                (bsi->uport != NULL);
}

void bluesleep_sleep_wakeup(void)
{
        if (test_bit(BT_ASLEEP, &flags)) {
                BT_SLEEP_DBG("waking up...");
                wake_lock(&bsi->wake_lock);
                /* Start the timer */
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
                gpio_set_value(bsi->ext_wake, 0);
                clear_bit(BT_ASLEEP, &flags);
                /*Activating UART */
                hsuart_power(1);
        }
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
        if (bluesleep_can_sleep()) {
                /* already asleep, this is an error case */
                if (test_bit(BT_ASLEEP, &flags)) {
                        BT_SLEEP_DBG("already asleep");
                        return;
                }

                //if (serial_hsu_tx_empty(bsi->uport)) {//lwl modify
				if (bsi->uport->ops->tx_empty(bsi->uport)) {
                        BT_SLEEP_DBG("going to sleep...");
                        set_bit(BT_ASLEEP, &flags);
                        /*Deactivating UART */
                        hsuart_power(0);
                        /* UART clk is not turned off immediately. Release
                         * wakelock after 500 ms.
                         */
                        wake_lock_timeout(&bsi->wake_lock, HZ * 2);
                } else {
                        mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
                        return;
                }
        }else if (gpio_get_value(bsi->ext_wake) && !test_bit(BT_ASLEEP, &flags)) {
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
                gpio_set_value(bsi->ext_wake, 0);
                BT_SLEEP_DBG("can not sleep now, mod_timer again...");
        }else {
                bluesleep_sleep_wakeup();
                BT_SLEEP_DBG("can not sleep now, wake up...");
        }
}

/**
 * A workqueue that runs in workqueue context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param work Not used.
 */
static void bluesleep_hostwake_work(struct work_struct *work)
{
        unsigned long irq_flags;

        BT_SLEEP_DBG("hostwake line change");

	spin_lock_irqsave(&rw_lock, irq_flags);

        if (gpio_get_value(bsi->host_wake))
                bluesleep_rx_busy();
        else
                bluesleep_rx_idle();

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
void bluesleep_outgoing_data(void)
{
        unsigned long irq_flags;
        int f = 0;

        spin_lock_irqsave(&rw_lock, irq_flags);

        /* log data passing by */
        set_bit(BT_TXDATA, &flags);

        /* if the tx side is sleeping... */
        if (gpio_get_value(bsi->ext_wake)) {
                BT_SLEEP_DBG("tx was sleeping");
                bluesleep_sleep_wakeup();
                f = 1;
        }

        spin_unlock_irqrestore(&rw_lock, irq_flags);
        if (f) {
            mdelay(10); //delay 10 mseconds
        }
}

void bluesleep_uart_open(struct uart_port *uport)
{
    if(bsi->uport == NULL) {
        bsi->uport = uport;
    }
}

void bluesleep_uart_close(struct uart_port *uport)
{
    if(bsi->uport == uport) {
        bsi->uport = NULL;
    }
}

/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
                                unsigned long event, void *data)
{
        struct hci_dev *hdev = (struct hci_dev *) data;
        struct hci_uart *hu;
        struct uart_state *state;

        if (!hdev)
                return NOTIFY_DONE;

        switch (event) {
        case HCI_DEV_REG:
                if (!bluesleep_hdev) {
                        bluesleep_hdev = hdev;
                        hu  = (struct hci_uart *) hdev->driver_data;
                        state = (struct uart_state *) hu->tty->driver_data;
                        bsi->uport = state->uart_port;
                        /* if bluetooth started with a new device, start bluesleep also.*/
                      //  bluesleep_start();
					  BT_SLEEP_DBG("bluesleep start work");
						bluesleep_start_work();
                }
                break;
        case HCI_DEV_UNREG:
                /* if bluetooth stopped, stop bluesleep also */
                //bluesleep_stop();
				bluesleep_stop_work();

                //bluesleep_hdev = NULL;
                //bsi->uport = NULL;
                break;
        case HCI_DEV_WRITE:
                bluesleep_outgoing_data();
                break;
        }

        return NOTIFY_DONE;
}

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
        unsigned long irq_flags;

        spin_lock_irqsave(&rw_lock, irq_flags);

        BT_SLEEP_DBG("Tx timer expired");

        /* were we silent during the last timeout? */
        if (!test_bit(BT_TXDATA, &flags)) {
                BT_SLEEP_DBG("Tx has been idle");
                gpio_set_value(bsi->ext_wake, 1);
                bluesleep_tx_idle();
        } else {
                BT_SLEEP_DBG("Tx data during last period");
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
        }

        /* clear the incoming data flag */
        clear_bit(BT_TXDATA, &flags);

        spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a workqueue to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
        //nvUART gpio_clear_detect_status(bsi->host_wake_irq);

	/* schedule a workqueue to handle the change in the host wake line */
        queue_work(hostwake_work_queue, &hostwake_work);
        return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
        int retval;
        unsigned long irq_flags;

        spin_lock_irqsave(&rw_lock, irq_flags);

        if (test_bit(BT_PROTO, &flags)) {
                spin_unlock_irqrestore(&rw_lock, irq_flags);
                return 0;
        }

        spin_unlock_irqrestore(&rw_lock, irq_flags);

        if (!atomic_dec_and_test(&open_count)) {
                atomic_inc(&open_count);
                return -EBUSY;
        }
		
//lwl modify for uart pm_runtime controls
	struct uart_port *port = bsi->uport;
	struct uart_hsu_port *up = container_of(port, struct uart_hsu_port, port);
	pm_runtime_get(up->dev);
	BT_SLEEP_DBG("langwenlong pm_runtime get");
//end 

        /* start the timer */
        mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));

        /* assert BT_WAKE */
        gpio_set_value(bsi->ext_wake, 0);
/* Tony 20110506 : Modify to PM8058 irq, use rising and falling trigge. [START] */
#if 0
//Tony, old for arm gpio
	retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				"bluetooth hostwake", NULL);
#endif
	retval = request_threaded_irq(bsi->host_wake_irq, NULL,
				  bluesleep_hostwake_isr,
				 IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				 "bluetooth hostwake",
				  NULL);
/* Tony 20110506 : Modify to PM8058 irq, use rising and falling trigge. [END] */
        if (retval  < 0) {
                BT_SLEEP_DBG("Couldn't acquire BT_HOST_WAKE IRQ");
                goto fail;
        }

        retval = enable_irq_wake(bsi->host_wake_irq);
        if (retval < 0) {
                BT_SLEEP_DBG("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
                free_irq(bsi->host_wake_irq, NULL);
                goto fail;
        }

        set_bit(BT_PROTO, &flags);
        wake_lock(&bsi->wake_lock);
        return 0;
fail:
        del_timer(&tx_timer);
        atomic_inc(&open_count);

        return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
        unsigned long irq_flags;

        spin_lock_irqsave(&rw_lock, irq_flags);

        if (!test_bit(BT_PROTO, &flags)) {
                spin_unlock_irqrestore(&rw_lock, irq_flags);
                return;
        }
// lwl modify for uart pm_runtime controls
	struct uart_port *port = bsi->uport;
	struct uart_hsu_port *up = container_of(port, struct uart_hsu_port, port);
	pm_runtime_put(up->dev);
	BT_SLEEP_DBG("langwenlong pm_runtime put");
//end
        /* assert BT_WAKE */
        gpio_set_value(bsi->ext_wake, 0);
        del_timer(&tx_timer);
        clear_bit(BT_PROTO, &flags);

        if (test_bit(BT_ASLEEP, &flags)) {
                clear_bit(BT_ASLEEP, &flags);
                hsuart_power(1);
        }

        atomic_inc(&open_count);

        spin_unlock_irqrestore(&rw_lock, irq_flags);
        if (disable_irq_wake(bsi->host_wake_irq))
                BT_SLEEP_DBG("Couldn't disable hostwake IRQ wakeup mode\n");
        free_irq(bsi->host_wake_irq, NULL);
        wake_lock_timeout(&bsi->wake_lock, HZ * 2);
		bluesleep_hdev = NULL;
        bsi->uport = NULL;
}

//add by lwl 
static void start_work(struct work_struct *work)
{
	bluesleep_start();
}

static void stop_work(struct work_struct *work)
{
	bluesleep_stop();
}

 //add by lwl for bt sleep
 
 #ifdef	CONFIG_PM
static int bluesleep_suspend(struct device *dev)
{
	if ( bsi->uport != NULL) {
		struct uart_port *port = bsi->uport;
		struct uart_hsu_port *up = container_of(port, struct uart_hsu_port, port);
//		pm_runtime_put(up->dev);
		BT_SLEEP_DBG("langwenlong bluesleep_suspend pm_runtime put");
	}
	return 0;
}

static int bluesleep_resume(struct device *dev)
{
	if ( bsi->uport != NULL) {
		struct uart_port *port = bsi->uport;
		struct uart_hsu_port *up = container_of(port, struct uart_hsu_port, port);
		pm_runtime_get(up->dev);
		BT_SLEEP_DBG("langwenlong bluesleep_suspend pm_runtime get");
	}
	return 0;
}
 
 static const struct dev_pm_ops bluesleep_pm_ops = {
	.suspend	= bluesleep_suspend,
	.resume		= bluesleep_resume,
};
 #endif

 //add end

//add end
/**
 * Read the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_btwake(char *page, char **start, off_t offset,
                                              int count, int *eof, void *data)
{
        *eof = 1;
        return sprintf(page, "btwake:%u\n", gpio_get_value(bsi->ext_wake));
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluepower_write_proc_btwake(struct file *file, const char *buffer,
                                            unsigned long count, void *data)
{
        char *buf;

        if (count < 1)
              return -EINVAL;

        buf = kmalloc(count, GFP_KERNEL);
        if (!buf)
              return -ENOMEM;

        if (copy_from_user(buf, buffer, count)) {
              kfree(buf);
              return -EFAULT;
        }

        if (buf[0] == '0') {
              gpio_set_value(bsi->ext_wake, 0);
        } else if (buf[0] == '1') {
              gpio_set_value(bsi->ext_wake, 1);
        } else {
              kfree(buf);
              return -EINVAL;
        }

        kfree(buf);

        return count;
}

/**
 * Read the <code>BT_HOST_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the pin
 * is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_hostwake(char *page, char **start, off_t offset,
                                                int count, int *eof, void *data)
{
        *eof = 1;
        return sprintf(page, "hostwake: %u \n", gpio_get_value(bsi->host_wake));
}


/**
 * Read the low-power status of the Host via the proc interface.
 * When this function returns, <code>page</code> contains a 1 if the Host
 * is asleep, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_asleep(char *page, char **start, off_t offset,
                                        int count, int *eof, void *data)
{
        unsigned int asleep;

        asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;
        *eof = 1;
        return sprintf(page, "asleep: %u\n", asleep);
}

/**
 * Read the low-power protocol being used by the Host via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the Host
 * is using the Sleep Mode Protocol, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_proto(char *page, char **start, off_t offset,
                                        int count, int *eof, void *data)
{
        unsigned int proto;

        proto = test_bit(BT_PROTO, &flags) ? 1 : 0;
        *eof = 1;
        return sprintf(page, "proto: %u\n", proto);
}

/**
 * Modify the low-power protocol used by the Host via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluesleep_write_proc_proto(struct file *file, const char *buffer,
                                        unsigned long count, void *data)
{
        char proto;

        if (count < 1)
                return -EINVAL;

        if (copy_from_user(&proto, buffer, 1))
                return -EFAULT;

        if (proto == '0')
                bluesleep_stop();
        else
                bluesleep_start();

        /* claim that we wrote everything */
        return count;
}


static int bluesleep_probe(struct platform_device *pdev)
{
        int ret;
        struct resource *res;

        bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
        if (!bsi)
                return -ENOMEM;

        res = platform_get_resource_byname(pdev, IORESOURCE_IO,
                                "gpio_host_wake");
        if (!res) {
                BT_SLEEP_DBG("couldn't find host_wake gpio\n");
                ret = -ENODEV;
                goto free_bsi;
        }
        bsi->host_wake = res->start;

        ret = gpio_request(bsi->host_wake, "bt_host_wake");
        if (ret)
                goto free_bsi;

        // configure host_wake as input
        ret = gpio_direction_input(bsi->host_wake);
        if (ret < 0) {
                pr_err("gpio-keys: failed to configure input"
                            " direction for GPIO %d, error %d\n",
                              bsi->host_wake, ret);
                gpio_free(bsi->host_wake);
                goto free_bsi;
        }

        res = platform_get_resource_byname(pdev, IORESOURCE_IO,
                                "gpio_ext_wake");
        if (!res) {
                BT_SLEEP_DBG("couldn't find ext_wake gpio\n");
                ret = -ENODEV;
                goto free_bt_host_wake;
        }
        bsi->ext_wake = res->start;

        ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
        if (ret)
                goto free_bt_host_wake;

        // configure ext_wake as output mode
        ret = gpio_direction_output(bsi->ext_wake, 1);
        if (ret < 0) {
                pr_err("gpio-keys: failed to configure output"
                            " direction for GPIO %d, error %d\n",
                              bsi->ext_wake, ret);
                gpio_free(bsi->ext_wake);
                goto free_bt_host_wake;
        }

        bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);
        if (bsi->host_wake_irq < 0) {
                BT_SLEEP_DBG("couldn't find host_wake irq\n");
                ret = -ENODEV;
                goto free_bt_ext_wake;
        }
        wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");

        return 0;

free_bt_ext_wake:
        gpio_free(bsi->ext_wake);
free_bt_host_wake:
        gpio_free(bsi->host_wake);
free_bsi:
        kfree(bsi);
        return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
        gpio_free(bsi->host_wake);
        gpio_free(bsi->ext_wake);
        wake_lock_destroy(&bsi->wake_lock);
        kfree(bsi);
        return 0;
}

static struct platform_driver bluesleep_driver = {
        .probe = bluesleep_probe,
        .remove = bluesleep_remove,
        .driver = {
                .name = "bluesleep",
                .owner = THIS_MODULE,
//add by lwl for bt sleep problem
#ifdef CONFIG_PM
//		.pm	= &bluesleep_pm_ops,
#endif
//add end
        },
};
/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
        int retval;
        struct proc_dir_entry *ent;
	    char name[64];
        BT_INFO("BlueSleep Mode Driver Ver %s", VERSION);

        retval = platform_driver_register(&bluesleep_driver);
        if (retval)
                return retval;

        bluesleep_hdev = NULL;

        bluetooth_dir = proc_mkdir("bluetooth", NULL);
        if (bluetooth_dir == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/bluetooth directory");
                return -ENOMEM;
        }

        sleep_dir = proc_mkdir("sleep", bluetooth_dir);
        if (sleep_dir == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s directory", PROC_DIR);
                return -ENOMEM;
        }

        /* Creating read/write "btwake" entry */
        ent = create_proc_entry("btwake", 0, sleep_dir);
        if (ent == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s/btwake entry", PROC_DIR);
                retval = -ENOMEM;
                goto fail;
        }
        ent->read_proc = bluepower_read_proc_btwake;
        ent->write_proc = bluepower_write_proc_btwake;

        /* read only proc entries */
        if (create_proc_read_entry("hostwake", 0, sleep_dir,
                                bluepower_read_proc_hostwake, NULL) == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s/hostwake entry", PROC_DIR);
                retval = -ENOMEM;
                goto fail;
        }

        /* read/write proc entries */
        ent = create_proc_entry("proto", 0, sleep_dir);
        if (ent == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s/proto entry", PROC_DIR);
                retval = -ENOMEM;
                goto fail;
        }
        ent->read_proc = bluesleep_read_proc_proto;
        ent->write_proc = bluesleep_write_proc_proto;

        /* read only proc entries */
        if (create_proc_read_entry("asleep", 0,
                        sleep_dir, bluesleep_read_proc_asleep, NULL) == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s/asleep entry", PROC_DIR);
                retval = -ENOMEM;
                goto fail;
        }

        flags = 0; /* clear all status bits */

        /* Initialize spinlock. */
        spin_lock_init(&rw_lock);

        /* Initialize timer */
        init_timer(&tx_timer);
        tx_timer.function = bluesleep_tx_timer_expire;
        tx_timer.data = 0;

	/* create the workqueue for the hostwake */
	snprintf(name, sizeof(name), "blue_sleep");
	hostwake_work_queue = create_singlethread_workqueue(name);
	if (hostwake_work_queue == NULL) {
		BT_SLEEP_DBG("Unable to create workqueue ");
		retval = -ENODEV;
		goto fail;
	}
	
	/* Initialise the work */
	INIT_WORK(&hostwake_work, bluesleep_hostwake_work);

        /* assert bt wake */
        gpio_set_value(bsi->ext_wake, 0);
        hci_register_notifier(&hci_event_nblock);

        return 0;

fail:
        remove_proc_entry("asleep", sleep_dir);
        remove_proc_entry("proto", sleep_dir);
        remove_proc_entry("hostwake", sleep_dir);
        remove_proc_entry("btwake", sleep_dir);
        remove_proc_entry("sleep", bluetooth_dir);
        remove_proc_entry("bluetooth", 0);
        return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
        /* assert bt wake */
        gpio_set_value(bsi->ext_wake, 0);
        if (test_bit(BT_PROTO, &flags)) {
                if (disable_irq_wake(bsi->host_wake_irq))
                        BT_SLEEP_DBG("Couldn't disable hostwake IRQ wakeup mode \n");
                free_irq(bsi->host_wake_irq, NULL);
                del_timer(&tx_timer);
                if (test_bit(BT_ASLEEP, &flags))
                        hsuart_power(1);
        }

        hci_unregister_notifier(&hci_event_nblock);
        platform_driver_unregister(&bluesleep_driver);

        remove_proc_entry("asleep", sleep_dir);
        remove_proc_entry("proto", sleep_dir);
        remove_proc_entry("hostwake", sleep_dir);
        remove_proc_entry("btwake", sleep_dir);
        remove_proc_entry("sleep", bluetooth_dir);
        remove_proc_entry("bluetooth", 0);
	destroy_workqueue(hostwake_work_queue);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

