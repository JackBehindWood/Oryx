"""
Exceptions Oryx raises.
"""
from __future__ import annotations
__all__: list[str] = ['IllegalActionError', 'NotInitialisedError', 'OryxAssertionError', 'OryxError', 'ParamError', 'ScriptError', 'SettingsError']
class IllegalActionError(ScriptError, ValueError):
    pass
class NotInitialisedError(OryxError, RuntimeError):
    pass
class OryxAssertionError(OryxError, AssertionError):
    pass
class OryxError(Exception):
    pass
class ParamError(OryxError, ValueError):
    pass
class ScriptError(OryxError):
    pass
class SettingsError(OryxError):
    pass
