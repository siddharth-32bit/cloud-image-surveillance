#include "esp_camera.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "board_config.h"

// =======================
// WiFi Credentials
// =======================
const char *ssid = "Your_WiFi_SSID";
const char *password = "#Your_WiFi_Pass";

// =======================
// Firebase Credentials
// =======================
#define API_KEY "**********"
#define USER_EMAIL "user@email.com"
#define USER_PASSWORD "email_password"
#define STORAGE_BUCKET_ID "firebase_bucket_ID"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =======================
// PIR
// =======================
#define PIR_PIN 13
unsigned long lastTriggerTime = 0;
const unsigned long cooldown = 5000;

// =======================
// Setup
// =======================

void tokenStatusCallback(TokenInfo info) {
  Serial.printf("Token status: %s\n", info.status == token_status_ready ? "ready" : "not ready");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nBooting...");
  pinMode(PIR_PIN, INPUT);

  // Camera configuration
  camera_config_t cam_config;
  cam_config.ledc_channel = LEDC_CHANNEL_0;
  cam_config.ledc_timer = LEDC_TIMER_0;
  cam_config.pin_d0 = Y2_GPIO_NUM;
  cam_config.pin_d1 = Y3_GPIO_NUM;
  cam_config.pin_d2 = Y4_GPIO_NUM;
  cam_config.pin_d3 = Y5_GPIO_NUM;
  cam_config.pin_d4 = Y6_GPIO_NUM;
  cam_config.pin_d5 = Y7_GPIO_NUM;
  cam_config.pin_d6 = Y8_GPIO_NUM;
  cam_config.pin_d7 = Y9_GPIO_NUM;
  cam_config.pin_xclk = XCLK_GPIO_NUM;
  cam_config.pin_pclk = PCLK_GPIO_NUM;
  cam_config.pin_vsync = VSYNC_GPIO_NUM;
  cam_config.pin_href = HREF_GPIO_NUM;
  cam_config.pin_sccb_sda = SIOD_GPIO_NUM;
  cam_config.pin_sccb_scl = SIOC_GPIO_NUM;
  cam_config.pin_pwdn = PWDN_GPIO_NUM;
  cam_config.pin_reset = RESET_GPIO_NUM;
  cam_config.xclk_freq_hz = 20000000;
  cam_config.pixel_format = PIXFORMAT_JPEG;

  // Recommended for your experiment
  cam_config.frame_size = FRAMESIZE_VGA;   // 640x480
  cam_config.jpeg_quality = 12;            // 10–20 range
  cam_config.fb_count = 2;
  cam_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  cam_config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&cam_config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  // =======================
  // WiFi
  // =======================
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // =======================
  // Firebase Init
  // =======================
  Serial.println("Initializing Firebase...");

  config.api_key = API_KEY;
  config.token_status_callback = tokenStatusCallback;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Waiting for Firebase authentication...");

  while (auth.token.uid == "") {
    Serial.print(".");
    delay(500);
  }

Serial.println("\nFirebase ready.");
}

// =======================
// Main Loop
// =======================
void loop() {

  int pirState = digitalRead(PIR_PIN);

  if (pirState == HIGH && millis() - lastTriggerTime > cooldown) {

    lastTriggerTime = millis();
    Serial.println("\nMotion detected!");

    // Capture start time
    unsigned long capture_start = millis();
    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    unsigned long capture_time = millis() - capture_start;

    Serial.print("Image size (bytes): ");
    Serial.println(fb->len);

    Serial.print("Capture time (ms): ");
    Serial.println(capture_time);

    // Unique filename
    String fileName = "/motion_" + String(millis()) + ".jpg";

    // Upload start time
    unsigned long upload_start = millis();

    bool uploadStatus = Firebase.Storage.upload(
      &fbdo,
      STORAGE_BUCKET_ID,
      fb->buf,
      fb->len,
      fileName,
      "image/jpeg"
    );

    unsigned long upload_time = millis() - upload_start;

    if (uploadStatus) {
      Serial.println("Upload successful");
      Serial.print("Upload time (ms): ");
      Serial.println(upload_time);

      Serial.print("Total delay (ms): ");
      Serial.println(capture_time + upload_time);
    } else {
      Serial.println("Upload failed:");
      Serial.println(fbdo.errorReason());
    }

    esp_camera_fb_return(fb);
  }

  delay(50);
}