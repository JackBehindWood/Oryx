#pragma once

#include "Oryx.h"

class OasisLayer : public oryx::Layer
{
public:
    OasisLayer();

    void update() override;
    void event(oryx::Event& e) override;

private:
    bool handle_app_tick(oryx::AppTickEvent& e);

    int32_t m_tick_count = 0;
};
