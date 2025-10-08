local buf = buffer.create(4)
local expected_encoded = 'AQD/AA=='
buffer.writeu8(buf, 0, 0x1)
buffer.writeu8(buf, 2, 0xff)
local buf_string = buffer.tostring(buf)

assert(llbase64.encode(buf) == expected_encoded)
assert(llbase64.encode(buf_string) == expected_encoded)
assert(llbase64.decode(expected_encoded) == buf_string)
-- We can optionally decode base64 directly to a buffer
local decoded_to_buf = llbase64.decode(expected_encoded, true)
assert(buffer.len(decoded_to_buf) == 4)
assert(llbase64.encode(decoded_to_buf) == expected_encoded)
-- APR base64 lib truncates at the first invalid character
assert(llbase64.encode(llbase64.decode("foo<bar")) == "foo=")
-- Incomplete padding returns potentially unexpected results, but returns what it can
assert(llbase64.encode(llbase64.decode("foobar")) == "foobag==")

return 'OK'
