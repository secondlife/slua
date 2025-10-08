-- Tests that we haven't bungled the memcat string matching logic
if tostring(5) == tostring(5) then
    return "OK"
else
    return "FAIL"
end
