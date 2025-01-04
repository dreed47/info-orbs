#include "TaskManager.h"
#include "GlobalResources.h"
#include "Utils.h"
#include <HTTPClient.h>

TaskManager *TaskManager::instance = nullptr;
QueueHandle_t TaskManager::requestQueue = nullptr;
QueueHandle_t TaskManager::responseQueue = nullptr;

// Initialize static debug counters
volatile uint32_t TaskManager::activeRequests = 0;
volatile uint32_t TaskManager::maxConcurrentRequests = 0;

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

bool TaskManager::addTask(const String &url, ResponseCallback callback,
                          PreProcessCallback preProcessResponse,
                          TaskExecCallback taskExec) {
    if (isUrlInQueue(url)) {
        Serial.printf("Request already in queue: %s\n", url.c_str());
        return false;
    }

    auto *params = new RequestParams{url, callback, preProcessResponse, taskExec};

    if (xQueueSend(requestQueue, &params, 0) != pdPASS) {
        delete params;
        Serial.println("Failed to queue task");
        return false;
    }

    Serial.printf("Task queued: %s\n", url.c_str());
    return true;
}

void TaskManager::processAwaitingTasks() { // Renamed from processRequestQueue
    // First check if there are any requests to process
    if (uxQueueMessagesWaiting(requestQueue) == 0) {
        return; // No requests in queue
    }

    // Only try to take semaphore if we have work to do
    if (xSemaphoreTake(taskSemaphore, 0) != pdTRUE) {
        // Serial.println("⚠️ Semaphore blocked - request already in progress");
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
    RequestParams *requestParams;
    if (xQueueReceive(requestQueue, &requestParams, 0) != pdPASS) {
        Serial.println("⚠️ Queue empty after size check!");
        activeRequests--;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
        return;
    }

    Serial.printf("Processing request: %s (Remaining in queue: %d)\n",
                  requestParams->url.c_str(),
                  uxQueueMessagesWaiting(requestQueue));

    // Create task to handle request
    TaskHandle_t taskHandle;
    BaseType_t result;
    if (requestParams->taskExec) {
        result = xTaskCreate(
            execTask, // Use our static wrapper method
            "TASK_EXEC",
            STACK_SIZE,
            requestParams, // Pass params to execTask
            TASK_PRIORITY,
            &taskHandle);
    } else {
        result = xTaskCreate(
            httpTask,
            "HTTP_REQ",
            STACK_SIZE,
            requestParams,
            TASK_PRIORITY,
            &taskHandle);
    }

    if (result != pdPASS) {
        Serial.println("Failed to create HTTP request task");
        delete requestParams;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
    }
}

void TaskManager::execTask(void *params) {
    auto *requestParams = static_cast<RequestParams *>(params);

    if (requestParams->taskExec) {
        requestParams->taskExec(); // Execute the callback
    }

    delete requestParams;
    Utils::setBusy(false);
    xSemaphoreGive(taskSemaphore);
    Serial.println("✅ Released semaphore");
    vTaskDelete(nullptr);
}

void TaskManager::httpTask(void *params) {
    auto *requestParams = static_cast<RequestParams *>(params);

    // Execute custom task if provided
    if (requestParams->taskExec) {
        requestParams->taskExec();
        delete requestParams;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
        vTaskDelete(nullptr);
        return;
    }

    // Otherwise execute default HTTP task
    Serial.printf("🔵 Starting HTTP request for: %s\n", requestParams->url.c_str());

    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        http.begin(client, requestParams->url);
        http.setTimeout(10000); // 10 second timeout

        int httpCode = http.GET();
        String response;

        if (httpCode > 0) {
            response = http.getString();
        }

        http.end();
        client.stop();

        // Explicitly reset the objects
        http.~HTTPClient(); // Call the destructor
        new (&http) HTTPClient(); // Reinitialize using placement new

        client.~WiFiClientSecure(); // Call the destructor
        new (&client) WiFiClientSecure(); // Reinitialize using placement new

        // Call preProcessResponse if provided
        if (requestParams->preProcessResponse) {
            requestParams->preProcessResponse(httpCode, response);
        }

        auto *responseData = new ResponseData{
            httpCode,
            response,
            requestParams->callback};

        if (xQueueSend(responseQueue, &responseData, 0) != pdPASS) {
            Serial.println("Failed to queue response");
            delete responseData;
        }
    }

    // UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("Remaining task stack space: ");
    // Serial.println(highWater);

    Serial.printf("🟢 Completed HTTP request for: %s\n", requestParams->url.c_str());
    activeRequests--;
    Serial.printf("Active requests now: %d\n", activeRequests);

    delete requestParams;
    Utils::setBusy(false);
    xSemaphoreGive(taskSemaphore);
    Serial.println("✅ Released semaphore");
    vTaskDelete(nullptr);
}

void TaskManager::processTaskResponses() { // Renamed from processResponseQueue
    // Early exit if queue is empty
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
    // Iterate through the queue to check for duplicate URLs
    UBaseType_t queueLength = uxQueueMessagesWaiting(requestQueue);
    for (UBaseType_t i = 0; i < queueLength; i++) {
        RequestParams *requestParams;
        if (xQueuePeek(requestQueue, &requestParams, 0) == pdPASS) {
            if (requestParams->url == url) {
                return true; // Duplicate URL found
            }
            // Move to the next item in the queue
            xQueueReceive(requestQueue, &requestParams, 0);
            xQueueSend(requestQueue, &requestParams, 0);
        }
    }
    return false; // No duplicate URL found
}
