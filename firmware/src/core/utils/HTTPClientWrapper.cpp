#include "HTTPClientWrapper.h"
#include <queue>

struct HttpRequestParams {
    std::string url;
    HTTPClientWrapper::Callback callback;
    SemaphoreHandle_t semaphore;
    int failedSemaphoreCount;
};

std::queue<HTTPClientWrapper::HttpRequestParams*> HTTPClientWrapper::awaitingRequests;

SemaphoreHandle_t HTTPClientWrapper::semaphore = xSemaphoreCreateBinary();

HTTPClientWrapper::HTTPClientWrapper() {
    xSemaphoreGive(semaphore); // Initialize semaphore to available state
}

void HTTPClientWrapper::makeRequest(const std::string& url, Callback callback) {
    if (xSemaphoreTake(semaphore, (TickType_t)10) == pdTRUE) {
        HttpRequestParams* params = new HttpRequestParams{url, callback, semaphore, 0};
        xTaskCreate(&HTTPClientWrapper::httpTask, "httpTask", 8192, params, 1, NULL);
    } else {
        HttpRequestParams* params = new HttpRequestParams{url, callback, semaphore, 0};
        awaitingRequests.push(params);
    }
}

void HTTPClientWrapper::processAwaitingRequests() {
    if (!awaitingRequests.empty()) {
        HttpRequestParams* params = awaitingRequests.front();
        if (xSemaphoreTake(semaphore, (TickType_t)10) == pdTRUE) {
            awaitingRequests.pop();
            xTaskCreate(&HTTPClientWrapper::httpTask, "httpTask", 8192, params, 1, NULL);
        } else {
            params->failedSemaphoreCount++;
            if (params->failedSemaphoreCount > 5) {
                params->callback("", -1, "Failed to acquire semaphore after multiple attempts");
                awaitingRequests.pop();
                delete params;
            }
        }
    }
}

void HTTPClientWrapper::httpTask(void* pvParameters) {
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
    delete params;
    vTaskDelete(NULL);
}
