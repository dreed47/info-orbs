#include "FeedsManager.h"
#include <ArduinoLog.h>

bool FeedsManager::loadFeedsFromJSON(const char *filename) {
    // Debug: List files to verify structure
    fsHelper.listFilesRecursively("/");

    // Open file
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Log.errorln("Failed to open %s", filename);
        return false;
    }

    // Check file size
    size_t size = file.size();
    if (size == 0) {
        Log.errorln("File %s is empty", filename);
        file.close();
        return false;
    }

    // Allocate JSON buffer (adjust size as needed)
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Log.errorln("JSON parse error: %s", error.c_str());
        return false;
    }

    // Process feeds array
    JsonArray feeds = doc["feeds"];
    for (JsonObject feed : feeds) {
        FeedConfig config = FeedsFactory::createFeed(feed);
        if (config.active) {
            activeFeeds.push_back(config);
            Log.infoln("Loaded feed: %s", config.name.c_str());
        }
    }

    return true;
}

const std::vector<FeedConfig> &FeedsManager::getActiveFeeds() const {
    return activeFeeds;
}
