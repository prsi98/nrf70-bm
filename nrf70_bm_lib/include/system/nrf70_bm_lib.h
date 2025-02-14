/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file
 * @brief nRF70 Bare Metal library.
 * @defgroup nrf70_bm nRF70 Bare Metal library
 */

#ifndef NRF70_BM_SYS_LIB_H__
#define NRF70_BM_SYS_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "common/nrf70_bm_lib.h"

extern struct nrf70_bm_sys_wifi_drv_priv nrf70_bm_sys_priv;

#define NR70_SCAN_SSID_MAX_LEN 33
#define NR70_MAC_ADDR_LEN 6
#define NRF70_SCAN_MAX_BSS_CNT 65535

#ifdef CONFIG_NRF70_SCAN_SSID_FILT_MAX
#define NRF70_SCAN_SSID_FILT_MAX CONFIG_NRF70_SCAN_SSID_FILT_MAX
#else
#define NRF70_SCAN_SSID_FILT_MAX 1
#endif /* CONFIG_NRF70_SCAN_SSID_FILT_MAX */

#ifdef CONFIG_NRF70_SCAN_CHAN_MAX_MANUAL
#define NRF70_SCAN_CHAN_MAX_MANUAL CONFIG_NRF70_SCAN_CHAN_MAX_MANUAL
#else
#define NRF70_SCAN_CHAN_MAX_MANUAL 1
#endif /* CONFIG_NRF70_SCAN_CHAN_MAX_MANUAL */

/** @brief IEEE 802.11 security types. */
enum nrf70_bm_sys_security_type {
	/** No security. */
	NRF70_SECURITY_TYPE_NONE = 0,
	/** WPA2-PSK security. */
	NRF70_SECURITY_TYPE_PSK,
	/** WPA2-PSK-SHA256 security. */
	NRF70_SECURITY_TYPE_PSK_SHA256,
	/** WPA3-SAE security. */
	NRF70_SECURITY_TYPE_SAE,
	/** GB 15629.11-2003 WAPI security. */
	NRF70_SECURITY_TYPE_WAPI,
	/** EAP security - Enterprise. */
	NRF70_SECURITY_TYPE_EAP,
	/** WEP security. */
	NRF70_SECURITY_TYPE_WEP,
	/** WPA-PSK security. */
	NRF70_SECURITY_TYPE_WPA_PSK,
	/** WPA/WPA2/WPA3 PSK security. */
	NRF70_SECURITY_TYPE_WPA_AUTO_PERSONAL,

/** @cond INTERNAL_HIDDEN */
	__NRF70_SECURITY_TYPE_AFTER_LAST,
	NRF70_SECURITY_TYPE_MAX = __NRF70_SECURITY_TYPE_AFTER_LAST - 1,
	NRF70_SECURITY_TYPE_UNKNOWN
/** @endcond */
};

/** Helper function to get user-friendly security type name. */
const char *nrf70_bm_sys_security_txt(enum nrf70_bm_sys_security_type security);

/** @brief IEEE 802.11w - Management frame protection. */
enum nrf70_bm_sys_mfp_options {
	/** MFP disabled. */
	NRF70_MFP_DISABLE = 0,
	/** MFP optional. */
	NRF70_MFP_OPTIONAL,
	/** MFP required. */
	NRF70_MFP_REQUIRED,

/** @cond INTERNAL_HIDDEN */
	__NRF70_MFP_AFTER_LAST,
	NRF70_MFP_MAX = __NRF70_MFP_AFTER_LAST - 1,
	NRF70_MFP_UNKNOWN
/** @endcond */
};

/** Helper function to get user-friendly MFP name.*/
const char *nrf70_bm_sys_mfp_txt(enum nrf70_bm_sys_mfp_options mfp);

/**
 * @brief IEEE 802.11 operational frequency bands (not exhaustive).
 */
enum nrf70_bm_sys_freq_bands {
	/** 2.4 GHz band. */
	NRF70_FREQ_BAND_2_4_GHZ = 0,
	/** 5 GHz band. */
	NRF70_FREQ_BAND_5_GHZ,
	/** 6 GHz band (Wi-Fi 6E, also extends to 7GHz). */
	NRF70_FREQ_BAND_6_GHZ,

	/** Number of frequency bands available. */
	__NRF70_FREQ_BAND_AFTER_LAST,
	/** Highest frequency band available. */
	NRF70_FREQ_BAND_MAX = __NRF70_FREQ_BAND_AFTER_LAST - 1,
	/** Invalid frequency band */
	NRF70_FREQ_BAND_UNKNOWN
};

/** Helper function to get user-friendly frequency band name. */
const char *nrf70_bm_sys_band_txt(enum nrf70_bm_sys_freq_bands band);

/** @brief Wi-Fi scanning types. */
enum nrf70_bm_sys_scan_type {
	/** Active scanning (default) - two probe requests are sent on each channel. */
	NRF70_SCAN_TYPE_ACTIVE = 0,
	/** Passive scanning. */
	NRF70_SCAN_TYPE_PASSIVE,
};

/**
 * @brief Wi-Fi structure to uniquely identify a band-channel pair
 */
struct nrf70_bm_sys_band_channel {
	/** Frequency band */
	uint8_t band;
	/** Channel */
	uint8_t channel;
};

/**
 * @brief Wi-Fi scan parameters structure.
 * Used to specify parameters which can control how the Wi-Fi scan
 * is performed.
 */
struct nrf70_bm_sys_scan_params {
	/** Scan type, see enum nrf70_bm_sys_scan_type.
	 *
	 * The scan_type is only a hint to the underlying Wi-Fi chip for the
	 * preferred mode of scan. The actual mode of scan can depend on factors
	 * such as the Wi-Fi chip implementation support, regulatory domain
	 * restrictions etc.
	 */
	enum nrf70_bm_sys_scan_type scan_type;
	/** Bitmap of bands to be scanned.
	 *  Refer to ::nrf70_bm_sys_freq_bands for bit position of each band.
	 */
	uint8_t bands;
	/** Active scan dwell time (in ms) on a channel - default 50ms */
	uint16_t dwell_time_active;
	/** Passive scan dwell time (in ms) on a channel - default 130ms */
	uint16_t dwell_time_passive;
	/** Array of SSID strings to scan.
	 */
	const char *ssids[NRF70_SCAN_SSID_FILT_MAX];
	/** Specifies the maximum number of scan results to return. These results would be the
	 * BSSIDS with the best RSSI values, in all the scanned channels. This should only be
	 * used to limit the number of returned scan results, and cannot be counted upon to limit
	 * the scan time, since the underlying Wi-Fi chip might have to scan all the channels to
	 * find the max_bss_cnt number of APs with the best signal strengths. A value of 0
	 * signifies that there is no restriction on the number of scan results to be returned.
	 */
	uint16_t max_bss_cnt;
	/** Channel information array indexed on Wi-Fi frequency bands and channels within that
	 * band.
	 * E.g. to scan channel 6 and 11 on the 2.4 GHz band, channel 36 on the 5 GHz band:
	 * @code{.c}
	 *     chan[0] = {NRF70FREQ_BAND_2_4_GHZ, 6};
	 *     chan[1] = {NRF70FREQ_BAND_2_4_GHZ, 11};
	 *     chan[2] = {NRF70FREQ_BAND_5_GHZ, 36};
	 * @endcode
	 *
	 *  This list specifies the channels to be __considered for scan__. The underlying
	 *  Wi-Fi chip can silently omit some channels due to various reasons such as channels
	 *  not conforming to regulatory restrictions etc. The invoker of the API should
	 *  ensure that the channels specified follow regulatory rules.
	 */
	struct nrf70_bm_sys_band_channel band_chan[NRF70_SCAN_CHAN_MAX_MANUAL];
};

/** @brief Wi-Fi scan result, each result is provided to the net_mgmt_event_callback
 * via its info attribute (see net_mgmt.h)
 */
struct nrf70_bm_sys_scan_result {
	/** SSID */
	uint8_t ssid[NR70_SCAN_SSID_MAX_LEN];
	/** SSID length */
	uint8_t ssid_len;
	/** Frequency band */
	uint8_t band;
	/** Channel */
	uint8_t channel;
	/** Security type */
	enum nrf70_bm_sys_security_type security;
	/** MFP options */
	enum nrf70_bm_sys_mfp_options mfp;
	/** RSSI */
	int8_t rssi;
	/** BSSID */
	uint8_t bssid[NR70_MAC_ADDR_LEN];
};

/** @brief Callback function to be called when a scan result is available.
 *
 * @param[in] entry Scan result entry. NULL if scan is done.
 */
typedef void (*nrf70_bm_sys_scan_result_cb_t)(struct nrf70_bm_sys_scan_result *entry);


/**@brief Initialize the WiFi module in system mode of operation.
 *
 * This function initializes the nRF70 device and prepares it for operation in
 * the system mode.
 * This also includes powering up the device and setting up the necessary
 * configurations including the download of the firmware patch for nRF70 device.
 *
 * @param[in] mac_addr MAC address of the device.
 * @param[in] reg_info Regulatory information.
 *
 * @retval 0 If the operation was successful.
 * @retval -1 If the operation failed.
 */
int nrf70_bm_sys_init(uint8_t *mac_addr, struct nrf70_bm_regulatory_info *reg_info);

/**@brief Clean up the WiFi module in system mode of operation.
 *
 * This function de-initializes the nRF70 device, operating in the system
 * mode of operation, free up resources and powers it down.
 * Any further operations on the device will require re-initialization.
 *
 * @retval 0 If the operation was successful.
 */
int nrf70_bm_sys_deinit(void);

/**@brief Start scanning for WiFi networks.
 *
 * If all the bands and channels as specified in the scan parameters are scanned, the
 * scan will be considered complete and the callback function will be called with
 * each scan result and finally with a NULL entry to indicate that the scan is done.
 *
 * Once the scan is complete, after a timeout the nRF70 device will automatically
 * switch to sleep state.
 *
 * @param[in] scan_params Scan parameters.
 * @param[in] cb Callback function to be called when a scan result is available.
 *
 * @retval 0 If the operation was successful.
 * @retval -EINVAL If the scan parameters are invalid.
 * @retval -EBUSY If the scan is already in progress.
 * @retval -EIO If the operation failed.
 * @retval -ENOMEM If there is not enough memory to start the scan.
 */
int nrf70_bm_sys_scan_start(struct nrf70_bm_sys_scan_params *scan_params,
			    nrf70_bm_sys_scan_result_cb_t cb);

/** @brief Set nRF70 regulatory information.
 *
 * This function sets the regulatory information of the nRF70 device.
 * The regulatory information includes the country code, the number of channels
 * supported and the channel information.
 *
 * @param[in] reg_info Regulatory information.
 *
 * @retval 0 If the operation was successful.
 * @retval -1 If the operation failed.
 */
int nrf70_bm_sys_set_reg(struct nrf70_bm_regulatory_info *reg_info);

/** @brief Get nRF70 regulatory information.
 *
 * This function retrieves the regulatory information of the nRF70 device.
 * The regulatory information includes the country code, the number of channels
 * supported and the channel information.
 *
 * @param[out] reg_info Regulatory information.
 *
 * @retval 0 If the operation was successful.
 * @retval -1 If the operation failed. *
 */
int nrf70_bm_sys_get_reg(struct nrf70_bm_regulatory_info *reg_info);


/**@brief Convert a MAC address to a string.
 *
 * @param[in] mac MAC address.
 * @param[out] mac_str MAC address string.
 * @param[in] size Size of the MAC address string.
 */
void nrf70_bm_sys_mac_txt(const unsigned char *mac,
			  char *mac_str,
			  size_t size);

/**@brief Get the nRF70 statistics.
 *
 * This function retrieves the statistics of the nRF70 device for UMAC, LMAc and PHY.
 *
 * @param[in] type Type of statistics to get.
 *
 * @retval 0 If the operation was successful.
 * @retval -EINVAL If the type is invalid.
 * @retval -EIO If the operation failed.
 */
int nrf70_bm_sys_dump_stats(const char *type);

#ifdef __cplusplus
}
#endif

#endif /* NRF70_BM_H__ */
