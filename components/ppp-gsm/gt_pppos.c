/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"
#include <unistd.h> 
#include <ctype.h>
#include "gt_pppos.h"
#include <time.h>
#include <sys/time.h>
#include "ppp.h"

#if CONFIG_GSM_SERIAL_CONFIG_USB
#include "esp_modem_usb_config.h"
#endif

// Define TAG before it's used in any function
static const char *TAG =  "-------gt_pppos" ;

 #if defined(CONFIG_GSM_FLOW_CONTROL_NONE)
#define GSM_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_GSM_FLOW_CONTROL_SW)
#define GSM_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_GSM_FLOW_CONTROL_HW)
#define GSM_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif

// USB terminal error handler and helper macro for handling USB disconnection
#if CONFIG_GSM_SERIAL_CONFIG_USB
void usb_terminal_error_handler(esp_modem_terminal_error_t err)
{
    if (err == ESP_MODEM_TERMINAL_DEVICE_GONE) {
        ESP_LOGI(TAG, "USB modem disconnected");
        assert(gsm_event_group);
        xEventGroupSetBits(gsm_event_group, USB_DISCONNECTED_BIT);
    }
}

#define CHECK_USB_DISCONNECTION(event_group) \
if ((xEventGroupGetBits(event_group) & USB_DISCONNECTED_BIT) == USB_DISCONNECTED_BIT) { \
    ESP_LOGE(TAG, "USB disconnected - aborting operation"); \
    return ESP_ERR_NO_MEM; \
}
#else
#define CHECK_USB_DISCONNECTION(event_group)
#endif

EventGroupHandle_t gsm_event_group = NULL;
esp_modem_dce_t *dce;
esp_netif_t *Global_Modem_Netif;



esp_err_t modem_init(){

    gsm_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    /* Configure the PPP netif */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_GSM_MODEM_PPP_APN);

    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

    Global_Modem_Netif = esp_netif_new(&netif_ppp_config);

    assert(Global_Modem_Netif);

#if defined(CONFIG_GSM_SERIAL_CONFIG_UART)
    /* Configure the UART DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    dte_config.uart_config.tx_io_num = CONFIG_GSM_MODEM_UART_RX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_GSM_MODEM_UART_TX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_GSM_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_GSM_MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = GSM_FLOW_CONTROL;
    dte_config.uart_config.rx_buffer_size = CONFIG_GSM_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_GSM_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_GSM_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_GSM_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_GSM_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_GSM_MODEM_UART_RX_BUFFER_SIZE / 2;
    dte_config.uart_config.port_num = CONFIG_GSM_MODEM_UART_NO;
    dte_config.uart_config.baud_rate = CONFIG_GSM_MODEM_UART_BAUD_RATE;


#if CONFIG_GSM_MODEM_DEVICE_BG96 == 1
        ESP_LOGI(TAG, "Initializing esp_modem for the BG96 module...");
        dce = esp_modem_new_dev(ESP_MODEM_DCE_BG96, &dte_config, &dce_config, Global_Modem_Netif);
#elif CONFIG_GSM_MODEM_DEVICE_SIM800 == 1
        ESP_LOGI(TAG,  "Initializing esp_modem for the SIM800 module..." );
        dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, Global_Modem_Netif);
#elif CONFIG_GSM_MODEM_DEVICE_SIM7000 == 1
        ESP_LOGI(TAG, "Initializing esp_modem for the SIM7000 module...");
        dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7000, &dte_config, &dce_config, Global_Modem_Netif);
#elif CONFIG_GSM_MODEM_DEVICE_SIM7070 == 1
        ESP_LOGI(TAG, "Initializing esp_modem for the SIM7070 module...");
        dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7070, &dte_config, &dce_config, Global_Modem_Netif);
#elif CONFIG_GSM_MODEM_DEVICE_SIM7600 == 1
        ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
        dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, Global_Modem_Netif);
#elif CONFIG_GSM_MODEM_DEVICE_CUSTOM == 1
        ESP_LOGI(TAG, "Initializing esp_modem with custom module...");
        dce = esp_modem_new_dev(ESP_MODEM_DCE_CUSTOM, &dte_config, &dce_config, Global_Modem_Netif);
#else
        ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
        dce = esp_modem_new(&dte_config, &dce_config, Global_Modem_Netif);
#endif

    
    assert(dce);
    
    if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW)
    {
        esp_err_t err = esp_modem_set_flow_control(dce, 2, 2); // 2/2 means HW Flow Control.
        if (err != ESP_OK)    ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
        ESP_LOGI(TAG, "HW set_flow_control OK");
    }

#elif defined(CONFIG_GSM_SERIAL_CONFIG_USB)
    // USB Driver configuration
    ESP_LOGI(TAG, "Initializing esp_modem with USB connection");
    
#if defined(CONFIG_GSM_MODEM_DEVICE_SIM7670) || defined(CONFIG_GSM_MODEM_DEVICE_A7670)
    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7670/A7670 module via USB...");
    
    // Use the SIM7600 driver for SIM7670/A7670 devices
    esp_modem_dce_device_t usb_dev_type = ESP_MODEM_DCE_SIM7600;
    
    // Create USB terminal configuration with custom parameters specifically for SIM7670/A7670
    struct esp_modem_usb_term_config usb_term_config = {
        .vid = 0x1E0E,                     // SimCom vendor ID
        .pid = 0x9011,                     // A7670 product ID
        .interface_idx = 5,                // Interface 5 for AT commands
        .secondary_interface_idx = -1,     // No secondary interface
        .timeout_ms = 10000,               // 10 seconds timeout
        .xCoreID = 0,                      // Core 0
        .cdc_compliant = true,             // This is critical - set it to CDC compliant
        .install_usb_host = true           // Install USB host driver
    };
    
    // Create DTE configuration with USB settings
    esp_modem_dte_config_t dte_usb_config = ESP_MODEM_DTE_DEFAULT_USB_CONFIG(usb_term_config);
    
    // Configure DTE buffer size - must be equal to CONFIG_TINYUSB_CDC_RX_BUFSIZE (4096)
    dte_usb_config.dte_buffer_size = 4096; // Matches CONFIG_TINYUSB_CDC_RX_BUFSIZE=4096 in sdkconfig
    dte_usb_config.task_stack_size = 4096; // Standard 4K stack for most ESP-IDF tasks
    dte_usb_config.task_priority = 10;
#else
#error USB modem device not selected or supported
#endif

    ESP_LOGI(TAG, "Waiting for USB device connection...");
    
    // Create the DCE with USB support
    dce = esp_modem_new_dev_usb(usb_dev_type, &dte_usb_config, &dce_config, Global_Modem_Netif);
    assert(dce);
    
    // Register USB error handler callback
    esp_modem_set_error_cb(dce, usb_terminal_error_handler);
    
    ESP_LOGI(TAG, "USB modem connected, waiting 5 seconds for boot...");
    vTaskDelay(pdMS_TO_TICKS(5000)); // Give DTE some time to boot
#else
#error Invalid serial connection to modem. Set CONFIG_GSM_SERIAL_CONFIG_UART or CONFIG_GSM_SERIAL_CONFIG_USB
#endif

    xEventGroupClearBits(gsm_event_group, PPP_GOT_IP_BIT);

    return ESP_OK;
}

/**
 * @brief Establish a PPP connection with the GSM modem.
 *
 * This function initializes the GSM modem, sets it to command mode, and then
 * attempts to establish a PPP connection. It waits for the PPP_GOT_IP_BIT
 * event group to be set, indicating a successful connection. If the connection
 * is not established after a few retries, the function returns an error.
 * When using USB, it also handles USB disconnection events.
 *
 * @return ESP_OK on successful connection, ESP_FAIL on failure.
 */
esp_err_t sim_ppp_connect() {
    const int STEP_DELAY_MS = 200;
    const int MAX_RETRIES = 2;
    
    char response[1000] = {0};
    int retry_count = 0;

    // Clear any previous USB disconnected event
    if (gsm_event_group) {
        xEventGroupClearBits(gsm_event_group, USB_DISCONNECTED_BIT);
    }

    if (esp_modem_sync(dce) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sync modem");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if USB is disconnected (for USB configuration)
    CHECK_USB_DISCONNECTION(gsm_event_group);
    
    // Clear modem state
    esp_modem_at(dce, "AT+CIPSHUT", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    esp_modem_at(dce, "AT+CGACT=0", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    esp_modem_at(dce, "AT+SAPBR=0,1", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

    while (retry_count < MAX_RETRIES) {

        ESP_LOGI(TAG, "Calling set_data_mode (attempt %d/%d)", retry_count + 1, MAX_RETRIES);

        if (set_data_mode(dce) != ESP_OK) {
            retry_count++;
            if (retry_count >= MAX_RETRIES) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(gsm_event_group, 
                                             PPP_GOT_IP_BIT,
                                             pdFALSE,
                                             pdFALSE, 
                                             pdMS_TO_TICKS(PPP_CONNECT_TIMEOUT_MS));

        if (bits & PPP_GOT_IP_BIT) {
            return ESP_OK;
        }
        
        ESP_LOGE(TAG, "PPP_GOT_IP_BIT timeout of %d sec",  PPP_CONNECT_TIMEOUT_MS /1000);
        
        retry_count++;
        if (retry_count >= MAX_RETRIES) {
            return ESP_ERR_NOT_FINISHED;
        }
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    }

    ESP_LOGE(TAG, "Failed to establish PPP connection after %d attempts",  MAX_RETRIES);
    
    return ESP_ERR_TIMEOUT;
}

const char* ppp_err_to_name(int32_t err) {
    switch (err) {
        case PPPERR_NONE: return "PPPERR_NONE";
        case PPPERR_PARAM: return "PPPERR_PARAM";
        case PPPERR_OPEN: return "PPPERR_OPEN";
        case PPPERR_DEVICE: return "PPPERR_DEVICE";
        case PPPERR_ALLOC: return "PPPERR_ALLOC";
        case PPPERR_USER: return "PPPERR_USER";
        case PPPERR_CONNECT: return "PPPERR_CONNECT";
        case PPPERR_AUTHFAIL: return "PPPERR_AUTHFAIL";
        case PPPERR_PROTOCOL: return "PPPERR_PROTOCOL";
        case PPPERR_PEERDEAD: return "PPPERR_PEERDEAD";
        case PPPERR_IDLETIMEOUT: return "PPPERR_IDLETIMEOUT";
        case PPPERR_CONNECTTIME: return "PPPERR_CONNECTTIME";
        case PPPERR_LOOPBACK: return "PPPERR_LOOPBACK";
        default: return "PPPERR_UNKNOWN";
    }
}

void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) 
{
    ESP_LOGD(TAG, "PPP state changed event %ld", event_id);

    if (event_id >= NETIF_PP_PHASE_OFFSET) {
        // Handle PPP phase events
        switch (event_id) {
            case NETIF_PPP_PHASE_INITIALIZE:
                ESP_LOGI(TAG, "PPP phase: Initialize");
                break;

            case NETIF_PPP_PHASE_ESTABLISH:
                ESP_LOGI(TAG, "PPP phase: Establish");
                break;

            case NETIF_PPP_PHASE_RUNNING:
                ESP_LOGI(TAG, "PPP phase: Running");
                if (gsm_event_group) {
                    xEventGroupSetBits(gsm_event_group, PPP_GOT_IP_BIT);
                }
                break;

            case NETIF_PPP_PHASE_TERMINATE:
            case NETIF_PPP_PHASE_DISCONNECT:
                ESP_LOGI(TAG, "PPP phase: Terminating/Disconnecting");
                if (gsm_event_group) {
                    xEventGroupClearBits(gsm_event_group, PPP_GOT_IP_BIT);
                }
                break;

            default:
                ESP_LOGD(TAG, "PPP phase event: %ld", event_id);
                break;
        }
    } else {
        // Handle PPP error events
        switch (event_id) {
            case NETIF_PPP_ERRORUSER:
                ESP_LOGI(TAG, "User interrupted event from netif:%p, error: PPPERR_USER", event_data);
                if (gsm_event_group) {
                    xEventGroupClearBits(gsm_event_group, PPP_GOT_IP_BIT);
                }
                break;

            case NETIF_PPP_ERRORCONNECT:
            case NETIF_PPP_ERRORAUTHFAIL:
            case NETIF_PPP_ERRORPROTOCOL:
                ESP_LOGE(TAG, "PPP error event: %ld", event_id);
                if (gsm_event_group) {
                    xEventGroupClearBits(gsm_event_group, PPP_GOT_IP_BIT);
                }
                break;

            default:
                ESP_LOGD(TAG, "PPP error event: %ld", event_id);
                break;
        }
    }
}

void on_ip_event(void *arg, esp_event_base_t event_base,
                       int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    
    switch (event_id) {
        case IP_EVENT_PPP_GOT_IP:
            {
                esp_netif_dns_info_t dns_info;
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                esp_netif_t *netif = event->esp_netif;
                
                ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
                ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
                ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
                
                // Get DNS info
                esp_netif_get_dns_info(netif, 0, &dns_info);
                ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
                esp_netif_get_dns_info(netif, 1, &dns_info);
                ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
                ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
                
                // Set IP obtained bit when we get IP
                if (gsm_event_group) {
                    xEventGroupSetBits(gsm_event_group, PPP_GOT_IP_BIT);
                }
            }
            break;
            
        case IP_EVENT_PPP_LOST_IP:
            ESP_LOGI(TAG, "IP lost");
            // Clear IP and connection bits when IP is lost
            if (gsm_event_group) {
                xEventGroupClearBits(gsm_event_group, PPP_GOT_IP_BIT);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
            break;
    }
}

/**
 * @brief Set the modem to command mode and perform various AT commands to clear the modem context and retrieve the operator name.
 *
 * This function attempts to set the modem to command mode, clear the modem context, and retrieve the operator name. 
 * It retries the operator name retrieval up to 10 times, and if that fails, 
 * it performs additional AT commands (AT+COPS=?, AT+CFUN=0, AT+CFUN=1,1) and retries the operator name retrieval up to 3 more times.
 * In between sets to command mode again. For USB connections, it handles disconnection events.
 * 
 * @param info Pointer to a `conn_info_t` struct to store the retrieved operator name and signal quality information.
 * @return `ESP_OK` if the operator name was successfully retrieved, `ESP_FAIL` otherwise.
 */
esp_err_t operator_register(conn_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    char response[1000] = {0};

    esp_err_t ret;

    // Check if USB is disconnected (for USB configuration)
    CHECK_USB_DISCONNECTION(gsm_event_group);

    //
    // Step 1: Make sure we are in COMMAND MODE
    //
    if (esp_modem_sync(dce) != ESP_OK) {
        // Check again for USB disconnection before trying command mode
        CHECK_USB_DISCONNECTION(gsm_event_group);
        
        // Try forcibly setting command mode once
        esp_err_t modeerr = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
        if (modeerr != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enter COMMAND mode");
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "Modem in command mode now.");

   //save APN to modem
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CGDCONT=1", response, 4000);
    vTaskDelay(pdMS_TO_TICKS(200));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CGDCONT=1,\"IP\",\"" CONFIG_GSM_MODEM_PPP_APN "\"", response, 4000);
    vTaskDelay(pdMS_TO_TICKS(200));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CSTT=\"" CONFIG_GSM_MODEM_PPP_APN "\"", response, 4000);
    vTaskDelay(pdMS_TO_TICKS(200));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CLTS=1", response, 3000);
    vTaskDelay(pdMS_TO_TICKS(200));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT&W", response, 4000);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    //
    // Step 2: Quick tries with "AT+COPS?" 
    //
    for (int i = 0; i < 8; i++) {
        ret = try_cops_query(info);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Operator found quickly with no reset needed");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    ESP_LOGW(TAG, "No operator found in quick tries => do full reset.");


    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CREG?", response, 1000);
    if (strstr(response, "+CREG: 0,1") || strstr(response, "+CREG: 0,5"))
    {
        ESP_LOGI(TAG, "Modem is registered on the network");
    }
    else
    {
        ESP_LOGW(TAG, "Modem is not registered");
        ESP_LOGI(TAG, "AT+CREG? response: %s", response);
    }

    // Clear modem context
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CIPSHUT", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(500));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CGACT=0", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(500));
    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+SAPBR=0,1", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(500));

   //
    // Step 3: Full modem reset
    //
    modem_full_reset();


    // Try again
    for (int i = 0; i < 8; i++) {
        ret = try_cops_query(info);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Operator found after full reset");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    memset(response, 0, sizeof(response));
    esp_modem_at(dce, "AT+CREG?", response, 1000);
    if (strstr(response, "+CREG: 0,1") || strstr(response, "+CREG: 0,5"))
    {
        ESP_LOGI(TAG, "Modem is registered on the network");
    }
    else
    {
        ESP_LOGW(TAG, "Modem is not registered");
    }

    // If we get here, no operator recognized
    ESP_LOGE(TAG,   "operator_register() FAIL: no operator after multiple attempts."  );
    return ESP_FAIL;

}

 esp_err_t try_cops_query(conn_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    char response[500] = {0};
    esp_err_t err;

    // Check for USB disconnection first
    CHECK_USB_DISCONNECTION(gsm_event_group);

    memset(response, 0, sizeof(response));

    // 1. Send "AT+COPS?"
    err = esp_modem_at(dce, "AT+COPS?", response, 3000);
    ESP_LOGI(TAG,   "AT+COPS? => response=\"%s\", err=%s " , response, esp_err_to_name(err));

    if (err != ESP_OK) {
        return ESP_FAIL;  // Command itself failed
    }

    // 2. Check if we got a known operator
    char name[32] = {0};
    if (!is_known_operator(response, name, sizeof(name))) {
        // Operator wasn't found in the response
        return ESP_FAIL;
    }

    // 3. We found an operator => gather signal quality
    int rssi = 0, ber = 0;
    esp_err_t csqerr = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (csqerr == ESP_FAIL) {
        vTaskDelay(pdMS_TO_TICKS(500));
        // Try once more if we want
        esp_modem_get_signal_quality(dce, &rssi, &ber);
    }

    // 4. Log it
    ESP_LOGD(TAG, "Operator = %s", name);
    ESP_LOGD(TAG, "RSSI=%d, BER=%d", rssi, ber);

    // 5. Save to the info structure
    strncpy(info->operator_name, name, sizeof(info->operator_name) - 1);
    info->operator_name[sizeof(info->operator_name) - 1] = '\0';
    info->rssi = rssi;
    info->ber  = ber;

    return ESP_OK;
}

esp_err_t modem_full_reset(void)
{
    char resp[500] = {0};
    esp_err_t ret;

    // First disable RF
    ret = esp_modem_at(dce, "AT+CFUN=0", resp, 3000);
    ESP_LOGI(TAG, "AT+CFUN=0 => response=\"%s\", err=%s", resp, esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Clear response buffer
    memset(resp, 0, sizeof(resp));
    
    // Send reset command
    ret = esp_modem_at(dce, "AT+CFUN=1,1", resp, 5000);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "AT+CFUN=1,1 Failed => response=\"%s\", err=%s", resp, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "AT+CFUN=1,1 => response=\"%s\", err=%s", resp, esp_err_to_name(ret));
    
    // Give the modem time to reset and recover
    ESP_LOGI(TAG, "Waiting for modem to reset...");
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second for modem to initialize

    // Try to sync with modem multiple times
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Attempting modem sync, try %d/5", i + 1);
        if (esp_modem_sync(dce) == ESP_OK) {
            ESP_LOGI(TAG, "Modem sync successful after reset");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second between attempts
    }

    ESP_LOGE(TAG, "Modem not responding after reset and sync attempts");
    return ESP_FAIL;
}

esp_err_t set_data_mode(esp_modem_dce_t *dce)
{
    if (dce == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 1000;
    int retry_count = 0;
    esp_err_t err;

    while (retry_count <= MAX_RETRIES) {
        // Check if USB is disconnected (for USB configuration)
        CHECK_USB_DISCONNECTION(gsm_event_group);

        err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "in set_data_mode : esp_modem_set_mode set ESP_MODEM_MODE_DATA success");
            return ESP_OK;
        }

        retry_count++;
        if (retry_count <= MAX_RETRIES) {
            ESP_LOGW(TAG, "in set_data_mode : Failed to set ESP_MODEM_MODE_DATA (attempt %d/%d): %d",  retry_count, MAX_RETRIES, err);

            // Check for USB disconnection before attempting to set command mode
            CHECK_USB_DISCONNECTION(gsm_event_group);
            
            esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
            
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "in set_data_mode : Failed to set ESP_MODEM_MODE_DATA after %d attempts", MAX_RETRIES);
    return ESP_FAIL;
}

// Extracts operator name from AT command response
bool is_known_operator(const char *response, char *name, size_t name_size)
{
    if (!response || !name)
        return false;

    // Extract operator name from response to COPS query 
    // First check direct match in a quoted string format like "+COPS: 0,2,\"20210\",7"
    const char *start_quote = strstr(response, "\"");
    if (start_quote) {
        const char *end_quote = strstr(start_quote + 1, "\"");
        if (end_quote && (end_quote - start_quote) < 32) {
            size_t len = end_quote - start_quote - 1;
            if (len < name_size) {
                strncpy(name, start_quote + 1, len);
                name[len] = '\0';
                // Return true since we found a valid operator in quotes
                return true;
            }
        }
    }

    // Convert to lowercase for case-insensitive comparison
    char resp_lower[1000];
    strncpy(resp_lower, response, sizeof(resp_lower) - 1);
    resp_lower[sizeof(resp_lower) - 1] = '\0';

    for (int i = 0; resp_lower[i]; i++)
    {
        resp_lower[i] = tolower(resp_lower[i]);
    }

    if (strstr(resp_lower, "vodafone"))
    {
        strncpy(name, "VODAFONE", name_size - 1);
        return true;
    }
    else if (strstr(resp_lower, "20205"))
    {
        strncpy(name, "20205", name_size - 1);
        return true;
    }
    else if (strstr(resp_lower, "cosmote"))
    {
        strncpy(name, "COSMOTE", name_size - 1);
        return true;
    }
    else if (strstr(resp_lower, "20201"))
    {
        strncpy(name, "20201", name_size - 1);
        return true;
    }
    else if (strstr(resp_lower, "20210"))
    {
        strncpy(name, "20210", name_size - 1);
        return true;
    }
    else if (strstr(resp_lower, "nova"))
    {
        strncpy(name, "NOVA", name_size - 1);
        return true;
    }
    return false;
}
