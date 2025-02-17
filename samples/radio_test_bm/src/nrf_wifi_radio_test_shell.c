/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* @file
 * @brief NRF Wi-Fi radio test shell module
 */

#include <nrf_wifi_radio_test_shell.h>
#include <util.h>

#include "radio_test/fmac_api.h"
#include <radio_test/nrf70_bm_core.h>
#include <radio_test/nrf70_bm_lib.h>

#ifdef CONFIG_ZEPHYR_SHELL
struct shell *shell_global;
#endif /* CONFIG_ZEPHYR_SHELL */

extern struct nrf70_bm_rt_wifi_drv_priv nrf70_bm_rt_priv;
struct nrf70_bm_rt_wifi_ctx *ctx = &nrf70_bm_rt_priv.rpu_ctx_bm;

static bool check_test_in_prog() {
  if (ctx->conf_params.rx) {
    RT_SHELL_PRINTF_ERROR("Disable RX\n");
    return false;
  }

  if (ctx->conf_params.tx) {
    RT_SHELL_PRINTF_ERROR("Disable TX\n");
    return false;
  }

  if (ctx->rf_test_run) {
    RT_SHELL_PRINTF_ERROR("Disable RF Test (%d)\n", ctx->rf_test);
    return false;
  }
  return true;
}

static bool check_valid_data_rate(unsigned char tput_mode, unsigned int nss,
                                  int dr) {
  bool is_mcs = dr & 0x80;
  bool ret = false;

  if (dr == -1)
    return true;

  if (is_mcs) {
    dr = dr & 0x7F;

    if (tput_mode & RPU_TPUT_MODE_HT) {
      if (nss == 2) {
        if ((dr >= 8) && (dr <= 15)) {
          ret = true;
        } else {
          RT_SHELL_PRINTF_ERROR("Invalid MIMO HT MCS: %d\n", dr);
        }
      }

      if (nss == 1) {
        if ((dr >= 0) && (dr <= 7)) {
          ret = true;
        } else {
          RT_SHELL_PRINTF_ERROR("Invalid SISO HT MCS: %d\n", dr);
        }
      }

    } else if (tput_mode == RPU_TPUT_MODE_VHT) {
      if ((dr >= 0 && dr <= 9)) {
        ret = true;
      } else {
        RT_SHELL_PRINTF_ERROR("Invalid VHT MCS value: %d\n", dr);
      }
    } else if (tput_mode == RPU_TPUT_MODE_HE_SU) {
      if ((dr >= 0 && dr <= 7)) {
        ret = true;
      } else {
        RT_SHELL_PRINTF_ERROR("Invalid HE_SU MCS value: %d\n", dr);
      }
    } else if (tput_mode == RPU_TPUT_MODE_HE_ER_SU) {
      if ((dr >= 0 && dr <= 2)) {
        ret = true;
      } else {
        RT_SHELL_PRINTF_ERROR("Invalid HE_ER_SU MCS value: %d\n", dr);
      }
    } else {
      RT_SHELL_PRINTF_ERROR("%s: Invalid throughput mode: %d\n", __func__, dr);
    }
  } else {
    if (tput_mode != RPU_TPUT_MODE_LEGACY) {
      ret = false;

      RT_SHELL_PRINTF_ERROR("Invalid rate_flags for legacy: %d\n", dr);
    }

    if ((dr == 1) || (dr == 2) || (dr == 55) || (dr == 11) || (dr == 6) ||
        (dr == 9) || (dr == 12) || (dr == 18) || (dr == 24) || (dr == 36) ||
        (dr == 48) || (dr == 54) || (dr == -1)) {
      ret = true;
    } else {
      RT_SHELL_PRINTF_ERROR("Invalid Legacy Rate value: %d\n", dr);
    }
  }

  return ret;
}

static bool check_valid_chan_2g(unsigned char chan_num) {
  if ((chan_num >= 1) && (chan_num <= 14)) {
    return true;
  }

  return false;
}

#ifndef CONFIG_NRF70_2_4G_ONLY
static bool check_valid_chan_5g(unsigned char chan_num) {
  if ((chan_num == 32) || (chan_num == 36) || (chan_num == 40) ||
      (chan_num == 44) || (chan_num == 48) || (chan_num == 52) ||
      (chan_num == 56) || (chan_num == 60) || (chan_num == 64) ||
      (chan_num == 68) || (chan_num == 96) || (chan_num == 100) ||
      (chan_num == 104) || (chan_num == 108) || (chan_num == 112) ||
      (chan_num == 116) || (chan_num == 120) || (chan_num == 124) ||
      (chan_num == 128) || (chan_num == 132) || (chan_num == 136) ||
      (chan_num == 140) || (chan_num == 144) || (chan_num == 149) ||
      (chan_num == 153) || (chan_num == 157) || (chan_num == 159) ||
      (chan_num == 161) || (chan_num == 163) || (chan_num == 165) ||
      (chan_num == 167) || (chan_num == 169) || (chan_num == 171) ||
      (chan_num == 173) || (chan_num == 175) || (chan_num == 177)) {
    return true;
  }

  return false;
}
#endif /* CONFIG_NRF70_2_4G_ONLY */

static bool check_valid_channel(unsigned char chan_num) {
  bool ret = false;

  ret = check_valid_chan_2g(chan_num);

  if (ret) {
    goto out;
  }

#ifndef CONFIG_NRF70_2_4G_ONLY
  ret = check_valid_chan_5g(chan_num);
#endif /* CONFIG_NRF70_2_4G_ONLY */

out:
  return ret;
}

static int check_channel_settings(unsigned char tput_mode,
                                  struct chan_params *chan) {
  if (tput_mode == RPU_TPUT_MODE_LEGACY) {
    if (chan->bw != RPU_CH_BW_20) {
      RT_SHELL_PRINTF_ERROR(
          "Invalid bandwidth setting for legacy channel = %d\n",
          chan->primary_num);
      return -1;
    }
  } else if (tput_mode == RPU_TPUT_MODE_HT) {
    if (chan->bw != RPU_CH_BW_20) {
      RT_SHELL_PRINTF_ERROR("Invalid bandwidth setting for HT channel = %d\n",
                            chan->primary_num);
      return -1;
    }
  } else if (tput_mode == RPU_TPUT_MODE_VHT) {
    if ((chan->primary_num >= 1) && (chan->primary_num <= 14)) {
      RT_SHELL_PRINTF_ERROR("VHT setting not allowed in 2.4GHz band\n");
      return -1;
    }

    if (chan->bw != RPU_CH_BW_20) {
      RT_SHELL_PRINTF_ERROR("Invalid bandwidth setting for VHT channel = %d\n",
                            chan->primary_num);
      return -1;
    }
  } else if ((tput_mode == RPU_TPUT_MODE_HE_SU) ||
             (tput_mode == RPU_TPUT_MODE_HE_TB) ||
             (tput_mode == RPU_TPUT_MODE_HE_ER_SU)) {
    if (chan->bw != RPU_CH_BW_20) {
      RT_SHELL_PRINTF_ERROR("Invalid bandwidth setting for HE channel = %d\n",
                            chan->primary_num);
      return -1;
    }
  } else {
    RT_SHELL_PRINTF_ERROR("Invalid throughput mode = %d\n", tput_mode);
    return -1;
  }

  return 0;
}

enum nrf_wifi_status
nrf_wifi_radio_test_conf_init(struct rpu_conf_params *conf_params) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  unsigned char country_code[NRF_WIFI_COUNTRY_CODE_LEN] = {0};

  /* Check and save regulatory country code currently set */
  if (strlen(conf_params->country_code)) {
    memcpy(country_code, conf_params->country_code, NRF_WIFI_COUNTRY_CODE_LEN);
  }

  memset(conf_params, 0, sizeof(*conf_params));

  /* Initialize values which are other than 0 */
  conf_params->op_mode = RPU_OP_MODE_RADIO_TEST;

  status = nrf_wifi_rt_fmac_rf_params_get(
      ctx->rpu_ctx, (struct nrf_wifi_phy_rf_params *)conf_params->rf_params);
  if (status != NRF_WIFI_STATUS_SUCCESS) {
    goto out;
  }

  conf_params->tx_pkt_nss = 1;
  conf_params->tx_pkt_gap_us = 0;

  conf_params->tx_power = MAX_TX_PWR_SYS_TEST;

  conf_params->chan.primary_num = 1;
  conf_params->tx_mode = 1;
  conf_params->tx_pkt_num = -1;
  conf_params->tx_pkt_len = 1400;
  conf_params->tx_pkt_preamble = 0;
  conf_params->tx_pkt_rate = 6;
  conf_params->he_ltf = 2;
  conf_params->he_gi = 2;
  conf_params->aux_adc_input_chain_id = 1;
  conf_params->ru_tone = 26;
  conf_params->ru_index = 1;
  conf_params->tx_pkt_cw = 15;
  conf_params->phy_calib = NRF_WIFI_DEF_PHY_CALIB;

  /* Store back the currently set country code */
  if (strlen(country_code)) {
    memcpy(conf_params->country_code, country_code, NRF_WIFI_COUNTRY_CODE_LEN);
  } else {
    memcpy(conf_params->country_code, "00", NRF_WIFI_COUNTRY_CODE_LEN);
  }
out:
  return status;
}

static int nrf_wifi_radio_test_set_defaults(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("Configuration init failed\n");
    return -ENOEXEC;
  }

  return 0;
}

static int nrf_wifi_radio_test_set_phy_calib_rxdc(size_t argc,
                                                  const char *argv[]) {
  char *ptr = NULL;
  unsigned long phy_calib_rxdc = 0;

  phy_calib_rxdc = strtoul(argv[1], &ptr, 10);

  if (phy_calib_rxdc > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid PHY RX DC calibration value(%lu).\n",
                          phy_calib_rxdc);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (phy_calib_rxdc)
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib | NRF_WIFI_PHY_CALIB_FLAG_RXDC);
  else
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib & ~(NRF_WIFI_PHY_CALIB_FLAG_RXDC));

  return 0;
}

static int nrf_wifi_radio_test_set_phy_calib_txdc(size_t argc,
                                                  const char *argv[]) {
  char *ptr = NULL;
  unsigned long phy_calib_txdc = 0;

  phy_calib_txdc = strtoul(argv[1], &ptr, 10);

  if (phy_calib_txdc > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid PHY TX DC calibration value(%lu).\n",
                          phy_calib_txdc);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (phy_calib_txdc)
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib | NRF_WIFI_PHY_CALIB_FLAG_TXDC);
  else
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib & ~(NRF_WIFI_PHY_CALIB_FLAG_TXDC));

  return 0;
}

static int nrf_wifi_radio_test_set_phy_calib_txpow(size_t argc,
                                                   const char *argv[]) {
  char *ptr = NULL;
  unsigned long phy_calib_txpow = 0;

  phy_calib_txpow = strtoul(argv[1], &ptr, 10);

  if (phy_calib_txpow > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid PHY TX power calibration value(%lu).\n",
                          phy_calib_txpow);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (phy_calib_txpow)
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib | NRF_WIFI_PHY_CALIB_FLAG_TXPOW);
  else
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib & ~(NRF_WIFI_PHY_CALIB_FLAG_TXPOW));

  return 0;
}

static int nrf_wifi_radio_test_set_phy_calib_rxiq(size_t argc,
                                                  const char *argv[]) {
  char *ptr = NULL;
  unsigned long phy_calib_rxiq = 0;

  phy_calib_rxiq = strtoul(argv[1], &ptr, 10);

  if (phy_calib_rxiq > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid PHY RX IQ calibration value(%lu).\n",
                          phy_calib_rxiq);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (phy_calib_rxiq)
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib | NRF_WIFI_PHY_CALIB_FLAG_RXIQ);
  else
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib & ~(NRF_WIFI_PHY_CALIB_FLAG_RXIQ));

  return 0;
}

static int nrf_wifi_radio_test_set_phy_calib_txiq(size_t argc,
                                                  const char *argv[]) {
  char *ptr = NULL;
  unsigned long phy_calib_txiq = 0;

  phy_calib_txiq = strtoul(argv[1], &ptr, 10);

  if (phy_calib_txiq > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid PHY TX IQ calibration value(%lu).\n",
                          phy_calib_txiq);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (phy_calib_txiq)
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib | NRF_WIFI_PHY_CALIB_FLAG_TXIQ);
  else
    ctx->conf_params.phy_calib =
        (ctx->conf_params.phy_calib & ~(NRF_WIFI_PHY_CALIB_FLAG_TXIQ));
  return 0;
}

static int nrf_wifi_radio_test_set_he_ltf(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long he_ltf = 0;

  he_ltf = strtoul(argv[1], &ptr, 10);

  if (he_ltf > 2) {
    RT_SHELL_PRINTF_ERROR("Invalid HE LTF value(%lu).\n", he_ltf);

    return -ENOEXEC;
  }

  ctx->conf_params.he_ltf = he_ltf;

  return 0;
}

static int nrf_wifi_radio_test_set_he_gi(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long he_gi = 0;

  he_gi = strtoul(argv[1], &ptr, 10);

  if (he_gi > 2) {
    RT_SHELL_PRINTF_ERROR("Invalid HE GI value(%lu).\n", he_gi);

    return -ENOEXEC;
  }

  ctx->conf_params.he_gi = he_gi;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_tput_mode(size_t argc,
                                                    const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val >= RPU_TPUT_MODE_MAX) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_tput_mode = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_sgi(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_sgi = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_preamble(size_t argc,
                                                   const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val >= RPU_PKT_PREAMBLE_MAX) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (ctx->conf_params.tx_pkt_tput_mode == 0) {
    if ((val != RPU_PKT_PREAMBLE_SHORT) && (val != RPU_PKT_PREAMBLE_LONG)) {
      RT_SHELL_PRINTF_ERROR("Invalid value %lu for legacy mode\n", val);
      return -ENOEXEC;
    }
  } else {
    if (val != RPU_PKT_PREAMBLE_MIXED) {
      RT_SHELL_PRINTF_ERROR(
          "Only mixed preamble mode supported in HT, VHT and HE modes\n");
      return -ENOEXEC;
    }
  }

  ctx->conf_params.tx_pkt_preamble = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_mcs(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  long val = -1;

  val = strtol(argv[1], &ptr, 10);

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (!(check_valid_data_rate(ctx->conf_params.tx_pkt_tput_mode,
                              ctx->conf_params.tx_pkt_nss, (val | 0x80)))) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_mcs = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_rate(size_t argc,
                                               const char *argv[]) {
  char *ptr = NULL;
  long val = -1;

  val = strtol(argv[1], &ptr, 10);

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  if (!(check_valid_data_rate(ctx->conf_params.tx_pkt_tput_mode,
                              ctx->conf_params.tx_pkt_nss, val))) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_rate = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_gap(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 200000) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_gap_us = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_num(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  long val = 0;

  val = strtol(argv[1], &ptr, 10);

  if ((val < -1) || (val == 0)) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_num = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_len(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val == 0) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_MAX) {
    RT_SHELL_PRINTF_ERROR("Throughput mode not set\n");
    return -ENOEXEC;
  }

  if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_LEGACY) {
    if (val > 4000) {
      RT_SHELL_PRINTF_ERROR("max 'tx_pkt_len' size for legacy is 4000 bytes\n");
      return -ENOEXEC;
    }
  } else if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_HE_TB) {
    if (ctx->conf_params.ru_tone == 26) {
      if (val >= 350) {
        RT_SHELL_PRINTF_ERROR("'tx_pkt_len' has to be less than 350 bytes\n");
        return -ENOEXEC;
      }
    } else if (ctx->conf_params.ru_tone == 52) {
      if (val >= 800) {
        RT_SHELL_PRINTF_ERROR("'tx_pkt_len' has to be less than 800 bytes\n");
        return -ENOEXEC;
      }
    } else if (ctx->conf_params.ru_tone == 106) {
      if (val >= 1800) {
        RT_SHELL_PRINTF_ERROR("'tx_pkt_len' has to be less than 1800 bytes\n");
        return -ENOEXEC;
      }
    } else if (ctx->conf_params.ru_tone == 242) {
      if (val >= 4000) {
        RT_SHELL_PRINTF_ERROR("'tx_pkt_len' has to be less than 4000 bytes\n");
        return -ENOEXEC;
      }
    } else {
      RT_SHELL_PRINTF_ERROR("'ru_tone' not set\n");
      return -ENOEXEC;
    }
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_len = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_power(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (((val > MAX_TX_PWR_RADIO_TEST) && (val != MAX_TX_PWR_SYS_TEST))) {
    RT_SHELL_PRINTF_ERROR("Invalid TX power setting\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_power = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx_tone_freq(size_t argc,
                                                const char *argv[]) {
  char *ptr = NULL;
  signed char val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if ((val > 10) || (val < -10)) {
    RT_SHELL_PRINTF_ERROR("'tx_tone_freq' has to be in between -10 to +10\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_tone_freq = val;

  return 0;
}

static int nrf_wifi_radio_test_set_rx_lna_gain(size_t argc,
                                               const char *argv[]) {
  char *ptr = NULL;
  signed char val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if ((val > 4) || (val < 0)) {
    RT_SHELL_PRINTF_ERROR("'lna_gain' has to be in between 0 to 4\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.lna_gain = val;

  return 0;
}

static int nrf_wifi_radio_test_set_rx_bb_gain(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  signed char val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if ((val > 31) || (val < 0)) {
    RT_SHELL_PRINTF_ERROR("'bb_gain' has to be in between 0 to 31\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.bb_gain = val;

  return 0;
}

static int nrf_wifi_radio_test_set_rx_capture_length(size_t argc,
                                                     const char *argv[]) {
  char *ptr = NULL;
  unsigned short int val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val >= 16384) {
    RT_SHELL_PRINTF_ERROR("'capture_length' has to be less than 16384\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.capture_length = val;

  return 0;
}

static int nrf_wifi_radio_test_set_rx_capture_timeout(size_t argc,
                                                     const char *argv[]) {
	char *ptr = NULL;
	unsigned short int val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > CAPTURE_DURATION_IN_SEC) {
		RT_SHELL_PRINTF_ERROR("'capture_timeout' has to be less than or equal to %d seconds\n", CAPTURE_DURATION_IN_SEC);
		return -ENOEXEC;
	}

	if (!check_test_in_prog()) {
		return -ENOEXEC;
	}

	ctx->conf_params.capture_timeout = val;

	return 0;
}

static int nrf_wifi_radio_test_set_ru_tone(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if ((val != 26) && (val != 52) && (val != 106) && (val != 242)) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.ru_tone = val;

  return 0;
}

static int nrf_wifi_radio_test_set_ru_index(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (ctx->conf_params.ru_tone == 26) {
    if ((val < 1) || (val > 9)) {
      RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

      return -ENOEXEC;
    }
  } else if (ctx->conf_params.ru_tone == 52) {
    if ((val < 1) || (val > 4)) {
      RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

      return -ENOEXEC;
    }
  } else if (ctx->conf_params.ru_tone == 106) {
    if ((val < 1) || (val > 2)) {
      RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

      return -ENOEXEC;
    }
  } else if (ctx->conf_params.ru_tone == 242) {
    if ((val != 1)) {
      RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

      return -ENOEXEC;
    }
  } else {
    RT_SHELL_PRINTF_ERROR("RU tone not set\n");

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.ru_index = val;

  return 0;
}

static int nrf_wifi_radio_test_init(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (!(check_valid_channel(val))) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);
    return -ENOEXEC;
  }

  if (ctx->conf_params.rx) {
    RT_SHELL_PRINTF_INFO("Disabling ongoing RX test\n");

    ctx->conf_params.rx = 0;

    status = nrf_wifi_rt_fmac_prog_rx(ctx->rpu_ctx, &ctx->conf_params);

    if (status != NRF_WIFI_STATUS_SUCCESS) {
      RT_SHELL_PRINTF_ERROR("Disabling RX failed\n");
      return -ENOEXEC;
    }
  }

  if (ctx->conf_params.tx) {
    RT_SHELL_PRINTF_INFO("Disabling ongoing TX test\n");

    ctx->conf_params.tx = 0;

    status = nrf_wifi_rt_fmac_prog_tx(ctx->rpu_ctx, &ctx->conf_params);

    if (status != NRF_WIFI_STATUS_SUCCESS) {
      RT_SHELL_PRINTF_ERROR("Disabling TX failed\n");
      return -ENOEXEC;
    }
  }

  if (ctx->rf_test_run) {
    if (ctx->rf_test != NRF_WIFI_RF_TEST_TX_TONE) {
      RT_SHELL_PRINTF_ERROR("Unexpected: RF Test (%d) running\n", ctx->rf_test);

      return -ENOEXEC;
    }

    RT_SHELL_PRINTF_INFO("Disabling ongoing TX tone test\n");

    status = nrf_wifi_rt_fmac_rf_test_tx_tone(ctx->rpu_ctx, 0,
					      ctx->conf_params.tx_tone_freq,
                                              ctx->conf_params.tx_power);

    if (status != NRF_WIFI_STATUS_SUCCESS) {
      RT_SHELL_PRINTF_ERROR("Disabling TX tone test failed\n");
      return -ENOEXEC;
    }

    ctx->rf_test_run = false;
    ctx->rf_test = NRF_WIFI_RF_TEST_MAX;
  }

  status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("Configuration init failed\n");
    return -ENOEXEC;
  }

  ctx->conf_params.chan.primary_num = val;

  status = nrf_wifi_rt_fmac_radio_test_init(ctx->rpu_ctx, &ctx->conf_params);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("Programming init failed\n");
    return -ENOEXEC;
  }

  return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_cw(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  long val = 0;

  val = strtol(argv[1], &ptr, 10);

  if (!((val == 0) || (val == 3) || (val == 7) || (val == 15) || (val == 31) ||
        (val == 63) || (val == 127) || (val == 255) || (val == 511) ||
        (val == 1023))) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.tx_pkt_cw = val;

  return 0;
}

static int nrf_wifi_radio_test_set_tx(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      return -ENOEXEC;
    }

    if (check_channel_settings(ctx->conf_params.tx_pkt_tput_mode,
                               &ctx->conf_params.chan) != 0) {
      RT_SHELL_PRINTF_ERROR("Invalid channel settings\n");
      return -ENOEXEC;
    }

    /** Max TX power values differ based on the test being performed.
     * For TX EVM Vs Power, Max TX power required is
     * "MAX_TX_PWR_RADIO_TEST" (24dB) whereas for testing the
     * Max TX power for which both EVM and spectrum mask are passing
     * for specific band and MCS/rate, TX power values will be read from
     * RF params string.
     */
    if (ctx->conf_params.tx_power != MAX_TX_PWR_SYS_TEST) {
      /** Max TX power is represented in 0.25dB resolution
       * So,multiply 4 to MAX_TX_PWR_RADIO_TEST and
       * configure the RF params corresponding to Max TX power.
       */
      memset(&ctx->conf_params.rf_params[NRF_WIFI_TX_PWR_CEIL_BYTE_OFFSET],
             (MAX_TX_PWR_RADIO_TEST << 2), sizeof(struct nrf_wifi_tx_pwr_ceil));
    }
  }

  ctx->conf_params.tx = val;

  status = nrf_wifi_rt_fmac_prog_tx(ctx->rpu_ctx, &ctx->conf_params);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("Programming TX failed\n");
    return -ENOEXEC;
  }

  return 0;
}

static int nrf_wifi_radio_test_set_rx(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    return -ENOEXEC;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      return -ENOEXEC;
    }
  }

  ctx->conf_params.rx = val;

  status = nrf_wifi_rt_fmac_prog_rx(ctx->rpu_ctx, &ctx->conf_params);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("Programming RX failed\n");
    return -ENOEXEC;
  }

  return 0;
}

#ifdef CONFIG_NRF70_SR_COEX_RF_SWITCH
static int nrf_wifi_radio_test_sr_ant_switch_ctrl(size_t argc,
                                                  const char *argv[]) {
  unsigned int val;

  if (argc < 2) {
    shell_fprintf(SHELL_ERROR, "invalid # of args : %d\n", argc);
    return -ENOEXEC;
  }

  val = strtoul(argv[1], NULL, 0);

  ctx->conf_params.sr_ant_switch_ctrl = val;

  return sr_ant_switch(val);
}
#endif /* CONFIG_NRF70_SR_COEX_RF_SWITCH */

static int nrf_wifi_radio_test_rx_cap(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  unsigned long rx_cap_type = 0;
  unsigned char *rx_cap_buf = NULL;
  unsigned char capture_status = 0;
  char *ptr = NULL;
  unsigned int i = 0;
  int ret = -ENOEXEC;

  rx_cap_type = strtoul(argv[1], &ptr, 10);

  if ((rx_cap_type != NRF_WIFI_RF_TEST_RX_ADC_CAP) &&
      (rx_cap_type != NRF_WIFI_RF_TEST_RX_STAT_PKT_CAP) &&
      (rx_cap_type != NRF_WIFI_RF_TEST_RX_DYN_PKT_CAP)) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", rx_cap_type);

    return -ENOEXEC;
  }

  if (!ctx->conf_params.capture_length) {
    RT_SHELL_PRINTF_ERROR("%s: Invalid rx_capture_length %d\n", __func__,
                          ctx->conf_params.capture_length);
    goto out;
  }

  if (rx_cap_type == NRF_WIFI_RF_TEST_RX_DYN_PKT_CAP) {
    if (!ctx->conf_params.capture_timeout) {
        RT_SHELL_PRINTF_ERROR("%s: Invalid rx_capture_timeout %d\n",__func__,
                              ctx->conf_params.capture_timeout);
    goto out;
    }
  }

  if (!check_test_in_prog()) {
    goto out;
  }

  rx_cap_buf = k_calloc((ctx->conf_params.capture_length * 3), sizeof(char));

  if (!rx_cap_buf) {
    RT_SHELL_PRINTF_ERROR("%s: Unable to allocate (%d) bytes for RX capture\n",
                          __func__, (ctx->conf_params.capture_length * 3));
    goto out;
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_RX_ADC_CAP;

  status = nrf_wifi_rt_fmac_rf_test_rx_cap(
      ctx->rpu_ctx, rx_cap_type, rx_cap_buf, ctx->conf_params.capture_length,
      ctx->conf_params.capture_timeout, ctx->conf_params.lna_gain, ctx->conf_params.bb_gain,
      &capture_status);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("RX ADC capture programming failed\n");
    goto out;
  }

  if (capture_status == 0) {
    RT_SHELL_PRINTF_INFO("************* RX capture data ***********\n");

    for (i = 0; i < (ctx->conf_params.capture_length); i++) {
      RT_SHELL_PRINTF_INFO("%02X%02X%02X\n", rx_cap_buf[i * 3 + 2],
                          rx_cap_buf[i * 3 + 1], rx_cap_buf[i * 3 + 0]);
    }
	} else if (capture_status == 1) {
    RT_SHELL_PRINTF_INFO("\n************* Capture failed ***********\n");
	} else {
    RT_SHELL_PRINTF_INFO("\n************* Packet detection failed ***********\n");
	}

  ret = 0;
out:
  if (rx_cap_buf)
    k_free(rx_cap_buf);

  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_test_tx_tone(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;
  int ret = -ENOEXEC;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    goto out;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      goto out;
    }
  }

  status = nrf_wifi_rt_fmac_rf_test_tx_tone(ctx->rpu_ctx, (unsigned char)val,
					    ctx->conf_params.tx_tone_freq,
					    ctx->conf_params.tx_power);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("TX tone programming failed\n");
    goto out;
  }

  if (val == 1) {
    ctx->rf_test_run = true;
    ctx->rf_test = NRF_WIFI_RF_TEST_TX_TONE;
  } else {
    ctx->rf_test_run = false;
    ctx->rf_test = NRF_WIFI_RF_TEST_MAX;
  }

  ret = 0;
out:
  return ret;
}

static int nrf_wifi_radio_set_dpd(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;
  int ret = -ENOEXEC;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    goto out;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      goto out;
    }
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_DPD;

  status = nrf_wifi_rt_fmac_rf_test_dpd(ctx->rpu_ctx, val);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("DPD programming failed\n");
    goto out;
  }

  ret = 0;
out:
  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_get_temperature(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;
  int ret = -ENOEXEC;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    goto out;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      goto out;
    }
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_GET_TEMPERATURE;

  status = nrf_wifi_rt_fmac_rf_get_temp(ctx->rpu_ctx);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("DPD programming failed\n");
    goto out;
  }

  ret = 0;
out:
  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_get_rf_rssi(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;
  int ret = -ENOEXEC;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);

    goto out;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      goto out;
    }
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_RF_RSSI;

  status = nrf_wifi_rt_fmac_rf_get_rf_rssi(ctx->rpu_ctx);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("RF RSSI get failed\n");
    goto out;
  }

  ret = 0;
out:
  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_set_xo_val(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  unsigned long val = 0;
  int ret = -ENOEXEC;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 0x7f) {
    RT_SHELL_PRINTF_ERROR("Invalid value XO value should be <= 0x7f\n");

    goto out;
  }

  if (val < 0) {
    RT_SHELL_PRINTF_ERROR("Invalid value XO value should be >= 0\n");

    goto out;
  }

  if (val == 1) {
    if (!check_test_in_prog()) {
      goto out;
    }
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_XO_CALIB;

  status = nrf_wifi_rt_fmac_set_xo_val(ctx->rpu_ctx, (unsigned char)val);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("XO value programming failed\n");
    goto out;
  }

  ctx->conf_params.rf_params[NRF_WIFI_XO_FREQ_BYTE_OFFSET] = val;

  ret = 0;
out:
  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_comp_opt_xo_val(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

  int ret = -ENOEXEC;

  if (!check_test_in_prog()) {
    goto out;
  }

  ctx->rf_test_run = true;
  ctx->rf_test = NRF_WIFI_RF_TEST_XO_TUNE;

  status = nrf_wifi_rt_fmac_rf_test_compute_xo(ctx->rpu_ctx);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("XO value computation failed\n");
    goto out;
  }

  ret = 0;
out:
  ctx->rf_test_run = false;
  ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

  return ret;
}

static int nrf_wifi_radio_test_set_ant_gain(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long ant_gain = 0;

  ant_gain = strtoul(argv[1], &ptr, 10);

  if ((ant_gain < 0) || (ant_gain > 6)) {
    RT_SHELL_PRINTF_ERROR("Invalid antenna gain setting\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  memset(&ctx->conf_params.rf_params[ANT_GAIN_2G_OFST], ant_gain, NUM_ANT_GAIN);

  return 0;
}

static int nrf_wifi_radio_test_set_edge_bo(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long edge_bo = 0;

  edge_bo = strtoul(argv[1], &ptr, 10);

  if ((edge_bo < 0) || (edge_bo > 10)) {
    RT_SHELL_PRINTF_ERROR("Invalid edge backoff setting\n");
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  memset(&ctx->conf_params.rf_params[BAND_2G_LW_ED_BKF_DSSS_OFST], edge_bo,
         NUM_EDGE_BACKOFF);

  return 0;
}

static int nrf_wifi_radio_test_show_cfg(size_t argc, const char *argv[]) {
  struct rpu_conf_params *conf_params = NULL;

  conf_params = &ctx->conf_params;

  RT_SHELL_PRINTF_INFO("************* Configured Parameters ***********\n");
  RT_SHELL_PRINTF_INFO("\n");

  RT_SHELL_PRINTF_INFO("tx_pkt_tput_mode = %d\n",
                       conf_params->tx_pkt_tput_mode);

  RT_SHELL_PRINTF_INFO("tx_pkt_sgi = %d\n", conf_params->tx_pkt_sgi);

  RT_SHELL_PRINTF_INFO("tx_pkt_preamble = %d\n", conf_params->tx_pkt_preamble);

  RT_SHELL_PRINTF_INFO("tx_pkt_mcs = %d\n", conf_params->tx_pkt_mcs);

  if (conf_params->tx_pkt_rate == 5)
    RT_SHELL_PRINTF_INFO("tx_pkt_rate = 5.5\n");
  else
    RT_SHELL_PRINTF_INFO("tx_pkt_rate = %d\n", conf_params->tx_pkt_rate);

  RT_SHELL_PRINTF_INFO("tx_pkt_gap = %d\n", conf_params->tx_pkt_gap_us);

  RT_SHELL_PRINTF_INFO(
      "phy_calib_rxdc = %d\n",
      (conf_params->phy_calib & NRF_WIFI_PHY_CALIB_FLAG_RXDC) ? 1 : 0);

  RT_SHELL_PRINTF_INFO(
      "phy_calib_txdc = %d\n",
      (conf_params->phy_calib & NRF_WIFI_PHY_CALIB_FLAG_TXDC) ? 1 : 0);

  RT_SHELL_PRINTF_INFO(
      "phy_calib_txpow = %d\n",
      (conf_params->phy_calib & NRF_WIFI_PHY_CALIB_FLAG_TXPOW) ? 1 : 0);

  RT_SHELL_PRINTF_INFO(
      "phy_calib_rxiq = %d\n",
      (conf_params->phy_calib & NRF_WIFI_PHY_CALIB_FLAG_RXIQ) ? 1 : 0);

  RT_SHELL_PRINTF_INFO(
      "phy_calib_txiq = %d\n",
      (conf_params->phy_calib & NRF_WIFI_PHY_CALIB_FLAG_TXIQ) ? 1 : 0);

  RT_SHELL_PRINTF_INFO("tx_pkt_num = %d\n", conf_params->tx_pkt_num);

  RT_SHELL_PRINTF_INFO("tx_pkt_len = %d\n", conf_params->tx_pkt_len);

  RT_SHELL_PRINTF_INFO("tx_power = %d\n", conf_params->tx_power);

  RT_SHELL_PRINTF_INFO("he_ltf = %d\n", conf_params->he_ltf);

  RT_SHELL_PRINTF_INFO("he_gi = %d\n", conf_params->he_gi);

  RT_SHELL_PRINTF_INFO("xo_val = %d\n",
                       conf_params->rf_params[NRF_WIFI_XO_FREQ_BYTE_OFFSET]);

  RT_SHELL_PRINTF_INFO("init = %d\n", conf_params->chan.primary_num);

  RT_SHELL_PRINTF_INFO("tx = %d\n", conf_params->tx);

  RT_SHELL_PRINTF_INFO("rx = %d\n", conf_params->rx);

  RT_SHELL_PRINTF_INFO("tx_tone_freq = %d\n", conf_params->tx_tone_freq);

  RT_SHELL_PRINTF_INFO("rx_lna_gain = %d\n", conf_params->lna_gain);

  RT_SHELL_PRINTF_INFO("rx_bb_gain = %d\n", conf_params->bb_gain);

  RT_SHELL_PRINTF_INFO("rx_capture_length = %d\n", conf_params->capture_length);

  RT_SHELL_PRINTF_INFO("rx_capture_timeout = %d\n", conf_params->capture_timeout);
#if defined(CONFIG_BOARD_NRF7002DK_NRF7001_NRF5340_CPUAPP) ||                  \
    defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP)
  RT_SHELL_PRINTF_INFO("sr_ant_switch_ctrl = %d\n",
                       conf_params->sr_ant_switch_ctrl);
#endif /* CONFIG_BOARD_NRF700XDK_NRF5340 */

  RT_SHELL_PRINTF_INFO("wlan_ant_switch_ctrl = %d\n",
                       conf_params->wlan_ant_switch_ctrl);
  RT_SHELL_PRINTF_INFO("tx_pkt_cw = %d\n", conf_params->tx_pkt_cw);

  RT_SHELL_PRINTF_INFO("reg_domain = %c%c\n", conf_params->country_code[0],
                       conf_params->country_code[1]);

  RT_SHELL_PRINTF_INFO("bypass_reg_domain = %d\n",
                       conf_params->bypass_regulatory);

  RT_SHELL_PRINTF_INFO("ru_tone = %d\n", conf_params->ru_tone);

  RT_SHELL_PRINTF_INFO("ru_index = %d\n", conf_params->ru_index);

  return 0;
}

static int nrf_wifi_radio_test_get_stats(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  struct rpu_rt_op_stats stats;

  memset(&stats, 0, sizeof(stats));

  status =
      nrf_wifi_rt_fmac_stats_get(ctx->rpu_ctx, ctx->conf_params.op_mode, &stats);

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("nrf_wifi_fmac_stats_get failed\n");
    return -ENOEXEC;
  }

  RT_SHELL_PRINTF_INFO("************* PHY STATS ***********\n");

  RT_SHELL_PRINTF_INFO("rssi_avg = %d dBm\n", stats.fw.phy.rssi_avg);

  RT_SHELL_PRINTF_INFO("ofdm_crc32_pass_cnt=%d\n",
                       stats.fw.phy.ofdm_crc32_pass_cnt);

  RT_SHELL_PRINTF_INFO("ofdm_crc32_fail_cnt=%d\n",
                       stats.fw.phy.ofdm_crc32_fail_cnt);

  RT_SHELL_PRINTF_INFO("dsss_crc32_pass_cnt=%d\n",
                       stats.fw.phy.dsss_crc32_pass_cnt);

  RT_SHELL_PRINTF_INFO("dsss_crc32_fail_cnt=%d\n",
                       stats.fw.phy.dsss_crc32_fail_cnt);

  return 0;
}

/* See enum CD2CM_MSG_ID_T in RPU Coexistence Manager API */
#define CD2CM_UPDATE_SWITCH_CONFIG 0x7
static int nrf_wifi_radio_test_wlan_switch_ctrl(size_t argc,
                                                const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  char *ptr = NULL;
  struct coex_wlan_switch_ctrl params = {0};

  if (argc < 2) {
    RT_SHELL_PRINTF_ERROR("Invalid number of parameters\n");

    return -ENOEXEC;
  }

  params.rpu_msg_id = CD2CM_UPDATE_SWITCH_CONFIG;
  params.switch_A = strtoul(argv[1], &ptr, 10);

  if (params.switch_A > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid WLAN switch config (%d)\n", params.switch_A);

    return -ENOEXEC;
  }

  ctx->conf_params.wlan_ant_switch_ctrl = params.switch_A;

  status = nrf_wifi_fmac_conf_srcoex(ctx->rpu_ctx, &params, sizeof(params));

  if (status != NRF_WIFI_STATUS_SUCCESS) {
    RT_SHELL_PRINTF_ERROR("WLAN switch configuration failed\n");
    return -ENOEXEC;
  }

  return 0;
}

static int nrf_wifi_radio_test_reg_domain(size_t argc, const char *argv[]) {
  enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
  int ret = -ENOEXEC;
  struct nrf70_bm_regulatory_info reg_domain_info = {0};

  if (argv[1]) {
    if (strlen(argv[1]) != 2) {
      RT_SHELL_PRINTF_WARNING(
          "Invalid reg domain: Length should be two letters/digits\n");
      goto out;
    }

    /* Two letter country code with special case of 00 for WORLD */
    if (((argv[1][0] < 'A' || argv[1][0] > 'Z') ||
         (argv[1][1] < 'A' || argv[1][1] > 'Z')) &&
        (argv[1][0] != '0' || argv[1][1] != '0')) {
      RT_SHELL_PRINTF_WARNING("Invalid reg domain %c%c\n", argv[1][0],
                              argv[2][1]);
      goto out;
    }

    ctx->conf_params.country_code[0] = argv[1][0];
    ctx->conf_params.country_code[1] = argv[1][1];

    if (!check_test_in_prog()) {
      goto out;
    }

    memcpy(reg_domain_info.country_code,
           ctx->conf_params.country_code,
           NRF_WIFI_COUNTRY_CODE_LEN);

    status = nrf70_bm_rt_set_reg(&reg_domain_info);

    if (status != NRF_WIFI_STATUS_SUCCESS) {
      RT_SHELL_PRINTF_ERROR("Regulatory programming failed\n");
      goto out;
    }
  } else {
    reg_domain_info.chan_info = malloc(sizeof(struct nrf70_bm_reg_chan_info) * NRF70_MAX_CHANNELS);
    if (!reg_domain_info.chan_info) {
      printf("Failed to allocate memory for regulatory info\n");
      goto out;
    }

    memset(reg_domain_info.chan_info,
           0,
           (sizeof(struct nrf70_bm_reg_chan_info) * NRF70_MAX_CHANNELS));

    status = nrf70_bm_rt_get_reg(&reg_domain_info);

    if (status != NRF_WIFI_STATUS_SUCCESS) {
      RT_SHELL_PRINTF_ERROR("Regulatory information get failed\n");
      goto out;
    }

    RT_SHELL_PRINTF_INFO("Reg domain set to: %s\n", reg_domain_info.country_code);
  }

  ret = 0;
out:
  if (reg_domain_info.chan_info) {
    free(reg_domain_info.chan_info);
  }
  return ret;
}

static int nrf_wifi_radio_test_set_bypass_reg(size_t argc, const char *argv[]) {
  char *ptr = NULL;
  unsigned long val = 0;

  val = strtoul(argv[1], &ptr, 10);

  if (val > 1) {
    RT_SHELL_PRINTF_ERROR("Invalid value %lu\n", val);
    return -ENOEXEC;
  }

  if (!check_test_in_prog()) {
    return -ENOEXEC;
  }

  ctx->conf_params.bypass_regulatory = val;

  return 0;
}

#ifdef CONFIG_ZEPHYR_SHELL
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_defaults)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_phy_calib_rxdc)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_phy_calib_txdc)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_phy_calib_txpow)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_phy_calib_rxiq)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_phy_calib_txiq)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_he_ltf)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_he_gi)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_tput_mode)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_sgi)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_preamble)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_mcs)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_rate)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_gap)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_num)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_len)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_power)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_ru_tone)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_ru_index)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_init)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_rx)
#ifdef CONFIG_NRF70_SR_COEX_RF_SWITCH
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_sr_ant_switch_ctrl)
#endif /* CONFIG_NRF70_SR_COEX_RF_SWITCH */
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_rx_lna_gain)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_rx_bb_gain)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_rx_capture_length)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_rx_capture_timeout)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_rx_cap)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_tone_freq)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_tx_tone)
DEFINE_CMD_HANDLER(nrf_wifi_radio_set_dpd)
DEFINE_CMD_HANDLER(nrf_wifi_radio_get_temperature)
DEFINE_CMD_HANDLER(nrf_wifi_radio_get_rf_rssi)
DEFINE_CMD_HANDLER(nrf_wifi_radio_set_xo_val)
DEFINE_CMD_HANDLER(nrf_wifi_radio_comp_opt_xo_val)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_show_cfg)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_get_stats)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_wlan_switch_ctrl)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_tx_pkt_cw)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_reg_domain)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_bypass_reg)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_ant_gain)
DEFINE_CMD_HANDLER(nrf_wifi_radio_test_set_edge_bo)


SHELL_STATIC_SUBCMD_SET_CREATE(
    nrf_wifi_radio_test_subcmds,
    SHELL_CMD_ARG(set_defaults, NULL,
                  "Reset configuration parameter to their default values",
                  RTSH(nrf_wifi_radio_test_set_defaults), 1, 0),
    SHELL_CMD_ARG(phy_calib_rxdc, NULL,
                  "0 - Disable RX DC calibration\n"
                  "1 - Enable RX DC calibration",
                  RTSH(nrf_wifi_radio_test_set_phy_calib_rxdc), 2, 0),
    SHELL_CMD_ARG(phy_calib_txdc, NULL,
                  "0 - Disable TX DC calibration\n"
                  "1 - Enable TX DC calibration",
                  RTSH(nrf_wifi_radio_test_set_phy_calib_txdc), 2, 0),
    SHELL_CMD_ARG(phy_calib_txpow, NULL,
                  "0 - Disable TX power calibration\n"
                  "1 - Enable TX power calibration",
                  RTSH(nrf_wifi_radio_test_set_phy_calib_txpow), 2, 0),
    SHELL_CMD_ARG(phy_calib_rxiq, NULL,
                  "0 - Disable RX IQ calibration\n"
                  "1 - Enable RX IQ calibration",
                  RTSH(nrf_wifi_radio_test_set_phy_calib_rxiq), 2, 0),
    SHELL_CMD_ARG(phy_calib_txiq, NULL,
                  "0 - Disable TX IQ calibration\n"
                  "1 - Enable TX IQ calibration",
                  RTSH(nrf_wifi_radio_test_set_phy_calib_txiq), 2, 0),
    SHELL_CMD_ARG(he_ltf, NULL,
                  "0 - 1x HE LTF\n"
                  "1 - 2x HE LTF\n"
                  "2 - 4x HE LTF                                        ",
                  RTSH(nrf_wifi_radio_test_set_he_ltf), 2, 0),
    SHELL_CMD_ARG(he_gi, NULL,
                  "0 - 0.8 us\n"
                  "1 - 1.6 us\n"
                  "2 - 3.2 us                                           ",
                  RTSH(nrf_wifi_radio_test_set_he_gi), 2, 0),
    SHELL_CMD_ARG(tx_pkt_tput_mode, NULL,
                  "0 - Legacy mode\n"
                  "1 - HT mode\n"
#ifndef CONFIG_NRF70_2_4G_ONLY
                  "2 - VHT mode\n"
#endif /* CONFIG_NRF70_2_4G_ONLY */
                  "3 - HE(SU) mode\n"
                  "4 - HE(ER SU) mode\n"
                  "5 - HE (TB) mode                                   ",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_tput_mode), 2, 0),
    SHELL_CMD_ARG(tx_pkt_sgi, NULL,
                  "0 - Disable\n"
                  "1 - Enable",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_sgi), 2, 0),
    SHELL_CMD_ARG(tx_pkt_preamble, NULL,
                  "0 - Long preamble\n"
                  "1 - Short preamble\n"
                  "2 - Mixed preamble                                   ",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_preamble), 2, 0),
    SHELL_CMD_ARG(tx_pkt_mcs, NULL,
                  "-1    - Not being used\n"
                  "<val> - MCS index to be used",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_mcs), 2, 0),
    SHELL_CMD_ARG(tx_pkt_rate, NULL,
                  "-1    - Not being used\n"
                  "<val> - Legacy rate to be used in Mbps (1, 2, 5.5, 11, 6, "
                  "9, 12, 18, 24, 36, 48, 54)",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_rate), 2, 0),
    SHELL_CMD_ARG(tx_pkt_gap, NULL,
                  "<val> - Interval between TX packets in us (Min: 200, Max: "
                  "200000, Default: 200)",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_gap), 2, 0),
    SHELL_CMD_ARG(tx_pkt_num, NULL,
                  "-1    - Transmit infinite packets\n"
                  "<val> - Number of packets to transmit",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_num), 2, 0),
    SHELL_CMD_ARG(tx_pkt_len, NULL,
                  "<val> - Length of the packet (in bytes) to be transmitted "
                  "(Default: 1400)",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_len), 2, 0),
    SHELL_CMD_ARG(tx_power, NULL, "<val> - Value in dBm",
                  RTSH(nrf_wifi_radio_test_set_tx_power), 2, 0),
    SHELL_CMD_ARG(ru_tone, NULL,
                  "<val> - Resource unit (RU) size (26,52,106 or 242)",
                  RTSH(nrf_wifi_radio_test_set_ru_tone), 2, 0),
    SHELL_CMD_ARG(ru_index, NULL,
                  "<val> - Location of resource unit (RU) in 20 MHz spectrum\n"
                  "        Valid Values:\n"
                  "           For 26 ru_tone: 1 to 9\n"
                  "           For 52 ru_tone:  1 to 4\n"
                  "           For 106 ru_tone:  1 to 2\n"
                  "           For 242 ru_tone:  1",
                  RTSH(nrf_wifi_radio_test_set_ru_index), 2, 0),
    SHELL_CMD_ARG(init, NULL, "<val> - Primary channel number",
                  RTSH(nrf_wifi_radio_test_init), 2, 0),
    SHELL_CMD_ARG(tx, NULL,
                  "0 - Disable TX\n"
                  "1 - Enable TX",
                  RTSH(nrf_wifi_radio_test_set_tx), 2, 0),
    SHELL_CMD_ARG(rx, NULL,
                  "0 - Disable RX\n"
                  "1 - Enable RX",
                  RTSH(nrf_wifi_radio_test_set_rx), 2, 0),
#ifdef CONFIG_NRF70_SR_COEX_RF_SWITCH
    SHELL_CMD_ARG(sr_ant_switch_ctrl, NULL,
                  "0 - Switch set to use the BLE antenna\n"
                  "1 - Switch set to use the shared Wi-Fi antenna",
                  RTSH(nrf_wifi_radio_test_sr_ant_switch_ctrl), 2, 0),
#endif /* CONFIG_NRF70_SR_COEX_RF_SWITCH */
    SHELL_CMD_ARG(rx_lna_gain, NULL,
                  "<val> - LNA gain to be configured.\n"
                  "0 = 24 dB\n"
                  "1 = 18 dB\n"
                  "2 = 12 dB\n"
                  "3 = 0 dB\n"
                  "4 = -12 dB                         ",
                  RTSH(nrf_wifi_radio_test_set_rx_lna_gain), 2, 0),
    SHELL_CMD_ARG(rx_bb_gain, NULL,
                  "<val> - Baseband gain to be configured\n"
                  "It is a 5 bit value. Supports 64dB range in steps of 2dB",
                  RTSH(nrf_wifi_radio_test_set_rx_bb_gain), 2, 0),
    SHELL_CMD_ARG(rx_capture_length, NULL,
                  "<val> - Number of RX samples to be captured\n"
                  "Max allowed length is 16384 complex samples",
                  RTSH(nrf_wifi_radio_test_set_rx_capture_length), 2,
                  0),
    SHELL_CMD_ARG(rx_capture_timeout, NULL,
                  "<val> - Wait time allowed in seconds\n"
                  "Max timeout allowed is 600 seconds",
                  RTSH(nrf_wifi_radio_test_set_rx_capture_timeout), 2,
                  0),
    SHELL_CMD_ARG(rx_cap, NULL,
                  "0 = ADC capture\n"
                  "1 = Static packet capture\n"
                  "2 = Dynamic packet captur              ",
                  RTSH(nrf_wifi_radio_test_rx_cap), 2, 0),
    SHELL_CMD_ARG(tx_tone_freq, NULL,
                  "<val> - Frequency offset with respect to center frequency in the range of -10 MHz to 10 MHz (resolution 1 MHz)",
                  RTSH(nrf_wifi_radio_test_set_tx_tone_freq), 2, 0),
    SHELL_CMD_ARG(tx_tone, NULL,
                  "<TONE CONTROL>\n"
                  "0: Disable tone\n"
                  "1: Enable tone                                       ",
                  RTSH(nrf_wifi_radio_test_tx_tone), 2, 0),
    SHELL_CMD_ARG(dpd, NULL,
                  "0 - Bypass DPD\n"
                  "1 - Enable DPD",
                  RTSH(nrf_wifi_radio_set_dpd), 2, 0),
    SHELL_CMD_ARG(get_temperature, NULL, "No arguments required",
                  RTSH(nrf_wifi_radio_get_temperature), 1, 0),
    SHELL_CMD_ARG(get_rf_rssi, NULL, "No arguments required",
                  RTSH(nrf_wifi_radio_get_rf_rssi), 1, 0),
    SHELL_CMD_ARG(set_xo_val, NULL, "<val> - XO value in the range 0 to 127",
                  RTSH(nrf_wifi_radio_set_xo_val), 2, 0),
    SHELL_CMD_ARG(compute_optimal_xo_val, NULL, "Compute optimal XO trim value",
                  RTSH(nrf_wifi_radio_comp_opt_xo_val), 1, 0),
    SHELL_CMD_ARG(show_config, NULL, "Display the current configuration values",
                  RTSH(nrf_wifi_radio_test_show_cfg), 1, 0),
    SHELL_CMD_ARG(get_stats, NULL, "Display statistics",
                  RTSH(nrf_wifi_radio_test_get_stats), 1, 0),
    SHELL_CMD_ARG(wlan_ant_switch_ctrl, NULL,
                  "Configure WLAN antenna switch (0-separate/1-shared)",
                  RTSH(nrf_wifi_radio_test_wlan_switch_ctrl), 2, 0),
    SHELL_CMD_ARG(tx_pkt_cw, NULL,
                  "<val> - Contention window value to be configured (0, 3, 7, "
                  "15, 31, 63, 127, 255, 511, 1023)",
                  RTSH(nrf_wifi_radio_test_set_tx_pkt_cw), 2, 0),
    SHELL_CMD_ARG(reg_domain, NULL,
                  "Configure WLAN regulatory domain country code",
                  RTSH(nrf_wifi_radio_test_reg_domain), 1, 1),
    SHELL_CMD_ARG(
        bypass_reg_domain, NULL,
        "Configure WLAN to bypass regulatory\n"
        "0 - TX power of the channel will be set to "
        "minimum between user configured TX power & "
        "maximum TX power of channel in the configured regulatory domain.\n"
        "1 - Configured TX power value will be used for the channel.	"
        "			",
        RTSH(nrf_wifi_radio_test_set_bypass_reg), 2, 0),
    SHELL_CMD_ARG(set_ant_gain, NULL,
                  "<val> - Antenna gain in dB (Min: 0, Max: 6)",
                  RTSH(nrf_wifi_radio_test_set_ant_gain), 2, 0),
    SHELL_CMD_ARG(set_edge_bo, NULL,
                  "<val> - Edge backoff in dB (Min: 0, Max: 10)",
                  RTSH(nrf_wifi_radio_test_set_edge_bo), 2, 0),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(wifi_radio_test, &nrf_wifi_radio_test_subcmds,
                   "nRF Wi-Fi radio test commands", NULL);
#endif /* CONFIG_ZEPHYR_SHELL */

int nrf_wifi_radio_test_shell_init(void)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		return -ENOEXEC;
	}

	return 0;
}
