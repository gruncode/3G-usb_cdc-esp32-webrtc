#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "esp_timer.h"

#include "peer_connection.h"

extern PeerConnection* g_pc;
extern int gDataChannelOpened;
extern PeerConnectionState eState;
extern SemaphoreHandle_t xSemaphore;
static const char* TAG = "Camera";

#if defined(CONFIG_ESP32S3_XIAO_SENSE)
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 10
#define CAM_PIN_SIOD 40
#define CAM_PIN_SIOC 39
#define CAM_PIN_D7 48
#define CAM_PIN_D6 11
#define CAM_PIN_D5 12
#define CAM_PIN_D4 14
#define CAM_PIN_D3 16
#define CAM_PIN_D2 18
#define CAM_PIN_D1 17
#define CAM_PIN_D0 15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF 47
#define CAM_PIN_PCLK 13
#endif

// Optimized camera configuration for cellular networks
static camera_config_t camera_config = {
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_pclk = CAM_PIN_PCLK,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .xclk_freq_hz = 20000000,        // Keep at 20MHz for stability
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,     // VGA (640x480) offers better network performance over cellular
    .jpeg_quality = 10,              // (range: 0-63, lower = better quality)
    .fb_count = 4,                   //  buffer count for cellular
    .grab_mode = CAMERA_GRAB_LATEST, // Always get latest frame for real-time viewing
    .fb_location = CAMERA_FB_IN_PSRAM,
};

int64_t get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

esp_err_t camera_init() {
  // initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera Init Failed");
    return err;
  }

  // Get sensor and apply optimization settings for cellular
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_framesize(sensor, camera_config.frame_size);
    
    sensor->set_quality(sensor, camera_config.jpeg_quality);
    
    // sensor->set_brightness(sensor, 0);      // Default brightness
    
    // Disable unnecessary processing to speed up frame acquisition
    // sensor->set_special_effect(sensor, 0);  // No special effects
    // sensor->set_hmirror(sensor, 0);         // No horizontal mirror
    // sensor->set_vflip(sensor, 0);           // No vertical flip
    // sensor->set_dcw(sensor, 0);             // Disable downsize cropping and window
    // sensor->set_colorbar(sensor, 0);        // Disable colorbar test
    
    // // Comment these out to avoid yellowish/dark image while still getting good FPS
    // sensor->set_whitebal(sensor, 0);        // Would cause yellowish tint
    // sensor->set_awb_gain(sensor, 0);        // Would cause color issues
    // sensor->set_aec2(sensor, 0);            // Would cause exposure issues
    // sensor->set_gain_ctrl(sensor, 0);       // Would cause darkness issues
    
    //ESP_LOGI(TAG, "Camera sensor optimized for high FPS cellular transmission");
  }

  return ESP_OK;
}

void camera_task(void* pvParameters) {
  // Statistics variables
  static int fps = 0;
  static int64_t last_time;
  int64_t curr_time;
  
  // Network congestion management - optimized for cellular
  static int no_space_errors = 0;
  static int delay_ms = 30;              // Initial delay in milliseconds between frames
  static int delay_increment = 5;      // Increment delay by 5ms on congestion
  static int min_delay_ms = 20;           // Minimum delay to avoid overloading network
  static int max_delay_ms = 80;          // Maximum delay to avoid freezing camera
  static int recovery_count = 0;
  static int recovery_threshold = 5;     //  successful frames before reducing delay
  
  // For calculating network bitrate
  static size_t bytes_sent = 0;
  
  // Track frame timing
  static int64_t frame_start_time;
  static int frame_processing_time;
  static int64_t bitrate_check_time; 
  
  camera_fb_t* fb = NULL;

  ESP_LOGI(TAG, "Cellular-Optimized Camera Task Started");
  last_time = get_timestamp();
  bitrate_check_time = last_time;

  for (;;) {
    // Apply dynamic delay based on network congestion
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    // Track frame timing
    frame_start_time = get_timestamp();
    
    // Only try to capture and send if connection is ready
    if ((eState == PEER_CONNECTION_COMPLETED) && gDataChannelOpened) {
      
      // Get frame from camera
      fb = esp_camera_fb_get();
      if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
      
      // Try to get access to data channel with  timeout
      if (xSemaphoreTake(xSemaphore, 20 / portTICK_PERIOD_MS)) {
        // Attempt to send the frame
        int ret = peer_connection_datachannel_send(g_pc, (char*)fb->buf, fb->len);
        
        if (ret == 0) {
          // Successful send
          bytes_sent += fb->len;
          fps++;
          recovery_count++;
          
          // If we've had several successful frames, try reducing delay (increasing FPS)
          if (recovery_count >= recovery_threshold && delay_ms > min_delay_ms) {
            delay_ms = delay_ms - 2;  //  gradual reduction
            if (delay_ms < min_delay_ms) delay_ms = min_delay_ms;
            recovery_count = 0;
            ESP_LOGI(TAG, "Network conditions improving, reducing frame delay to %d ms", delay_ms);
          }
          
          // Log stats every ~2 seconds
          curr_time = get_timestamp();
          if ((curr_time - last_time) > 2000) {
            float elapsed_sec = (float)(curr_time - last_time) / 1000.0f;
            float actual_fps = (float)fps / elapsed_sec;
            float kbps = (float)(bytes_sent * 8) / elapsed_sec / 1000.0f;
            
            ESP_LOGI(TAG, "Camera: %.1f FPS, %.1f Kbps, delay: %d ms, frame size: %d bytes", 
                    actual_fps, kbps, delay_ms, fb->len);
            
            // Reset counters
            fps = 0;
            bytes_sent = 0;
            no_space_errors = 0;
            last_time = curr_time;
          }
        } else {
          // Failed to send - handle errors
          if (ret == -2) { // No space error
            no_space_errors++;
            recovery_count = 0;
            
            // More sophisticated congestion control for cellular
            // After multiple errors, back off 
            if (no_space_errors > 3) {
              delay_ms += (delay_increment * 2);
            } else {
              delay_ms += delay_increment;
            }
            
            if (delay_ms > max_delay_ms) delay_ms = max_delay_ms;
            
            ESP_LOGW(TAG, "WebRTC buffer full - increased frame delay to %d ms (error %d)", 
                    delay_ms, no_space_errors);
          } else {
            // Other error
            ESP_LOGD(TAG, "Failed to send camera frame: error %d", ret);
          }
        }
        
        xSemaphoreGive(xSemaphore);
      } else {
        ESP_LOGW(TAG, "Semaphore timeout - datachannel busy");
      }
      
      // Always return the frame buffer
      esp_camera_fb_return(fb);
      
      // Calculate frame processing time
      frame_processing_time = get_timestamp() - frame_start_time;
      if (frame_processing_time > delay_ms) {
        // Log when processing time exceeds our target delay
        ESP_LOGD(TAG, "Frame processing took %lld ms (exceeds target delay of %d ms)", 
                frame_processing_time, delay_ms);
      }
    } else {
      // Connection not ready, wait 
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}
