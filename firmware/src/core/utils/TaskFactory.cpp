#include "TaskFactory.h"
#include "GlobalResources.h"
#include "TaskManager.h"
#include "Utils.h"
#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <StreamUtils.h>

void TaskFactory::httpGetTask(const String &url, Task::ResponseCallback callback, Task::PreProcessCallback preProcess) {
    Log.noticeln("🔵 Starting HTTP request for: %s", url.c_str());

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
            Log.errorln("🔴 HTTP request failed, error code: %d", httpCode);
        }

        http.end();
        client.stop();

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
            Log.errorln("Failed to queue response");
            delete responseData; // Ensure cleanup if queueing fails
        }
    }
    TaskManager::activeRequests--;

#ifdef TASKMANAGER_DEBUG
    Log.noticeln("Active requests now: %d", TaskManager::activeRequests);
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
    Log.noticeln("Remaining task stack space: %d", highWater);
#endif
}

void TaskFactory::httpPostTask(const String &url, const String &payload, Task::ResponseCallback callback, Task::PreProcessCallback preProcess) {
    Log.noticeln("🔵 Starting HTTP POST request for: %s", url.c_str());

    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        http.begin(client, url);
        http.setTimeout(10000); // 10-second timeout
        http.addHeader("Content-Type", "application/json");

        int httpCode = http.POST(payload);
        String response;

        if (httpCode == 200) {
            // Check if the response is chunked
            if (http.header("Transfer-Encoding") == "chunked") {
                Log.noticeln("🔵 Response is chunked, processing stream...");

                // Use a stream to read the response in chunks
                Stream &rawStream = http.getStream();
                ChunkDecodingStream decodedStream(rawStream);
                response = decodedStream.readString();
            } else {
                // If not chunked, read the entire response at once
                response = http.getString();
            }

            // Validate the response
            if (response.length() == 0) {
                Log.errorln("🔴 Empty response received from server");
                httpCode = -1; // Indicate an error
            }
        } else {
            Log.errorln("🔴 HTTP POST request failed, error code: %d", httpCode);
        }

        http.end();
        client.stop();

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
            Log.errorln("Failed to queue response");
            delete responseData; // Ensure cleanup if queueing fails
        }
    }
    TaskManager::activeRequests--;

#ifdef TASKMANAGER_DEBUG
    Log.noticeln("Active requests now: %d", TaskManager::activeRequests);
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
    Log.noticeln("Remaining task stack space: %d", highWater);
#endif
}
