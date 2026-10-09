#pragma once

#include "Oryx.h"
#include "unit/Interface/support/ImTestDriver.h"
#include "unit/Renderer/FakeFontSource.h"

namespace oryx::test
{

struct GuiFixture
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    GuiContext context;
    GuiTheme theme;
    ContextScope<GuiContext> scope{ context };
    test::ImTestDriver<GuiContext> driver{ context, { 200.0f, 100.0f } };

    GuiSettings saved_settings = settings_of<GuiSettings>();

    GuiFixture()
    {
        theme.font = &font;
        context.set_theme(theme);
        update_settings<GuiSettings>([](GuiSettings& settings) { settings.layout_file.clear(); settings.docking = true; });
    }

    ~GuiFixture() { update_settings<GuiSettings>([this](GuiSettings& settings) { settings = saved_settings; }); }

    template<typename Body>
    auto frame_of(Body&& body)
    {
        return [&body]
        {
            LayoutStyle root;
            root.width = grow();
            root.height = grow();
            gui::BoxScope scope("root", root);
            body();
        };
    }

    template<typename Body>
    auto column_of(Body&& body)
    {
        return [&body]
        {
            LayoutStyle root;
            root.width = grow();
            root.height = grow();
            root.direction = Direction::Column;
            gui::BoxScope scope("root", root);
            body();
        };
    }

    // The nth box that draws `icon`, or null.
    const LayoutNode* find_icon(Icon icon, uint32_t nth = 0) const
    {
        for (uint32_t index = 0; index < context.layout().node_count(); ++index)
        {
            if (context.layout().node(index).paint.icon == icon && nth-- == 0)
            {
                return &context.layout().node(index);
            }
        }
        return nullptr;
    }

    // The first box whose painted text is `text`, or null.
    const LayoutNode* find_text(std::string_view text) const
    {
        for (uint32_t index = 0; index < context.layout().node_count(); ++index)
        {
            if (context.layout().node(index).paint.text == text)
            {
                return &context.layout().node(index);
            }
        }
        return nullptr;
    }
};

inline LayoutStyle fixed_box(float width, float height)
{
    LayoutStyle box;
    box.width = fixed(width);
    box.height = fixed(height);
    return box;
}

} // namespace oryx::test
