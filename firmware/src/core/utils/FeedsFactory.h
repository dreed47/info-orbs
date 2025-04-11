#ifndef FEEDS_FACTORY_H
#define FEEDS_FACTORY_H

#include <ArduinoJson.h>

struct FeedConfig {
    String name;
    bool active;
    String api_url;
    String api_key;
    int update_interval;
};

class FeedsFactory {
public:
    static FeedConfig createFeed(JsonObject feedJson);
};

#endif
