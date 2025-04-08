#ifndef WIDGET_REGISTRY_H
#define WIDGET_REGISTRY_H

#include "ConfigManager.h"
#include "ScreenManager.h"
#include "WidgetSet.h"

// Registers all widgets with the widget set.
void registerWidgets(WidgetSet *widgetSet, ScreenManager *sm, ConfigManager *config);

#endif // WIDGET_REGISTRY_H
