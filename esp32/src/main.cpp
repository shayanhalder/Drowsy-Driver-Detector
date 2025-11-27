#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "camera_pin_layout.h"
#include <secrets.h> 

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
String serverName = SERVER_IP;

String serverPath = "/detect";
const int serverPort = 5241;

const int timerInterval = 2000;    // time between each HTTP POST image

void setup() {
    Serial.begin(115200);
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(WiFi.macAddress());

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        delay(1000);
        ESP.restart();
    }
}

void loop() {
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if(!fb) {
        Serial.println("Camera capture failed");
        delay(1000);
        ESP.restart();
    }

    // send the image as a post request
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://" + serverName + ":" + String(serverPort) + serverPath;
        http.begin(url.c_str());
        http.addHeader("Content-Type", "image/jpeg");

        Serial.println(fb->len);
        int httpResponseCode = http.POST(fb->buf, fb->len);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response:");
            Serial.println(response);
        } 
        else {
            Serial.printf("Error occurred while sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected");
    }

    esp_camera_fb_return(fb);
    
    delay(timerInterval);
}          

