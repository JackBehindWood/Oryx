#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Oryx/Scripting/Support/InitGuard.h"
#include "Oryx/Scripting/Support/ScriptLog.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

constexpr int32_t kLoggingInfo = 20;
constexpr int32_t kLoggingWarning = 30;
constexpr int32_t kLoggingError = 40;
constexpr int32_t kLoggingCritical = 50;

inline void log_trace(std::string_view message) { script_log(ScriptLogLevel::Trace, message); }
inline void log_info(std::string_view message) { script_log(ScriptLogLevel::Info, message); }
inline void log_warn(std::string_view message) { script_log(ScriptLogLevel::Warn, message); }
inline void log_error(std::string_view message) { script_log(ScriptLogLevel::Error, message); }
inline void log_critical(std::string_view message) { script_log(ScriptLogLevel::Critical, message); }

void log_record(int32_t logging_level, std::string_view message)
{
    if (logging_level >= kLoggingCritical)
    {
        log_critical(message);
    }
    else if (logging_level >= kLoggingError)
    {
        log_error(message);
    }
    else if (logging_level >= kLoggingWarning)
    {
        log_warn(message);
    }
    else if (logging_level >= kLoggingInfo)
    {
        log_info(message);
    }
    else
    {
        log_trace(message);
    }
}

// A logging.Handler subclass has to be a Python class; it is built here so the package needs no .py file.
py::object make_handler_class(const py::module_& logging)
{
    py::dict members;
    members["__module__"] = "oryx.log";
    members["__doc__"] = "Forwards ``logging`` records to Oryx's client logger.";
    py::object handler = py::module_::import("builtins").attr("type")("Handler", py::make_tuple(logging.attr("Handler")), members);

    handler.attr("format") = py::cpp_function([default_format = logging.attr("Formatter")("%(name)s: %(message)s")](const py::object& self, const py::object& record)
        {
            py::object custom = self.attr("formatter");
            return (custom.is_none() ? default_format : custom).attr("format")(record);
        }, py::name("format"), py::is_method(handler), py::arg("record"));

    handler.attr("emit") = py::cpp_function([](const py::object& self, const py::object& record)
        {
            try
            {
                std::string text = self.attr("format")(record).cast<std::string>();
                OX_GUARDED_FUNC(log_record, "oryx.log.Handler.emit")(record.attr("levelno").cast<int32_t>(), text);
            }
            catch (const std::exception&)
            {
                self.attr("handleError")(record);
            }
        }, py::name("emit"), py::is_method(handler), py::arg("record"));

    return handler;
}

} // namespace

void bind_log(py::module_& module)
{
    py::module_ log = module.def_submodule("log", "Routes messages to Oryx's client logger.");
    log.def("trace", OX_GUARDED_FUNC(log_trace, "oryx.log.trace"), py::arg("message"));
    log.def("info", OX_GUARDED_FUNC(log_info, "oryx.log.info"), py::arg("message"));
    log.def("warn", OX_GUARDED_FUNC(log_warn, "oryx.log.warn"), py::arg("message"));
    log.def("error", OX_GUARDED_FUNC(log_error, "oryx.log.error"), py::arg("message"));
    log.def("critical", OX_GUARDED_FUNC(log_critical, "oryx.log.critical"), py::arg("message"));

    py::module_ logging = py::module_::import("logging");
    py::object handler = make_handler_class(logging);
    log.attr("Handler") = handler;

    log.def("install", [logging, handler](const py::object& logger, int32_t level)
        {
            py::object instance = handler(level);
            (logger.is_none() ? logging.attr("getLogger")() : logger).attr("addHandler")(instance);
            return instance;
        }, py::arg("logger") = py::none(), py::arg("level") = 0, "Attach a Handler to `logger` (the root logger by default) and return it.");
}

} // namespace oryx::python
