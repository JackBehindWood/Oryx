#include "OasisLayer.h"

namespace {
constexpr int32_t kReferenceLoopTicks = 10;
}

OasisLayer::OasisLayer()
    : oryx::Layer("OasisLayer")
{
}

void OasisLayer::update()
{
    ++m_tick_count;

    oryx::AppTickEvent tick_event;
    oryx::Application::Get().post_event(tick_event);

    if (m_tick_count >= kReferenceLoopTicks)
    {
        oryx::Application::Get().close();
    }
}

void OasisLayer::event(oryx::Event& e)
{
    oryx::EventDispatcher dispatcher(e);
    dispatcher.dispatch<oryx::AppTickEvent>(OX_BIND_EVENT_FN(OasisLayer::handle_app_tick));
}

bool OasisLayer::handle_app_tick(oryx::AppTickEvent&)
{
    OX_INFO("Oasis tick {}", m_tick_count);
    return true;
}
