"""
Exceptions Oryx raises.
"""
from __future__ import annotations
__all__: list[str] = ['OryxAssertionError', 'OryxError', 'ParamError', 'ScriptError']
class OryxAssertionError(OryxError):
    pass
class OryxError(Exception):
    pass
class ParamError(OryxError):
    pass
class ScriptError(OryxError):
    pass
