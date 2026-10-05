#pragma once

#include "Options.h"

namespace oasis
{

class OasisApp : public oryx::Application
{
public:
    explicit OasisApp(oryx::ApplicationCommandLineArgs args);

protected:
    void on_layer_disabled(oryx::Layer& layer, std::string_view phase) override;
};

} // namespace oasis
