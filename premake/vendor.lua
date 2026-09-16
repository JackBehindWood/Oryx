-- Generalizes the doctest vendoring pattern (a header-only lib checked out
-- as a git submodule under <project>/vendor/<lib>/) so any project can
-- vendor one with a single line instead of hand-written
-- includedirs/removefiles. By default the header is assumed to sit directly
-- under vendor/<lib>/ (the common case for single-header libs). Pass
-- `headerSubdir` for a submodule whose own repo layout nests the real
-- header one level down (e.g. doctest: vendor/doctest/doctest/doctest.h ->
-- useVendorHeader("doctest", "doctest")).
function useVendorHeader(libName, headerSubdir)
    local dir = "vendor/" .. libName
    if headerSubdir and headerSubdir ~= "" then
        dir = dir .. "/" .. headerSubdir
    end
    includedirs { dir }
    removefiles { "vendor/" .. libName .. "/**" }
end
