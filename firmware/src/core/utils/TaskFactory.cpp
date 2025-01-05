#include "TaskFactory.h"
#include "GlobalResources.h"
#include "TaskManager.h" // Include TaskManager.h to access TaskParams and ResponseData
#include "Utils.h"
#include <HTTPClient.h>

void TaskFactory::httpTask(const String &url, Task::ResponseCallback callback, Task::PreProcessCallback preProcess) {
    Serial.printf("🔵 Starting HTTP request for: %s\n", url.c_str());

    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        http.begin(client, url);
        http.setTimeout(10000); // 10 second timeout

        int httpCode = http.GET();
        String response;

        if (httpCode > 0) {
            response = http.getString();
        } else {
            Serial.printf("🔴 HTTP request failed, error code: %d\n", httpCode);
        }

        http.end(); // Proper cleanup of HTTPClient
        client.stop(); // Proper cleanup of WiFiClientSecure

        // Explicitly reset the objects
        http.~HTTPClient(); // Call the destructor
        new (&http) HTTPClient(); // Reinitialize using placement new
        client.~WiFiClientSecure(); // Call the destructor
        new (&client) WiFiClientSecure(); // Reinitialize using placement new

        if (preProcess) {
            preProcess(httpCode, response);
        }

        auto *responseData = new TaskManager::ResponseData{httpCode, response, callback};

        if (xQueueSend(TaskManager::responseQueue, &responseData, 0) != pdPASS) {
            Serial.println("Failed to queue response");
            delete responseData; // Ensure cleanup if queueing fails
        }
    }
    
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
    Serial.print("Remaining task stack space: ");
    Serial.println(highWater);

    TaskManager::activeRequests--;
    Serial.printf("Active requests now: %d\n", TaskManager::activeRequests);
    Utils::setBusy(false);
    xSemaphoreGive(taskSemaphore); // Release semaphore
    Serial.println("✅ Released semaphore");
    vTaskDelete(nullptr); // FreeRTOS task termination
}
