class OryxError(Exception):
    detail: str

class ParamError(OryxError):
    key: str

class ScriptError(OryxError): ...
class OryxAssertionError(OryxError): ...
