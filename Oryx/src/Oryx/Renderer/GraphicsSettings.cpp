#include "oxpch.h"
#include "Oryx/Renderer/GraphicsSettings.h"

namespace oryx
{

KeyCode key_from_name(std::string_view name)
{
    if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name[0])))
    {
        return static_cast<KeyCode>(static_cast<int32_t>(KeyCode::A) + (std::toupper(static_cast<unsigned char>(name[0])) - 'A'));
    }
    if ((name.size() == 2 || name.size() == 3) && (name[0] == 'F' || name[0] == 'f'))
    {
        int32_t number = 0;
        for (size_t i = 1; i < name.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(name[i])))
            {
                return KeyCode::Unknown;
            }
            number = number * 10 + (name[i] - '0');
        }
        if (number >= 1 && number <= 12)
        {
            return static_cast<KeyCode>(static_cast<int32_t>(KeyCode::F1) + number - 1);
        }
    }
    return KeyCode::Unknown;
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::GraphicsSettings, "graphics")
