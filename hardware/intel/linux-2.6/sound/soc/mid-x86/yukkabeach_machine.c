/*
 *  yukkabeach_machine.c - ASoc Machine driver for Yukkabeach board
 *  based on Intel Medfield MID platform.Yukkabeach is a variant of
 *  Lexington board.
 *
 *  Copyright (C) 2010-12 Intel Corp
 *  Author: Vinod Koul <vinod.koul@intel.com>
 *  Author: Ramesh babu K V <ramesh.babu@intel.com>
 *  Author: Jayachandran.B <jayachandran.b@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/wakelock.h>
#include <linux/ipc_device.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel_mid_gpadc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/tlv.h>
#include <sound/msic_audio_platform.h>
#include "../codecs/sn95031.h"
#include "mfld_common.h"

/* The Yukkbeach jack is different, debounce time needs to be more */
#define MFLD_JACK_DEBOUNCE_TIME	  700 /* mS */

/* Maximum number of retry for HS detection */
#define MAX_HS_DET_RETRY 10

/*Maximum delay for workqueue */
#define MAX_DELAY 2000

/* Support for slow HS jack inserts */
#define JACK_DET_RETRY 4
#define JACK_POLL_INTERVAL 500

/* ADC channel number for thermal probe */
#define MFLD_YB_THERM_PROBE_ADC_CH_ID        0x0B

/* Jack detection zones for YB. Mostly same as typical mfld values
   except for the upper limit for Headset. This was changed because:
   When an accessory is insered into the jack it is checked for thermal probe
   (TP) first and if it is not, the usual headset detection algo is invoked.
   In the corner case where a TP is insered, but is not detected as TP we dont
   want the HS detection alogo to report a HS or HP. Normally if the voltage
   at ADC6 is more than 2000mv, HS detection algo reports that neither HS nor HP
   is connected. To make this more robust, HW Eng. suggested changing this limit
   to 1900 mV
*/
enum soc_mic_bias_zones_yb {
	YB_MV_START = MFLD_MV_START,
	/* mic bias volutage range for American Headset*/
	YB_MV_AM_HS = MFLD_MV_AM_HS,
	/* mic bias voltage range for Headset*/
	YB_MV_HS = 1900, /* This value is changed from typical mfld value*/
};
/* jack detection voltage zones */
static struct snd_soc_jack_zone mfld_zones[] = {
	{YB_MV_START, YB_MV_AM_HS, SND_JACK_HEADPHONE},
	{YB_MV_AM_HS, YB_MV_HS, SND_JACK_HEADSET},
};

static int mfld_get_headset_state(struct snd_soc_jack *jack)
{
	int micbias, jack_type, gpio_state;
	struct mfld_mc_private *ctx =
			snd_soc_card_get_drvdata(jack->codec->card);

	mfld_jack_enable_mic_bias_gen(jack->codec, "AMIC1Bias");
	micbias = mfld_jack_read_voltage(jack);

	jack_type = snd_soc_jack_get_type(jack, micbias);
	pr_debug("jack type detected = %d, micbias = %d\n", jack_type, micbias);

	if (jack_type != SND_JACK_HEADSET && jack_type != SND_JACK_HEADPHONE) {
		gpio_state = mfld_read_jack_gpio(ctx);
		if (gpio_state == 0) {
			jack_type = SND_JACK_HEADPHONE;
			pr_debug("GPIO says there is a headphone, reporting it\n");
		}
	}
	if (jack_type == SND_JACK_HEADPHONE || jack_type == SND_JACK_HEADSET ) {
		gpio_state = mfld_read_jack_gpio(ctx);
		if (gpio_state != 0){
			jack_type = 0;
		}
	}
	if (jack_type == SND_JACK_HEADSET || jack_type == SND_JACK_HEADPHONE)
		/* enable btn press detection */
		snd_soc_update_bits(jack->codec, SN95031_BTNCTRL2, BIT(0), 1);
	else
		mfld_jack_disable_mic_bias_gen(jack->codec, "AMIC1Bias");

	return jack_type;
}

#ifndef  CONFIG_SND_THERMAL_PROBE_GPIO
#define THERM_PROBE_SWITCH	114 
#else
#define THERM_PROBE_SWITCH	171
#endif
static void mfld_jack_report(struct snd_soc_jack *jack, unsigned int status)
{
	unsigned int mask = SND_JACK_BTN_0 | SND_JACK_HEADSET;

	pr_info("jack reported of type: 0x%x\n", status);
#ifndef CONFIG_SND_THERMAL_PROBE_GPIO
	gpio_request(THERM_PROBE_SWITCH, "therm_probe_switch");
#else
	gpio_request(THERM_PROBE_SWITCH, "THERM_PROBE_EN");
#endif	
	if ((status == SND_JACK_HEADSET) || (status == SND_JACK_HEADPHONE)) {
		/*
		 * if we detected valid headset then disable headset ground,
		 * this is required for jack detection to work well
		 */
		snd_soc_update_bits(jack->codec, SN95031_BTNCTRL2, BIT(1), 0);
		gpio_direction_output(THERM_PROBE_SWITCH, 1);
	} else if (status == 0) {
		snd_soc_update_bits(jack->codec, SN95031_BTNCTRL2,
						BIT(1), BIT(1));
		gpio_direction_output(THERM_PROBE_SWITCH, 0);
	}
	gpio_free(THERM_PROBE_SWITCH);
	snd_soc_jack_report(jack, status, mask);
#ifdef CONFIG_SWITCH_MID
	/* report to the switch driver as well */
	if (status) {
		if (status == SND_JACK_HEADPHONE)
			mid_headset_report((1<<1));
		else if (status == SND_JACK_HEADSET)
			mid_headset_report(1);
	} else {
		mid_headset_report(0);
	}
#endif
}

void mfld_jack_wq(struct work_struct *work)
{
	unsigned int mask = SND_JACK_BTN_0 | SND_JACK_HEADSET;
	struct mfld_mc_private *ctx =
		container_of(work, struct mfld_mc_private, jack_work.work.work);
	struct mfld_jack_work *jack_work = &ctx->jack_work;
	struct snd_soc_jack *jack = jack_work->jack;
	unsigned int voltage, status = 0, intr_id = jack_work->intr_id;
	int gpio_state;

	pr_info("jack status in wq: 0x%x\n", intr_id);
	if (intr_id & SN95031_JACK_INSERTED) {

		/* Check if thermal probe is connected. If yes return from here
		   without going though HS detection/jack report */
#ifndef CONFIG_SND_NO_THERMAL_PROBE
		if (mfld_therm_probe_on_connect(jack))
			return;
#endif

		status = mfld_get_headset_state(jack);
		/* unmask button press interrupts */
		if (status == SND_JACK_HEADSET)
			snd_soc_update_bits(jack->codec, SN95031_ACCDETMASK,
							BIT(1)|BIT(0), 0);
		/*
		 * At this point the HS may be half inserted and still be
		 * detected as HP, so recheck after 500mS
		 */
		else if (!atomic_dec_and_test(&ctx->hs_det_retry)) {
			pr_debug("HS Jack detect Retry %d\n",
					atomic_read(&ctx->hs_det_retry));
#ifdef CONFIG_HAS_WAKELOCK
			/* Give sufficient time for the detection to propagate*/
			wake_lock_timeout(ctx->jack_wake_lock, 2*HZ);
#endif
			schedule_delayed_work(&jack_work->work,
				msecs_to_jiffies(ctx->jack_poll_interval));

		}
		if (ctx->jack_status == status)
			return;
		else
			ctx->jack_status = status;

	} else if (intr_id & SN95031_JACK_REMOVED) {
		gpio_state = mfld_read_jack_gpio(ctx);
		if (gpio_state == 0) {
			pr_debug("remove interrupt, but GPIO says inserted\n");
			return;
		}
		pr_debug("reporting jack as removed\n");
		snd_soc_update_bits(jack->codec, SN95031_BTNCTRL2, BIT(0), 0);
		snd_soc_update_bits(jack->codec, SN95031_ACCDETMASK, BIT(2), 0);
		mfld_jack_disable_mic_bias_gen(jack->codec, "AMIC1Bias");
		jack_work->intr_id = 0;
		ctx->jack_status = 0;
		cancel_delayed_work(&ctx->jack_work.work);

		/* Check if the accessory removed was indeed thermal probe.
		   If yes return without going through jack report */
#ifndef CONFIG_SND_NO_THERMAL_PROBE
		if (mfld_therm_probe_on_removal(jack))
			return;
#endif			

	} else if (intr_id & SN95031_JACK_BTN0) {
		if (ctx->mfld_jack_lp_flag) {
			snd_soc_jack_report(jack, SND_JACK_HEADSET, mask);
			ctx->mfld_jack_lp_flag = 0;
			pr_debug("short press on releasing long press, "
				   "report button release\n");
			return;
		} else {
			status = SND_JACK_HEADSET | SND_JACK_BTN_0;
			pr_debug("short press detected\n");
			snd_soc_jack_report(jack, status, mask);
			/* send explicit button release */
			if (status & SND_JACK_BTN_0)
				snd_soc_jack_report(jack,
						SND_JACK_HEADSET, mask);
			return;
		}
	} else if (intr_id & SN95031_JACK_BTN1) {
		/*
		 * we get spurious interrupts if jack key is held down
		 * so we ignore them until key is released by checking the
		 * voltage level
		 */
		if (ctx->mfld_jack_lp_flag) {
			voltage = mfld_jack_read_voltage(jack);
			if (voltage > MFLD_LP_THRESHOLD_VOLTAGE) {
				snd_soc_jack_report(jack,
						SND_JACK_HEADSET, mask);
				ctx->mfld_jack_lp_flag = 0;
				pr_debug("button released after long press\n");
			}
			return;
		}
		/* Codec sends separate long press event after button pressed
		 * for a specified time. Need to send separate button pressed
		 * and released events for Android
		 */
		status = SND_JACK_HEADSET | SND_JACK_BTN_0;
		ctx->mfld_jack_lp_flag = 1;
		pr_debug("long press detected\n");
	} else {
		pr_err("Invalid intr_id:0x%x\n", intr_id);
		return;
	}
	mfld_jack_report(jack, status);
}

static int mfld_schedule_jack_wq(struct mfld_jack_work *jack_work)
{
	struct mfld_mc_private *ctx =
		container_of(jack_work, struct mfld_mc_private, jack_work);
	/* Reset the HS slow jack detect retry count  on interrupt*/
	atomic_set(&ctx->hs_det_retry, ctx->jack_poll_retry);

	return schedule_delayed_work(&jack_work->work,
			msecs_to_jiffies(MFLD_JACK_DEBOUNCE_TIME));
}

static void mfld_jack_detection(unsigned int intr_id,
				struct mfld_jack_work *jack_work)
{
	int retval;
	struct mfld_mc_private *ctx =
		container_of(jack_work, struct mfld_mc_private, jack_work);

	pr_debug("interrupt id read in sram = 0x%x\n", intr_id);

	if (intr_id & SN95031_JACK_INSERTED ||
				intr_id & SN95031_JACK_REMOVED) {
		ctx->jack_work.intr_id = intr_id;
		retval = mfld_schedule_jack_wq(jack_work);
		if (!retval)
			pr_debug("jack inserted/removed,intr already queued\n");
		/* mask button press interrupts until jack is reported*/
		snd_soc_update_bits(ctx->mfld_jack.codec,
		     SN95031_ACCDETMASK, BIT(1)|BIT(0), BIT(1)|BIT(0));
		return;
	}

	if (intr_id & SN95031_JACK_BTN0 ||
				intr_id & SN95031_JACK_BTN1) {
		if ((ctx->mfld_jack.status & SND_JACK_HEADSET) != 0 && (mfld_read_jack_gpio(ctx) == 0)) {
			ctx->jack_work.intr_id = intr_id;
			retval = mfld_schedule_jack_wq(jack_work);
			if (!retval) {
				pr_debug("spurious btn press, lp_flag:%d\n",
						ctx->mfld_jack_lp_flag);
				return;
			}
			pr_debug("BTN_Press detected\n");
		} else {
			pr_debug("BTN_press received, but jack is removed\n");
		}
	}
}

static const DECLARE_TLV_DB_SCALE(out_tlv, -6200, 100, 0);

static const struct snd_kcontrol_new mfld_snd_controls[] = {
	SOC_ENUM_EXT("Playback Switch", mfld_headset_enum,
			mfld_headset_get_switch, mfld_headset_set_switch),
	SOC_ENUM_EXT("PCM1 Mode", sn95031_pcm1_mode_config_enum,
			mfld_get_pcm1_mode, mfld_set_pcm1_mode),
	/* Add digital volume and mute controls for Headphone/Headset*/
	SOC_DOUBLE_R_EXT_TLV("Headphone Playback Volume", SN95031_HSLVOLCTRL,
				SN95031_HSRVOLCTRL, 0, 71, 1,
				snd_soc_get_volsw_2r, mfld_set_vol_2r,
				out_tlv),
	SOC_DOUBLE_R_EXT_TLV("Speaker Playback Volume", SN95031_IHFLVOLCTRL,
				SN95031_IHFRVOLCTRL, 0, 71, 1,
				snd_soc_get_volsw_2r, mfld_set_vol_2r,
				out_tlv),
};

static const struct snd_soc_dapm_widget mfld_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("BuiltinMic", NULL),
	SND_SOC_DAPM_MIC("HeadsetMic", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
	/* Dummy widget to trigger VAUDA on/off */
	SND_SOC_DAPM_SUPPLY("Vibra1Clock", SND_SOC_NOPM, 0, 0,
			mfld_vibra_enable_clk,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("Vibra2Clock", SND_SOC_NOPM, 0, 0,
			mfld_vibra_enable_clk,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route mfld_map[] = {
	{ "HPOUTL", NULL, "Headset Rail"},
	{ "HPOUTR", NULL, "Headset Rail"},
	/* Yukkabeach uses BOOST regulator for Speaker */
	{ "IHFOUTL", NULL, "Speaker Rail"},
	{ "IHFOUTR", NULL, "Speaker Rail"},
	{"Headphones", NULL, "HPOUTR"},
	{"Headphones", NULL, "HPOUTL"},
	/* Yukkabeach uses DMIC for built-in MIC */
	{"DMIC1", NULL, "BuiltinMic"},

	{"AMIC2", NULL, "BuiltinMic"},
	
	{"AMIC1", NULL, "HeadsetMic"},
	{"VIB1SPI", NULL, "Vibra1Clock"},
	{"VIB2SPI", NULL, "Vibra2Clock"},
};

static int mfld_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(runtime->card);
	int ret_val;

	/* Add jack sense widgets */
	ret_val = snd_soc_add_codec_controls(codec, mfld_snd_controls,
				ARRAY_SIZE(mfld_snd_controls));
	if (ret_val) {
		pr_err("soc_add_controls failed %d", ret_val);
		return ret_val;
	}
	/* default is earpiece pin, userspace sets it explcitly */
	snd_soc_dapm_disable_pin(dapm, "Headphones");
	/* default is lineout NC, userspace sets it explcitly */
	snd_soc_dapm_disable_pin(dapm, "LINEOUTL");
	snd_soc_dapm_disable_pin(dapm, "LINEOUTR");
	ctx->hs_switch = 0;
	/* we dont use linein in this so set to NC */
	snd_soc_dapm_disable_pin(dapm, "LINEINL");
	snd_soc_dapm_disable_pin(dapm, "LINEINR");
	snd_soc_dapm_disable_pin(dapm, "AMIC2");

	snd_soc_dapm_disable_pin(dapm, "DMIC1");
	
	snd_soc_dapm_disable_pin(dapm, "DMIC2");
	snd_soc_dapm_disable_pin(dapm, "DMIC3");
	snd_soc_dapm_disable_pin(dapm, "DMIC4");
	snd_soc_dapm_disable_pin(dapm, "DMIC5");
	snd_soc_dapm_disable_pin(dapm, "DMIC6");	

	/*
	 * Keep the voice call paths active during
	 * suspend. Mark the end points ignore_suspend
	 */
	snd_soc_dapm_ignore_suspend(dapm, "PCM1_IN");
	snd_soc_dapm_ignore_suspend(dapm, "PCM1_Out");
	snd_soc_dapm_ignore_suspend(dapm, "EPOUT");
	/* Yukkabeach uses Stereo Speakers */
	snd_soc_dapm_ignore_suspend(dapm, "IHFOUTL");
	snd_soc_dapm_ignore_suspend(dapm, "IHFOUTR");
	snd_soc_dapm_ignore_suspend(dapm, "Headphones");
	snd_soc_dapm_ignore_suspend(dapm, "BuiltinMic");
	snd_soc_dapm_ignore_suspend(dapm, "HeadsetMic");
	mutex_lock(&codec->mutex);
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&codec->mutex);
	/* Headset and button jack detection */
	ret_val = snd_soc_jack_new(codec, "Intel(R) MID Audio Jack",
			SND_JACK_HEADSET | SND_JACK_BTN_0,
			&ctx->mfld_jack);
	if (ret_val) {
		pr_err("jack creation failed\n");
		return ret_val;
	}

	ret_val = snd_soc_jack_add_zones(&ctx->mfld_jack,
			ARRAY_SIZE(mfld_zones), mfld_zones);
	if (ret_val) {
		pr_err("adding jack zones failed\n");
		return ret_val;
	}

	ctx->jack_work.jack = &ctx->mfld_jack;

	/* Init thermal probe adc/gpio etc.*/
#ifndef CONFIG_SND_NO_THERMAL_PROBE
	mfld_therm_probe_init(ctx, MFLD_YB_THERM_PROBE_ADC_CH_ID);
#endif	
	/*
	 * we want to check if anything is inserted at boot,
	 * so send a fake event to codec and it will read adc
	 * to find if anything is there or not
	 */
	ctx->jack_work.intr_id = MFLD_JACK_INSERT_ID;
	mfld_schedule_jack_wq(&ctx->jack_work);
	return ret_val;
}

#ifdef CONFIG_SND_MFLD_MONO_SPEAKER_SUPPORT
static int mfld_speaker_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *cpu_dai = runtime->cpu_dai;
	struct snd_soc_dapm_context *dapm = &runtime->codec->dapm;
	struct snd_soc_codec *codec = runtime->codec;

	snd_soc_dapm_disable_pin(dapm, "IHFOUTR");
	mutex_lock(&codec->mutex);
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&codec->mutex);
	return cpu_dai->driver->ops->set_tdm_slot(cpu_dai, 0, 0, 1, 0);
}
#endif

static int mfld_media_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);

	pr_debug("%s\n", __func__);

	/* Force the data width to 24 bit in MSIC since post processing
	 * algorithms in DSP enabled with 24 bit precision
	 */
	ret = snd_soc_codec_set_params(codec, SNDRV_PCM_FORMAT_S24_LE);
	if (ret < 0) {
		pr_debug("codec_set_params returned error %d\n", ret);
		return ret;
	}

	/* if PCM1 is running and in slave mode, dont reconfigure PLL */
	if (ctx->voice_usage && !ctx->sn95031_pcm1_master_mode) {
		pr_debug("FM active, do not reconfigure PLL\n");
		snd_soc_dai_set_tristate(rtd->codec_dai, 0);
		return 0;
	}


	/* VAUD needs to be on before configuring PLL */
	snd_soc_dapm_force_enable_pin(&codec->dapm, "VirtBias");
	mutex_lock(&codec->mutex);
	snd_soc_dapm_sync(&codec->dapm);
	mutex_unlock(&codec->mutex);
	usleep_range(5000, 6000);

	snd_soc_codec_set_pll(codec, 0, SN95031_PLLIN, 1, 1);
	sn95031_configure_pll(codec, SN95031_ENABLE_PLL);
	/* enable PCM2 */
	snd_soc_dai_set_tristate(rtd->codec_dai, 0);
	return 0;
}

static int mfld_voice_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *soc_card = rtd->card;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);
	pr_debug("%s\n", __func__);

	if (ctx->sn95031_pcm1_master_mode) { /* VOIP call */
		snd_soc_codec_set_pll(codec, 0, SN95031_PLLIN, 1, 1);
		snd_soc_dai_set_fmt(rtd->codec_dai, SND_SOC_DAIFMT_CBM_CFM
						| SND_SOC_DAIFMT_DSP_A);
		/* Sets the PCM1 clock rate */
		snd_soc_update_bits(codec, SN95031_PCM1C1, BIT(0)|BIT(1),
								BIT(0)|BIT(1));
	} else { /* CSV call */
		snd_soc_codec_set_pll(codec, 0, SN95031_PCM1BCLK, 1, 1);
		snd_soc_dai_set_fmt(rtd->codec_dai, SND_SOC_DAIFMT_CBS_CFS
						| SND_SOC_DAIFMT_I2S);
		snd_soc_update_bits(codec, SN95031_PCM1C1, BIT(0)|BIT(1), 0);
	}

	/* VAUD needs to be on before configuring PLL */
	snd_soc_dapm_force_enable_pin(&codec->dapm, "VirtBias");
	mutex_lock(&codec->mutex);
	snd_soc_dapm_sync(&codec->dapm);
	mutex_unlock(&codec->mutex);
	usleep_range(5000, 6000);
	sn95031_configure_pll(codec, SN95031_ENABLE_PLL);
	return 0;
}

static unsigned int rates_44100[] = {
	44100,
};

static struct snd_pcm_hw_constraint_list constraints_44100 = {
	.count	= ARRAY_SIZE(rates_44100),
	.list	= rates_44100,
};

static int mfld_media_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	pr_debug("%s - applying rate constraint\n", __func__);
	snd_pcm_hw_constraint_list(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_44100);
	intel_scu_ipc_set_osc_clk0(true, CLK0_MSIC);
	ctx->media_usage++;
	return 0;
}

static void mfld_media_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	pr_debug("%s\n", __func__);

	snd_soc_dapm_disable_pin(&rtd->codec->dapm, "VirtBias");
	ctx->media_usage--;

	/* switch off PCM2 port if no media streams active */
	if (!ctx->media_usage)
		snd_soc_dai_set_tristate(codec_dai, 1);
}

static int mfld_voice_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	intel_scu_ipc_set_osc_clk0(true, CLK0_MSIC);
	ctx->voice_usage++;
	return 0;
}

static void mfld_voice_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_codec *codec = rtd->codec;
	pr_debug("%s\n", __func__);

	snd_soc_dapm_disable_pin(&rtd->codec->dapm, "VirtBias");
	ctx->voice_usage--;

	/* currently in slave mode. FM exiting and media still running,
	 * reconfigure PLL for media
	 */
	if (!ctx->sn95031_pcm1_master_mode && ctx->media_usage > 0) {
		pr_debug("FM exiting, reconfiguring PLL for media\n");
		snd_soc_codec_set_pll(codec, 0, SN95031_PLLIN, 1, 1);
		sn95031_configure_pll(codec, SN95031_ENABLE_PLL);
	}

}

static struct snd_soc_ops mfld_media_ops = {
	.startup = mfld_media_startup,
	.shutdown = mfld_media_shutdown,
	.hw_params = mfld_media_hw_params,
};

static struct snd_soc_ops mfld_voice_ops = {
	.startup = mfld_voice_startup,
	.shutdown = mfld_voice_shutdown,
	.hw_params = mfld_voice_hw_params,
};

static struct snd_soc_dai_link mfld_msic_dailink[] = {
	{
		.name = "Medfield Headset",
		.stream_name = "Headset",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_dai_name = "SN95031 Headset",
		.codec_name = "sn95031",
		.platform_name = "sst-platform",
		.init = mfld_init,
		.ignore_suspend = 1,
		.ops = &mfld_media_ops,
	},
	{
		.name = "Medfield Speaker",
		.stream_name = "Speaker",
		.cpu_dai_name = "Speaker-cpu-dai",
		.codec_dai_name = "SN95031 Speaker",
		.codec_name = "sn95031",
		.platform_name = "sst-platform",
#ifdef CONFIG_SND_MFLD_MONO_SPEAKER_SUPPORT
		.init = mfld_speaker_init,
#else
		.init = NULL,
#endif
		.ignore_suspend = 1,
		.ops = &mfld_media_ops,
	},
	{
		.name = "Medfield Voice",
		.stream_name = "Voice",
		.cpu_dai_name = "Voice-cpu-dai",
		.codec_dai_name = "SN95031 Voice",
		.codec_name = "sn95031",
		.platform_name = "sst-platform",
		.init = NULL,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &mfld_voice_ops,
	},
};

#ifdef CONFIG_PM

static int snd_mfld_mc_suspend(struct device *dev)
{
	pr_debug("In %s device name\n", __func__);
	snd_soc_suspend(dev);
	return 0;
}
static int snd_mfld_mc_resume(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_resume(dev);
	return 0;
}

static int snd_mfld_mc_poweroff(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_poweroff(dev);
	return 0;
}

#else
#define snd_mfld_mc_suspend NULL
#define snd_mfld_mc_resume NULL
#define snd_mfld_mc_poweroff NULL
#endif

static int mfld_card_stream_event(struct snd_soc_dapm_context *dapm, int event)
{
	struct snd_soc_codec *codec;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_pcm_runtime *rtd;
	int i;
	if (!dapm) {
		pr_err("%s: Null dapm\n", __func__);
		return -EINVAL;
	}
	/* we have only one codec in this machine */
	codec = list_entry(dapm->card->codec_dev_list.next,
			struct snd_soc_codec, card_list);
	if (!codec) {
		pr_err("%s: Null codec\n", __func__);
		return -EIO;
	}
	pr_debug("machine stream event: %d\n", event);
	if (event == SND_SOC_DAPM_STREAM_STOP) {
		if (!codec->active) {
			pr_debug("machine stream event: %d; codec NOT active\n", event);
			for (i = 0; i < card->num_rtd; i++) {
				rtd = &card->rtd[i];
				if (rtd->codec_dai->pop_wait) {
					pr_debug("%s: pop wait for rtd =%d; return \n", __func__, i);
					return 0;
				}
			}
			sn95031_configure_pll(codec, SN95031_DISABLE_PLL);
			return intel_scu_ipc_set_osc_clk0(false, CLK0_MSIC);
		}
	}
	return 0;
}

/* SoC card */
static struct snd_soc_card snd_soc_card_mfld = {
	.name = "medfield_audio",
	.dai_link = mfld_msic_dailink,
	.num_links = ARRAY_SIZE(mfld_msic_dailink),
	.dapm_widgets = mfld_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mfld_widgets),
	.dapm_routes = mfld_map,
	.num_dapm_routes = ARRAY_SIZE(mfld_map),
};

static irqreturn_t snd_mfld_jack_intr_handler(int irq, void *dev)
{
	struct mfld_mc_private *ctx = (struct mfld_mc_private *) dev;
	u16 intr_status = 0;

	memcpy_fromio(&intr_status, ((void *)(ctx->int_base)), sizeof(u16));
	/* not overwrite status here */
	spin_lock(&ctx->lock);
	/*To retrieve the jack_interrupt_status value (MSB)*/
	ctx->jack_interrupt_status |= 0x0F & (intr_status >> 8);
	/*To retrieve the oc_interrupt_status value (LSB)*/
	ctx->oc_interrupt_status |= 0x1F & intr_status;
	spin_unlock(&ctx->lock);
#ifdef CONFIG_HAS_WAKELOCK
	/*
	 * We don't have any call back from the jack detection completed.
	 * Take wakelock for two seconds to give time for the detection
	 * to finish. Jack detection is happening rarely so this doesn't
	 * have big impact to power consumption.
	 */
	wake_lock_timeout(ctx->jack_wake_lock, 2*HZ);
#endif
	return IRQ_WAKE_THREAD;
}

static irqreturn_t snd_mfld_codec_intr_detection(int irq, void *data)
{
	struct mfld_mc_private *ctx = (struct mfld_mc_private *) data;
	unsigned long flags;
	u8 jack_int_value = 0;

	if (ctx->mfld_jack.codec == NULL) {
		pr_debug("codec NULL returning..");
		spin_lock_irqsave(&ctx->lock, flags);
		ctx->jack_interrupt_status = 0;
		ctx->oc_interrupt_status = 0;
		spin_unlock_irqrestore(&ctx->lock, flags);
		goto ret;
	}
	spin_lock_irqsave(&ctx->lock, flags);
	if (!(ctx->jack_interrupt_status || ctx->oc_interrupt_status)) {
		spin_unlock_irqrestore(&ctx->lock, flags);
		pr_err("OC and Jack Intr with status 0, return....\n");
		goto ret;
	}
	if (ctx->oc_interrupt_status) {
		pr_info("OC int value: %d\n", ctx->oc_interrupt_status);
		ctx->oc_interrupt_status = 0;
	}
	if (ctx->jack_interrupt_status) {
		jack_int_value = ctx->jack_interrupt_status;
		ctx->jack_interrupt_status = 0;
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
	pr_info("%s jack_int_value = 0x%02x \n ",__func__,jack_int_value);
	if (jack_int_value)
		mfld_jack_detection(jack_int_value, &ctx->jack_work);

ret:
	return IRQ_HANDLED;
}

static ssize_t jack_retry_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ipc_device *ipcdev =
			container_of(dev, struct ipc_device, dev);

	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);

	return sprintf(buf, "%d\n", ctx->jack_poll_retry);
}

static ssize_t jack_retry_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	long value;
	struct ipc_device *ipcdev =
			container_of(dev, struct ipc_device, dev);

	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);
	ret = kstrtol(buf, 0, &value);
	if (ret < 0) {
		pr_err("kstrtol() failed with ret: %d\n", ret);
		return ret;
	}
	if (value > MAX_HS_DET_RETRY)
		ctx->jack_poll_retry = MAX_HS_DET_RETRY;
	else if (value < 0)
		ctx->jack_poll_retry = JACK_DET_RETRY;
	else
		ctx->jack_poll_retry = value;


	return count;
}

static DEVICE_ATTR(jack_poll_retry, 0644, jack_retry_show, jack_retry_set);

static ssize_t jack_interval_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ipc_device *ipcdev =
			container_of(dev, struct ipc_device, dev);

	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);

	return sprintf(buf, "%ld\n", ctx->jack_poll_interval);
}

static ssize_t jack_interval_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	long value;
	struct ipc_device *ipcdev =
			container_of(dev, struct ipc_device, dev);

	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);
	ret = kstrtol(buf, 0, &value);
	if (ret < 0) {
		pr_err("kstrtol() failed with ret: %d\n", ret);
		return ret;
	}
	if (value > MAX_DELAY)
		ctx->jack_poll_interval = MAX_DELAY;
	else if (value < 0)
		ctx->jack_poll_interval = JACK_POLL_INTERVAL;
	else
		ctx->jack_poll_interval = value;


	return count;
}


static DEVICE_ATTR(jack_poll_interval, 0644, jack_interval_show, jack_interval_set);

static int __devinit snd_mfld_mc_probe(struct ipc_device *ipcdev)
{
	int ret_val = 0, irq;
	struct mfld_mc_private *ctx;
	struct resource *irq_mem;

	pr_debug("snd_mfld_mc_probe called\n");

	/* retrive the irq number */
	irq = ipc_get_irq(ipcdev, 0);

	/* audio interrupt base of SRAM location where
	 * interrupts are stored by System FW
	 */
	ctx = kzalloc(sizeof(*ctx), GFP_ATOMIC);
	if (!ctx) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}
	spin_lock_init(&ctx->lock);
#ifdef CONFIG_HAS_WAKELOCK
	ctx->jack_wake_lock =
		kzalloc(sizeof(*(ctx->jack_wake_lock)), GFP_ATOMIC);
	wake_lock_init(ctx->jack_wake_lock,
		       WAKE_LOCK_SUSPEND, "jack_detect");
#endif

	irq_mem = ipc_get_resource_byname(ipcdev, IORESOURCE_MEM, "IRQ_BASE");
	if (!irq_mem) {
		pr_err("no mem resource given\n");
		ret_val = -ENODEV;
		goto unalloc;
	}

	/*GPADC handle for audio_detection*/
	ctx->audio_adc_handle =
		intel_mid_gpadc_alloc(MFLD_AUDIO_SENSOR,
				      MFLD_AUDIO_DETECT_CODE);
	if (!ctx->audio_adc_handle) {
		pr_err("invalid ADC handle\n");
		ret_val = -ENOMEM;
		goto unalloc;
	}
	INIT_DELAYED_WORK(&ctx->jack_work.work, mfld_jack_wq);

	/* Set default HS retry number*/
	ctx->jack_poll_retry = JACK_DET_RETRY;
	ctx->jack_poll_interval = JACK_POLL_INTERVAL;
	ctx->jack_status = 0;

	ret_val = device_create_file(&ipcdev->dev, &dev_attr_jack_poll_retry);
	if (ret_val < 0)
		pr_err("Err createing poll_retry sysfs file %d\n", ret_val);

	ret_val = device_create_file(&ipcdev->dev, &dev_attr_jack_poll_interval);
	if (ret_val < 0)
		pr_err("Err creating poll_interval sysfs file %d\n", ret_val);

	/* Store jack gpio pin number in ctx for future reference */
	ctx->jack_gpio = get_gpio_by_name("audio_jack_gpio");
	if (ctx->jack_gpio >= 0) {
		pr_info("GPIO for jack det is %d\n", ctx->jack_gpio);
		ret_val = gpio_request_one(ctx->jack_gpio,
						GPIOF_DIR_IN,
						"headset_detect_gpio");
		if (ret_val) {
			pr_err("Headset detect GPIO alloc fail:%d\n", ret_val);
			goto free_gpadc;
		}
	}

	ctx->int_base = ioremap_nocache(irq_mem->start, resource_size(irq_mem));
	if (!ctx->int_base) {
		pr_err("Mapping of cache failed\n");
		ret_val = -ENOMEM;
		goto free_gpio;
	}
	/* register for interrupt */
	ret_val = request_threaded_irq(irq, snd_mfld_jack_intr_handler,
			snd_mfld_codec_intr_detection,
			IRQF_SHARED | IRQF_NO_SUSPEND,
			ipcdev->dev.driver->name, ctx);
	if (ret_val) {
		pr_err("cannot register IRQ\n");
		goto free_gpio;
	}
	/* register the soc card */
	snd_soc_card_mfld.dev = &ipcdev->dev;
	snd_soc_card_mfld.dapm.stream_event = mfld_card_stream_event;
	snd_soc_card_set_drvdata(&snd_soc_card_mfld, ctx);
	ret_val = snd_soc_register_card(&snd_soc_card_mfld);
	if (ret_val) {
		pr_debug("snd_soc_register_card failed %d\n", ret_val);
		goto freeirq;
	}
	ctx->pdata = ipcdev->dev.platform_data;
	ipc_set_drvdata(ipcdev, &snd_soc_card_mfld);
	pr_debug("successfully exited probe\n");
	return ret_val;

freeirq:
	free_irq(irq, ctx);
free_gpio:
	gpio_free(ctx->jack_gpio);
free_gpadc:
	intel_mid_gpadc_free(ctx->audio_adc_handle);
	device_remove_file(&ipcdev->dev, &dev_attr_jack_poll_retry);
	device_remove_file(&ipcdev->dev, &dev_attr_jack_poll_interval);
unalloc:
	kfree(ctx);
	return ret_val;
}

static int __devexit snd_mfld_mc_remove(struct ipc_device *ipcdev)
{
	struct snd_soc_card *soc_card = ipc_get_drvdata(ipcdev);
	struct mfld_mc_private *ctx = snd_soc_card_get_drvdata(soc_card);
	pr_debug("snd_mfld_mc_remove called\n");
	free_irq(ipc_get_irq(ipcdev, 0), ctx);
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active(ctx->jack_wake_lock))
		wake_unlock(ctx->jack_wake_lock);
	wake_lock_destroy(ctx->jack_wake_lock);
	kfree(ctx->jack_wake_lock);
#endif
	device_remove_file(&ipcdev->dev, &dev_attr_jack_poll_retry);
	device_remove_file(&ipcdev->dev, &dev_attr_jack_poll_interval);
	cancel_delayed_work(&ctx->jack_work.work);
	intel_mid_gpadc_free(ctx->audio_adc_handle);
	if (ctx->jack_gpio >= 0)
		gpio_free(ctx->jack_gpio);

	mfld_therm_probe_deinit(ctx);

	kfree(ctx);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	ipc_set_drvdata(ipcdev, NULL);
	return 0;
}
static const struct dev_pm_ops snd_mfld_mc_pm_ops = {
	.suspend = snd_mfld_mc_suspend,
	.resume = snd_mfld_mc_resume,
	.poweroff = snd_mfld_mc_poweroff,
};

static struct ipc_driver snd_mfld_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "msic_audio",
		.pm   = &snd_mfld_mc_pm_ops,
	},
	.probe = snd_mfld_mc_probe,
	.remove = __devexit_p(snd_mfld_mc_remove),
};

static int __init snd_mfld_driver_init(void)
{
	pr_info("snd_mfld_driver_init called\n");
	return ipc_driver_register(&snd_mfld_mc_driver);
}
late_initcall(snd_mfld_driver_init);

static void __exit snd_mfld_driver_exit(void)
{
	pr_debug("snd_mfld_driver_exit called\n");
	ipc_driver_unregister(&snd_mfld_mc_driver);
}
module_exit(snd_mfld_driver_exit);

MODULE_DESCRIPTION("ASoC Intel(R) MID Machine driver");
MODULE_AUTHOR("Vinod Koul <vinod.koul@intel.com>");
MODULE_AUTHOR("Ramesh Babu K V <ramesh.babu@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("ipc:msic-audio");
