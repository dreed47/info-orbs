#include "FeedsFactory.h"

FeedConfig FeedsFactory::createFeed(JsonObject feedJson) {
    FeedConfig config;
    config.name = feedJson["name"].as<String>();
    config.active = feedJson["active"].as<bool>();
    config.api_url = feedJson["api_url"].as<String>();
    config.api_key = feedJson["api_key"].as<String>();
    config.update_interval = feedJson["update_interval"].as<int>();
    return config;
}
