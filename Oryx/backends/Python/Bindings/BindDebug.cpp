#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Oryx/Scripting/Support/InitGuard.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

constexpr int32_t kLoggingInfo = 20;
constexpr int32_t kLoggingWarning = 30;
constexpr int32_t kLoggingError = 40;
constexpr int32_t kLoggingCritical = 50;

template<spdlog::level::level_enum Level>
void log_at(std::string_view message)
{
    Log::message(Level, message);
}

spdlog::level::level_enum level_from_logging(int32_t logging_level)
{
    if (logging_level >= kLoggingCritical)
    {
        return spdlog::level::critical;
    }
    if (logging_level >= kLoggingError)
    {
        return spdlog::level::err;
    }
    if (logging_level >= kLoggingWarning)
    {
        return spdlog::level::warn;
    }
    if (logging_level >= kLoggingInfo)
    {
        return spdlog::level::info;
    }
    return spdlog::level::trace;
}

void log_record(int32_t logging_level, std::string_view message)
{
    Log::message(level_from_logging(logging_level), message);
}

void check_assertion(bool condition, std::string_view message)
{
    check(condition, message);
}

// A logging.Handler subclass has to be a Python class; it is built here so the package needs no .py file.
py::object make_handler_class(const py::module_& logging)
{
    py::dict members;
    members["__module__"] = "oryx.debug";
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
                OX_GUARDED_FUNC(log_record, "oryx.debug.Handler.emit")(record.attr("levelno").cast<int32_t>(), text);
            }
            catch (const std::exception&)
            {
                self.attr("handleError")(record);
            }
        }, py::name("emit"), py::is_method(handler), py::arg("record"));

    return handler;
}

} // namespace

void bind_debug(py::module_& module)
{
    py::module_ debug = module.def_submodule("debug", "Logging to Oryx's client logger and checks that raise OryxAssertionError instead of trapping the process.");
    debug.def("trace", OX_GUARDED_FUNC(log_at<spdlog::level::trace>, "oryx.debug.trace"), py::arg("message"));
    debug.def("info", OX_GUARDED_FUNC(log_at<spdlog::level::info>, "oryx.debug.info"), py::arg("message"));
    debug.def("warn", OX_GUARDED_FUNC(log_at<spdlog::level::warn>, "oryx.debug.warn"), py::arg("message"));
    debug.def("error", OX_GUARDED_FUNC(log_at<spdlog::level::err>, "oryx.debug.error"), py::arg("message"));
    debug.def("critical", OX_GUARDED_FUNC(log_at<spdlog::level::critical>, "oryx.debug.critical"), py::arg("message"));
    debug.def("check", OX_GUARDED_FUNC(check_assertion, "oryx.debug.check"), py::arg("condition"), py::arg("message") = "");

    py::module_ logging = py::module_::import("logging");
    py::object handler = make_handler_class(logging);
    debug.attr("Handler") = handler;

    debug.def("install", [logging, handler](const py::object& logger, int32_t level)
        {
            py::object instance = handler(level);
            (logger.is_none() ? logging.attr("getLogger")() : logger).attr("addHandler")(instance);
            return instance;
        }, py::arg("logger") = py::none(), py::arg("level") = 0, "Attach a Handler to `logger` (the root logger by default) and return it.");
}

} // namespace oryx::python
