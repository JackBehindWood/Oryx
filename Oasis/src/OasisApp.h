#pragma once

#include "Oryx.h"

class OasisApp : public oryx::Application
{
public:
    explicit OasisApp(oryx::ApplicationCommandLineArgs args);

protected:
    void update() override;

private:
    int m_tick_count = 0;
};
