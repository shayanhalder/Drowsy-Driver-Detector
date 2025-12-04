#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "camera_pin_layout.h"
#include <secrets.h>
#include <ArduinoJson.h> 


const int BAUAD_RATE = 115200;
const int timerInterval = 1000;    // time between each HTTP POST image
const int drowsyCooldown = 10 * 60000; // 10 minutes cooldown after drowsiness detected

HTTPClient detectHttp;
String detectEndpoint = "http://" + SERVER_IP + SERVER_DETECT_PATH;

HTTPClient routeHttp;
String routeEndpoint = String("http://") + MOBILE_IP + ":8080" + "/navigate";

HTTPClient coordinatesHttp;
String coordinatesEndpoint = String("http://") + MOBILE_IP + ":8080" + "/location";

HTTPClient gasStationHttp;

JsonDocument getNearestGasStation(float latitude, float longitude) {
    JsonDocument doc;
    doc["includedTypes"][0] = "gas_station";
    doc["maxResultCount"] = 10;
    doc["locationRestriction"]["circle"]["center"]["latitude"] = latitude;
    doc["locationRestriction"]["circle"]["center"]["longitude"] = longitude;
    doc["locationRestriction"]["circle"]["radius"] = 5000.0;

    String requestBody;
    serializeJson(doc, requestBody);
    int httpResponseCode = gasStationHttp.POST(requestBody);

    JsonDocument responseDoc;

    if (httpResponseCode > 0) {
        String response = gasStationHttp.getString();
        Serial.println("Response:");
        Serial.println(response);

        DeserializationError error = deserializeJson(responseDoc, response);
        if (error) {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
        } else {
            // Extract latitude and longitude from first place in array
            if (responseDoc.containsKey("places") && responseDoc["places"].is<JsonArray>()) {
                JsonArray places = responseDoc["places"].as<JsonArray>();
                if (places.size() > 0) {
                    JsonObject firstPlace = places[0];
                    float gasStationLat = firstPlace["location"]["latitude"];
                    float gasStationLng = firstPlace["location"]["longitude"];

                    Serial.println("Nearest Gas Station:");
                    Serial.printf("Latitude: %.6f\n", gasStationLat);
                    Serial.printf("Longitude: %.6f\n", gasStationLng);
                    return responseDoc;
                } else {
                    Serial.println("No places found in response");
                }
            }
        }
        
    } else {
        Serial.printf("Error occurred while sending POST: %s\n", gasStationHttp.errorToString(httpResponseCode).c_str());
    }


    return responseDoc;
}

JsonDocument getCoordinates() {
    JsonDocument doc;
    int httpResponseCode = coordinatesHttp.GET();

    if (httpResponseCode > 0) {
        String response = coordinatesHttp.getString();
        Serial.println("Response:");
        Serial.println(response);
        DeserializationError error = deserializeJson(doc, response);
        if (error) {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
        }
        float latitude = doc["latitude"];
        float longitude = doc["longitude"];
        Serial.print("Latitude: ");
        Serial.println(latitude);
        Serial.print("Longitude: ");
        Serial.println(longitude);
    } else {
        Serial.printf("Error occurred while sending POST: %s\n", coordinatesHttp.errorToString(httpResponseCode).c_str());
    }

    return doc;
}


void setup() {
    Serial.begin(BAUAD_RATE);

    Serial.println("Wifi ssid: " + WIFI_SSID);
    Serial.println("Wifi password: " + WIFI_PASSWORD);
    Serial.println("Connecting to WiFi...");

    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
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

    detectHttp.begin(detectEndpoint.c_str());
    detectHttp.addHeader("Content-Type", "image/jpeg");

    routeHttp.begin(routeEndpoint);
    routeHttp.addHeader("Content-Type", "application/json");

    coordinatesHttp.begin(coordinatesEndpoint);
    coordinatesHttp.addHeader("Content-Type", "application/json");

    gasStationHttp.begin(GOOGLE_MAPS_API_ENDPOINT);
    gasStationHttp.addHeader("Content-Type", "application/json");
    gasStationHttp.addHeader("X-Goog-Api-Key", GOOGLE_MAPS_API_KEY);
    gasStationHttp.addHeader("X-Goog-FieldMask", "places.displayName,places.location");

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
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        esp_camera_fb_return(fb);
        delay(timerInterval);
        return;
    }

    int httpResponseCode = detectHttp.POST(fb->buf, fb->len);

    if (httpResponseCode <= 0) {
        Serial.printf("Error occurred while sending POST: %s\n", detectHttp.errorToString(httpResponseCode).c_str());
        esp_camera_fb_return(fb);
        delay(timerInterval);
        return;
    }

    String response = detectHttp.getString();
    Serial.println("Response:");
    Serial.println(response);
    
    // parse json response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
        
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        esp_camera_fb_return(fb);
        delay(timerInterval);
        return;
    }

    if (!doc.containsKey("drowsy")) {
        Serial.println("ERROR: No drowsy key in response");
        esp_camera_fb_return(fb);
        delay(timerInterval);
        return;
    }
    
    bool drowsy = doc["drowsy"];
    if (!drowsy) {
        Serial.println("Driver is alert.");
        esp_camera_fb_return(fb);
        delay(timerInterval);
        return;
    }

    Serial.println("ALERT: Drowsiness detected!");

    // get the current GPS coordinates
    JsonDocument locationDoc = getCoordinates();
    float latitude = locationDoc["latitude"];
    float longitude = locationDoc["longitude"];

    JsonDocument gasStationDoc = getNearestGasStation(latitude, longitude);
    if (gasStationDoc["places"].is<JsonArray>()) {
        JsonArray places = gasStationDoc["places"].as<JsonArray>();
        if (places.size() == 0) {
            Serial.println("No gas stations found nearby.");
            esp_camera_fb_return(fb);
            delay(timerInterval);
            return;
        }
        
        JsonObject firstPlace = places[0];
        float gasStationLat = firstPlace["location"]["latitude"];
        float gasStationLng = firstPlace["location"]["longitude"];

        JsonDocument routeDoc;
        routeDoc["latitude"] = gasStationLat;
        routeDoc["longitude"] = gasStationLng;

        int routeHttpResponseCode = routeHttp.POST(routeDoc.as<String>());

        if (routeHttpResponseCode > 0) {
            String routeResponse = routeHttp.getString();
            Serial.println("Route Response:");
            Serial.println(routeResponse);
        } else {
            Serial.printf("Error occurred while sending POST: %s\n", routeHttp.errorToString(routeHttpResponseCode).c_str());
        }

        esp_camera_fb_return(fb);
        delay(drowsyCooldown);
        return;
    }

    esp_camera_fb_return(fb);
    delay(timerInterval);
}

