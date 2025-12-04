#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "camera_pin_layout.h"
#include <secrets.h>
#include <ArduinoJson.h> 

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
String serverName = SERVER_IP;

String serverPath = "/detect";
const int serverPort = 5241;
const int BAUAD_RATE = 115200;
const int timerInterval = 1000;    // time between each HTTP POST image
const int drowsyCooldown = 10 * 60000; // 10 minutes cooldown
JsonDocument getNearestGasStation(float latitude, float longitude) {
    HTTPClient http;
    http.begin(GOOGLE_MAPS_API_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Goog-Api-Key", GOOGLE_MAPS_API_KEY);
    http.addHeader("X-Goog-FieldMask", "places.displayName,places.location");

    JsonDocument doc;
    doc["includedTypes"][0] = "gas_station";
    doc["maxResultCount"] = 10;
    doc["locationRestriction"]["circle"]["center"]["latitude"] = latitude;
    doc["locationRestriction"]["circle"]["center"]["longitude"] = longitude;
    doc["locationRestriction"]["circle"]["radius"] = 5000.0;

    String requestBody;
    serializeJson(doc, requestBody);
    int httpResponseCode = http.POST(requestBody);

    JsonDocument responseDoc;

    if (httpResponseCode > 0) {
        String response = http.getString();
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

                    
                    // const char* displayName = firstPlace["displayName"]["text"];
                    
                    Serial.println("Nearest Gas Station:");
                    // Serial.printf("Name: %s\n", displayName);
                    Serial.printf("Latitude: %.6f\n", gasStationLat);
                    Serial.printf("Longitude: %.6f\n", gasStationLng);
                    return responseDoc;
                    // return [gasStationLat, gasStationLng];
                } else {
                    Serial.println("No places found in response");
                }
            }
        }
        
    } else {
        Serial.printf("Error occurred while sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
    // int response[] = {0};

    return responseDoc;
}

JsonDocument getCoordinates() {
    HTTPClient http;
    String locationEndpoint = String("http://") + MOBILE_IP + ":8080" + "/location";
    http.begin(locationEndpoint);
    JsonDocument doc;

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
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
        Serial.printf("Error occurred while sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();

    return doc;
}


void setup() {
    Serial.begin(BAUAD_RATE);
    Serial.println("Connecting to WiFi...");
    Serial.println("Wifi ssid: " + String(ssid));
    Serial.println("Wifi password: " + String(password));

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
        String url = "http://" + String(SERVER_IP) + ":" + "5241" + serverPath;
        http.begin(url.c_str());
        http.addHeader("Content-Type", "image/jpeg");

        int httpResponseCode = http.POST(fb->buf, fb->len);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response:");
            Serial.println(response);
            
            // parse json response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            
            if (error) {
                Serial.print("JSON parsing failed: ");
                Serial.println(error.c_str());
            } else {
                if (doc.containsKey("drowsy")) {
                    bool drowsy = doc["drowsy"];
                    Serial.print("Drowsy: ");
                    Serial.println(drowsy ? "true" : "false");
                    if (drowsy) {
                        Serial.println("ALERT: Drowsiness detected!");
                        // get the current GPS coordinates
                        JsonDocument locationDoc = getCoordinates();
                        float latitude = locationDoc["latitude"];
                        float longitude = locationDoc["longitude"];

                        JsonDocument gasStationDoc = getNearestGasStation(latitude, longitude);
                        if (gasStationDoc["places"].is<JsonArray>()) {
                            JsonArray places = gasStationDoc["places"].as<JsonArray>();
                            if (places.size() > 0) {
                                JsonObject firstPlace = places[0];
                                float gasStationLat = firstPlace["location"]["latitude"];
                                float gasStationLng = firstPlace["location"]["longitude"];

                                HTTPClient routeHttp;
                                String routeEndpoint = String("http://") + MOBILE_IP + ":8080" + "/navigate";
                                routeHttp.begin(routeEndpoint);
                                routeHttp.addHeader("Content-Type", "application/json");
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
                                routeHttp.end();
                                delay(drowsyCooldown);
                            }
                        }
                    }
                
                if (doc.containsKey("score")) {
                    float score = doc["score"];
                    Serial.print("score: ");
                    Serial.println(score);
                }
            } 
        
        else {
            Serial.printf("Error occurred while sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    }
    } else {
        Serial.println("WiFi not connected");
    }

    esp_camera_fb_return(fb);
    
    delay(timerInterval);

}          
}

