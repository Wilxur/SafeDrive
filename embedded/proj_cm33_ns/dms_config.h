/*******************************************************************************
 * File Name:   dms_config.h
 *
 * Description: DMS server connectivity and authentication configuration.
 *              Edit this file to change server address, WiFi, or device
 *              credentials before flashing.
 *
 ********************************************************************************/

#ifndef DMS_CONFIG_H_
#define DMS_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Wi-Fi Station Credentials ========== */
#define DMS_WIFI_SSID         "HONOR"
#define DMS_WIFI_PASSWORD     "12345678"
#define DMS_WIFI_SECURITY     CY_WCM_SECURITY_WPA2_AES_PSK

/* ========== DMS Backend Server ========== */
#define DMS_SERVER_HOST_STR   "8.133.22.44"
#define DMS_SERVER_PORT       (80U)

/* ========== Device Account (registered in backend DB) ========== */
#define DMS_DEVICE_USERNAME   "test"
#define DMS_DEVICE_PASSWORD   "test123"
#define DMS_DEVICE_ID         "DEVICE-001"

/* ========== JWT Token Refresh ========== */
/* Re-login after 12 hours (backend JWT expires in 24h) */
#define DMS_TOKEN_REFRESH_SEC (43200U)

/* ========== Detection Rate Control ========== */
/* Minimum interval between sending two detection frames (ms).
 * Avoids flooding the backend when many objects are detected
 * in rapid succession (e.g., 2-5 fps inference). */
#define DMS_SEND_INTERVAL_MS  (200U)

/* Maximum number of individual detection POSTs per frame.
 * Each box becomes one POST; this cap prevents bursts. */
#define DMS_MAX_SEND_PER_FRAME  (5U)

#ifdef __cplusplus
}
#endif

#endif /* DMS_CONFIG_H_ */
