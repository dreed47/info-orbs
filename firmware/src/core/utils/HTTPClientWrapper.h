#ifndef HTTPCLIENTWRAPPER_H
#define HTTPCLIENTWRAPPER_H

#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <functional>
#include <string>
#include <ArduinoJson.h>
#include <queue>

class HTTPClientWrapper {
public:
    using Callback = std::function<void(const std::string& payload, int statusCode, const std::string& error)>;

    HTTPClientWrapper();
    void makeRequest(const std::string& url, Callback callback);
    static void processAwaitingRequests();

private:
    struct HttpRequestParams {
        std::string url;
        Callback callback;
        SemaphoreHandle_t semaphore;
        int failedSemaphoreCount;
    };

    static void httpTask(void* pvParameters);
    static SemaphoreHandle_t semaphore; // Make semaphore static
    static std::queue<HttpRequestParams*> awaitingRequests; // FIFO stack for awaiting requests
};

#endif // HTTPCLIENTWRAPPER_H
