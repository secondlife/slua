-- This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
-- ServerLua: Tests for uncatchable termination errors
print("testing kill errors")

-- Simple infinite loop function for testing kill errors
function infiniteloop()
    while true do end
end

-- Test pcall with infinite loop
function testpcall()
    local success, err = pcall(infiniteloop)
    -- If we get here, pcall caught the error (normal behavior)
    assert(success == false)
    return err
end

-- Test nested pcalls with infinite loop
function testnested()
    pcall(function()
        pcall(function()
            pcall(function()
                pcall(infiniteloop)
                error("should not reach here")
            end)
            error("should not reach here")
        end)
        error("should not reach here")
    end)
    error("should not reach here")
end

return "OK"
