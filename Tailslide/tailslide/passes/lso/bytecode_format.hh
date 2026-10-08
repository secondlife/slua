#pragma once

#include <cstdint>

#include "../../bitstream.hh"
#include "../../lslmini.hh"
#include "../../symtab.hh"
#include "../../types.hh"

namespace Tailslide {

const int32_t TOTAL_LSO_MEMORY = 0x4000;
const int32_t LSO_VERSION_NUM = 0x0200;

enum LSORegisters {
  LREG_TM,  // top of memory
  LREG_IP,  // instruction pointer
  LREG_VN,  // version number
  LREG_BP,  // base pointer - what local variables are referenced from
  LREG_SP,  // stack pointer - where the top of the stack is
  LREG_HR,  // heap register - where in memory does the heap start
  LREG_HP,  // heap pointer - where is the top of the heap? Should be one past the last entry
  LREG_CS,  // current state
  LREG_NS,  // next state - what state are we switching to?
  // CE IE and ER are NOT used in version 2 bytecode.
  // repopulated whenever state is switched as they related to the _state_.
  LREG_CE,  // current events - what events are waiting to be handled?
  LREG_IE,  // in event - which event handler are we currently in?
  LREG_ER,  // event register - what events do we have active handlers for?
  LREG_FR,  // fault register - which errors are currently active?
  LREG_SLR, // sleep register - are we sleeping? For how long?
  LREG_GVR, // global variable register - where do global variables start
  LREG_GFR, // global function register - where do global functions start
  LREG_PR,  // parameter register - data passed to script from launcher
  LREG_ESR, // energy supply register - how much energy do we have on board?
  LREG_SR,  // state register - where do state details start

  // 64-bit registers, only exist in version 2 bytecode
  LREG_NCE, // 64 bit current events - what events are waiting to be handled?
  LREG_NIE, // 64 bit in event - which event handler are we currently in?
  LREG_NER, // 64 bit event register - what events do we have active handlers for?
  LREG_MAX
};

const int32_t LSO_REGISTER_OFFSETS[LREG_MAX + 1] = {
  0,   // TM
  4,   // IP
  8,   // VN
  12,  // BP
  16,  // SP
  20,  // HR
  24,  // HP
  28,  // CS
  32,  // NS
  36,  // CE
  40,  // IE
  44,  // ER
  48,  // FR
  52,  // SLR
  56,  // GVR
  60,  // GFR
  64,  // PR
  68,  // ESR
  72,  // SR
  76,  // NCE
  84,  // NIE
  92,  // NER
  100, // end of registers
};

// Event handlers are numbered by their position in builtins.txt, see
// LSLSymbol::getEventIndex(). Bit (index - 1) in the event registers.
typedef int LSOHandlerType;

const uint32_t LSO_TYPE_DATA_SIZES[LST_MAX] = {
  0,  // null
  4,  // int
  4,  // float
  4,  // string (just a pointer)
  4,  // key (just a pointer)
  12, // vector
  16, // quaternion
  4,  // list (just a pointer)
  0,  // error!
};

typedef enum : uint8_t {
    LOPC_NOOP = 0x00,
    LOPC_POP = 0x01,
    LOPC_POPS = 0x02,
    LOPC_POPL = 0x03,
    LOPC_POPV = 0x04,
    LOPC_POPQ = 0x05,
    LOPC_POPARG = 0x06,
    LOPC_POPIP = 0x07,
    LOPC_POPBP = 0x08,
    LOPC_POPSP = 0x09,
    LOPC_POPSLR = 0x0a,
    LOPC_DUP = 0x20,
    LOPC_DUPS = 0x21,
    LOPC_DUPL = 0x22,
    LOPC_DUPV = 0x23,
    LOPC_DUPQ = 0x24,
    LOPC_STORE = 0x30,
    LOPC_STORES = 0x31,
    LOPC_STOREL = 0x32,
    LOPC_STOREV = 0x33,
    LOPC_STOREQ = 0x34,
    LOPC_STOREG = 0x35,
    LOPC_STOREGS = 0x36,
    LOPC_STOREGL = 0x37,
    LOPC_STOREGV = 0x38,
    LOPC_STOREGQ = 0x39,
    LOPC_LOADP = 0x3a,
    LOPC_LOADSP = 0x3b,
    LOPC_LOADLP = 0x3c,
    LOPC_LOADVP = 0x3d,
    LOPC_LOADQP = 0x3e,
    LOPC_LOADGP = 0x3f,
    LOPC_LOADGSP = 0x40,
    LOPC_LOADGLP = 0x41,
    LOPC_LOADGVP = 0x42,
    LOPC_LOADGQP = 0x43,
    LOPC_PUSH = 0x50,
    LOPC_PUSHS = 0x51,
    LOPC_PUSHL = 0x52,
    LOPC_PUSHV = 0x53,
    LOPC_PUSHQ = 0x54,
    LOPC_PUSHG = 0x55,
    LOPC_PUSHGS = 0x56,
    LOPC_PUSHGL = 0x57,
    LOPC_PUSHGV = 0x58,
    LOPC_PUSHGQ = 0x59,
    LOPC_PUSHIP = 0x5a,
    LOPC_PUSHBP = 0x5b,
    LOPC_PUSHSP = 0x5c,
    LOPC_PUSHARGB = 0x5d,
    LOPC_PUSHARGI = 0x5e,
    LOPC_PUSHARGF = 0x5f,
    LOPC_PUSHARGS = 0x60,
    LOPC_PUSHARGV = 0x61,
    LOPC_PUSHARGQ = 0x62,
    LOPC_PUSHE = 0x63,
    LOPC_PUSHEV = 0x64,
    LOPC_PUSHEQ = 0x65,
    LOPC_PUSHARGE = 0x66,
    LOPC_ADD = 0x70,
    LOPC_SUB = 0x71,
    LOPC_MUL = 0x72,
    LOPC_DIV = 0x73,
    LOPC_MOD = 0x74,
    LOPC_EQ = 0x75,
    LOPC_NEQ = 0x76,
    LOPC_LEQ = 0x77,
    LOPC_GEQ = 0x78,
    LOPC_LESS = 0x79,
    LOPC_GREATER = 0x7a,
    LOPC_BITAND = 0x7b,
    LOPC_BITOR = 0x7c,
    LOPC_BITXOR = 0x7d,
    LOPC_BOOLAND = 0x7e,
    LOPC_BOOLOR = 0x7f,
    LOPC_NEG = 0x80,
    LOPC_BITNOT = 0x81,
    LOPC_BOOLNOT = 0x82,
    LOPC_JUMP = 0x90,
    LOPC_JUMPIF = 0x91,
    LOPC_JUMPNIF = 0x92,
    LOPC_STATE = 0x93,
    LOPC_CALL = 0x94,
    LOPC_RETURN = 0x95,
    LOPC_CAST = 0xa0,
    LOPC_STACKTOS = 0xb0,
    LOPC_STACKTOL = 0xb1,
    LOPC_PRINT = 0xc0,
    LOPC_CALLLIB = 0xd0,
    LOPC_CALLLIB_TWO_BYTE = 0xd1,
    LOPC_SHL = 0xe0,
    LOPC_SHR = 0xe1
} LSOOpCode;

const LSOOpCode LSO_TYPE_POP_OPCODE[LST_MAX] = {
    LOPC_NOOP, // null
    LOPC_POP,  // int
    LOPC_POP,  // float
    LOPC_POPS, // str
    LOPC_POPS, // key
    LOPC_POPV, // vec
    LOPC_POPQ, // quat
    LOPC_POPL, // list
    LOPC_NOOP, // error
};

/// like the LOPC_STORE opcodes, except that LOAD pops the stack
/// while STORE does not. Technically there are global versions
/// as well, but they'll never be emitted by LL's compiler, since
/// LOAD is only really used for retvals and declaration of locals.
/// "LOAD" is kind of a weird name, I assume it means "load into".
const LSOOpCode LSO_TYPE_LOAD_LOCAL_OPCODE[LST_MAX] = {
    LOPC_NOOP,   // null
    LOPC_LOADP,  // int
    LOPC_LOADP,  // float
    LOPC_LOADSP, // str
    LOPC_LOADSP, // key
    LOPC_LOADVP, // vec
    LOPC_LOADQP, // quat
    LOPC_LOADLP, // list
    LOPC_NOOP,   // error
};

/// pack two types into a single byte (common for LSO)
inline uint8_t pack_lso_types(LSLIType left, LSLIType right) {
  return right | (left << 4);
}

/// LSO-specific bitstream with LSO-specific serialization helpers
class LSOBitStream : public BitStream {
  public:
  explicit LSOBitStream(Endianness endian=ENDIAN_BIG) : BitStream(endian) {}
  LSOBitStream(LSOBitStream &&other) noexcept: BitStream(std::move(other)) {}
  LSOBitStream(const LSOBitStream &other) = delete;
  /// need to help the type system realize that this returns an LSOBitStream.
  /// should be fine because we never refer to BitStream subclasses by their base class.
  template<typename T>
  LSOBitStream &operator<<(const T &data) {
    return static_cast<LSOBitStream&>(BitStream::operator<<(data));
  };
  template<typename T>
  LSOBitStream &operator>>(T &data) {
    return static_cast<LSOBitStream&>(BitStream::operator>>(data));
  };
};


inline LSOBitStream& operator<<(LSOBitStream &bs, const Vector3 &v) {
  bs << v.z;
  bs << v.y;
  bs << v.x;
  return bs;
}

inline LSOBitStream& operator>>(LSOBitStream &bs, Vector3 &v) {
  bs >> v.z;
  bs >> v.y;
  bs >> v.x;
  return bs;
}

inline LSOBitStream& operator<<(LSOBitStream &bs, const Quaternion &v) {
  bs << v.s;
  bs << v.z;
  bs << v.y;
  bs << v.x;
  return bs;
}

inline LSOBitStream& operator>>(LSOBitStream &bs, Quaternion &v) {
  bs >> v.s;
  bs >> v.z;
  bs >> v.y;
  bs >> v.x;
  return bs;
}


}