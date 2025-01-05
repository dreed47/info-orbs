#include "TaskManager.h"
#include "GlobalResources.h"
#include "Utils.h"
#include <HTTPClient.h>
#include <memory>

// Define static members
TaskManager *TaskManager::instance = nullptr;
QueueHandle_t TaskManager::requestQueue = nullptr;
QueueHandle_t TaskManager::responseQueue = nullptr;
volatile uint32_t TaskManager::activeRequests = 0; // Definition (not extern)
volatile uint32_t TaskManager::maxConcurrentRequests = 0; // Definition (not extern)

TaskManager::TaskManager() {
    if (!requestQueue) {
        requestQueue = xQueueCreate(REQUEST_QUEUE_SIZE, REQUEST_QUEUE_ITEM_SIZE);
    }
    if (!responseQueue) {
        responseQueue = xQueueCreate(RESPONSE_QUEUE_SIZE, RESPONSE_QUEUE_ITEM_SIZE);
    }
}

TaskManager *TaskManager::getInstance() {
    if (!instance) {
        instance = new TaskManager();
    }
    return instance;
}

bool TaskManager::addTask(std::unique_ptr<Task> task) {
    if (isUrlInQueue(task->url)) {
        return false; // Task is automatically cleaned up when unique_ptr goes out of scope
    }

    auto *params = new TaskParams{task->url, task->callback, task->preProcessResponse, task->taskExec};
    if (xQueueSend(requestQueue, &params, 0) != pdPASS) {
        delete params;
        return false;
    }

    return true; // task is automatically cleaned up when unique_ptr goes out of scope
}

void TaskManager::processAwaitingTasks() {
    // First check if there are any requests to process
    if (uxQueueMessagesWaiting(requestQueue) == 0) {
        return; // No requests in queue
    }

    // Only try to take semaphore if we have work to do
    if (xSemaphoreTake(taskSemaphore, 0) != pdTRUE) {
        return;
    }

    Utils::setBusy(true);
    Serial.println("✅ Obtained semaphore");
    activeRequests++;

    if (activeRequests > maxConcurrentRequests) {
        maxConcurrentRequests = activeRequests;
    }
    Serial.printf("Active requests: %d (Max seen: %d)\n", activeRequests, maxConcurrentRequests);

    // Get next request
    TaskParams *taskParams;
    if (xQueueReceive(requestQueue, &taskParams, 0) != pdPASS) {
        Serial.println("⚠️ Queue empty after size check!");
        activeRequests--;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
        return;
    }

    Serial.printf("Processing request: %s (Remaining in queue: %d)\n",
                  taskParams->url.c_str(),
                  uxQueueMessagesWaiting(requestQueue));

    TaskHandle_t taskHandle;
    BaseType_t result = xTaskCreate(
        [](void *params) {
            auto *taskParams = static_cast<TaskParams *>(params);
            taskParams->taskExec();
            delete taskParams; // Ensure cleanup after execution
            Utils::setBusy(false);
            xSemaphoreGive(taskSemaphore);
            Serial.println("✅ Released semaphore");
            vTaskDelete(nullptr);
        },
        "TASK_EXEC",
        STACK_SIZE,
        taskParams,
        TASK_PRIORITY,
        &taskHandle);

    if (result != pdPASS) {
        Serial.println("Failed to create HTTP request task");
        delete taskParams; // Ensure the object is deleted if task creation fails
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
    }
}

void TaskManager::processTaskResponses() {
    if (uxQueueMessagesWaiting(responseQueue) == 0) {
        return;
    }

    ResponseData *responseData;
    while (xQueueReceive(responseQueue, &responseData, 0) == pdPASS) {
        responseData->callback(responseData->httpCode, responseData->response);
        delete responseData; // Ensure the object is deleted after processing
    }
}

bool TaskManager::isUrlInQueue(const String &url) {
    UBaseType_t queueLength = uxQueueMessagesWaiting(requestQueue);
    for (UBaseType_t i = 0; i < queueLength; i++) {
        TaskParams *taskParams;
        if (xQueuePeek(requestQueue, &taskParams, 0) == pdPASS) {
            if (taskParams->url == url) {
                return true;
            }
            xQueueReceive(requestQueue, &taskParams, 0);
            xQueueSend(requestQueue, &taskParams, 0);
        }
    }
    return false;
}
