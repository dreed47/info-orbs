#ifndef INTERNETCHECKWIDGET_H
#define INTERNETCHECKWIDGET_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Widget.h"
#include "config_helper.h"

class InternetCheckWidget : public Widget {
public:
    InternetCheckWidget(ScreenManager &manager, ConfigManager &config);
    void setup() override;
    void update(bool force = false) override;
    void draw(bool force = false) override;
    void buttonPressed(uint8_t buttonId, ButtonState state) override;
    String getName() override;

private:
    void checkInternetConnection();
    void processPingResult(bool isConnected);

    bool m_internetConnected = false;
    bool m_lastConnectionState = false;

    static constexpr uint32_t INTERNET_CHECK_DELAY = 30000; // 30 seconds
    static constexpr uint32_t DRAW_UPDATE_DELAY = 5000; // 5 seconds

    unsigned long m_lastCheckTime = 0;
    unsigned long m_lastDrawTime = 0;
};

#endif // INTERNETCHECKWIDGET_H
