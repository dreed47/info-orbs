#ifndef TASK_FACTORY_H
#define TASK_FACTORY_H

#include "TaskManager.h"

class TaskFactory {
public:
    static Task *createHttpTask(const String &url, Task::ResponseCallback callback, Task::PreProcessCallback preProcess = nullptr) {
        // Create a Task object with the URL and callbacks
        return new Task(url, callback, preProcess, nullptr); // taskExec is nullptr
    }

    static Task *createMqttTask(const String &topic, Task::ResponseCallback callback) {
        // Create a Task object for MQTT (placeholder logic)
        return new Task(topic, callback, nullptr, nullptr); // taskExec is nullptr
    }
};

#endif // TASK_FACTORY_H
