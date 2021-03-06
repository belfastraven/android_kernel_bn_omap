/*
 * Board support file for OMAP44xx B&N Nook HD/HD+.
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/usb/otg.h>
#include <linux/hwspinlock.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_info.h>

#include <mach/hardware.h>
#include <mach/omap-secure.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/omap-serial.h>
#include <plat/remoteproc.h>
#include <plat/omap-pm.h>
#include <plat/drm.h>

#include "mux.h"
#include "hsmmc.h"
#include "common.h"
#include "control.h"
#include "common-board-devices.h"
#include "pm.h"
#include "omap4_ion.h"
#include "omap_ram_console.h"

#include "board-bn-hd.h"

#ifdef CONFIG_CHARGER_BQ2419x
#include <linux/i2c/bq2419x.h>
#endif
#ifdef CONFIG_BATTERY_BQ27x00
#include <linux/power/bq27x00_battery.h>
#endif

#ifdef CONFIG_MACH_OMAP_OVATION
#define EXT_FET_EN				4
#define CHG_LEVEL				44
#endif

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
#define CHG_LEVEL				84
#define GG_CE					113
#endif

#define GPIO_UART_DDC_SWITCH	182
#define GPIO_LS_DCDC_EN			60
#define GPIO_LS_OE				81
#define BQ27500_BAT_LOW_GPIO	42

static void __init omap_bn_init_early(void)
{
	struct omap_hwmod *oh;
	int i;
	char const * const hwmods[] = {
		[0] = "gpio1",
		[3] = "gpio2",
		[1] = "gpio3",
		[2] = "gpio5"
	};

	omap4430_init_early();

	for (i = 0; i < sizeof(hwmods)/sizeof(char*); i++) {
		oh = omap_hwmod_lookup(hwmods[i]);
		if (oh) {
			if (omap_hwmod_no_setup_reset(oh))
				printk("Failed to disable reset for %s\n", hwmods[i]);
		} else
			printk("%s hwmod not found\n", hwmods[i]);
	}

	oh = omap_hwmod_lookup("uart3");
	if (oh) {
		oh->flags &= ~(HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET);
	} else
		printk("uart3 hwmod not found\n");

	oh = omap_hwmod_lookup("uart4");
	if (oh) {
		oh->flags |= HWMOD_SWSUP_SIDLE;
	} else
		printk("uart4 hwmod not found\n");
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_OTG,
	.power			= 200,
};

static struct omap2_hsmmc_info bn_mmc[] = {
	{
		.mmc		= 2,
		.caps		=  MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
					MMC_CAP_1_8V_DDR,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable   = true,
		.ocr_mask	= MMC_VDD_29_30,
		.no_off_init	= true,
	},
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_1_8V_DDR,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
	},
	{
		.mmc		= 3,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.built_in	= 1,
		.nonremovable	= true,
	},
	{}	/* Terminator */
};

static void omap4_audio_conf(void)
{
	/* twl6040 naudint */
	omap_mux_init_signal("sys_nirq2.sys_nirq2", \
		OMAP_PIN_INPUT_PULLUP);

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	omap_mux_init_signal("usbb1_ulpitll_dat4.abe_mcbsp3_dr",\
		OMAP_PIN_INPUT | OMAP_PIN_INPUT_PULLDOWN);

	omap_mux_init_signal("usbb1_ulpitll_dat5.abe_mcbsp3_dx",\
		OMAP_PIN_OUTPUT | OMAP_PIN_OFF_OUTPUT_LOW);

	omap_mux_init_signal("usbb1_ulpitll_dat6.abe_mcbsp3_clkx",\
		OMAP_PIN_INPUT | OMAP_PIN_OFF_INPUT_PULLDOWN);

	omap_mux_init_signal("usbb1_ulpitll_dat7.abe_mcbsp3_fsx",\
		OMAP_PIN_INPUT | OMAP_PIN_OFF_INPUT_PULLDOWN);
#endif
}

#ifdef CONFIG_BATTERY_BQ27x00
static struct bq27x00_platform_data __initdata bq27520_platform_data = {
#if 0 // AM: old pdata that may be implemented in the future
	.gpio_ce = 113,
	.gpio_soc_int = 176,
	.gpio_bat_low = 42,
#endif
	.number_actions = 2,
	.cooling_actions = {
		{ .priority = 0, .percentage = 100, }, /* Full Charging Cur */
		{ .priority = 1, .percentage = 0, }, /* Charging OFF */
	},
};

static struct i2c_board_info __initdata bq27520_i2c_boardinfo = {
	I2C_BOARD_INFO("bq27500", 0x55),
	//.flags = I2C_CLIENT_WAKE,
	.irq = 176,
	//.platform_data = &bq27520_platform_data,
};
#endif

#ifdef CONFIG_CHARGER_BQ2419x
static struct bq2419x_platform_data __initdata bq24196_platform_data = {
	.max_charger_voltagemV = 4200,
	.max_charger_currentmA = 1550,
	.gpio_int = 142,
	.gpio_ce = 158,
	.gpio_psel = 85,
	.stimer_sdp = CHGTIMER_12h,
	.stimer_dcp = machine_is_omap_ovation() ? CHGTIMER_12h : CHGTIMER_8h,
};

static struct i2c_board_info __initdata bq24196_i2c_boardinfo = {
	I2C_BOARD_INFO("bq24196", 0x6b),
	.platform_data = &bq24196_platform_data,
};
#endif

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request(USE_MUTEX_LOCK);
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id, USE_MUTEX_LOCK);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
}

static struct omap_i2c_bus_board_data __initdata bn_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata bn_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata bn_i2c_3_bus_pdata;

//#define OMAP4_I2Cx_SDA_PULLUPRESX_MASK(x) OMAP4_I2C##x##_SDA_PULLUPRESX_MASK
//#define OMAP4_I2Cx_SCL_PULLUPRESX_MASK(x) OMAP4_I2C##x##_SCL_PULLUPRESX_MASK
#define OMAP4_I2Cx_PULLUPRESX_MASK(x) ( OMAP4_I2C##x##_SDA_PULLUPRESX_MASK |	\
										OMAP4_I2C##x##_SCL_PULLUPRESX_MASK )

#define omap2_i2c_pullups_en_dis(bus_id, enable)								\
({																				\
	u32 val = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);	\
	enable ? (val &= ~OMAP4_I2Cx_PULLUPRESX_MASK(bus_id)) :						\
			 (val |=  OMAP4_I2Cx_PULLUPRESX_MASK(bus_id));						\
	omap4_ctrl_pad_writel(val, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);		\
})

static int __init omap4_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &bn_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &bn_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &bn_i2c_3_bus_pdata);

	omap_register_i2c_bus_board_data(1, &bn_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &bn_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &bn_i2c_3_bus_pdata);

	bn_power_init();

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev >= OVATION_EVT1B) {
#endif
#ifdef CONFIG_BATTERY_BQ27x00
		i2c_register_board_info(1, &bq27520_i2c_boardinfo, 1);
#endif
#ifdef CONFIG_CHARGER_BQ2419x
		i2c_register_board_info(1, &bq24196_i2c_boardinfo, 1);
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	}
#endif

	omap_register_i2c_bus(3, 400, NULL, 0);

	// Disable the strong pull-ups on I2C3 and I2C4
	omap2_i2c_pullups_en_dis(3, 0);
	//omap2_i2c_pullups_en_dis(4, 0);

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	/*
	 * Drive MSECURE high for TWL6030 write access.
	 */
	omap_mux_init_signal("fref_clk0_out.gpio_wk6", OMAP_PIN_OUTPUT);
	gpio_request(6, "msecure");
	gpio_direction_output(6, 1);

	return 0;
}

static bool enable_suspend_off = true;
module_param(enable_suspend_off, bool, S_IRUSR | S_IRGRP | S_IROTH);

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP4_MUX(USBB2_ULPITLL_CLK, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	OMAP4_MUX(SYS_NIRQ1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_WAKEUP_EN),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

#define DEFAULT_UART_AUTOSUSPEND_DELAY	3000	/* Runtime autosuspend (msecs)*/

static struct omap_device_pad bn_uart1_pads[] __initdata = {
	{
		.name	= "i2c2_sda.uart1_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name	= "i2c2_scl.uart1_rx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE1,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE1,
	},
};

static struct omap_device_pad bn_uart4_pads[] __initdata = {
	{
		.name   = "abe_dmic_clk1.uart4_cts",
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE5,
		.flags  = OMAP_DEVICE_PAD_REMUX,
		.idle   = OMAP_WAKEUP_EN | OMAP_PIN_OFF_INPUT_PULLUP |
			  OMAP_MUX_MODE5,
	},
	{
		.name   = "abe_dmic_din1.uart4_rts",
		.flags  = OMAP_DEVICE_PAD_REMUX,
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE5,
		.idle   = OMAP_PIN_OFF_INPUT_PULLUP | OMAP_MUX_MODE7,
	},
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_board_data bn_uart1_board_data __initdata = {
	.id = 0,
	.pads = bn_uart1_pads,
	.pads_cnt = ARRAY_SIZE(bn_uart1_pads),
};

static struct omap_board_data bn_uart4_board_data __initdata = {
	.id = 3,
	.pads = bn_uart4_pads,
	.pads_cnt = ARRAY_SIZE(bn_uart4_pads),
};

static struct omap_uart_port_info bn_uart_info __initdata = {
	.dma_enabled = 0,
	.autosuspend_timeout = DEFAULT_UART_AUTOSUSPEND_DELAY,
	//.wer = machine_is_omap_ovation() ? 0 : (OMAP_UART_WER_TX | OMAP_UART_WER_RX | OMAP_UART_WER_CTS),
};

static inline void __init board_serial_init(void)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if (system_rev == HUMMINGBIRD_EVT0) {
#endif
		/* default switch to  UART1 output */
		gpio_request(GPIO_UART_DDC_SWITCH, "UART1_DDC_SWITCH");
		gpio_direction_output(GPIO_UART_DDC_SWITCH, 1);
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	}
#endif

	/* NOTE: Didn't control the LS_DCDC_EN/CT_CP_HPD and LS_OE gpio pins,
	 * because they're controlled by the DSS HDMI subsystem now.
	 */

	omap_serial_init_port(&bn_uart1_board_data, &bn_uart_info);
	omap_serial_init_port(&bn_uart4_board_data, &bn_uart_info);
}

static struct regulator_consumer_supply bn_lcd_tp_supply[] = {
	{ .supply = "vtp" },
	{ .supply = "vlcd" },
};

static struct regulator_init_data bn_lcd_tp_vinit = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 2,
	.consumer_supplies = bn_lcd_tp_supply,
};

static struct fixed_voltage_config bn_lcd_touch_reg_data = {
	.supply_name = "vdd_lcdtp",
	.microvolts = 3300000,
	.gpio = 36,
	.enable_high = 1,
	.enabled_at_boot = 1,
	.init_data = &bn_lcd_tp_vinit,
};

static struct platform_device bn_lcd_touch_regulator_device = {
	.name   = "reg-fixed-voltage",
	.id     = -1,
	.dev    = {
		.platform_data = &bn_lcd_touch_reg_data,
	},
};

static struct platform_device *bn_lcd_touch_devices[] __initdata = {
	&bn_lcd_touch_regulator_device,
};

static void __init bn_lcd_touch_init(void)
{
	platform_add_devices(bn_lcd_touch_devices,
				ARRAY_SIZE(bn_lcd_touch_devices));
}

#if 0
static struct regulator *bn_tp_vdd;
static int bn_tp_vdd_enabled;
static struct regulator *bn_lcd_tp_pwr[ARRAY_SIZE(bn_lcd_tp_supply)];
static int bn_lcd_tp_pwr_enabled[ARRAY_SIZE(bn_lcd_tp_supply)];

int Vdd_LCD_CT_PEN_request_supply(struct device *dev, const char *supply_name)
{
	int ret;
	int iLoop;
	int index;
	int n_users;

	for(iLoop=0, index=-1; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(!strcmp(supply_name, bn_lcd_tp_supply[iLoop].supply))
		{
			index = iLoop;
			break;
		}
	}
	if(index >= 0)
	{
		for(iLoop=0, n_users=0; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
		{
			if(bn_lcd_tp_pwr[iLoop] != NULL)
			{
				n_users++;
				break; /* We just need to check if there is at least one user for the lcd_tp supply */
			}
		}
		if(!n_users)
		{
			if(bn_tp_vdd == NULL)
			{
				bn_tp_vdd = regulator_get(dev, "touch_vdd");
				if(IS_ERR(bn_tp_vdd))
				{
					bn_tp_vdd = NULL;
					pr_err("%s: touch vdd regulator not valid\n", __func__);
					ret = -ENODEV;
					goto err_regulator_tp_vdd_get;
				}
			}
		}

		if(bn_lcd_tp_pwr[index] == NULL)
		{
			bn_lcd_tp_pwr[index] = regulator_get(dev, bn_lcd_tp_supply[index].supply);
			if(IS_ERR(bn_lcd_tp_pwr[index]))
			{
				bn_lcd_tp_pwr[index] = NULL;
				pr_err("%s: Could not get \"%s\" regulator\n", __func__, supply_name);
				ret = -ENODEV;
				goto err_regulator_lcd_tp_pwr_get;
			}
			ret = 0;
			n_users++;
			goto err_return;
		}
		else
		{
			pr_debug("%s: \"%s\" already in use\n", __func__, supply_name);
			ret = 0;
			goto err_return;
		}
	}
	else
	{
		pr_err("%s: no regulator with name \"%s\" found\n", __func__, supply_name);
		ret = -ENODEV;
		goto err_return;
	}

err_regulator_lcd_tp_pwr_get:
	if(n_users == 0) /* No devices using the regulator */
	{
		if(bn_tp_vdd != NULL)
		{
			regulator_put(bn_tp_vdd);
			bn_tp_vdd = NULL;
		}
	}
err_regulator_tp_vdd_get:
err_return:
	return ret;
}
EXPORT_SYMBOL(Vdd_LCD_CT_PEN_request_supply);

int Vdd_LCD_CT_PEN_release_supply(struct device *dev, const char *supply_name)
{
	int iLoop;
	int index;
	int n_users;

	for(iLoop=0, index=-1; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(!strcmp(supply_name, bn_lcd_tp_supply[iLoop].supply))
		{
			index = iLoop;
			break;
		}
	}
	if(index >= 0)
	{
			if(bn_lcd_tp_pwr[index] != NULL)
			{
				regulator_put(bn_lcd_tp_pwr[index]);
				bn_lcd_tp_pwr[index] = NULL;
			}
			else
			{
				pr_debug("%s: \"%s\" already released\n", __func__, supply_name);
			}
	}

	for(iLoop=0, n_users=0; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(bn_lcd_tp_pwr[iLoop] != NULL)
		{
			n_users++;
			break; /* We just need to check if there is at least one user for the lcd_tp supply */
		}
	}
	if(n_users == 0)
	{
		if(bn_tp_vdd != NULL)
		{
			regulator_put(bn_tp_vdd);
			bn_tp_vdd = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(Vdd_LCD_CT_PEN_release_supply);

int Vdd_LCD_CT_PEN_enable(struct device *dev, const char *supply_name)
{
	int ret = 0;
	int iLoop;
	int index;
	int n_users;

	for(iLoop=0, index=-1; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(!strcmp(supply_name, bn_lcd_tp_supply[iLoop].supply))
		{
			index = iLoop;
			break;
		}
	}
	if(index >= 0)
	{
		if(bn_tp_vdd_enabled == 0)
		{
			if(bn_tp_vdd != NULL)
			{
				ret = regulator_enable(bn_tp_vdd);
				if(ret)
				{
					pr_err("%s: Could not enable touch vdd regulator\n", __func__);
					ret = -EBUSY;
					goto err_regulator_tp_vdd_enable;
				}
				bn_tp_vdd_enabled = 1;
				msleep(5);
			}
			else
			{
				pr_err("%s: touch vdd regulator is not valid\n", __func__);
				ret = -ENODEV;
				goto err_regulator_tp_vdd_enable;
			}
		}
		if(bn_lcd_tp_pwr_enabled[index] == 0)
		{
			if(bn_lcd_tp_pwr[index] != NULL)
			{
				ret = regulator_enable(bn_lcd_tp_pwr[index]);
				if(ret)
				{
					pr_err("%s: Could not enable \"%s\" regulator\n", __func__, supply_name);
					ret = -EBUSY;
					goto err_regulator_lcd_tp_pwr_enable;
				}
				bn_lcd_tp_pwr_enabled[index] = 1;
			}
			else
			{
				pr_err("%s: \"%s\" regulator is not valid\n", __func__, supply_name);
				ret = -ENODEV;
				goto err_regulator_lcd_tp_pwr_enable;
			}
		}
	}
	goto err_return;

err_regulator_lcd_tp_pwr_enable:
	for(iLoop=0, n_users=0; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(bn_lcd_tp_pwr_enabled[iLoop] != 0)
		{
			n_users++;
			break; /* We just need to check if there is at least one user for the lcd_tp supply */
		}
	}
	if(n_users == 0)
	{
		if(bn_tp_vdd != NULL)
		{
			regulator_disable(bn_tp_vdd);
			bn_tp_vdd_enabled = 0;
		}
		else
		{
			pr_err("%s: touch vdd regulator is not valid\n", __func__);
		}
	}
err_regulator_tp_vdd_enable:
err_return:
	return ret;
}
EXPORT_SYMBOL(Vdd_LCD_CT_PEN_enable);

int Vdd_LCD_CT_PEN_disable(struct device *dev, const char *supply_name)
{
	int iLoop;
	int index;
	int n_users;

	for(iLoop=0, index=-1; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
	{
		if(!strcmp(supply_name, bn_lcd_tp_supply[iLoop].supply))
		{
			index = iLoop;
			break;
		}
	}
	if(index >= 0)
	{
		if(bn_lcd_tp_pwr_enabled[index] != 0)
		{
			if(bn_lcd_tp_pwr[index] != NULL)
			{
				regulator_disable(bn_lcd_tp_pwr[index]);
				bn_lcd_tp_pwr_enabled[index] = 0;
			}
			else
			{
				pr_err("%s: \"%s\" regulator is not valid\n", __func__, supply_name);
			}
		}
		for(iLoop=0, n_users=0; iLoop < ARRAY_SIZE(bn_lcd_tp_supply); iLoop++)
		{
			if(bn_lcd_tp_pwr_enabled[iLoop] != 0)
			{
				n_users++;
				break; /* We just need to check if there is at least one user for the lcd_tp supply */
			}
		}
		if(n_users == 0)
		{
			if(bn_tp_vdd_enabled != 0)
			{
				if(bn_tp_vdd != NULL)
				{
					regulator_disable(bn_tp_vdd);
					bn_tp_vdd_enabled = 0;
				}
				else
				{
					pr_err("%s: touch vdd regulator is not valid\n", __func__);
				}
			}
			else
			{
				pr_err("%s: touch vdd regulator state is not valid\n", __func__);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(Vdd_LCD_CT_PEN_disable);
#endif

static void __init omap_bn_init(void)
{
	int package = OMAP_PACKAGE_CBS;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;
	omap4_mux_init(board_mux, NULL, package);
	omap_pm_setup_oscillator(4000, 1);

#ifdef CONFIG_MACH_OMAP_OVATION
	/* Turn off the external FET for twl6032 charger */
	gpio_request ( EXT_FET_EN , "EXT-FET-EN" );
	gpio_direction_output ( EXT_FET_EN , 0 );

	/* EVT1a bringup only */
	if (system_rev == OVATION_EVT1A) {
		/* The GPIO for Charging level - 1=2A, 0=1A */
		gpio_request(CHG_LEVEL, "CHG-LEVEL");
		gpio_direction_output(CHG_LEVEL, 1);
	}
#endif
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	// For EVT1 Bring-up
	gpio_request(CHG_LEVEL,"CHG-LEVEL");
	gpio_direction_output(CHG_LEVEL, 0);

	// For EVT1 Bring-up
	gpio_request(GG_CE,"GG-CE");
	gpio_direction_output(GG_CE, 0);
#endif

	bn_emif_init();
	omap_create_board_props();
	omap4_audio_conf();
	omap4_i2c_init();
	bn_lcd_touch_init();
	bn_touch_init();
	bn_panel_init();
	bn_button_init();
	bn_accel_init();
	omap_init_dmm_tiler();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	board_serial_init();
	bn_wilink_init();

	omap_sdrc_init(NULL, NULL);
	omap4_twl6030_hsmmc_init(bn_mmc);
	usb_musb_init(&musb_board_data);

	omap_enable_smartreflex_on_init();
        if (enable_suspend_off)
                omap_pm_enable_off_mode();

	/* Enable GG bat_low irq to wake up device to inform framework to shutdown. */
	//omap_mux_init_gpio(BQ27500_BAT_LOW_GPIO, OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE);
}

static void __init omap_bn_reserve(void)
{
	omap_init_ram_size();

	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT, OMAP_RAM_CONSOLE_SIZE_DEFAULT);
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);

	bn_android_display_setup();
#ifdef CONFIG_ION_OMAP
	omap4_ion_init();
#endif

	omap4_secure_workspace_addr_default();

	omap_reserve();
}

#ifdef CONFIG_MACH_OMAP_OVATION
MACHINE_START(OMAP_OVATION, "B&N Nook HD+ (ovation)")
#endif
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
MACHINE_START(OMAP_HUMMINGBIRD, "B&N Nook HD (hummingbird)")
#endif
	.atag_offset	= 0x100,
	.reserve	= omap_bn_reserve,
	.map_io		= omap4_map_io,
	.init_early	= omap_bn_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_bn_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
