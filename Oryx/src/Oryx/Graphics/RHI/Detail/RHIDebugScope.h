#pragma once

namespace oryx
{

// RAII debug group for anything with push_debug_group/pop_debug_group; the pop is skipped while unwinding because the recording is abandoned anyway.
template<typename Recorder>
class RHIDebugScope
{
public:
    RHIDebugScope(Recorder& recorder, const char* name)
        : m_recorder(recorder)
        , m_exceptions(std::uncaught_exceptions())
    {
        m_recorder.push_debug_group(name);
    }

    ~RHIDebugScope()
    {
        if (std::uncaught_exceptions() == m_exceptions)
        {
            m_recorder.pop_debug_group();
        }
    }

    RHIDebugScope(const RHIDebugScope&) = delete;
    RHIDebugScope& operator=(const RHIDebugScope&) = delete;

private:
    Recorder& m_recorder;
    int m_exceptions;
};

} // namespace oryx
