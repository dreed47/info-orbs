#ifndef STOCK_WIDGET_H
#define STOCK_WIDGET_H

#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <TaskManager.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "StockDataModel.h"
#include "Widget.h"
#include "config_helper.h"

#define MAX_STOCKS 15

class StockWidget : public Widget {
public:
    StockWidget(ScreenManager &manager, ConfigManager &config);
    void setup() override;
    void update(bool force = false) override;
    void draw(bool force = false) override;
    void buttonPressed(uint8_t buttonId, ButtonState state) override;
    String getName() override;

    void changeMode();

private:
    void processResponse(StockDataModel &stock, int httpCode, const String &response);
    void displayStock(int8_t displayIndex, StockDataModel &stock, uint32_t backgroundColor, uint32_t textColor);
    void nextPage();

    int8_t m_page = 0;
    int8_t m_pageCount = 0;

#ifdef STOCK_TICKER_LIST
    std::string m_stockList = STOCK_TICKER_LIST;
#else
    std::string m_stockList = "";
#endif

    StockDataModel m_stocks[MAX_STOCKS];
    int8_t m_stockCount;

#ifndef STOCK_API_URL
    #define STOCK_API_URL "https://api.twelvedata.com/quote"
#endif

#ifndef STOCK_CHANGE_FORMAT
    #define STOCK_CHANGE_FORMAT 0
#endif

    int m_stockchangeformat = STOCK_CHANGE_FORMAT; // Show percent change (0) or price change (1) for stocks

    int m_switchinterval = 10;
    unsigned long m_prevMillisSwitch = 0;

    WidgetTimer &m_drawTimer;
    WidgetTimer &m_updateTimer;

#ifndef STOCK_UPDATE_DELAY
    #define STOCK_UPDATE_DELAY TimeFrequency::FifteenMinutes
#endif

#ifndef STOCK_DRAW_DELAY
    #define STOCK_DRAW_DELAY TimeFrequency::ThreeSeconds
#endif
};

#endif // STOCK_WIDGET_H
