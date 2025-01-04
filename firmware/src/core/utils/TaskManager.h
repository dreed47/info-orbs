#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <functional>

class TaskManager {
public:
    using ResponseCallback = std::function<void(int httpCode, const String &response)>;
    using PreProcessCallback = std::function<void(int httpCode, String &response)>;
    using TaskExecCallback = std::function<void(void)>;

    static TaskManager *getInstance();
    bool addTask(const String &url, ResponseCallback callback,
                 PreProcessCallback preProcessResponse = nullptr,
                 TaskExecCallback taskExec = nullptr);
    void processAwaitingTasks(); // Renamed from processRequestQueue
    void processTaskResponses(); // Renamed from processResponseQueue

    static void execTask(void *params); // Add this new static method

private:
    // Add debug counters
    static volatile uint32_t activeRequests;
    static volatile uint32_t maxConcurrentRequests;

    TaskManager();

    static void httpTask(void *params);

    struct RequestParams {
        String url;
        ResponseCallback callback;
        PreProcessCallback preProcessResponse;
        TaskExecCallback taskExec;
    };

    struct ResponseData {
        int httpCode;
        String response;
        ResponseCallback callback;
    };

    static TaskManager *instance;
    static QueueHandle_t requestQueue;
    static QueueHandle_t responseQueue;

    static const uint16_t STACK_SIZE = 8192;
    static const UBaseType_t TASK_PRIORITY = 1;
    static const UBaseType_t REQUEST_QUEUE_SIZE = 20;
    static const UBaseType_t REQUEST_QUEUE_ITEM_SIZE = sizeof(RequestParams *);
    static const UBaseType_t RESPONSE_QUEUE_SIZE = 20;
    static const UBaseType_t RESPONSE_QUEUE_ITEM_SIZE = sizeof(ResponseData *);
    static const TickType_t QUEUE_CHECK_DELAY = pdMS_TO_TICKS(100); // 100ms between queue checks
    bool isUrlInQueue(const String &url);
};

#endif // TASK_MANAGER_H
