#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <functional>

// Forward declaration of TaskManager to avoid circular dependencies
class TaskManager;

// Define the Task class
class Task {
public:
    using ResponseCallback = std::function<void(int httpCode, const String &response)>;
    using PreProcessCallback = std::function<void(int httpCode, String &response)>;
    using TaskExecCallback = std::function<void(void)>;

    Task(const String &url, ResponseCallback callback, PreProcessCallback preProcess = nullptr, TaskExecCallback exec = nullptr)
        : url(url), callback(callback), preProcessResponse(preProcess), taskExec(exec) {}

    String url;
    ResponseCallback callback;
    PreProcessCallback preProcessResponse;
    TaskExecCallback taskExec;
};

class TaskManager {
public:
    using ResponseCallback = Task::ResponseCallback;
    using PreProcessCallback = Task::PreProcessCallback;
    using TaskExecCallback = Task::TaskExecCallback;

    static TaskManager *getInstance();
    bool addTask(Task *task); // Updated to accept Task objects
    void processAwaitingTasks();
    void processTaskResponses();
    static void httpTask(void *params);

    struct TaskParams {
        String url;
        ResponseCallback callback;
        PreProcessCallback preProcessResponse;
        TaskExecCallback taskExec;
    };

private:
    // Add debug counters
    static volatile uint32_t activeRequests; // Declare as static
    static volatile uint32_t maxConcurrentRequests; // Declare as static

    TaskManager();

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
    static const UBaseType_t REQUEST_QUEUE_ITEM_SIZE = sizeof(TaskParams *);
    static const UBaseType_t RESPONSE_QUEUE_SIZE = 20;
    static const UBaseType_t RESPONSE_QUEUE_ITEM_SIZE = sizeof(ResponseData *);
    static const TickType_t QUEUE_CHECK_DELAY = pdMS_TO_TICKS(100); // 100ms between queue checks
    bool isUrlInQueue(const String &url);
};

#endif // TASK_MANAGER_H
