-- With --no-graphics nothing here adds a file, include path, define or link.

newoption {
    trigger = "no-graphics",
    description = "Build without window backends, the RHI and the 2D renderer",
}

function graphicsEnabled()
    return _OPTIONS["no-graphics"] == nil
end

function useOryxGraphics()
    if graphicsEnabled() then
        defines { "OX_ENABLE_GRAPHICS" }
    end
end
