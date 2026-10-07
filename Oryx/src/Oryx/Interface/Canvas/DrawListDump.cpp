#include "oxpch.h"
#include "Oryx/Interface/Canvas/DrawList.h"
#include "Oryx/Interface/Canvas/FrameArena.h"

namespace oryx
{

namespace
{

void put(std::string& out, const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf_c(buffer, sizeof(buffer), format, args);
    va_end(args);
    out.append(buffer);
}

void put_rect(std::string& out, const Rect& rect)
{
    put(out, "[%.2f %.2f %.2f %.2f]", rect.min[0], rect.min[1], rect.size[0], rect.size[1]);
}

void put_colour(std::string& out, const Colour& colour)
{
    put(out, " rgba(%.2f %.2f %.2f %.2f)", colour.r, colour.g, colour.b, colour.a);
}

void put_radius(std::string& out, const CornerRadius& radius)
{
    put(out, " r(%.2f %.2f %.2f %.2f)", radius.top_left, radius.top_right, radius.bottom_right, radius.bottom_left);
}

void put_clip(std::string& out, const DrawList& list, uint32_t clip)
{
    if (clip == k_no_clip)
    {
        return;
    }
    out.append(" clip=");
    put_rect(out, list.clip(clip));
}


struct Dumper
{
    std::string& out;
    const DrawList& list;

    void operator()(const RectCmd& command)
    {
        out.append("  rect ");
        put_rect(out, command.rect);
        put_colour(out, command.colour);
        end(command.clip);
    }

    void operator()(const RoundedRectCmd& command)
    {
        out.append("  rounded ");
        put_rect(out, command.rect);
        put_radius(out, command.radius);
        put_colour(out, command.colour);
        end(command.clip);
    }

    void operator()(const BorderCmd& command)
    {
        out.append("  border ");
        put_rect(out, command.rect);
        put_radius(out, command.radius);
        put(out, " t=%.2f", command.thickness);
        put_colour(out, command.colour);
        end(command.clip);
    }

    void operator()(const LineCmd& command)
    {
        put(out, "  line (%.2f %.2f) (%.2f %.2f) t=%.2f", command.from[0], command.from[1], command.to[0], command.to[1], command.thickness);
        put_colour(out, command.colour);
        end(command.clip);
    }

    void operator()(const ImageCmd& command)
    {
        out.append("  image ");
        put_rect(out, command.rect);
        put(out, " #%u uv(%.2f %.2f %.2f %.2f)", command.image.index, command.uv_min[0], command.uv_min[1], command.uv_max[0], command.uv_max[1]);
        put_radius(out, command.radius);
        put_colour(out, command.tint);
        end(command.clip);
    }

    void operator()(const TextCmd& command)
    {
        put(out, "  text (%.2f %.2f) h=%.2f align=%u \"", command.origin[0], command.origin[1], command.pixel_height, static_cast<uint32_t>(command.align));
        out.append(list.text(command));
        out.append("\"");
        put_colour(out, command.colour);
        end(command.clip);
    }

private:
    void end(uint32_t clip)
    {
        put_clip(out, list, clip);
        out.push_back('\n');
    }
};

} // namespace

std::string dump(const DrawList& list)
{
    std::string out;
    put(out, "surface %u\n", list.surface());
    Dumper dumper{ out, list };
    for (uint32_t index = 0; index < list.channel_count(); ++index)
    {
        const DrawChannel& channel = list.channel(index);
        put(out, "channel %u\n", index);
        for (const DrawRun& run : channel.runs)
        {
            for_each_command(channel, run, dumper);
        }
    }
    return out;
}

} // namespace oryx
