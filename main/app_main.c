#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "driver/gpio.h"

#include "gt_pppos.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "peer.h"

static const char* TAG = "webrtc";

static TaskHandle_t xPcTaskHandle = NULL;
static TaskHandle_t xPsTaskHandle = NULL;
static TaskHandle_t xCameraTaskHandle = NULL;


extern esp_err_t camera_init();
extern void camera_task(void* pvParameters);

SemaphoreHandle_t xSemaphore = NULL;

PeerConnection* g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

conn_info_t gsm_info = {
  .operator_name = "",
  .rssi = -1,
  .ber = -1};

static void oniceconnectionstatechange(PeerConnectionState state, void* user_data) {
  ESP_LOGI(TAG, "PeerConnectionState: %d", state);
  eState = state;
  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED) {
    gDataChannelOpened = 0;
  }
}

static void onmessage(char* msg, size_t len, void* userdata, uint16_t sid) {
  ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
}

void onopen(void* userdata) {
  ESP_LOGI(TAG, "Datachannel opened");
  gDataChannelOpened = 1;
}

static void onclose(void* userdata) {
}

void peer_signaling_task(void* arg) {
  ESP_LOGI(TAG, "peer_signaling_task started");

  for (;;) {
    peer_signaling_loop();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void peer_connection_task(void* arg) {
  ESP_LOGI(TAG, "peer_connection_task started");

  for (;;) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
      peer_connection_loop(g_pc);
      xSemaphoreGive(xSemaphore);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}



void app_main(void) {

  static char deviceid[32] = {0};
  uint8_t mac[8] = {0};

  PeerConfiguration config = {
      .ice_servers = {
          // {
          //     .urls = "turn:global.relay.metered.ca:80",
          //     .username = "**********Your Username********",
          //     .credential = "**********Your credentials********",
          // },
          // {
          //     .urls = "turn:global.relay.metered.ca:80?transport=tcp",
          //     .username = "**********Your Username********",
          //     .credential = "**********Your credentials********",
          // },
          {
              .urls = "turns:standard.relay.metered.ca:443?transport=tcp",
              .username = "**********Your Username********",
              .credential = "**********Your credentials********",
          },
          {
              .urls = "turns:standard.relay.metered.ca:443",
              .username = "**********Your Username********",
              .credential = "**********Your credentials********",
          },
          {
              .urls = "stun:stun.l.google.com:19302",
          },
      },

      .datachannel = DATA_CHANNEL_BINARY,
  };

  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // ESP_ERROR_CHECK(example_connect());

  modem_init();
  operator_register(&gsm_info);
  sim_ppp_connect();

  // if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
  //     sprintf(deviceid, "esp32-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  //     ESP_LOGI(TAG, "Device ID: %s", deviceid);
  // }

  xSemaphore = xSemaphoreCreateMutex();

  peer_init();

  camera_init();

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  ServiceConfiguration service_config = SERVICE_CONFIG_DEFAULT();
  service_config.client_id = deviceid;
  service_config.pc = g_pc;
  service_config.mqtt_url = "broker.emqx.io";
  peer_signaling_set_config(&service_config);
  
  peer_signaling_join_channel(deviceid, g_pc);

  xTaskCreatePinnedToCore(camera_task, "camera", 52768, NULL, 10, &xCameraTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_connection_task, "peer_connection", 8192, NULL, 5, &xPcTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_signaling_task, "peer_signaling", 8192, NULL, 6, &xPsTaskHandle, 1);

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
