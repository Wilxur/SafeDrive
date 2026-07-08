/*******************************************************************************
* File Name:   tcp_client.c
*
* Description: This file contains the DMS network client task.
*              - Connects to Wi-Fi STA
*              - Logs into the DMS backend (JWT auth)
*              - Polls CM55 shared memory for detection results
*              - Sends each detection as an HTTP POST to the backend
*
* Related Document: See README.md
*
********************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cybsp.h"
#include "cyabs_rtos.h"
#include <string.h>
#include "retarget_io_init.h"
#include "shared_dms.h"
#include "dms_config.h"

/* Secure socket header file. */
#include "cy_secure_sockets.h"

/* Wi-Fi connection manager header files. */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/* TCP client task header file. */
#include "tcp_client.h"

/* IP address related header files. */
#include "cy_nw_helper.h"

/* Standard C header files */
#include <inttypes.h>

/*******************************************************************************
* Macros
*******************************************************************************/
/* To use the Wi-Fi device in AP interface mode, set this macro as '1' */
#define USE_AP_INTERFACE                         (0U)

#define MAKE_IP_PARAMETERS(a, b, c, d)           ((((uint32_t) d) << 24) | \
                                                 (((uint32_t) c) << 16) | \
                                                 (((uint32_t) b) << 8) |\
                                                 ((uint32_t) a))

#if(USE_AP_INTERFACE)
    #define WIFI_INTERFACE_TYPE                  CY_WCM_INTERFACE_TYPE_AP

    /* SoftAP Credentials */
    #define SOFTAP_SSID                          "MY_SOFT_AP"
    #define SOFTAP_PASSWORD                      "psoc1234"

    /* Security type of the SoftAP */
    #define SOFTAP_SECURITY_TYPE                  CY_WCM_SECURITY_WPA2_AES_PSK

    #define SOFTAP_IP_ADDRESS_COUNT               (2U)

    #define SOFTAP_IP_ADDRESS                     MAKE_IP_PARAMETERS(192, 168, 10, 1)
    #define SOFTAP_NETMASK                        MAKE_IP_PARAMETERS(255, 255, 255, 0)
    #define SOFTAP_GATEWAY                        MAKE_IP_PARAMETERS(192, 168, 10, 1)
    #define SOFTAP_RADIO_CHANNEL                  (1U)
#else
    #define WIFI_INTERFACE_TYPE                   CY_WCM_INTERFACE_TYPE_STA

    /* Wi-Fi max connection retries */
    #define MAX_WIFI_CONN_RETRIES                 (10U)

    /* Wi-Fi re-connection time interval in milliseconds */
    #define WIFI_CONN_RETRY_INTERVAL_MSEC         (1000U)
#endif /* USE_AP_INTERFACE */

/* Maximum number of connection retries to the TCP server. */
#define MAX_TCP_SERVER_CONN_RETRIES               (3U)

/* TCP keep alive related macros. */
#define TCP_KEEP_ALIVE_IDLE_TIME_MS               (10000U)
#define TCP_KEEP_ALIVE_INTERVAL_MS                (1000U)
#define TCP_KEEP_ALIVE_RETRY_COUNT                (2U)

/* --- DMS Server Address (constructed from dms_config.h) --- */
#define DMS_SERVER_IP_ADDR                       \
    MAKE_IP_PARAMETERS(8, 133, 22, 44)

/* Buffer sizes */
#define MAX_JSON_BODY_SIZE                       (512U)
#define MAX_HTTP_REQUEST_SIZE                    (2048U)
#define MAX_HTTP_RESPONSE_SIZE                   (1024U)
#define MAX_JWT_TOKEN_LEN                        (512U)

/* Polling interval between shared-memory checks (milliseconds) */
#define DMS_POLL_INTERVAL_MS                     (50U)

/* Buffer for IP address display strings */
#define UART_BUFFER_SIZE                          (20U)

#define RTOS_TICK_TO_WAIT                         (1U)

#define SEMAPHORE_LIMIT                           (1U)

#define INIT_COUNT_FOR_SEMAPHORE                  (0U)
#define CARRIAGE_RETURN                           ('\r')
#define NEWLINE                                   ('\n')
#define BACKSPACE                                 ('\b')
#define NULLCHARACTER                             ('\0')
#define DISCONNECTION_TIMEOUT                     (0U)
#define VALUE_TO_BE_FILLED                        (0U)
#define BYTE_ZERO                                 (0U)
#define RESET_VAL                                 (0U)

#define APP_SDIO_INTERRUPT_PRIORITY               (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY          (2U)
#define APP_SDIO_FREQUENCY_HZ                     (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK                   (64U)

/* Mapping: CM55 class_id (0-7) → backend category string.
 * Must match dms_class_names[] in inference_task.c:
/* 0:steering_wheel, 1:hand, 2:yawn, 3:seatbelt, 4:no_seatbelt, 5:phone */
static const char* CLASS_ID_TO_CATEGORY[6] = {
    "steering_wheel",   /* 0 */
    "hand",             /* 1 */
    "yawn",             /* 2 */
    "seatbelt",         /* 3 */
    "no_seatbelt",      /* 4 */
    "phone",            /* 5 */
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
cy_rslt_t create_tcp_client_socket(void);
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg);
cy_rslt_t connect_to_tcp_server(cy_socket_sockaddr_t address);

#if(USE_AP_INTERFACE)
    static cy_rslt_t softap_start(void);
#else
    static cy_rslt_t connect_to_wifi_ap(void);
#endif /* USE_AP_INTERFACE */

/* --- New DMS backend functions --- */
static int  build_login_json(char *buf, size_t buf_size);
static int  build_detection_json(const char *category, float confidence,
                                  const char *device_id,
                                  char *buf, size_t buf_size);
static int  build_http_post_with_auth(const char *json_body, int json_len,
                                       const char *auth_token,
                                       const char *path,
                                       char *http_buf, size_t buf_size);
static bool extract_token_from_json(const char *json, size_t len,
                                     char *token_buf, size_t token_buf_size);
static bool dms_perform_login(void);
static bool dms_send_http_post(const char *path, const char *json_body,
                                int json_len);

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* TCP client socket handle */
cy_socket_t client_handle;

/* Binary semaphore handle to keep track of TCP server connection. */
cy_semaphore_t connect_to_server;

/* Holds the IP address obtained for SoftAP using Wi-Fi Connection Manager (WCM). */
cy_wcm_ip_address_t softap_ip_address;

extern cy_stc_scb_uart_context_t    DEBUG_UART_context;
static mtb_hal_sdio_t sdio_instance;
static cy_stc_sd_host_context_t sdhc_host_context;
static cy_wcm_config_t wcm_config;

/* --- DMS state --- */
static char g_jwt_token[MAX_JWT_TOKEN_LEN] = {0};
static uint32_t g_token_obtained_loop = 0;  /* loop counter at login time */
static bool g_logged_in = false;

/* Rate-limit tracking */
static uint32_t g_last_send_loop = 0;

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

/* SysPm callback parameter structure for SDHC */
static cy_stc_syspm_callback_params_t sdcardDSParams =
{
    .context   = &sdhc_host_context,
    .base      = CYBSP_WIFI_SDIO_HW
};

/* SysPm callback structure for SDHC*/
static cy_stc_syspm_callback_t sdhcDeepSleepCallbackHandler =
{
    .callback           = Cy_SD_Host_DeepSleepCallback,
    .skipMode           = SYSPM_SKIP_MODE,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &sdcardDSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};
#endif

/*******************************************************************************
* Function Name: sdio_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for SDIO instance.
*
*******************************************************************************/
static void sdio_interrupt_handler(void)
{
    mtb_hal_sdio_process_interrupt(&sdio_instance);
}

/*******************************************************************************
* Function Name: host_wake_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for the host wake up input pin.
*******************************************************************************/
static void host_wake_interrupt_handler(void)
{
    mtb_hal_gpio_process_interrupt(&wcm_config.wifi_host_wake_pin);
}

/*******************************************************************************
* Function Name: app_sdio_init
********************************************************************************
* Summary:
* This function configures and initializes the SDIO instance used in
* communication between the host MCU and the wireless device.
*
*******************************************************************************/
static void app_sdio_init(void)
{
    cy_rslt_t result;
    mtb_hal_sdio_cfg_t sdio_hal_cfg;

    cy_stc_sysint_t sdio_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_SDIO_IRQ,
        .intrPriority = APP_SDIO_INTERRUPT_PRIORITY
    };

    cy_stc_sysint_t host_wake_intr_cfg =
    {
            .intrSrc = CYBSP_WIFI_HOST_WAKE_IRQ,
            .intrPriority = APP_HOST_WAKE_INTERRUPT_PRIORITY
    };

    /* Initialize the SDIO interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status = Cy_SysInt_Init(&sdio_intr_cfg, sdio_interrupt_handler);

    /* SDIO interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_SDIO_IRQ);

    /* Setup SDIO using the HAL object and desired configuration */
    result = mtb_hal_sdio_setup(&sdio_instance, &CYBSP_WIFI_SDIO_sdio_hal_config, NULL, &sdhc_host_context);

    /* SDIO setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Initialize and Enable SD HOST */
    Cy_SD_Host_Enable(CYBSP_WIFI_SDIO_HW);
    Cy_SD_Host_Init(CYBSP_WIFI_SDIO_HW, CYBSP_WIFI_SDIO_sdio_hal_config.host_config, &sdhc_host_context);
    Cy_SD_Host_SetHostBusWidth(CYBSP_WIFI_SDIO_HW, CY_SD_HOST_BUS_WIDTH_4_BIT);

    sdio_hal_cfg.frequencyhal_hz = APP_SDIO_FREQUENCY_HZ;
    sdio_hal_cfg.block_size = SDHC_SDIO_64BYTES_BLOCK;

    /* Configure SDIO */
    mtb_hal_sdio_configure(&sdio_instance, &sdio_hal_cfg);

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
    /* SDHC SysPm callback registration */
    Cy_SysPm_RegisterCallback(&sdhcDeepSleepCallbackHandler);
#endif /* (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP) */

    /* Setup GPIO using the HAL object for WIFI WL REG ON  */
    mtb_hal_gpio_setup(&wcm_config.wifi_wl_pin, CYBSP_WIFI_WL_REG_ON_PORT_NUM, CYBSP_WIFI_WL_REG_ON_PIN);

    /* Setup GPIO using the HAL object for WIFI HOST WAKE PIN  */
    mtb_hal_gpio_setup(&wcm_config.wifi_host_wake_pin, CYBSP_WIFI_HOST_WAKE_PORT_NUM, CYBSP_WIFI_HOST_WAKE_PIN);

    /* Initialize the Host wakeup interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status_host_wake =  Cy_SysInt_Init(&host_wake_intr_cfg, host_wake_interrupt_handler);

    /* Host wake up interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status_host_wake)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_HOST_WAKE_IRQ);
}

/*******************************************************************************
* Function Name: uart_bytesAvailable
********************************************************************************
* Summary:
*  Checks the number of bytes available to read from the receive buffers
*
* Return:
*  uint32_t number_available: Return The number of readable bytes
*
*******************************************************************************/
static uint32_t uart_bytesAvailable(void)
{
    uint32_t numofBytes_available = Cy_SCB_UART_GetNumInRxFifo(SCB2);
    if(NULL != DEBUG_UART_context.rxRingBuf )
    {
        numofBytes_available += Cy_SCB_UART_GetNumInRingBuffer(SCB2, &DEBUG_UART_context);
    }
    return numofBytes_available;
}

/*******************************************************************************
* Function Name: uart_get_data
********************************************************************************
* Summary:
*  Reads the input data received from uart
*
* Parameters:
*  value : The value read from the serial port
*
*******************************************************************************/
static void uart_get_data(uint8_t *value)
{
    uint32_t read_value = Cy_SCB_UART_Get(SCB2);
    while (CY_SCB_UART_RX_NO_DATA == read_value)
    {
        read_value = Cy_SCB_UART_Get(SCB2);
    }
    *value = (uint8_t)read_value;
}

/*******************************************************************************
* Function Name: uart_put
********************************************************************************
* Summary:
*  Sends a character
*
* Parameters:
*  value : The character to be sent
*
*******************************************************************************/
void uart_put(uint32_t value)
{
  Cy_SCB_UART_Put(SCB2, value);
}

/*******************************************************************************
* Function Name: read_uart_input
********************************************************************************
* Summary:
*  Function to read user input from UART terminal.
*
* Parameters:
*  uint8_t* input_buffer_ptr: Pointer to input buffer
*
*******************************************************************************/
static void read_uart_input(uint8_t* input_buffer_ptr)
{
    uint8_t *input_ptr = input_buffer_ptr;
    uint32_t numBytes;

    do
    {
        /* Check for data in the UART buffer */
        numBytes = uart_bytesAvailable();

        if(numBytes)
        {
            uart_get_data(input_ptr);

            if((CARRIAGE_RETURN == *input_ptr) || (NEWLINE == *input_ptr))
            {
                printf("\n");
            }
            else
            {
                /* Echo the received character */
                uart_put(*input_ptr);
                if (BACKSPACE != *input_ptr)
                {
                    input_ptr++;
                }
                else if(input_ptr != input_buffer_ptr)
                {
                    input_ptr--;
                }
            }
        }

        cy_rtos_delay_milliseconds(RTOS_TICK_TO_WAIT);

    } while((*input_ptr != CARRIAGE_RETURN) && (*input_ptr != NEWLINE));

    /* Terminate string with NULL character. */
    *input_ptr = NULLCHARACTER;
}

/*******************************************************************************
* Function Name: build_login_json
********************************************************************************
* Summary:
*  Builds the JSON body for the login request.
*  Format: {"username":"<user>","password":"<pass>"}
*
* Parameters:
*  buf, buf_size — output buffer
*
* Return:
*  Number of characters written (excluding null terminator)
*
*******************************************************************************/
static int build_login_json(char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size,
        "{\"username\":\"%s\",\"password\":\"%s\"}",
        DMS_DEVICE_USERNAME,
        DMS_DEVICE_PASSWORD);
}

/*******************************************************************************
* Function Name: build_detection_json
********************************************************************************
* Summary:
*  Builds a JSON payload for a single detection record.
*  Format matches the backend POST /api/detection/upload:
*  {
*    "category": "closed_eyes",
*    "confidence": 0.85,
*    "device_id": "DEVICE-001"
*  }
*
* Parameters:
*  category   — detection class name string
*  confidence — confidence value (0.0 ~ 1.0)
*  device_id  — device identifier
*  buf, buf_size — output buffer
*
* Return:
*  Number of characters written (excluding null terminator)
*
*******************************************************************************/
static int build_detection_json(const char *category, float confidence,
                                 const char *device_id,
                                 char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size,
        "{\"category\":\"%s\",\"confidence\":%.2f,\"device_id\":\"%s\"}",
        category,
        (double)confidence,
        device_id);
}

/*******************************************************************************
* Function Name: build_http_post_with_auth
********************************************************************************
* Summary:
*  Builds a complete HTTP/1.1 POST request with JWT Bearer token.
*
* Parameters:
*  json_body — pre-built JSON string
*  json_len  — length of json_body
*  auth_token — JWT token string (empty string = no auth header)
*  path      — URL path, e.g. "/api/detection/upload"
*  http_buf, buf_size — output buffer
*
* Return:
*  Total length of the HTTP request (excluding null terminator)
*
*******************************************************************************/
static int build_http_post_with_auth(const char *json_body, int json_len,
                                      const char *auth_token,
                                      const char *path,
                                      char *http_buf, size_t buf_size)
{
    if (auth_token && auth_token[0] != '\0') {
        return snprintf(http_buf, buf_size,
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "%s",
            path,
            DMS_SERVER_HOST_STR,
            (int)DMS_SERVER_PORT,
            auth_token,
            json_len,
            json_body);
    } else {
        /* No auth token — used for login request itself */
        return snprintf(http_buf, buf_size,
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "%s",
            path,
            DMS_SERVER_HOST_STR,
            (int)DMS_SERVER_PORT,
            json_len,
            json_body);
    }
}

/*******************************************************************************
* Function Name: extract_token_from_json
********************************************************************************
* Summary:
*  Parses a JSON login response to extract the JWT access_token.
*  Looks for "token":"<value>" pattern.
*  Simple string-based parser — no full JSON library needed.
*
* Parameters:
*  json       — raw JSON response string
*  len        — length of json
*  token_buf  — output buffer for token
*  token_buf_size — size of token_buf
*
* Return:
*  true if token was successfully extracted
*
*******************************************************************************/
static bool extract_token_from_json(const char *json, size_t len,
                                     char *token_buf, size_t token_buf_size)
{
    const char *key = "\"token\":\"";
    size_t key_len = strlen(key);

    /* Find "token":" */
    const char *start = NULL;
    for (size_t i = 0; i + key_len < len; i++) {
        if (memcmp(&json[i], key, key_len) == 0) {
            start = &json[i + key_len];
            break;
        }
    }

    if (start == NULL) {
        printf("[CM33] Login: 'token' key not found in response\n");
        return false;
    }

    /* Find closing quote */
    const char *end = start;
    while (*end != '\0' && *end != '"' && (size_t)(end - json) < len) {
        end++;
    }

    size_t token_len = (size_t)(end - start);
    if (token_len == 0 || token_len >= token_buf_size) {
        printf("[CM33] Login: token length %u invalid (max %u)\n",
               (unsigned)token_len, (unsigned)(token_buf_size - 1));
        return false;
    }

    memcpy(token_buf, start, token_len);
    token_buf[token_len] = '\0';

    return true;
}

/*******************************************************************************
* Function Name: dms_send_http_post
********************************************************************************
* Summary:
*  Sends an HTTP POST request over the current TCP connection and reads
*  the response.
*
* Parameters:
*  path       — URL path (e.g. "/api/detection/upload")
*  json_body  — pre-built JSON body
*  json_len   — length of json_body
*
* Return:
*  true if send + response succeeded
*
*******************************************************************************/
static bool dms_send_http_post(const char *path, const char *json_body,
                                int json_len)
{
    cy_rslt_t result;
    char http_buf[MAX_HTTP_REQUEST_SIZE];
    char resp_buf[MAX_HTTP_RESPONSE_SIZE];

    int http_len = build_http_post_with_auth(json_body, json_len,
                                              g_jwt_token, path,
                                              http_buf, sizeof(http_buf));

    if (http_len <= 0 || http_len >= (int)sizeof(http_buf)) {
        printf("[CM33] HTTP build overflow\n");
        return false;
    }

    uint32_t bytes_sent = 0;
    result = cy_socket_send(client_handle, http_buf,
                            (uint32_t)http_len,
                            CY_SOCKET_FLAGS_NONE, &bytes_sent);
    if (CY_RSLT_SUCCESS != result || bytes_sent == 0) {
        printf("[CM33] Send failed (err=0x%08" PRIx32 ")\n", (uint32_t)result);
        return false;
    }

    /* Read response */
    uint32_t bytes_recv = 0;
    result = cy_socket_recv(client_handle, resp_buf,
                            sizeof(resp_buf) - 1,
                            CY_SOCKET_FLAGS_NONE, &bytes_recv);
    if (CY_RSLT_SUCCESS == result && bytes_recv > 0) {
        resp_buf[bytes_recv] = '\0';

        /* Print first 200 chars of response for debugging */
        printf("[CM33] HTTP resp: %.200s\n", resp_buf);

        /* Check for HTTP 401 → token expired */
        if (strstr(resp_buf, "401") != NULL ||
            strstr(resp_buf, "Unauthorized") != NULL) {
            printf("[CM33] Got 401, token may be expired\n");
            g_logged_in = false;
            g_jwt_token[0] = '\0';
            return false;
        }

        /* Check for HTTP redirect (301/302) — wrong URL */
        if (strstr(resp_buf, "301") != NULL ||
            strstr(resp_buf, "302") != NULL) {
            printf("[CM33] Got redirect, wrong URL?\n");
            return false;
        }

        /* Check for success status: HTTP 2xx, extract credit_score + blacklisted */
        if (strstr(resp_buf, "200 OK")   != NULL ||
            strstr(resp_buf, "201 Created") != NULL ||
            strstr(resp_buf, "200")      != NULL ||
            strstr(resp_buf, "201")      != NULL)
        {
            /* Parse JSON for "credit_score": and "blacklisted": */
            volatile dms_shared_mem_t *shm = DMS_SHARED_MEM_ADDR;
            char *p = strstr(resp_buf, "\"credit_score\":");
            if (p) {
                shm->credit_score = (int32_t)strtol(p + 15, NULL, 10);
            }
            p = strstr(resp_buf, "\"blacklisted\":");
            if (p) {
                shm->blacklisted = (uint8_t)strtol(p + 14, NULL, 10);
            }
            DMS_BARRIER();
            return true;
        }

        /* Anything else (4xx, 5xx) — log and fail */
        printf("[CM33] Unexpected HTTP status in response\n");
        return false;
    }

    /* No response at all */
    printf("[CM33] No HTTP response received\n");
    return false;
}

/*******************************************************************************
* Function Name: dms_perform_login
********************************************************************************
* Summary:
*  Logs into the DMS backend, obtains a JWT access token.
*  Sends POST /api/auth/login with device credentials, extracts token.
*
* Return:
*  true if login succeeded and token was stored
*
*******************************************************************************/
static bool dms_perform_login(void)
{
    char json_buf[MAX_JSON_BODY_SIZE];
    char http_buf[MAX_HTTP_REQUEST_SIZE];
    char resp_buf[MAX_HTTP_RESPONSE_SIZE];

    printf("[CM33] Logging in as '%s'...\n", DMS_DEVICE_USERNAME);

    int json_len = build_login_json(json_buf, sizeof(json_buf));
    if (json_len <= 0) {
        printf("[CM33] Login: JSON build failed\n");
        return false;
    }

    /* Login doesn't use auth token (build with empty token) */
    int http_len = build_http_post_with_auth(json_buf, json_len, "",
                                              "/api/auth/login",
                                              http_buf, sizeof(http_buf));
    if (http_len <= 0 || http_len >= (int)sizeof(http_buf)) {
        printf("[CM33] Login: HTTP build overflow\n");
        return false;
    }

    cy_rslt_t result;
    uint32_t bytes_sent = 0;
    result = cy_socket_send(client_handle, http_buf,
                            (uint32_t)http_len,
                            CY_SOCKET_FLAGS_NONE, &bytes_sent);
    if (CY_RSLT_SUCCESS != result || bytes_sent == 0) {
        printf("[CM33] Login: send failed (err=0x%08" PRIx32 ")\n",
               (uint32_t)result);
        return false;
    }

    uint32_t bytes_recv = 0;
    result = cy_socket_recv(client_handle, resp_buf,
                            sizeof(resp_buf) - 1,
                            CY_SOCKET_FLAGS_NONE, &bytes_recv);
    if (CY_RSLT_SUCCESS != result || bytes_recv == 0) {
        printf("[CM33] Login: recv failed (err=0x%08" PRIx32 ")\n",
               (uint32_t)result);
        return false;
    }

    resp_buf[bytes_recv] = '\0';

    /* Extract JWT token from JSON response */
    if (!extract_token_from_json(resp_buf, bytes_recv,
                                  g_jwt_token, sizeof(g_jwt_token))) {
        printf("[CM33] Login: failed to parse token from response\n");
        printf("[CM33] Raw resp: %.200s\n", resp_buf);
        return false;
    }

    printf("[CM33] Login OK! Token (len=%u): %.50s...\n",
           (unsigned)strlen(g_jwt_token), g_jwt_token);

    DMS_SHARED_MEM_ADDR->cloud_connected = true;
    DMS_BARRIER();
    return true;
}

/*******************************************************************************
* Function Name: tcp_client_task
********************************************************************************
* Summary:
*  Main network task: Wi-Fi → login → poll shared memory → send detections.
*
* Parameters:
*  void *args : Task parameter (unused).
*
*******************************************************************************/
void tcp_client_task(void *arg)
{
    cy_rslt_t result ;

    /* IP address and TCP port of the DMS backend */
    cy_socket_sockaddr_t tcp_server_address =
    {
        .ip_address.version = CY_SOCKET_IP_VER_V4,
        .port = DMS_SERVER_PORT
    };

    app_sdio_init();

    wcm_config.interface = WIFI_INTERFACE_TYPE;
    wcm_config.wifi_interface_instance = &sdio_instance;

    /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wcm_config);

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Wi-Fi Connection Manager initialization failed! "
               "Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        handle_app_error();
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

    #if(USE_AP_INTERFACE)
        /* Start the Wi-Fi device as a Soft AP interface. */
        result = softap_start();
        if (CY_RSLT_SUCCESS != result)
        {
            printf("Failed to Start Soft AP! Error code: 0x%08"PRIx32"\n",
                   (uint32_t)result);
            handle_app_error();
        }
    #else
        /* Connect to Wi-Fi AP */
        result = connect_to_wifi_ap();
        if(CY_RSLT_SUCCESS != result)
        {
            printf("\n Failed to connect to Wi-Fi AP! Error code: 0x%08"PRIx32"\n",
                   (uint32_t)result);
            handle_app_error();
        }
    #endif /* USE_AP_INTERFACE */

    /* Create a binary semaphore to keep track of TCP server connection. */
    cy_rtos_semaphore_init(&connect_to_server, SEMAPHORE_LIMIT,
                            INIT_COUNT_FOR_SEMAPHORE);

    /* Give the semaphore so as to connect to TCP server.  */
    cy_rtos_semaphore_set(&connect_to_server);

    /* Initialize secure socket library. */
    result = cy_socket_init();

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Secure Socket initialization failed!\n");
        handle_app_error();
    }
    printf("Secure Socket initialized\n");

    /* ===== DMS Backend Address ===== */
    tcp_server_address.ip_address.ip.v4 = DMS_SERVER_IP_ADDR;
    printf("[CM33] DMS Backend: %s:%d\n",
           DMS_SERVER_HOST_STR, (int)DMS_SERVER_PORT);
    printf("[CM33] Device: user=%s id=%s\n",
           DMS_DEVICE_USERNAME, DMS_DEVICE_ID);

    /* Buffers for JSON and HTTP */
    char json_buf[MAX_JSON_BODY_SIZE];

    uint32_t last_frame_id = 0;
    bool connected = false;
    uint32_t loop_count = 0;

    CY_UNUSED_PARAMETER(arg);

    for (;;)
    {
        /* --- Ensure TCP connection --- */
        if (!connected) {
            printf("[CM33] Connecting to %s:%d...\n",
                   DMS_SERVER_HOST_STR, (int)DMS_SERVER_PORT);
            result = connect_to_tcp_server(tcp_server_address);
            if (CY_RSLT_SUCCESS == result) {
                connected = true;
                /* Keep existing JWT token if we have one —
                 * token is valid across TCP connections */
                printf("[CM33] TCP connected!\n");
            } else {
                printf("[CM33] Connect failed (err=0x%08" PRIx32 "), "
                       "retrying in 3s...\n", (uint32_t)result);
                cy_rtos_delay_milliseconds(3000);
                continue;
            }
        }

        /* --- Login / refresh JWT token --- */
        if (!g_logged_in ||
            (loop_count - g_token_obtained_loop) >
                ((uint32_t)DMS_TOKEN_REFRESH_SEC * 1000U / DMS_POLL_INTERVAL_MS))
        {
            if (dms_perform_login()) {
                g_logged_in = true;
                g_token_obtained_loop = loop_count;
            } else {
                printf("[CM33] Login failed, retrying in 3s...\n");
                /* Might be connection issue — reconnect */
                cy_socket_delete(client_handle);
                connected = false;
                /* Drain pending disconnect semaphore */
                cy_rtos_semaphore_get(&connect_to_server, 0);
                cy_rtos_delay_milliseconds(3000);
                continue;
            }
        }

        /* --- Periodic heartbeat --- */
        loop_count++;
        if (loop_count % 200 == 0) {  /* ~every 10 seconds */
            printf("[CM33] Alive (loop=%" PRIu32 ", last_frm=%" PRIu32
                   ", logged_in=%d)\n",
                   loop_count, last_frame_id, (int)g_logged_in);
        }

        /* --- Poll shared memory --- */
        volatile dms_shared_mem_t *shm = DMS_SHARED_MEM_ADDR;

        /* Check if CM55 is in ACTIVE mode */
        DMS_BARRIER();
        bool cm55_active = shm->cm55_active;
        DMS_BARRIER();

        if (cm55_active) {

            DMS_BARRIER();
            bool is_fresh = shm->fresh;
            DMS_BARRIER();

            if (is_fresh && (shm->frame_id != last_frame_id))
            {
                /* Snapshot shared memory before clearing fresh flag */
                uint32_t frame_id    = shm->frame_id;
                uint8_t  box_count   = shm->box_count;
                dms_box_t boxes_snapshot[DMS_MAX_BOXES];

                if (box_count > DMS_MAX_BOXES) box_count = DMS_MAX_BOXES;
                for (int i = 0; i < box_count; i++) {
                    boxes_snapshot[i] = shm->boxes[i];
                }

                DMS_BARRIER();
                shm->fresh = false;
                DMS_BARRIER();

                last_frame_id = frame_id;

                /* --- Rate limit: don't send faster than DMS_SEND_INTERVAL_MS --- */
                uint32_t loops_since_last =
                    loop_count - g_last_send_loop;
                uint32_t min_loops =
                    DMS_SEND_INTERVAL_MS / DMS_POLL_INTERVAL_MS;

                if (box_count > 0 &&
                    (g_last_send_loop == 0 || loops_since_last >= min_loops))
                {
                    g_last_send_loop = loop_count;

                    /* Send each detection box as an individual POST */
                    int send_count = (box_count < DMS_MAX_SEND_PER_FRAME)
                                     ? box_count : DMS_MAX_SEND_PER_FRAME;

                    for (int i = 0; i < send_count; i++) {
                        uint16_t cid = boxes_snapshot[i].class_id;
                        float conf = (float)boxes_snapshot[i].confidence
                                     / 10000.0f;  /* stored as x10000 */

                        const char *category;
                        if (cid == 255) {
                            category = "hands_off";           /* 推断结果 */
                        } else if (cid == 0 || cid == 1) {
                            continue;                         /* 不上报steering_wheel(0)/hand(1) */
                        } else if (cid >= 2 && cid < 6) {
                            category = CLASS_ID_TO_CATEGORY[cid];
                        } else {
                            printf("[CM33] Unknown class_id=%u, skip\n",
                                   (unsigned)cid);
                            continue;
                        }

                        int json_len = build_detection_json(
                            category, conf, DMS_DEVICE_ID,
                            json_buf, sizeof(json_buf));

                        if (json_len <= 0) continue;

                        bool ok = dms_send_http_post(
                            "/api/detection/upload",
                            json_buf, json_len);

                        printf("[CM33] Frm=%" PRIu32 " box=%d/%d "
                               "cat=%s conf=%.0f%% -> %s\n",
                               frame_id, i + 1, send_count,
                               category, conf * 100.0f,
                               ok ? "OK" : "FAIL");

                        if (!ok) {
                            /* Connection lost — clean up and reconnect */
                            cy_socket_delete(client_handle);
                            connected = false;
                            /* Drain pending disconnect semaphore to
                             * avoid double-handling by the async callback */
                            cy_rtos_semaphore_get(&connect_to_server, 0);
                            break;
                        }

                        /* Small delay between posts in a batch */
                        cy_rtos_delay_milliseconds(10);
                    }
                }
                else if (box_count > 0) {
                    /* Rate-limited: skip this frame */
                    /* (uncomment for debug):
                    printf("[CM33] Frm=%" PRIu32 " skipped (rate-limit)\n",
                           frame_id);
                    */
                }
            }

        } /* end if (cm55_active) */

        /* Check for unexpected disconnect */
        if (CY_RSLT_SUCCESS ==
                cy_rtos_semaphore_get(&connect_to_server, 0)) {
            printf("[CM33] Server closed connection (normal HTTP). "
                   "Will reconnect.\n");
            connected = false;
            DMS_SHARED_MEM_ADDR->cloud_connected = false;
            DMS_BARRIER();
            /* Don't reset g_logged_in — JWT token is still valid */
            continue;
        }

        cy_rtos_delay_milliseconds(DMS_POLL_INTERVAL_MS);
    }
 }

#if(!USE_AP_INTERFACE)
/*******************************************************************************
* Function Name: connect_to_wifi_ap()
********************************************************************************
* Summary:
*  Connects to Wi-Fi AP using credentials from dms_config.h.
*  Retries up to MAX_WIFI_CONN_RETRIES times.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: CY_RSLT_SUCCESS if successful.
*
*******************************************************************************/
cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;
    char ip_addr_str[UART_BUFFER_SIZE];

    /* Variables used by Wi-Fi connection manager.*/
    cy_wcm_connect_params_t wifi_conn_param;
    cy_wcm_ip_address_t ip_address;

    /* IP variable for network utility functions */
    cy_nw_ip_address_t nw_ip_addr =
    {
        .version = NW_IP_IPV4
    };

     /* Set the Wi-Fi SSID, password and security type from dms_config.h */
    memset(&wifi_conn_param, RESET_VAL, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID,
           DMS_WIFI_SSID, sizeof(DMS_WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password,
           DMS_WIFI_PASSWORD, sizeof(DMS_WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = DMS_WIFI_SECURITY;

    printf("Connecting to Wi-Fi: %s\n", DMS_WIFI_SSID);

    /* Join the Wi-Fi AP. */
    for(uint32_t conn_retries = 0;
        conn_retries < MAX_WIFI_CONN_RETRIES;
        conn_retries++ )
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);

        if(CY_RSLT_SUCCESS == result)
        {
            printf("Wi-Fi connected: '%s'\n",
                   wifi_conn_param.ap_credentials.SSID);
            nw_ip_addr.ip.v4 = ip_address.ip.v4;
            cy_nw_ntoa(&nw_ip_addr, ip_addr_str);
            printf("IP Address Assigned: %s\n", ip_addr_str);
            DMS_SHARED_MEM_ADDR->wifi_connected = true;
            DMS_BARRIER();
            return result;
        }

        printf("Wi-Fi connect failed (err=%d). Retrying in %d ms...\n",
               (int)result, WIFI_CONN_RETRY_INTERVAL_MSEC);

        cy_rtos_delay_milliseconds(WIFI_CONN_RETRY_INTERVAL_MSEC);
    }

    /* Stop retrying after maximum retry attempts. */
    printf("Exceeded maximum Wi-Fi connection attempts\n");

    return result;
}
#endif /* USE_AP_INTERFACE */

#if(USE_AP_INTERFACE)
/*******************************************************************************
* Function Name: softap_start
********************************************************************************
* Summary:
*  This function configures device in AP mode and initializes
*  a SoftAP with the given credentials.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: Returns CY_RSLT_SUCCESS if the Soft AP is started successfully.
*
*******************************************************************************/
static cy_rslt_t softap_start(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    char ip_addr_str[UART_BUFFER_SIZE];

    /* IP variable for network utility functions */
    cy_nw_ip_address_t nw_ip_addr =
    {
        .version = NW_IP_IPV4
    };

    /* Initialize the Wi-Fi device as a Soft AP. */
    cy_wcm_ap_credentials_t softap_credentials = {SOFTAP_SSID, SOFTAP_PASSWORD,
                                                  SOFTAP_SECURITY_TYPE};
    cy_wcm_ip_setting_t softap_ip_info = {
        .ip_address = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_IP_ADDRESS},
        .gateway = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_GATEWAY},
        .netmask = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_NETMASK}};

    cy_wcm_wifi_band_t wifi_band = CY_WCM_WIFI_BAND_ANY;
    cy_wcm_ap_config_t softap_config = {softap_credentials, wifi_band,
                                         SOFTAP_RADIO_CHANNEL,
                                         softap_ip_info,
                                         NULL};

    /* Start the the Wi-Fi device as a Soft AP. */
    result = cy_wcm_start_ap(&softap_config);

    if(CY_RSLT_SUCCESS == result)
    {
        printf("Wi-Fi Device configured as Soft AP\n");
        printf("Connect TCP client device to the network: "
               "SSID: %s Password:%s\n",
                SOFTAP_SSID, SOFTAP_PASSWORD);
        nw_ip_addr.ip.v4 = softap_ip_info.ip_address.ip.v4;
        cy_nw_ntoa(&nw_ip_addr, ip_addr_str);
        printf("SofAP IP Address : %s\n\n", ip_addr_str);
    }

    return result;
}
#endif /* USE_AP_INTERFACE */

/*******************************************************************************
* Function Name: create_tcp_client_socket
********************************************************************************
* Summary:
*  Function to create a socket and set the socket options.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: Returns CY_RSLT_SUCCESS if the TCP client socket is created.
*
*******************************************************************************/
cy_rslt_t create_tcp_client_socket(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;;

    /* TCP keep alive parameters. */
    int keep_alive = 1;
#if defined (COMPONENT_LWIP)
    uint32_t keep_alive_interval = TCP_KEEP_ALIVE_INTERVAL_MS;
    uint32_t keep_alive_count    = TCP_KEEP_ALIVE_RETRY_COUNT;
    uint32_t keep_alive_idle_time = TCP_KEEP_ALIVE_IDLE_TIME_MS;
#endif

    /* Variables used to set socket options. */
    cy_socket_opt_callback_t tcp_disconnect_option;

    /* Create a new secure TCP socket. */
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM,
                              CY_SOCKET_IPPROTO_TCP, &client_handle);

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Failed to create socket!\n");
        return result;
    }

    /* Register the callback function to handle disconnection. */
    tcp_disconnect_option.callback = tcp_disconnection_handler;
    tcp_disconnect_option.arg = NULL;

    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                  CY_SOCKET_SO_DISCONNECT_CALLBACK,
                                  &tcp_disconnect_option,
                                  sizeof(cy_socket_opt_callback_t));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: "
               "CY_SOCKET_SO_DISCONNECT_CALLBACK failed\n");
    }

#if defined (COMPONENT_LWIP)
    /* Set the TCP keep alive interval. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL,
                                  &keep_alive_interval,
                                  sizeof(keep_alive_interval));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: "
               "CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL failed\n");
        return result;
    }

    /* Set the retry count for TCP keep alive packet. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_COUNT,
                                  &keep_alive_count,
                                  sizeof(keep_alive_count));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: "
               "CY_SOCKET_SO_TCP_KEEPALIVE_COUNT failed\n");
        return result;
    }

    /* Set the network idle time before sending the TCP keep alive packet. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME,
                                  &keep_alive_idle_time,
                                  sizeof(keep_alive_idle_time));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: "
               "CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME failed\n");
        return result;
    }
#endif

    /* Enable TCP keep alive. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                      CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE,
                                          &keep_alive, sizeof(keep_alive));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: "
               "CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE failed\n");
        return result;
    }

    return result;
}

/*******************************************************************************
* Function Name: connect_to_tcp_server
********************************************************************************
* Summary:
*  Function to connect to TCP server.
*
* Parameters:
*  cy_socket_sockaddr_t address: Address of TCP server socket
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if a successful
*  connection to the TCP server was established.
*
*******************************************************************************/
cy_rslt_t connect_to_tcp_server(cy_socket_sockaddr_t address)
{
    cy_rslt_t result = CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;
    cy_rslt_t conn_result= CY_RSLT_SUCCESS;

    for(uint32_t conn_retries = 0;
        conn_retries < MAX_TCP_SERVER_CONN_RETRIES;
        conn_retries++)
    {
        /* Create a TCP socket */
        conn_result = create_tcp_client_socket();

        if(CY_RSLT_SUCCESS != conn_result)
        {
            printf("Socket creation failed!\n");
            handle_app_error();
        }

        conn_result = cy_socket_connect(client_handle, &address,
                                        sizeof(cy_socket_sockaddr_t));

        if (CY_RSLT_SUCCESS == conn_result)
        {
            printf("================================================"
                   "============\n");
            printf("Connected to TCP server\n");

            return conn_result;
        }

        printf("Could not connect to TCP server. "
               "Error code: 0x%08"PRIx32"\n", (uint32_t)conn_result);
        printf("Trying to reconnect...\n");

        /* The resources allocated during the socket creation
         * (cy_socket_create) should be deleted.
         */
        cy_socket_delete(client_handle);
    }

     /* Stop retrying after maximum retry attempts. */
     printf("Exceeded maximum connection attempts to the TCP server\n");

     return conn_result;  /* 修复: 返回实际错误码而非初始化值 */
}

/*******************************************************************************
* Function Name: tcp_disconnection_handler
********************************************************************************
* Summary:
*  Callback function to handle TCP socket disconnection event.
*
* Parameters:
*  cy_socket_t socket_handle: Connection handle for the TCP client socket
*  void *args : Parameter passed on to the function (unused)
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if the socket disconnected
*
*******************************************************************************/
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Disconnect the TCP client. */
    result = cy_socket_disconnect(socket_handle, DISCONNECTION_TIMEOUT);

    /* Free the resources allocated to the socket. */
    cy_socket_delete(socket_handle);

    printf("Disconnected from the TCP server! \n");

    /* Give the semaphore so as to connect to TCP server. */
    cy_rtos_semaphore_set(&connect_to_server);

    return result;
}

/* [] END OF FILE */
