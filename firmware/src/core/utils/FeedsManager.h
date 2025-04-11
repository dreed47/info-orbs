#ifndef FEEDS_MANAGER_H
#define FEEDS_MANAGER_H

#include "FeedsFactory.h"
#include "LittleFSHelper.h"
#include <ArduinoJson.h>
#include <vector>

class FeedsManager {
public:
    FeedsManager(LittleFSHelper &fsHelper) : fsHelper(fsHelper) {}
    bool loadFeedsFromJSON(const char *filename);
    const std::vector<FeedConfig> &getActiveFeeds() const;

private:
    std::vector<FeedConfig> activeFeeds;
    LittleFSHelper &fsHelper;
};

#endif
