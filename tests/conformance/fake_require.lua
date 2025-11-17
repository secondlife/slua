local Bundler = {}
Bundler.__index = Bundler

function Bundler.new()
    return setmetatable({
        _cache={},
        modules={},
    }, Bundler)
end

function Bundler:require(name)
    local cached_mod = self._cache[name]
    if cached_mod then
        return cached_mod[1]
    end
    local mod = self.modules[name]
    if not mod then
        error(`Unrecognized module {name}`)
    end
    -- We're allowed to do this dangerous thing because we're bundler code.
    local ret = dangerouslyexecuterequiredmodule(mod)
    -- We box the return in a table so we can properly cache `nil` returns.
    self._cache[name] = {ret}
    return ret
end

local bundler = Bundler.new()

-- This would be splatted out by the bundling preprocessor
bundler.modules = {
    ["./foo"] = (
        function()
            local fake_called = false
            -- Make a require() that can only be called once, the thing we're requiring
            -- should not use it.
            local old_require = require
            -- Give it a distinctive name for stacktraces
            function fake_require(name)
                if fake_called then
                    error("booby-trapped require() called more than once")
                end
                fake_called = true
                return old_require(name)
            end
            require = fake_require

            -- shared should not use our booby-trapped require
            local shared = old_require("./shared")
            FOO = "bar"
            return {FOO, shared.sharedfunc}
        end
    ),
    ["./bar"] = (
        function()
            -- Return a typical table-like module retval
            local shared = require("./shared")
            return {
                func1=function() return require end,
                func2=function() end,
                sharedfunc=shared.sharedfunc,
            }
        end
    ),
    ["./shared"] = (
        function()
            -- Return a function that multiple modules can use
            return {
                sharedfunc=function() return "foo" end
            }
        end
    )
}

-- Create a global require function, as the design demands, have it delegate to `bundler`.
function require(name)
    return bundler:require(name)
end

-- Actual user code starts here
local foo = require('./foo')
assert(foo[1] == "bar")

local bar = require('./bar')
local bar2 = require('./bar')
-- Result is cached, diamond pattern is okay!
assert(bar == bar2)
assert(bar.sharedfunc == foo[2])
-- require() should have been inherited exactly
assert(bar.func1() == require)

return "OK"
