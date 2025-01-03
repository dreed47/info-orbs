#include "TaskManager.h"
#include "GlobalResources.h"
#include "Utils.h"
#include <HTTPClient.h>

TaskManager *TaskManager::instance = nullptr;
QueueHandle_t TaskManager::taskQueue = nullptr;
QueueHandle_t TaskManager::responseQueue = nullptr;

// Initialize static debug counters
volatile uint32_t TaskManager::activeTasks = 0;
volatile uint32_t TaskManager::maxConcurrentTasks = 0;

TaskManager::TaskManager() {
    if (!taskQueue) {
        taskQueue = xQueueCreate(TASK_QUEUE_SIZE, TASK_QUEUE_ITEM_SIZE);
    }
    if (!responseQueue) {
        responseQueue = xQueueCreate(RESPONSE_QUEUE_SIZE, RESPONSE_QUEUE_ITEM_SIZE);
    }

    // Initialize task map
    taskMap["httpTask"] = [this](const String& url, ResponseCallback responseCallback, PreProcessCallback preProcessCallback) {
        httpTask(url, responseCallback, preProcessCallback);
    };
}

TaskManager* TaskManager::getInstance() {
    if (!instance) {
        instance = new TaskManager();
    }
    return instance;
}

bool TaskManager::addTask(const String& taskType, const String& url, ResponseCallback responseCallback, PreProcessCallback preProcessCallback) {
    auto* params = new TaskParams{taskType, url, responseCallback, preProcessCallback};
    
    if (xQueueSend(taskQueue, &params, 0) != pdPASS) {
        delete params;
        Serial.println("Failed to queue task");
        return false;
    }
    
    Serial.printf("Queued task: %s (Queue length: %d)\n", taskType.c_str(), uxQueueMessagesWaiting(taskQueue));
    return true;
}

void TaskManager::processAwaitingTasks() {
    if (uxQueueMessagesWaiting(taskQueue) == 0) {
        return; // No tasks in queue
    }

    if (xSemaphoreTake(taskSemaphore, 0) != pdTRUE) {
        return; // Semaphore blocked
    }

    Utils::setBusy(true);
    Serial.println("✅ Obtained semaphore");
    activeTasks++;
    if (activeTasks > maxConcurrentTasks) {
        maxConcurrentTasks = activeTasks;
    }
    Serial.printf("Active tasks: %d (Max seen: %d)\n", activeTasks, maxConcurrentTasks);

    // Get next task
    TaskParams* taskParams;
    if (xQueueReceive(taskQueue, &taskParams, 0) != pdPASS) {
        Serial.println("⚠️ Queue empty after size check!");
        activeTasks--;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
        return;
    }

    Serial.printf("Processing task: %s (Remaining in queue: %d)\n", 
                 taskParams->taskType.c_str(), 
                 uxQueueMessagesWaiting(taskQueue));

    // Create task to handle execution
    TaskHandle_t taskHandle;
    BaseType_t result = xTaskCreate(
        taskExecutionWrapper,
        "TASK",
        STACK_SIZE,
        taskParams,  // Pass params to task
        TASK_PRIORITY,
        &taskHandle
    );

    if (result != pdPASS) {
        Serial.println("Failed to create task");
        delete taskParams;
        Utils::setBusy(false);
        xSemaphoreGive(taskSemaphore);
    }
}

void TaskManager::taskExecutionWrapper(void* params) {
    auto* taskParams = static_cast<TaskParams*>(params);
    
    Serial.printf("🔵 Starting task: %s\n", taskParams->taskType.c_str());

    // Look up the task function in the map
    auto it = TaskManager::getInstance()->taskMap.find(taskParams->taskType);
    if (it != TaskManager::getInstance()->taskMap.end()) {
        // Execute the task function
        it->second(taskParams->url, taskParams->responseCallback, taskParams->preProcessCallback);
    } else {
        Serial.printf("Unknown task type: %s\n", taskParams->taskType.c_str());
    }

    Serial.printf("🟢 Completed task: %s\n", taskParams->taskType.c_str());
    activeTasks--;
    Serial.printf("Active tasks now: %d\n", activeTasks);
    
    delete taskParams;
    Utils::setBusy(false);
    xSemaphoreGive(taskSemaphore);
    Serial.println("✅ Released semaphore");
    vTaskDelete(nullptr);
}

void TaskManager::httpTask(const String& url, ResponseCallback responseCallback, PreProcessCallback preProcessCallback) {
    Serial.printf("🔵 Starting HTTP task for: %s\n", url.c_str());

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); 
    
    http.begin(client, url);
    http.setTimeout(10000); // 10 second timeout
    
    int httpCode = http.GET();
    String response;
    
    if (httpCode > 0) {
        response = http.getString();
    }
    
    http.end();
    client.stop();

    // Call preProcessCallback if provided
    if (preProcessCallback) {
        preProcessCallback(httpCode, response);
    }

    // Queue the response for processing in the main loop
    auto* responseData = new ResponseData{httpCode, response, responseCallback};
    if (xQueueSend(responseQueue, &responseData, 0) != pdPASS) {
        Serial.println("Failed to queue response");
        delete responseData;
    }

    Serial.printf("🟢 Completed HTTP task for: %s\n", url.c_str());
}

void TaskManager::processTaskResponses() {
    // Early exit if queue is empty
    if (uxQueueMessagesWaiting(responseQueue) == 0) {
        return;
    }
    
    ResponseData* responseData;
    while (xQueueReceive(responseQueue, &responseData, 0) == pdPASS) {
        responseData->callback(responseData->resultCode, responseData->response);
        delete responseData;
    }
}