#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <functional>
#include <map>

class TaskManager {
public:
    using ResponseCallback = std::function<void(int resultCode, const String& response)>;
    using PreProcessCallback = std::function<void(int resultCode, String& response)>;
    
    static TaskManager* getInstance();
    bool addTask(const String& taskType, const String& url, ResponseCallback responseCallback, PreProcessCallback preProcessCallback = nullptr);  // taskType maps to internal methods
    void processAwaitingTasks();
    void processTaskResponses();  // Reintroduced for response processing in the main loop

private:
    // Task execution function type
    using TaskFunction = std::function<void(const String&, ResponseCallback, PreProcessCallback)>;

    // Task mapping
    std::map<String, TaskFunction> taskMap;

    // Private methods
    static void httpTask(const String& url, ResponseCallback responseCallback, PreProcessCallback preProcessCallback);  // Private HTTP task function
    static void taskExecutionWrapper(void* params);  // Wrapper function for task execution

    // Task parameters
    struct TaskParams {
        String taskType;
        String url;
        ResponseCallback responseCallback;
        PreProcessCallback preProcessCallback;
    };

    // Response data
    struct ResponseData {
        int resultCode;
        String response;
        ResponseCallback callback;
    };

    // Singleton instance
    static TaskManager *instance;
    static QueueHandle_t taskQueue;
    static QueueHandle_t responseQueue;  // Queue for storing responses

    // Debug counters
    static volatile uint32_t activeTasks;
    static volatile uint32_t maxConcurrentTasks;

    // Constants
    static const uint16_t STACK_SIZE = 8192;
    static const UBaseType_t TASK_PRIORITY = 1;
    static const UBaseType_t TASK_QUEUE_SIZE = 20;
    static const UBaseType_t TASK_QUEUE_ITEM_SIZE = sizeof(TaskParams*);
    static const UBaseType_t RESPONSE_QUEUE_SIZE = 20;
    static const UBaseType_t RESPONSE_QUEUE_ITEM_SIZE = sizeof(ResponseData*);
    static const TickType_t QUEUE_CHECK_DELAY = pdMS_TO_TICKS(100); // 100ms between queue checks

    // Constructor
    TaskManager();
};

#endif // TASK_MANAGER_H