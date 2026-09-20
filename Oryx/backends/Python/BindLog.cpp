#include "oxpch.h"
#include "BindOryx.h"

#include "Oryx/Scripting/InitGuard.h"
#include "Oryx/Scripting/ScriptLog.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

inline void log_trace(std::string_view message) { script_log(ScriptLogLevel::Trace, message); }
inline void log_info(std::string_view message) { script_log(ScriptLogLevel::Info, message); }
inline void log_warn(std::string_view message) { script_log(ScriptLogLevel::Warn, message); }
inline void log_error(std::string_view message) { script_log(ScriptLogLevel::Error, message); }
inline void log_critical(std::string_view message) { script_log(ScriptLogLevel::Critical, message); }

} // namespace

void bind_log(py::module_& module)
{
    py::module_ log = module.def_submodule("log", "Routes messages to Oryx's client logger.");
    log.def("trace", OX_GUARDED_FUNC(log_trace, "oryx.log.trace"), py::arg("message"));
    log.def("info", OX_GUARDED_FUNC(log_info, "oryx.log.info"), py::arg("message"));
    log.def("warn", OX_GUARDED_FUNC(log_warn, "oryx.log.warn"), py::arg("message"));
    log.def("error", OX_GUARDED_FUNC(log_error, "oryx.log.error"), py::arg("message"));
    log.def("critical", OX_GUARDED_FUNC(log_critical, "oryx.log.critical"), py::arg("message"));
}

} // namespace oryx::python
