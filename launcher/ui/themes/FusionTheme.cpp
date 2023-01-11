#include "FusionTheme.h"
#include "ThemeManager.h"

void FusionTheme::apply()
{
    changed_qcc_theme = qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_CONF");
    ThemeManager::writeGlobalQMLTheme();

    ITheme::apply();
}

QString FusionTheme::qtTheme()
{
    return "Fusion";
}
