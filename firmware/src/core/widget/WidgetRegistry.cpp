#include "WidgetRegistry.h"

// Include widget headers, always wrap them in a check for WIDGET_DISABLED
// to avoid including them in the build if they are disabled
#include "clockwidget/ClockWidget.h"
#if INCLUDE_WEATHER != WIDGET_DISABLED
    #include "weatherwidget/WeatherWidget.h"
#endif
#if INCLUDE_STOCK != WIDGET_DISABLED
    #include "stockwidget/StockWidget.h"
#endif
#if INCLUDE_PARQET != WIDGET_DISABLED
    #include "parqetwidget/ParqetWidget.h"
#endif
#if INCLUDE_WEBDATA != WIDGET_DISABLED
    #ifdef WEB_DATA_WIDGET_URL
        #include "webdatawidget/WebDataWidget.h"
    #endif
    #ifdef WEB_DATA_STOCK_WIDGET_URL
        #include "webdatawidget/WebDataWidget.h"
    #endif
#endif
#if INCLUDE_MQTT != WIDGET_DISABLED
    #include "mqttwidget/MQTTWidget.h"
#endif
#if INCLUDE_5ZONE != WIDGET_DISABLED
    #include "5zonewidget/5ZoneWidget.h"
#endif
#if INCLUDE_MATRIXSCREEN != WIDGET_DISABLED
    #include "matrixwidget/MatrixWidget.h"
#endif

void registerWidgets(WidgetSet *widgetSet, ScreenManager *sm, ConfigManager *config) {
    // Always add clock widget
    widgetSet->add(new ClockWidget(*sm, *config));

// Add other widgets based on compile-time flags, and only if they are not disabled
#if INCLUDE_WEATHER != WIDGET_DISABLED
    widgetSet->add(new WeatherWidget(*sm, *config));
#endif

#if INCLUDE_STOCK != WIDGET_DISABLED
    widgetSet->add(new StockWidget(*sm, *config));
#endif

#if INCLUDE_PARQET != WIDGET_DISABLED
    widgetSet->add(new ParqetWidget(*sm, *config));
#endif

#if INCLUDE_WEBDATA != WIDGET_DISABLED
    #ifdef WEB_DATA_WIDGET_URL
    widgetSet->add(new WebDataWidget(*sm, *config, WEB_DATA_WIDGET_URL));
    #endif
    #ifdef WEB_DATA_STOCK_WIDGET_URL
    widgetSet->add(new WebDataWidget(*sm, *config, WEB_DATA_STOCK_WIDGET_URL));
    #endif
#endif

#if INCLUDE_MQTT != WIDGET_DISABLED
    widgetSet->add(new MQTTWidget(*sm, *config));
#endif

#if INCLUDE_5ZONE != WIDGET_DISABLED
    widgetSet->add(new FiveZoneWidget(*sm, *config));
#endif

#if INCLUDE_MATRIXSCREEN != WIDGET_DISABLED
    widgetSet->add(new MatrixWidget(*sm, *config));
#endif
}
