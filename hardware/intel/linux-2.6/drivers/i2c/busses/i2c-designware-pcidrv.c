/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 * Copyright (C) 2011 Intel corporation.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/semaphore.h>
#include "i2c-designware-core.h"

#define DRIVER_NAME "i2c-designware-pci"

enum dw_pci_ctl_id_t {
	moorestown_0,
	moorestown_1,
	moorestown_2,

	medfield_0,
	medfield_1,
	medfield_2,
	medfield_3,
	medfield_4,
	medfield_5,

	cloverview_0,
	cloverview_1,
	cloverview_2,
	cloverview_3,
	cloverview_4,
	cloverview_5,

	merrifield_0,
	merrifield_1,
	merrifield_2,
	merrifield_3,
	merrifield_4,
	merrifield_5,
	merrifield_6,
};

struct dw_pci_controller {
	u32 bus_num;
	u32 bus_cfg;
	u32 tx_fifo_depth;
	u32 rx_fifo_depth;
	u32 clk_khz;
	int enable_stop;
	int (*scl_cfg) (struct dw_i2c_dev *dev);
};

#define INTEL_MID_STD_CFG  (DW_IC_CON_MASTER |			\
				DW_IC_CON_SLAVE_DISABLE |	\
				DW_IC_CON_RESTART_EN)

static int mfld_i2c_scl_cfg(struct dw_i2c_dev *dev)
{
	dw_writel(dev, PNW_SS_SCLK_HCNT, DW_IC_SS_SCL_HCNT);
	dw_writel(dev, PNW_SS_SCLK_LCNT, DW_IC_SS_SCL_LCNT);

	dw_writel(dev, PNW_FS_SCLK_HCNT, DW_IC_FS_SCL_HCNT);
	dw_writel(dev, PNW_FS_SCLK_LCNT, DW_IC_FS_SCL_LCNT);

	return 0;
}

static int ctp_i2c_scl_cfg(struct dw_i2c_dev *dev)
{
	dw_writel(dev, CLV_SS_SCLK_HCNT, DW_IC_SS_SCL_HCNT);
	dw_writel(dev, CLV_SS_SCLK_LCNT, DW_IC_SS_SCL_LCNT);

	dw_writel(dev, CLV_FS_SCLK_HCNT, DW_IC_FS_SCL_HCNT);
	dw_writel(dev, CLV_FS_SCLK_LCNT, DW_IC_FS_SCL_LCNT);

	return 0;
}

static struct  dw_pci_controller  dw_pci_controllers[] = {
	[moorestown_0] = {
		.bus_num     = 0,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 25000,
	},
	[moorestown_1] = {
		.bus_num     = 1,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 25000,
	},
	[moorestown_2] = {
		.bus_num     = 2,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 25000,
	},
	[medfield_0] = {
		.bus_num     = 0,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 20500,
		.scl_cfg = mfld_i2c_scl_cfg,
	},
	[medfield_1] = {
		.bus_num     = 1,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 20500,
		.scl_cfg = mfld_i2c_scl_cfg,
	},
	[medfield_2] = {
		.bus_num     = 2,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = mfld_i2c_scl_cfg,
	},
	[medfield_3] = {
		.bus_num     = 3,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 20500,
		.scl_cfg = mfld_i2c_scl_cfg,
	},
	[medfield_4] = {
		.bus_num     = 4,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = mfld_i2c_scl_cfg,
	},
	[medfield_5] = {
		.bus_num     = 5,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = mfld_i2c_scl_cfg,
	},

	[cloverview_0] = {
		.bus_num     = 0,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = ctp_i2c_scl_cfg,
	},
	[cloverview_1] = {
		.bus_num     = 1,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = ctp_i2c_scl_cfg,
	},
	[cloverview_2] = {
		.bus_num     = 2,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = ctp_i2c_scl_cfg,
	},
	[cloverview_3] = {
		.bus_num     = 3,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 20500,
		.scl_cfg = ctp_i2c_scl_cfg,
	},
	[cloverview_4] = {
		.bus_num     = 4,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = ctp_i2c_scl_cfg,
	},
	[cloverview_5] = {
		.bus_num     = 5,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_FAST,
		.tx_fifo_depth = 256,
		.rx_fifo_depth = 256,
		.clk_khz      = 17000,
		.scl_cfg = ctp_i2c_scl_cfg,
	},

	[merrifield_0] = {
		.bus_num     = 1,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_1] = {
		.bus_num     = 2,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_2] = {
		.bus_num     = 3,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_3] = {
		.bus_num     = 4,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_4] = {
		.bus_num     = 5,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_5] = {
		.bus_num     = 6,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
	[merrifield_6] = {
		.bus_num     = 7,
		.bus_cfg   = INTEL_MID_STD_CFG | DW_IC_CON_SPEED_STD,
		.tx_fifo_depth = 32,
		.rx_fifo_depth = 32,
		.clk_khz      = 17000,
		.enable_stop = 1,
	},
};

static struct i2c_algorithm i2c_dw_algo = {
	.master_xfer	= i2c_dw_xfer,
	.functionality	= i2c_dw_func,
};

static int i2c_dw_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct dw_i2c_dev *i2c = pci_get_drvdata(pdev);

	dev_dbg(dev, "suspend called\n");
	if (down_trylock(&i2c->lock))
		return -EBUSY;
	i2c_dw_disable(i2c);
	i2c->status &= ~STATUS_POWERON;

	return 0;
}

static int i2c_dw_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct dw_i2c_dev *i2c = pci_get_drvdata(pdev);
	int err;

	dev_dbg(dev, "runtime suspend called\n");
	i2c_dw_disable(i2c);

	err = pci_save_state(pdev);
	if (err) {
		dev_err(&pdev->dev, "pci_save_state failed\n");
		return err;
	}

	err = pci_set_power_state(pdev, PCI_D3hot);
	if (err) {
		dev_err(&pdev->dev, "pci_set_power_state failed\n");
		return err;
	}

	return 0;
}

static int i2c_dw_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct dw_i2c_dev *i2c = pci_get_drvdata(pdev);

	dev_dbg(dev, "resume called\n");
	i2c_dw_init(i2c);
	i2c->status |= STATUS_POWERON;
	up(&i2c->lock);

	return 0;
}

static int i2c_dw_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct dw_i2c_dev *i2c = pci_get_drvdata(pdev);
	int err;

	dev_dbg(dev, "runtime resume called\n");
	err = pci_set_power_state(pdev, PCI_D0);
	if (err) {
		dev_err(&pdev->dev, "pci_set_power_state() failed\n");
		return err;
	}
	pci_restore_state(pdev);
	i2c_dw_init(i2c);

	return 0;
}

static const struct dev_pm_ops i2c_dw_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(i2c_dw_pci_suspend,
				i2c_dw_pci_resume)
	SET_RUNTIME_PM_OPS(i2c_dw_pci_runtime_suspend,
			   i2c_dw_pci_runtime_resume,
			   NULL)
};

static u32 i2c_dw_get_clk_rate_khz(struct dw_i2c_dev *dev)
{
	return dev->controller->clk_khz;
}

static int __devinit i2c_dw_pci_probe(struct pci_dev *pdev,
const struct pci_device_id *id)
{
	struct dw_i2c_dev *dev;
	struct i2c_adapter *adap;
	unsigned long start, len;
	void __iomem *base;
	int r;
	int bus_idx;
	static int bus_num;
	struct  dw_pci_controller *controller;

	bus_idx = id->driver_data + bus_num;
	bus_num++;

	if (bus_idx >= ARRAY_SIZE(dw_pci_controllers)) {
		pr_err("i2c_dw_pci_probe: invalid bus index %d\n",
			bus_idx);
		return -EINVAL;
	}

	controller = &dw_pci_controllers[bus_idx];

	r = pci_enable_device(pdev);
	if (r) {
		dev_err(&pdev->dev, "Failed to enable I2C PCI device (%d)\n",
			r);
		goto exit;
	}

	/* Determine the address of the I2C area */
	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);
	if (!start || len == 0) {
		dev_err(&pdev->dev, "base address not set\n");
		r = -ENODEV;
		goto exit;
	}

	r = pci_request_region(pdev, 0, DRIVER_NAME);
	if (r) {
		dev_err(&pdev->dev, "failed to request I2C region "
			"0x%lx-0x%lx\n", start,
			(unsigned long)pci_resource_end(pdev, 0));
		goto exit;
	}

	base = ioremap_nocache(start, len);
	if (!base) {
		dev_err(&pdev->dev, "I/O memory remapping failed\n");
		r = -ENOMEM;
		goto err_release_region;
	}


	dev = kzalloc(sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev) {
		r = -ENOMEM;
		goto err_release_region;
	}

	init_completion(&dev->cmd_complete);
	sema_init(&dev->lock, 1);
	dev->status = STATUS_IDLE;
	dev->clk = NULL;
	dev->controller = controller;
	dev->get_clk_rate_khz = i2c_dw_get_clk_rate_khz;
	dev->base = base;
	dev->dev = get_device(&pdev->dev);
	dev->functionality =
		I2C_FUNC_I2C |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
	dev->master_cfg =  controller->bus_cfg;
	dev->get_scl_cfg = controller->scl_cfg;
	dev->enable_stop = controller->enable_stop;

	pci_set_drvdata(pdev, dev);

	dev->tx_fifo_depth = controller->tx_fifo_depth;
	dev->rx_fifo_depth = controller->rx_fifo_depth;
	r = i2c_dw_init(dev);
	if (r)
		goto err_iounmap;

	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	adap->class = 0;
	adap->algo = &i2c_dw_algo;
	adap->dev.parent = &pdev->dev;
	adap->nr = controller->bus_num;
	snprintf(adap->name, sizeof(adap->name), "i2c-designware-pci-%d",
		adap->nr);

	r = request_irq(pdev->irq, i2c_dw_isr, IRQF_SHARED, adap->name, dev);
	if (r) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", dev->irq);
		goto err_iounmap;
	}

	i2c_dw_disable_int(dev);
	i2c_dw_clear_int(dev);
	r = i2c_add_numbered_adapter(adap);
	if (r) {
		dev_err(&pdev->dev, "failure adding adapter\n");
		goto err_free_irq;
	}

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);

	return 0;

err_free_irq:
	free_irq(pdev->irq, dev);
err_iounmap:
	iounmap(dev->base);
	pci_set_drvdata(pdev, NULL);
	put_device(&pdev->dev);
	kfree(dev);
err_release_region:
	pci_release_region(pdev, 0);
exit:
	return r;
}

static void __devexit i2c_dw_pci_remove(struct pci_dev *pdev)
{
	struct dw_i2c_dev *dev = pci_get_drvdata(pdev);

	i2c_dw_disable(dev);
	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	pci_set_drvdata(pdev, NULL);
	i2c_del_adapter(&dev->adapter);
	put_device(&pdev->dev);

	free_irq(dev->irq, dev);
	kfree(dev);
	pci_release_region(pdev, 0);
}

/* work with hotplug and coldplug */
MODULE_ALIAS("i2c_designware-pci");

DEFINE_PCI_DEVICE_TABLE(i2c_designware_pci_ids) = {
	/* Moorestown */
	{ PCI_VDEVICE(INTEL, 0x0802), moorestown_0 },
	{ PCI_VDEVICE(INTEL, 0x0803), moorestown_0 },
	{ PCI_VDEVICE(INTEL, 0x0804), moorestown_0 },
	/* Medfield */
	{ PCI_VDEVICE(INTEL, 0x0817), medfield_0 },
	{ PCI_VDEVICE(INTEL, 0x0818), medfield_0 },
	{ PCI_VDEVICE(INTEL, 0x0819), medfield_0 },
	{ PCI_VDEVICE(INTEL, 0x082C), medfield_0 },
	{ PCI_VDEVICE(INTEL, 0x082D), medfield_0 },
	{ PCI_VDEVICE(INTEL, 0x082E), medfield_0 },
	/* Cloverview */
	{ PCI_VDEVICE(INTEL, 0x08E2), cloverview_0 },
	{ PCI_VDEVICE(INTEL, 0x08E3), cloverview_0 },
	{ PCI_VDEVICE(INTEL, 0x08E4), cloverview_0 },
	{ PCI_VDEVICE(INTEL, 0x08F4), cloverview_0 },
	{ PCI_VDEVICE(INTEL, 0x08F5), cloverview_0 },
	{ PCI_VDEVICE(INTEL, 0x08F6), cloverview_0 },
	/* Merrifield */
	{ PCI_VDEVICE(INTEL, 0x1195), merrifield_0 },
	{ PCI_VDEVICE(INTEL, 0x1196), merrifield_0 },
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, i2c_designware_pci_ids);

static struct pci_driver dw_i2c_driver = {
	.name		= DRIVER_NAME,
	.id_table	= i2c_designware_pci_ids,
	.probe		= i2c_dw_pci_probe,
	.remove		= __devexit_p(i2c_dw_pci_remove),
	.driver         = {
		.pm     = &i2c_dw_pm_ops,
	},
};

static int __init dw_i2c_init_driver(void)
{
	return  pci_register_driver(&dw_i2c_driver);
}
module_init(dw_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	pci_unregister_driver(&dw_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("Synopsys DesignWare PCI I2C bus adapter");
MODULE_LICENSE("GPL");
