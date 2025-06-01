#ifndef GT_PPPOS_H
#define GT_PPPOS_H

#include "esp_err.h"
#include "esp_modem_api.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h" // Include PPP definitions
#include "esp_event.h"
#include <time.h>

// Include USB modem support if configured
#if CONFIG_GSM_SERIAL_CONFIG_USB
#include "esp_modem_usb_c_api.h"
#include "freertos/task.h"
#endif

// Connection info structure
typedef struct {
    char operator_name[32];
    int rssi;
    int ber;
} conn_info_t;

// Global variables
extern EventGroupHandle_t gsm_event_group;
extern esp_modem_dce_t *dce;
extern esp_netif_t *Global_Modem_Netif;

// Event group bits
#define PPP_GOT_IP_BIT     BIT0  
#define PPP_STOPPED_BIT    BIT1    
#define GOT_DATA_BIT       BIT2   
#define USB_DISCONNECTED_BIT BIT5  // Used to signal USB device disconnection

// PPP state bits
#define PPP_CONNECTING_BIT BIT3    // PPP starting connection
#define PPP_CONNECTED_BIT  BIT4    // PPP connection established
#define PPP_ERROR_BIT      BIT6    // PPP error occurred
#define IP_ACQUIRED_BIT    BIT7    // IP address obtained

// Combined state masks
#define PPP_SUCCESS_STATE  (PPP_CONNECTED_BIT | PPP_RUNNING_BIT | IP_ACQUIRED_BIT)
#define PPP_FAILED_STATE   (PPP_ERROR_BIT)

#define PPP_CONNECT_TIMEOUT_MS    15000


// Function prototypes
esp_err_t modem_init();
esp_err_t sim_ppp_connect(void);
esp_err_t operator_register(conn_info_t *info);
esp_err_t set_data_mode(esp_modem_dce_t *dce);
bool is_valid_ip(const char *ip);
void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
const char* ppp_err_to_name(int32_t err) ;
 bool is_known_operator(const char *response, char *name, size_t name_size) ;
 esp_err_t try_cops_query(conn_info_t *info);
 esp_err_t modem_full_reset(void);

#if CONFIG_GSM_SERIAL_CONFIG_USB
void usb_terminal_error_handler(esp_modem_terminal_error_t err);
#endif

#endif // GT_PPPOS_H