-- basic identity cases
assert(bit32.s32(0) == 0)
assert(bit32.s32(1) == 1)
assert(bit32.s32(-1) == -1)

-- positive values within signed range are unchanged
assert(bit32.s32(0x7FFFFFFF) == 2147483647)

-- unsigned values above INT32_MAX wrap to negative
assert(bit32.s32(0x80000000) == -2147483648)
assert(bit32.s32(0xFFFFFFFF) == -1)
assert(bit32.s32(0xFFFFFFFE) == -2)

-- composing with bit32 operations (the primary use case)
assert(bit32.s32(bit32.bnot(0)) == -1)
assert(bit32.s32(bit32.bor(0x80000000, 1)) == -2147483647)

-- truncation toward zero
assert(bit32.s32(2.7) == 2)
assert(bit32.s32(-2.7) == -2)
assert(bit32.s32(0.9) == 0)
assert(bit32.s32(-0.9) == 0)

-- modular wrapping for values beyond uint32 range
assert(bit32.s32(0x100000000) == 0)
assert(bit32.s32(0x100000001) == 1)
assert(bit32.s32(0x1FFFFFFFF) == -1)

-- large values still wrap correctly (no UB through fmod path)
assert(bit32.s32(1e15) == -1530494976)
assert(bit32.s32(-1e15) == 1530494976)

-- NaN and Inf produce INT32_MIN (matches i386 "integer indefinite")
assert(bit32.s32(0/0) == -2147483648)
assert(bit32.s32(1/0) == -2147483648)
assert(bit32.s32(-1/0) == -2147483648)

-- result is always a plain number, never an LSL integer
assert(type(bit32.s32(42)) == "number")

-- smul: basic multiplication
assert(bit32.smul(3, 4) == 12)
assert(bit32.smul(-3, 4) == -12)
assert(bit32.smul(-3, -4) == 12)

-- smul: overflow wrapping
assert(bit32.smul(0x7FFFFFFF, 2) == -2)
assert(bit32.smul(0x80000000, -1) == -2147483648)

-- smul: the case that bit32.s32(a*b) gets wrong due to float64 precision loss
assert(bit32.smul(0x10000, 0x10000) == 0)

-- smul: result is always a plain number
assert(type(bit32.smul(2, 3)) == "number")

return "OK"
