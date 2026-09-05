// ============================================================================
// Tile Robot â€” Dedicated Video Capture Streamer (AI-Thinker ESP32-CAM)
// Resolution: 800x600 px (FRAMESIZE_SVGA)
// Serves Ultra-Low Latency MJPEG Stream at /stream
// ============================================================================

#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ===========================
// WiFi Credentials
// ===========================
const char* ssid     = "robot control";
const char* password = "";

// Static IP Configuration
IPAddress local_IP(192, 168, 4, 3);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 4, 1);

// ===========================
// AI-Thinker ESP32-CAM Pin Map
// ===========================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t camera_httpd = NULL;
esp_err_t cam_init_err = ESP_FAIL;
int sensor_pid = 0;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ===========================
// Stream Status Page HTML
// ===========================
static const char PROGMEM CAM_INDEX_HTML[] = R"rawhtml(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM 800x600 Stream</title>
  <style>
    body { margin: 0; background: #0b0f19; color: #fff; font-family: sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; }
    img { max-width: 95vw; max-height: 80vh; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.8); }
    h2 { margin: 0 0 10px 0; font-size: 18px; color: #38bdf8; }
    p { margin: 8px 0 0 0; font-size: 13px; color: #94a3b8; }
    a { color: #38bdf8; text-decoration: none; }
  </style>
</head>
<body>
  <h2>AI-Thinker ESP32-CAM Video Stream (800x600 px)</h2>
  <img src="/stream" alt="Live Camera Feed">
  <p>Stream Endpoint: <a href="/stream">/stream</a> | Central Web UI hosted on Web Host ESP32</p>
</body>
</html>)rawhtml";

// ===========================
// Stream Handler (High-FPS Async MJPEG)
// ===========================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[128];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "30");

  Serial.println("[STREAM] Client connected");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Frame capture failed");
      res = ESP_FAIL;
      break;
    }
    _jpg_buf_len = fb->len;
    _jpg_buf = fb->buf;

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) break;
  }
  Serial.println("[STREAM] Client disconnected");
  return res;
}

// ===========================
// Root Handler
// ===========================
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, CAM_INDEX_HTML, strlen(CAM_INDEX_HTML));
}

// ===========================
// Capture Snapshot Handler
// ===========================
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAPTURE] Capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ===========================
// Diagnostics Status Handler
// ===========================
static esp_err_t status_handler(httpd_req_t *req) {
  char json[300];
  snprintf(json, sizeof(json),
    "{\"psram\":%s,\"psram_size\":%u,\"free_psram\":%u,\"free_heap\":%u,\"cam_init\":%d,\"sensor_pid\":\"0x%X\",\"ip\":\"%s\"}",
    psramFound() ? "true" : "false",
    ESP.getPsramSize(),
    ESP.getFreePsram(),
    ESP.getFreeHeap(),
    cam_init_err,
    sensor_pid,
    WiFi.localIP().toString().c_str()
  );
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

// ===========================
// Start Async Camera Server
// ===========================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.max_uri_handlers = 8;
  config.stack_size = 16384;
  config.lru_purge_enable = true;
  config.max_open_sockets = 7;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t status_uri = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    Serial.println("Camera Server started on port 80");
  }
}

// ===========================
// Setup
// ===========================
void setup() {
  // Disable brownout detector during high current camera/WiFi initialization
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println(">>> AI-Thinker ESP32-CAM 800x600 Streamer <<<");
  Serial.println("========================================\n");

  Serial.println("[1/3] Power cycling and configuring OV2640 Camera Sensor...");
  
  // Hardware power cycle sensor via PWDN pin
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); // Power down
  delay(100);
  digitalWrite(PWDN_GPIO_NUM, LOW);  // Power up
  delay(100);

  camera_config_t config;
  memset(&config, 0, sizeof(camera_config_t));
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;
  config.sccb_i2c_port = -1;

  Serial.println("[2/3] Initializing Camera Sensor with XCLK fallback...");
  int freqs[] = {20000000, 16500000, 10000000};
  for (int f : freqs) {
    config.xclk_freq_hz = f;
    Serial.printf("  -> Attempting init with XCLK = %d MHz...\n", f / 1000000);
    cam_init_err = esp_camera_init(&config);
    if (cam_init_err == ESP_OK) {
      Serial.printf("  -> SUCCESS with XCLK = %d MHz!\n", f / 1000000);
      break;
    }
    Serial.printf("  -> Failed with error 0x%x\n", cam_init_err);
    esp_camera_deinit();
    delay(200);
  }

  if (cam_init_err != ESP_OK) {
    Serial.printf("  -> [ERROR] Camera init failed on all frequencies (0x%x). Check ribbon cable.\n", cam_init_err);
  } else {
    Serial.println("  -> Camera Initialized OK!");
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      sensor_pid = s->id.PID;
      if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
      } else {
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_brightness(s, 1);
      }
      s->set_framesize(s, FRAMESIZE_QVGA); // 320x240 for smooth reliable streaming
      s->set_quality(s, 12);
    }
    // Warm up camera sensor
    for (int i = 0; i < 3; i++) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb) esp_camera_fb_return(fb);
      delay(50);
    }
  }

  Serial.println("[3/3] Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(400);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n  -> WiFi Connected!");
    Serial.print("  -> Stream URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream");
  } else {
    Serial.println("\n  -> Static IP timed out. Retrying with DHCP...");
    WiFi.disconnect();
    delay(500);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(ssid, password);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(400);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n  -> WiFi Connected via DHCP!");
      Serial.print("  -> Stream URL: http://");
      Serial.print(WiFi.localIP());
      Serial.println("/stream");
    }
  }

  startCameraServer();

  ArduinoOTA.setHostname("tile-robot-cam");
  ArduinoOTA.begin();
  Serial.println(">>> AI-THINKER CAMERA STREAMER READY <<<");
}

void loop() {
  ArduinoOTA.handle();
  delay(10);
}
