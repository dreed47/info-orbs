#include "HTTPClientWrapper.h"
#include <queue>

// Add static member initialization
TaskHandle_t HTTPClientWrapper::m_taskHandle = NULL;
std::queue<HTTPClientWrapper::HttpRequestParams*> HTTPClientWrapper::awaitingRequests;
SemaphoreHandle_t HTTPClientWrapper::semaphore = xSemaphoreCreateBinary();

struct HttpRequestParams {
    std::string url;
    HTTPClientWrapper::Callback callback;
    SemaphoreHandle_t semaphore;
    int failedSemaphoreCount;
};

HTTPClientWrapper::HTTPClientWrapper() {
    xSemaphoreGive(semaphore); // Initialize semaphore to available state
    Serial.println("In HTTPClientWrapper::HTTPClientWrapper constructor");
}

void HTTPClientWrapper::makeRequest(const std::string& url, Callback callback) {
    Serial.println("In HTTPClientWrapper::makeRequest");
    if (xSemaphoreTake(semaphore, (TickType_t)10) == pdTRUE) {
        HttpRequestParams* params = new HttpRequestParams{url, callback, semaphore, 0};
        Serial.printf("Allocated HttpRequestParams at %p\n", params);
        if (xTaskCreate(httpTask, "httpTask", 5000, params, 1, &HTTPClientWrapper::m_taskHandle) != pdPASS) {
            Serial.println("Failed to create task");
            delete params;
            xSemaphoreGive(semaphore);
        }
    } else {
        HttpRequestParams* params = new HttpRequestParams{url, callback, semaphore, 0};
        Serial.printf("Allocated HttpRequestParams at %p and pushed to awaitingRequests\n", params);
        awaitingRequests.push(params);
    }
}

void HTTPClientWrapper::processAwaitingRequests() {
    Serial.println("In processAwaitingRequests");
    if (!awaitingRequests.empty()) {
        HttpRequestParams* params = awaitingRequests.front();
        if (xSemaphoreTake(semaphore, (TickType_t)10) == pdTRUE) {
            awaitingRequests.pop();
            Serial.printf("Popped HttpRequestParams at %p from awaitingRequests\n", params);
            if (xTaskCreate(httpTask, "httpTask", 5000, params, 1, &m_taskHandle) != pdPASS) {
                Serial.println("Failed to create task");
                delete params;
                xSemaphoreGive(semaphore);
            }
        } else {
            params->failedSemaphoreCount++;
            if (params->failedSemaphoreCount > 5) {
                params->callback("", -1, "Failed to acquire semaphore after multiple attempts");
                awaitingRequests.pop();
                Serial.printf("Popped and deleted HttpRequestParams at %p from awaitingRequests due to failed attempts\n", params);
                delete params;
            }
        }
    }
}

void HTTPClientWrapper::httpTask(void* pvParameters) {
    Serial.println("In HTTPClientWrapper::httpTask");
    HttpRequestParams* params = static_cast<HttpRequestParams*>(pvParameters);
    HTTPClient http;
    http.begin(params->url.c_str());
    int httpCode = http.GET();
    std::string payload;
    std::string error;

    if (httpCode > 0) {
        payload = http.getString().c_str();
    } else {
        error = http.errorToString(httpCode).c_str();
    }

    http.end();
    params->callback(payload, httpCode, error);
    xSemaphoreGive(params->semaphore);
    Serial.printf("Deleting HttpRequestParams at %p\n", params);
    delete params; // Ensure proper deletion of params
    vTaskDelete(NULL);
}
