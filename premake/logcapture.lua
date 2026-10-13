-- With --no-log-capture nothing here adds a define: the console has no log lines and no commands.

newoption {
    trigger = "no-log-capture",
    description = "Build without log capture and console commands",
}

function logCaptureEnabled()
    return _OPTIONS["no-log-capture"] == nil
end

function useOryxLogCapture()
    if logCaptureEnabled() then
        defines { "OX_ENABLE_LOG_CAPTURE", "OX_ENABLE_CONSOLE_COMMANDS" }
    end
end
