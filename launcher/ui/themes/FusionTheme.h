#pragma once

#include "ITheme.h"

class FusionTheme: public ITheme
{
public:
    ~FusionTheme() override = default;

    void apply() override;
    QString qtTheme() override;
};
