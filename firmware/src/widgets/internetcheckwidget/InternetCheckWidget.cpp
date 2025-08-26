#include "InternetCheckWidget.h"
#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

InternetCheckWidget::InternetCheckWidget(ScreenManager &manager, ConfigManager &config)
    : Widget(manager, config) {
    m_enabled = true; // Always enabled for internet checking
    Log.infoln("InternetCheckWidget initialized");
}

void InternetCheckWidget::setup() {
    m_lastCheckTime = millis();
    m_lastDrawTime = millis();
    // Force initial check and draw
    update(true);
    draw(true);
}

void InternetCheckWidget::update(bool force) {
    unsigned long currentTime = millis();

    // Check internet connection at regular intervals or when forced
    if (force || (currentTime - m_lastCheckTime >= INTERNET_CHECK_DELAY)) {
        checkInternetConnection();
        m_lastCheckTime = currentTime;
    }
}

void InternetCheckWidget::draw(bool force) {
    unsigned long currentTime = millis();

    // Only redraw if connection state changed, forced, or time for periodic update
    if (force || (m_internetConnected != m_lastConnectionState) ||
        (currentTime - m_lastDrawTime >= DRAW_UPDATE_DELAY)) {

        uint16_t color = m_internetConnected ? TFT_GREEN : TFT_RED;
        String statusText = m_internetConnected ? "INTERNET UP" : "INTERNET DOWN";

        Log.traceln("Drawing internet status: %s on all screens", statusText.c_str());

        // Paint ALL screens (0 through NUM_SCREENS-1) with the appropriate color
        for (int8_t i = 0; i < NUM_SCREENS; i++) {
            m_manager.selectScreen(i);
            m_manager.fillScreen(color);

            // Add status text in the center
            m_manager.setFontColor(TFT_BLACK, color);
            m_manager.drawCentreString(statusText, ScreenCenterX, ScreenCenterY - 20, 24);

            // Add IP address if connected
            if (m_internetConnected && WiFi.status() == WL_CONNECTED) {
                m_manager.drawCentreString(WiFi.localIP().toString(), ScreenCenterX, ScreenCenterY + 20, 18);
            } else if (!m_internetConnected) {
                // Show WiFi status when internet is down
                String wifiStatus;
                switch (WiFi.status()) {
                case WL_CONNECTED:
                    wifiStatus = "WiFi: Connected";
                    break;
                case WL_NO_SHIELD:
                    wifiStatus = "WiFi: No Shield";
                    break;
                case WL_IDLE_STATUS:
                    wifiStatus = "WiFi: Idle";
                    break;
                case WL_NO_SSID_AVAIL:
                    wifiStatus = "WiFi: No SSID";
                    break;
                case WL_SCAN_COMPLETED:
                    wifiStatus = "WiFi: Scan Done";
                    break;
                case WL_CONNECT_FAILED:
                    wifiStatus = "WiFi: Failed";
                    break;
                case WL_CONNECTION_LOST:
                    wifiStatus = "WiFi: Lost";
                    break;
                case WL_DISCONNECTED:
                    wifiStatus = "WiFi: Disconnected";
                    break;
                default:
                    wifiStatus = "WiFi: Unknown";
                    break;
                }
                m_manager.drawCentreString(wifiStatus, ScreenCenterX, ScreenCenterY + 20, 18);
            }
        }

        m_lastConnectionState = m_internetConnected;
        m_lastDrawTime = currentTime;
        Log.infoln("Internet status drawn: %s", statusText.c_str());
    }
}

void InternetCheckWidget::buttonPressed(uint8_t buttonId, ButtonState state) {
    if (buttonId == BUTTON_OK && state == BTN_SHORT) {
        // Manual refresh on OK button press
        Log.infoln("Manual internet check triggered");
        update(true);
        draw(true);
    }
}

String InternetCheckWidget::getName() {
    return "Internet Check";
}

void InternetCheckWidget::checkInternetConnection() {
    bool connected = false;

    Log.traceln("Starting internet connection check...");
    Log.traceln("WiFi status: %d", WiFi.status());

    // First check if WiFi is connected at all
    if (WiFi.status() == WL_CONNECTED) {
        Log.traceln("WiFi is connected, checking internet access...");

        // Method 1: Simple and reliable - try to resolve a well-known domain
        Log.traceln("Trying DNS resolution...");
        IPAddress ip;
        if (WiFi.hostByName("google.com", ip) || WiFi.hostByName("cloudflare.com", ip)) {
            connected = true;
            Log.traceln("DNS resolution successful to %s", ip.toString().c_str());
        } else {
            Log.traceln("DNS resolution failed");
        }

        // Method 2: If DNS failed, try a simple HTTP request
        if (!connected) {
            Log.traceln("Trying HTTP request...");
            WiFiClient client;
            HTTPClient http;
            http.setTimeout(3000); // Shorter timeout

            if (http.begin(client, "http://captive.apple.com/hotspot-detect.html")) {
                int httpCode = http.GET();
                if (httpCode > 0) {
                    connected = true;
                    Log.traceln("HTTP request successful, code: %d", httpCode);
                }
                http.end();
            }
        }

        // Method 3: If still not connected, try a direct connection test
        if (!connected) {
            Log.traceln("Trying direct connection test...");
            WiFiClient testClient;
            if (testClient.connect(IPAddress(1, 1, 1, 1), 80) || // Cloudflare DNS
                testClient.connect(IPAddress(8, 8, 8, 8), 53)) { // Google DNS
                connected = true;
                testClient.stop();
                Log.traceln("Direct connection successful");
            }
        }
    } else {
        Log.traceln("WiFi not connected - status: %d", WiFi.status());
    }

    processPingResult(connected);
}

void InternetCheckWidget::processPingResult(bool isConnected) {
    bool stateChanged = (m_internetConnected != isConnected);
    m_internetConnected = isConnected;

    Log.infoln("Internet check completed: %s", isConnected ? "CONNECTED" : "DISCONNECTED");

    // Force immediate redraw if state changed
    if (stateChanged) {
        Log.infoln("Internet state changed, forcing redraw");
        draw(true);
    }
}
