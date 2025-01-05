#include "TaskManager.h"
#include "GlobalResources.h"
#include "Utils.h"
#include <HTTPClient.h>

// Define static members
TaskManager *TaskManager::instance = nullptr;
QueueHandle_t TaskManager::requestQueue = nullptr;
QueueHandle_t TaskManager::responseQueue = nullptr;

// Initialize static debug counters
volatile uint32_t TaskManager::activeRequests = 0; // Define as static
volatile uint32_t TaskManager::maxConcurrentRequests = 0; // Define as static

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

bool TaskManager::addTask(Task *task) {
    if (isUrlInQueue(task->url)) {
        Serial.printf("Request already in queue: %s\n", task->url.c_str());
        return false;
    }

    auto *params = new TaskParams{task->url, task->callback, task->preProcessResponse, task->taskExec};

    if (xQueueSend(requestQueue, &params, 0) != pdPASS) {
        delete params;
        Serial.println("Failed to queue task");
        return false;
    }

    Serial.printf("Task queued: %s\n", task->url.c_str());
    return true;
}

void TaskManager::processAwaitingTasks() {
    if (uxQueueMessagesWaiting(requestQueue) == 0) {
        return;
    }

    // Acquire the global semaphore to ensure only one task runs at a time
    if (xSemaphoreTake(taskSemaphore, QUEUE_CHECK_DELAY) == pdTRUE) {
        Utils::setBusy(true);
        Serial.println("✅ Obtained semaphore");
        activeRequests++;
        if (activeRequests > maxConcurrentRequests) {
            maxConcurrentRequests = activeRequests;
        }
        Serial.printf("Active requests: %d (Max seen: %d)\n", activeRequests, maxConcurrentRequests);

        TaskParams *taskParams;
        if (xQueueReceive(requestQueue, &taskParams, 0) != pdPASS) {
            Serial.println("⚠️ Queue empty after size check!");
            activeRequests--;
            Utils::setBusy(false);
            xSemaphoreGive(taskSemaphore); // Release the semaphore
            return;
        }

        Serial.printf("Processing request: %s (Remaining in queue: %d)\n",
                      taskParams->url.c_str(),
                      uxQueueMessagesWaiting(requestQueue));

        TaskHandle_t taskHandle;
        BaseType_t result;

        // Use httpTask as the default execution method if taskExec is nullptr
        TaskExecCallback taskToExecute = taskParams->taskExec ? taskParams->taskExec : [taskParams]() {
            httpTask(taskParams);
        };

        result = xTaskCreate(
            [](void *params) {
                auto *taskParams = static_cast<TaskParams *>(params);
                if (taskParams->taskExec) {
                    taskParams->taskExec(); // Execute the custom taskExec callback
                } else {
                    httpTask(taskParams); // Execute the default HTTP task
                }
                delete taskParams;
                Utils::setBusy(false);
                xSemaphoreGive(taskSemaphore); // Release the semaphore after task completion
                vTaskDelete(nullptr);
            },
            "TASK_EXEC",
            STACK_SIZE,
            taskParams,
            TASK_PRIORITY,
            &taskHandle);

        if (result != pdPASS) {
            Serial.println("Failed to create HTTP request task");
            delete taskParams;
            Utils::setBusy(false);
            xSemaphoreGive(taskSemaphore); // Release the semaphore on failure
        }
    } else {
        // Serial.println("⚠️ Could not acquire semaphore, skipping task processing");
    }
}

void TaskManager::httpTask(void *params) {
    auto *taskParams = static_cast<TaskParams *>(params);

    Serial.printf("🔵 Starting HTTP request for: %s\n", taskParams->url.c_str());

    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        http.begin(client, taskParams->url);
        http.setTimeout(10000); // 10 second timeout

        int httpCode = http.GET();
        String response;

        if (httpCode > 0) {
            response = http.getString();
            // Serial.printf("🟢 HTTP response for %s:\n%s\n", taskParams->url.c_str(), response.c_str());
        } else {
            Serial.printf("🔴 HTTP request failed, error code: %d\n", httpCode);
        }

        http.end();
        client.stop();

        // Explicitly reset the objects
        http.~HTTPClient(); // Call the destructor
        new (&http) HTTPClient(); // Reinitialize using placement new

        client.~WiFiClientSecure(); // Call the destructor
        new (&client) WiFiClientSecure(); // Reinitialize using placement new

        // Call preProcessResponse if provided
        if (taskParams->preProcessResponse) {
            taskParams->preProcessResponse(httpCode, response);
        }

        auto *responseData = new ResponseData{
            httpCode,
            response,
            taskParams->callback};

        if (xQueueSend(responseQueue, &responseData, 0) != pdPASS) {
            Serial.println("Failed to queue response");
            delete responseData;
        }
    }

    Serial.printf("🟢 Completed HTTP request for: %s\n", taskParams->url.c_str());
    activeRequests--;
    Serial.printf("Active requests now: %d\n", activeRequests);

    delete taskParams;
    Utils::setBusy(false);
    xSemaphoreGive(taskSemaphore); // Release the semaphore after task completion
    Serial.println("✅ Released semaphore");
    vTaskDelete(nullptr);
}

void TaskManager::processTaskResponses() {
    if (uxQueueMessagesWaiting(responseQueue) == 0) {
        return;
    }

    ResponseData *responseData;
    while (xQueueReceive(responseQueue, &responseData, 0) == pdPASS) {
        responseData->callback(responseData->httpCode, responseData->response);
        delete responseData;
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
