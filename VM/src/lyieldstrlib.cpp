// ServerLua: Yieldable pattern-matching functions for the string library.
//
// Replaces the base (recursive) match/find/gmatch/gsub with iterative,
// yieldable equivalents that use an explicit backtracking stack stored
// in UTAG_OPAQUE_BUFFER userdata objects (Ares-serializable).
//
// The helper functions (classend, singlematch, etc.) are duplicated here
// from lstrlib.cpp so that this file is self-contained.
//
// It bears mentioning that quite a lot of this file was written by an LLM
// under my direction over a period of a week. Normally I wouldn't like to
// use one for this sort of sensitive work, but if I'm being honest, Lua's
// pattern matching scheme isn't terribly performant to begin with, and
// I'd much rather provide a sane regex library. However, Lua's pattern
// matching needs to be supported for cross-compatibility with other Luas,
// so we still need a yieldable version of it even knowing it's not ideal.
//
// It has near 100% code coverage through the existing tests, and all the
// yield points are similarly tested. Its behavior is further validated to
// conform to `lstrlib.cpp`'s non-yieldable versions through the fuzzing
// harness in `fuzz/gsub.cpp`. Let's call this fine. Probably.

#define lyieldstrlib_c
#include "lualib.h"

#include "lstring.h"
#include "lgc.h"
#include "llsl.h"

#include "lyieldablemacros.h"
#include "lstrbuf.h"

#include <ctype.h>
#include <string.h>

// macro to `unsign' a character
#define uchar(c) ((unsigned char)(c))

static int posrelat(int pos, size_t len)
{
    // relative string position: negative means back from end
    if (pos < 0)
        pos += (int)len + 1;
    return (pos >= 0) ? pos : 0;
}

/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/

#define CAP_UNFINISHED (-1)
#define CAP_POSITION (-2)

// Pinned wire format constants — the serialized layout depends on these.
// static_assert catches divergence from upstream luaconf.h values.
static constexpr int WIRE_MAXCAPTURES = 32;
static constexpr int WIRE_MAXBACKTRACK = 200;
static_assert(WIRE_MAXCAPTURES == LUA_MAXCAPTURES);
static_assert(WIRE_MAXBACKTRACK == LUAI_MAXCCALLS);

// Forward declaration for MatchState.
struct ImatchFrame;

typedef struct MatchState
{
    const char* src_init; // init of source string
    const char* src_end;  // end ('\0') of source string
    const char* p_end;    // end ('\0') of pattern
    lua_State* L;
    int level;            // total number of captures (finished or unfinished)
    int stk;              // backtrack stack pointer
    struct
    {
        const char* init;
        ptrdiff_t len;
    } capture[WIRE_MAXCAPTURES];
    ImatchFrame* backtrack; // points into MatchStateWire in UTAG_OPAQUE_BUFFER userdata

    ImatchFrame& pushBt(lua_State* L);
    ImatchFrame& getBtTop();
    void popBt() { stk--; }
    bool isBtEmpty() const { return stk == 0; }
} MatchState;

#define L_ESC '%'
#define SPECIALS "^$*+?.([%-"

static int check_capture(MatchState* ms, int l)
{
    l -= '1';
    if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
        luaL_error(ms->L, "invalid capture index %%%d", l + 1);
    return l;
}

static int capture_to_close(MatchState* ms)
{
    int level = ms->level;
    for (level--; level >= 0; level--)
        if (ms->capture[level].len == CAP_UNFINISHED)
            return level;
    luaL_error(ms->L, "invalid pattern capture");
}

static const char* classend(MatchState* ms, const char* p)
{
    switch (*p++)
    {
    case L_ESC:
    {
        if (p == ms->p_end)
            luaL_error(ms->L, "malformed pattern (ends with '%%')");
        return p + 1;
    }
    case '[':
    {
        if (*p == '^')
            p++;
        do
        { // look for a `]'
            if (p == ms->p_end)
                luaL_error(ms->L, "malformed pattern (missing ']')");
            if (*(p++) == L_ESC && p < ms->p_end)
                p++; // skip escapes (e.g. `%]')
        } while (*p != ']');
        return p + 1;
    }
    default:
    {
        return p;
    }
    }
}

static int match_class(int c, int cl)
{
    int res;
    switch (tolower(cl))
    {
    case 'a':
        res = isalpha(c);
        break;
    case 'c':
        res = iscntrl(c);
        break;
    case 'd':
        res = isdigit(c);
        break;
    case 'g':
        res = isgraph(c);
        break;
    case 'l':
        res = islower(c);
        break;
    case 'p':
        res = ispunct(c);
        break;
    case 's':
        res = isspace(c);
        break;
    case 'u':
        res = isupper(c);
        break;
    case 'w':
        res = isalnum(c);
        break;
    case 'x':
        res = isxdigit(c);
        break;
    case 'z':
        res = (c == 0);
        break; // deprecated option
    default:
        return (cl == c);
    }
    return (islower(cl) ? res : !res);
}

static int matchbracketclass(int c, const char* p, const char* ec)
{
    int sig = 1;
    if (*(p + 1) == '^')
    {
        sig = 0;
        p++; // skip the `^'
    }
    while (++p < ec)
    {
        if (*p == L_ESC)
        {
            p++;
            if (match_class(c, uchar(*p)))
                return sig;
        }
        else if ((*(p + 1) == '-') && (p + 2 < ec))
        {
            p += 2;
            if (uchar(*(p - 2)) <= c && c <= uchar(*p))
                return sig;
        }
        else if (uchar(*p) == c)
            return sig;
    }
    return !sig;
}

static int singlematch(MatchState* ms, const char* s, const char* p, const char* ep)
{
    if (s >= ms->src_end)
        return 0;
    else
    {
        int c = uchar(*s);
        switch (*p)
        {
        case '.':
            return 1; // matches any char
        case L_ESC:
            return match_class(c, uchar(*(p + 1)));
        case '[':
            return matchbracketclass(c, p, ep - 1);
        default:
            return (uchar(*p) == c);
        }
    }
}


static const char* match_capture(MatchState* ms, const char* s, int l)
{
    size_t len;
    l = check_capture(ms, l);
    len = ms->capture[l].len;
    if ((size_t)(ms->src_end - s) >= len && memcmp(ms->capture[l].init, s, len) == 0)
        return s + len;
    else
        return NULL;
}

static const char* lmemfind(const char* s1, size_t l1, const char* s2, size_t l2)
{
    if (l2 == 0)
        return s1; // empty strings are everywhere
    else if (l2 > l1)
        return NULL; // avoids a negative `l1'
    else
    {
        const char* init; // to search for a `*s2' inside `s1'
        l2--;             // 1st char will be checked by `memchr'
        l1 = l1 - l2;     // `s2' cannot be found after that
        while (l1 > 0 && (init = (const char*)memchr(s1, *s2, l1)) != NULL)
        {
            init++; // 1st char is already checked
            if (memcmp(init, s2 + 1, l2) == 0)
                return init - 1;
            else
            { // correct `l1' and `s1' to try again
                l1 -= init - s1;
                s1 = init;
            }
        }
        return NULL; // not found
    }
}

static void push_onecapture(MatchState* ms, int i, const char* s, const char* e)
{
    if (i >= ms->level)
    {
        if (i == 0)                           // ms->level == 0, too
            lua_pushlstring(ms->L, s, e - s); // add whole match
        else
            luaL_error(ms->L, "invalid capture index");
    }
    else
    {
        ptrdiff_t l = ms->capture[i].len;
        if (l == CAP_UNFINISHED)
            luaL_error(ms->L, "unfinished capture");
        if (l == CAP_POSITION)
            lua_pushinteger(ms->L, (int)(ms->capture[i].init - ms->src_init) + 1);
        else
            lua_pushlstring(ms->L, ms->capture[i].init, l);
    }
}

static int push_captures(MatchState* ms, const char* s, const char* e)
{
    int i;
    int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
    luaL_checkstack(ms->L, nlevels, "too many captures");
    for (i = 0; i < nlevels; i++)
        push_onecapture(ms, i, s, e);
    return nlevels; // number of strings pushed
}

// check whether pattern has no special characters
static int nospecials(const char* p, size_t l)
{
    size_t upto = 0;
    do
    {
        if (strpbrk(p + upto, SPECIALS))
            return 0;                 // pattern has a special character
        upto += strlen(p + upto) + 1; // may have more after \0
    } while (upto <= l);
    return 1; // no special chars found
}

// }======================================================

/*
** {======================================================
** ITERATIVE MATCH ENGINE
** =======================================================
*/

// Backtracking frame site identifiers
enum IMatchFrameKind : int32_t
{
    IMATCH_START_CAPTURE = 0,
    IMATCH_END_CAPTURE = 1,
    IMATCH_MAX_EXPAND = 2,
    IMATCH_MIN_EXPAND = 3,
    IMATCH_OPTIONAL = 4,
};

// Backtracking frame stored in a UTAG_OPAQUE_BUFFER userdata as a packed struct array.
// Fields are site-specific; a union gives each site named access.
struct ImatchFrame
{
    IMatchFrameKind kind;
    union
    {
        struct { int32_t saved_level; } start_cap;
        struct { int32_t cap_idx; int32_t saved_len; } end_cap;
        struct { int32_t s_base_off; int32_t ep_off; int32_t count; int32_t p_off; } max_exp;
        struct { int32_t s_retry_off; int32_t p_off; int32_t ep_off; } min_exp;
        struct { int32_t s_orig_off; int32_t ep_off; } optional;
    };
};

// Capture entry stored in a UTAG_OPAQUE_BUFFER userdata as a packed struct array.
struct IMatchCapture
{
    int32_t offset;
    int32_t len;
};

static_assert(sizeof(ImatchFrame[2]) == 2 * 5 * sizeof(int32_t));
static_assert(sizeof(IMatchCapture[2]) == 2 * 2 * sizeof(int32_t));

inline ImatchFrame& MatchState::pushBt(lua_State* L)
{
    if (stk >= WIRE_MAXBACKTRACK)
        luaL_error(L, "pattern too complex");
    return backtrack[stk++];
}

inline ImatchFrame& MatchState::getBtTop()
{
    return backtrack[stk - 1];
}

// Wire format: single UTAG_OPAQUE_BUFFER userdata holding all serializable match state.
// Backtrack frames are offset-based (no conversion needed). Captures
// store offsets and are converted to/from pointers by MatchStateGuard.
struct MatchStateWire
{
    int32_t level;
    int32_t stk;
    IMatchCapture captures[WIRE_MAXCAPTURES];
    ImatchFrame backtrack[WIRE_MAXBACKTRACK];
};

// Max chars processed per inner-loop batch before yielding back to the
// scheduler.  Balances yield frequency against per-yield overhead.
static constexpr int YIELD_BATCH_SIZE = 256;

static_assert(MAXSSIZE <= INT32_MAX, "MAXSSIZE exceeds int32_t range; pattern matcher offsets would overflow");

// Truncate ptrdiff_t to int32_t for the wire format.
// Probably pointless (MAXSSIZE <= INT32_MAX), but it helps me sleep at night.
static inline int32_t safe_int32_trunc(lua_State* L, ptrdiff_t v)
{
    if (v < INT32_MIN || v > INT32_MAX)
        luaL_error(L, "pattern match offset overflow");
    return (int32_t)v;
}

#define SAFE_INT32(v) safe_int32_trunc(L, (v))

enum class MatchMode : uint8_t
{
    MATCH = 0,
    FIND = 1,
};

// RAII guard for MatchState serialization on yield/resume.
// On resume: deserializes capture offsets to pointers from the wire buffer.
// On yield: serializes capture pointers to offsets into the wire buffer.
// Backtrack frames live directly in wire->backtrack (ms->backtrack points
// there), so no copy is needed for the backtrack stack.
struct MatchStateGuard : YieldGuard
{
    MatchState* ms;
    MatchStateWire* wire;

    MatchStateGuard(SlotManager& slots, MatchState* ms, MatchStateWire* wire)
        : YieldGuard(slots), ms(ms), wire(wire)
    {
        ms->backtrack = wire->backtrack;
        if (isInit())
            return;
        // Resume: offset -> pointer
        LUAU_ASSERT(wire->level >= 0 && wire->level <= WIRE_MAXCAPTURES);
        LUAU_ASSERT(wire->stk >= 0 && wire->stk <= WIRE_MAXBACKTRACK);
        ms->level = wire->level;
        ms->stk = wire->stk;
        for (int i = 0; i < ms->level; i++)
        {
            const auto &wire_capture = wire->captures[i];
            ms->capture[i] = {ms->src_init + wire_capture.offset, wire_capture.len};
        }
    }

    ~MatchStateGuard()
    {
        if (!isYielding())
            return;
        // Yield: pointer -> offset (backtrack already in wire)
        wire->level = ms->level;
        wire->stk = ms->stk;
        for (int i = 0; i < ms->level; i++)
        {
            const auto &live_capture = ms->capture[i];
            ptrdiff_t off = live_capture.init - ms->src_init;
            LUAU_ASSERT(off >= INT32_MIN && off <= INT32_MAX);
            wire->captures[i] = {(int32_t)off, (int32_t)live_capture.len};
        }
    }
};

static void prepstate(MatchState* ms, lua_State* L, const char* s, size_t ls, const char* p, size_t lp)
{
    ms->L = L;
    ms->src_init = s;
    ms->src_end = s + ls;
    ms->p_end = p + lp;
}

static void reprepstate(MatchState* ms)
{
    ms->level = 0;
    ms->stk = 0;
}

// Budget-gated yield check for iterative_match_helper modes.
// Captures locals: yield_budget, s, s_off, src_str, p, p_off, pat_str.
#define BUDGET_YIELD_CHECK(L, phase_name)                        \
    do {                                                         \
        if (--yield_budget <= 0)                                 \
        {                                                        \
            if (s)                                               \
                s_off = SAFE_INT32(s - src_str);                 \
            p_off = SAFE_INT32(p - pat_str);                     \
            YIELD_CHECK(L, phase_name, LUA_INTERRUPT_STDLIB);    \
            yield_budget = YIELD_BATCH_SIZE;                     \
        }                                                        \
    } while (0)

// Iterative match helper — called via YIELD_HELPER from find/match/gmatch/gsub.
// Creates a child SlotManager chained to the caller's, so yield/resume is
// handled automatically. Takes a caller-owned MatchState* (with captures and
// backtrack pointer into the wire buffer). Returns end offset or -1.
// Callers are responsible for pushing captures after the helper returns.
static int iterative_match_helper(lua_State* L, SlotManager& parent_slots,
    MatchState* ms, const char* pat_str, size_t patl,
    int start_off, int wire_idx)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        MAIN_YIELD = 1,
        FORWARD_YIELD = 2,
        GREEDY_YIELD = 3,
        BALANCE_YIELD = 4,
    };

    enum class Mode : uint8_t
    {
        FORWARD = 0,
        BACKTRACK = 1,
        GREEDY = 2,
        BALANCE = 3,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, s_off, 0);
    DEFINE_SLOT(int32_t, p_off, 0);
    DEFINE_SLOT(int32_t, result_s_off, -1);
    DEFINE_SLOT(Mode, mode, Mode::FORWARD);
    DEFINE_SLOT(int32_t, greedy_i, 0);
    DEFINE_SLOT(int32_t, greedy_ep_off, 0);
    DEFINE_SLOT(int32_t, bal_off, 0);
    DEFINE_SLOT(int32_t, bal_cont, 0);

    // Direct access to opaque buffer — avoids API call overhead per entry.
    auto* wire = (MatchStateWire*)uvalue(L->base + (wire_idx - 1))->data;
    // Only used as an RAII guard!
    [[maybe_unused]] MatchStateGuard guard(slots, ms, wire);

    slots.finalize();

    const char* src_str = ms->src_init;
    const char* src_end = ms->src_end;
    const char* pat_end = pat_str + patl;

    if (slots.isInit())
    {
        s_off = start_off;
        reprepstate(ms);
    }

    const char* s = src_str + s_off;
    const char* p = pat_str + p_off;

    // Shared yield budget across all modes. Declared before the YIELD_DISPATCH
    // to avoid goto-crossing-initialization issues. Not a DEFINE_SLOT — doesn't
    // need to survive yield, just a rate limiter reset after each yield check.
    int yield_budget = YIELD_BATCH_SIZE;

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(MAIN_YIELD);
    YIELD_DISPATCH(FORWARD_YIELD);
    YIELD_DISPATCH(GREEDY_YIELD);
    YIELD_DISPATCH(BALANCE_YIELD);
    YIELD_DISPATCH_END();

    for (;;)
    {
        // Sync slots for potential yield.
        // s can be NULL after a failed forward-pass match; skip sync to avoid UB.
        // Backtrack mode restores s from frame offsets, so stale s_off is harmless.
        if (s)
            s_off = SAFE_INT32(s - src_str);
        p_off = SAFE_INT32(p - pat_str);
        YIELD_CHECK(L, MAIN_YIELD, LUA_INTERRUPT_STDLIB);
        yield_budget = YIELD_BATCH_SIZE;

        if (mode == Mode::GREEDY)
        {
            for (;;)
            {
                BUDGET_YIELD_CHECK(L, GREEDY_YIELD);
                if (!singlematch(ms, s + greedy_i, p, pat_str + greedy_ep_off))
                {
                    auto& bt = ms->pushBt(L);
                    bt.kind = IMATCH_MAX_EXPAND;
                    bt.max_exp = {SAFE_INT32(s - src_str), greedy_ep_off,
                        greedy_i, SAFE_INT32(p - pat_str)};
                    s = s + greedy_i;
                    p = pat_str + greedy_ep_off + 1;
                    mode = Mode::FORWARD;
                    goto imatch_init;
                }
                greedy_i++;
            }
            // Loop exits only via goto imatch_init above.
        }

        if (mode == Mode::BALANCE)
        {
            for (;;)
            {
                BUDGET_YIELD_CHECK(L, BALANCE_YIELD);
                char bopen = p[2];
                char bclose = p[3];
                if (src_str + bal_off >= src_end)
                {
                    result_s_off = -1;
                    mode = Mode::BACKTRACK;
                    break;
                }
                char c = src_str[bal_off];
                bal_off++;
                if (c == bclose)
                {
                    if (--bal_cont == 0)
                    {
                        s = src_str + bal_off;
                        p += 4;
                        mode = Mode::FORWARD;
                        goto imatch_init;
                    }
                }
                else if (c == bopen)
                {
                    bal_cont++;
                }
            }
            // Reached from break (end-of-string -> BACKTRACK). Skip FORWARD block.
            continue;
        }

        if (mode == Mode::FORWARD)
        {
            // FORWARD: process pattern at (s, p)
        imatch_init:
            // We need to yield, we have no forward budget left!
            BUDGET_YIELD_CHECK(L, FORWARD_YIELD);
            if (p != pat_end)
            {
                switch (*p)
                {
                case '(':
                {
                    int cap_kind;
                    if (*(p + 1) == ')')
                    {
                        cap_kind = CAP_POSITION;
                        p += 2;
                    }
                    else
                    {
                        cap_kind = CAP_UNFINISHED;
                        p += 1;
                    }
                    if (ms->level >= WIRE_MAXCAPTURES)
                        luaL_error(L, "too many captures");
                    auto& cap = ms->capture[ms->level];
                    cap.init = s;
                    cap.len = cap_kind;
                    auto& bt = ms->pushBt(L);
                    bt.kind = IMATCH_START_CAPTURE;
                    bt.start_cap = {ms->level};
                    ms->level++;
                    goto imatch_init;
                }
                case ')':
                {
                    int l = capture_to_close(ms);
                    auto& cap = ms->capture[l];
                    ptrdiff_t old_len = cap.len;
                    cap.len = s - cap.init;
                    auto& bt = ms->pushBt(L);
                    bt.kind = IMATCH_END_CAPTURE;
                    bt.end_cap = {l, SAFE_INT32(old_len)};
                    p = p + 1;
                    goto imatch_init;
                }
                case '$':
                {
                    if ((p + 1) != pat_end)
                        goto imatch_dflt;
                    s = (s == src_end) ? s : NULL;
                    break;
                }
                case L_ESC:
                {
                    switch (*(p + 1))
                    {
                    case 'b':
                    {
                        if (p + 3 >= pat_end)
                            luaL_error(L, "malformed pattern (missing arguments to '%%b')");
                        if (*s != *(p + 2))
                        {
                            s = NULL;
                            break;
                        }
                        bal_off = SAFE_INT32(s - src_str) + 1;
                        bal_cont = 1;
                        mode = Mode::BALANCE;
                        continue;
                    }
                    case 'f':
                    {
                        const char* ep;
                        char previous;
                        p += 2;
                        if (*p != '[')
                            luaL_error(L, "missing '[' after '%%f' in pattern");
                        ep = classend(ms, p);
                        previous = (s == ms->src_init) ? '\0' : *(s - 1);
                        if (!matchbracketclass(uchar(previous), p, ep - 1) &&
                            matchbracketclass(uchar(*s), p, ep - 1))
                        {
                            p = ep;
                            goto imatch_init;
                        }
                        s = NULL;
                        break;
                    }
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    {
                        s = match_capture(ms, s, uchar(*(p + 1)));
                        if (s != NULL)
                        {
                            p += 2;
                            goto imatch_init;
                        }
                        break;
                    }
                    default:
                        goto imatch_dflt;
                    }
                    break;
                }
                default:
                imatch_dflt:
                {
                    const char* ep = classend(ms, p);
                    if (!singlematch(ms, s, p, ep))
                    {
                        if (*ep == '*' || *ep == '?' || *ep == '-')
                        {
                            p = ep + 1;
                            goto imatch_init;
                        }
                        else
                            s = NULL;
                    }
                    else
                    {
                        switch (*ep)
                        {
                        case '?':
                        {
                            auto& bt = ms->pushBt(L);
                            bt.kind = IMATCH_OPTIONAL;
                            bt.optional = {SAFE_INT32(s - src_str), SAFE_INT32(ep - pat_str)};
                            s = s + 1;
                            p = ep + 1;
                            goto imatch_init;
                        }
                        case '+':
                            s++;
                            s_off = SAFE_INT32(s - src_str);
                            LUAU_FALLTHROUGH;
                        case '*':
                        {
                            greedy_i = 0;
                            greedy_ep_off = SAFE_INT32(ep - pat_str);
                            mode = Mode::GREEDY;
                            continue;
                        }
                        case '-':
                        {
                            auto& bt = ms->pushBt(L);
                            bt.kind = IMATCH_MIN_EXPAND;
                            bt.min_exp = {SAFE_INT32(s - src_str), SAFE_INT32(p - pat_str),
                                SAFE_INT32(ep - pat_str)};
                            p = ep + 1;
                            goto imatch_init;
                        }
                        default:
                            s++;
                            p = ep;
                            goto imatch_init;
                        }
                    }
                    break;
                }
                }
            }
            // Terminal: pattern exhausted or s became NULL
            result_s_off = s ? SAFE_INT32(s - src_str) : -1;
            mode = Mode::BACKTRACK;
            continue;
        }

        // BACKTRACK: handle result from sub-match
        if (ms->isBtEmpty())
        {
            // If there's nothing left on the backtracking stack, we're done here
            break;
        }

        ImatchFrame& fr = ms->getBtTop();

        switch (fr.kind)
        {
        case IMATCH_START_CAPTURE:
        {
            if (result_s_off < 0)
                ms->level = fr.start_cap.saved_level;
            ms->popBt();
            continue;
        }
        case IMATCH_END_CAPTURE:
        {
            if (result_s_off < 0)
                ms->capture[fr.end_cap.cap_idx].len = fr.end_cap.saved_len;
            ms->popBt();
            continue;
        }
        case IMATCH_MAX_EXPAND:
        {
            if (result_s_off >= 0)
            {
                ms->popBt();
                continue;
            }
            int i = --fr.max_exp.count;
            if (i >= 0)
            {
                s = src_str + fr.max_exp.s_base_off + i;
                p = pat_str + fr.max_exp.ep_off + 1;
                mode = Mode::FORWARD;
                continue;
            }
            result_s_off = -1;
            ms->popBt();
            continue;
        }
        case IMATCH_MIN_EXPAND:
        {
            if (result_s_off >= 0)
            {
                ms->popBt();
                continue;
            }
            const char* s_retry = src_str + fr.min_exp.s_retry_off;
            const char* p_pat = pat_str + fr.min_exp.p_off;
            const char* ep_pat = pat_str + fr.min_exp.ep_off;
            if (singlematch(ms, s_retry, p_pat, ep_pat))
            {
                s_retry++;
                fr.min_exp.s_retry_off = SAFE_INT32(s_retry - src_str);
                s = s_retry;
                p = ep_pat + 1;
                mode = Mode::FORWARD;
                continue;
            }
            result_s_off = -1;
            ms->popBt();
            continue;
        }
        case IMATCH_OPTIONAL:
        {
            if (result_s_off >= 0)
            {
                ms->popBt();
                continue;
            }
            ms->popBt();
            s = src_str + fr.optional.s_orig_off;
            p = pat_str + fr.optional.ep_off + 1;
            mode = Mode::FORWARD;
            continue;
        }
        default:
            LUAU_ASSERT(!"invalid backtrack site");
            break;
        }
    }

    return result_s_off;
}
#undef BUDGET_YIELD_CHECK

// }======================================================

/*
** {======================================================
** YIELDABLE FIND / MATCH
** =======================================================
*/

static int str_find_match_body(lua_State* L, bool is_init, MatchMode match_mode)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum Arg
    {
        ARG_SOURCE   = 2,
        ARG_PATTERN  = 3,
        ARG_INIT     = 4,
        ARG_PLAIN    = 5,
        STACK_WIRE   = 4,
    };
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        MATCH_CALL = 1,
        PLAIN_YIELD = 2,
    };

    // Fast path: plain find or no-specials pattern — bypass all yieldable
    // machinery. Pre-SlotManager arg positions: source=1, pattern=2, init=3, plain=4.
    if (is_init && match_mode == MatchMode::FIND)
    {
        size_t ls, lp;
        const char* s = luaL_checklstring(L, 1, &ls);
        const char* p = luaL_checklstring(L, 2, &lp);
        int init = posrelat(luaL_optinteger(L, 3, 1), ls);
        if (init < 1)
            init = 1;
        else if (init > (int)ls + 1)
        {
            lua_pushnil(L);
            return 1;
        }
        // ServerLua: gate the fast (unyieldable) lmemfind path by pattern length.
        // Long patterns must go through yieldable paths:
        //  - nospecials: strpbrk scans the entire pattern before any YIELD_CHECK
        //  - plain=true: lmemfind is O(N*M) for adversarial input
        // The pattern matching path handles literal characters with yield checks;
        // plain=true with long patterns gets a dedicated yieldable search loop.
        constexpr size_t MAX_PLAIN_STR = 512;
        if (lp <= MAX_PLAIN_STR && (lua_toboolean(L, 4) || nospecials(p, lp)))
        {
            const char* s2 = lmemfind(s + init - 1, ls - init + 1, p, lp);
            if (s2)
            {
                lua_pushinteger(L, (int)(s2 - s + 1));
                lua_pushinteger(L, (int)(s2 - s + lp));
                return 2;
            }
            lua_pushnil(L);
            return 1;
        }
    }

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, s1_off, 0);
    DEFINE_SLOT(bool, is_anchor, false);
    DEFINE_SLOT(bool, is_plain, false);
    slots.finalize();

    if (is_init)
    {
        size_t ls, lp;
        luaL_checklstring(L, ARG_SOURCE, &ls);
        const char* p = luaL_checklstring(L, ARG_PATTERN, &lp);
        int init = posrelat(luaL_optinteger(L, ARG_INIT, 1), ls);
        if (init < 1)
            init = 1;
        else if (init > (int)ls + 1)
        {
            lua_pushnil(L);
            return 1;
        }

        is_plain = (match_mode == MatchMode::FIND) && lua_toboolean(L, ARG_PLAIN);
        s1_off = init - 1;

        if (is_plain)
        {
            // Plain search: just trim args, no wire buffer or anchor handling needed
            lua_settop(L, ARG_PATTERN);
        }
        else
        {
            // Pattern search: set up loop state
            is_anchor = (*p == '^');
            if (is_anchor)
            {
                // Strip anchor from pattern on stack
                lua_pushlstring(L, p + 1, lp - 1);
                lua_replace(L, ARG_PATTERN);
            }

            // Truncate: keep source + pattern, then create wire buffer
            lua_settop(L, ARG_PATTERN);
            lua_newuserdatatagged(L, sizeof(MatchStateWire), UTAG_OPAQUE_BUFFER);  // wire at STACK_WIRE
        }
    }

    // Re-read string data (fresh pointers after potential yield/resume)
    size_t ls, lp;
    const char* s = lua_tolstring(L, ARG_SOURCE, &ls);
    const char* p = lua_tolstring(L, ARG_PATTERN, &lp);
    int end_off = -1;
    // Declared before YIELD_DISPATCH to avoid goto-crossing-initialization.
    const char* found = nullptr;

    MatchState ms;
    prepstate(&ms, L, s, ls, p, lp);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(MATCH_CALL);
    YIELD_DISPATCH(PLAIN_YIELD);
    YIELD_DISPATCH_END();

    // Yieldable plain substring search for long patterns — same algorithm as
    // lmemfind, inlined because YIELD_CHECK labels can't cross function boundaries.
    // Each iteration does one memchr + one O(lp) memcmp, so per-iteration
    // yield checks are appropriate (no budget needed for patterns > 512).
    if (is_plain)
    {
        while ((size_t)s1_off + lp <= ls)
        {
            found = (const char*)memchr(s + s1_off, p[0], ls - lp - s1_off + 1);
            if (!found)
                break;
            if (memcmp(found + 1, p + 1, lp - 1) == 0)
            {
                int pos = (int)(found - s);
                lua_pushinteger(L, pos + 1);
                lua_pushinteger(L, pos + (int)lp);
                return 2;
            }
            s1_off = (int)(found - s) + 1;
            YIELD_CHECK(L, PLAIN_YIELD, LUA_INTERRUPT_STDLIB);
            // Re-read after potential yield (pointers may have moved)
            s = lua_tolstring(L, ARG_SOURCE, &ls);
            p = lua_tolstring(L, ARG_PATTERN, &lp);
        }
        lua_pushnil(L);
        return 1;
    }

    for (; s1_off <= (int)ls; ++s1_off)
    {
        lua_settop(L, STACK_WIRE);
        reprepstate(&ms);

        YIELD_HELPER(L, MATCH_CALL, end_off = iterative_match_helper(L, slots,
            &ms, p, lp, s1_off, STACK_WIRE));

        if (end_off >= 0)
        {
            if (match_mode == MatchMode::FIND)
            {
                int ncaps = push_captures(&ms, nullptr, nullptr);
                lua_checkstack(L, 2);
                lua_pushinteger(L, s1_off + 1);
                lua_insert(L, STACK_WIRE + 1);
                lua_pushinteger(L, end_off);
                lua_insert(L, STACK_WIRE + 2);
                return ncaps + 2;
            }
            else
            {
                const char* match_start = s + s1_off;
                const char* match_end = s + end_off;
                return push_captures(&ms, match_start, match_end);
            }
        }

        if (is_anchor)
            break;
    }

    lua_pushnil(L);
    return 1;
}

int yieldable_str_find_v0(lua_State* L)
{
    return str_find_match_body(L, true, MatchMode::FIND);
}
int yieldable_str_find_v0_k(lua_State* L, int status)
{
    return str_find_match_body(L, false, MatchMode::FIND);
}

int yieldable_str_match_v0(lua_State* L)
{
    return str_find_match_body(L, true, MatchMode::MATCH);
}
int yieldable_str_match_v0_k(lua_State* L, int status)
{
    return str_find_match_body(L, false, MatchMode::MATCH);
}

// }======================================================

/*
** {======================================================
** YIELDABLE GMATCH
** =======================================================
*/

// Upvalues: [1] source string, [2] pattern, [3] current position, [4] wire buffer (nil if taken)
DEFINE_YIELDABLE_EXTERN(yieldable_gmatch_aux, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum Stack
    {
        STACK_WIRE = 2,
    };
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        MATCH_CALL = 1,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, src_off, 0);
    slots.finalize();

    size_t ls, lp;
    const char* s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    const char* p = lua_tolstring(L, lua_upvalueindex(2), &lp);
    int end_off = -1;

    if (is_init)
    {
        src_off = (int)lua_tointeger(L, lua_upvalueindex(3));
        lua_settop(L, 1);
        // Take wire buffer from upvalue 4; error if already taken (re-entrant call)
        TValue* uv = &clvalue(L->ci->func)->c.upvals[3];
        if (ttisnil(uv))
            luaL_error(L, "gmatch iterator is already in use");
        setobj2s(L, L->top, uv);
        L->top++;
        setnilvalue(uv);
        // wire at STACK_WIRE
    }

    MatchState ms;
    prepstate(&ms, L, s, ls, p, lp);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(MATCH_CALL);
    YIELD_DISPATCH_END();

    for (; src_off <= (int)ls; ++src_off)
    {
        lua_settop(L, STACK_WIRE);
        reprepstate(&ms);

        YIELD_HELPER(L, MATCH_CALL, end_off = iterative_match_helper(L, slots,
            &ms, p, lp, src_off, STACK_WIRE));

        if (end_off >= 0)
        {
            const char* match_start = s + src_off;
            const char* match_end = s + end_off;
            int ncaps = push_captures(&ms, match_start, match_end);
            lua_checkstack(L, 1);
            int newstart = end_off;
            if (end_off == src_off)
                newstart++;
            lua_pushinteger(L, newstart);
            lua_replace(L, lua_upvalueindex(3));
            // Return wire buffer to upvalue for reuse by next iteration
            Closure* cl = clvalue(L->ci->func);
            setobj(L, &cl->c.upvals[3], L->base + (STACK_WIRE - 1));
            luaC_barrier(L, cl, L->base + (STACK_WIRE - 1));
            return ncaps;
        }
    }

    // Return wire buffer to upvalue for reuse
    {
        Closure* cl = clvalue(L->ci->func);
        setobj(L, &cl->c.upvals[3], L->base + (STACK_WIRE - 1));
        luaC_barrier(L, cl, L->base + (STACK_WIRE - 1));
    }
    return 0;
}

int yieldable_gmatch(lua_State* L)
{
    luaL_checkstring(L, 1);
    luaL_checkstring(L, 2);
    lua_settop(L, 2);
    lua_pushinteger(L, 0);
    lua_newuserdatatagged(L, sizeof(MatchStateWire), UTAG_OPAQUE_BUFFER);
    lua_pushcclosurek(L, yieldable_gmatch_aux_v0, "gmatch_aux", 4, yieldable_gmatch_aux_v0_k);
    return 1;
}

// }======================================================

/*
** {======================================================
** YIELDABLE GSUB
** =======================================================
*/

// Yieldable str_gsub. Uses lua_YieldSafeStrBuf (raw memory buffer in a
// tagged userdata at a fixed slot) instead of luaL_Strbuf (whose
// stack-relative box is incompatible with yield/resume).
// Calls iterative_match_helper via YIELD_HELPER for yieldable pattern matching.
DEFINE_YIELDABLE_EXTERN(yieldable_str_gsub, 0)
{
    enum Arg
    {
        ARG_SOURCE    = 2,
        ARG_PATTERN   = 3,
        ARG_REPL      = 4,
        STACK_STRBUF  = 5,
        STACK_WIRE    = 6,
    };

    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        MATCH_CALL = 1,
        REPL_CALL = 2,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, n, 0);
    DEFINE_SLOT(int32_t, src_off, 0);
    DEFINE_SLOT(int32_t, max_s, 0);
    DEFINE_SLOT(int32_t, repl_type, 0);
    DEFINE_SLOT(int32_t, end_off, 0);
    DEFINE_SLOT(bool, is_anchor, false);
    slots.finalize();

    lua_YieldSafeStrBuf* buf;

    if (is_init)
    {
        size_t srcl, lp;
        luaL_checklstring(L, ARG_SOURCE, &srcl);
        const char* p = luaL_checklstring(L, ARG_PATTERN, &lp);
        int t = lua_type(L, ARG_REPL);
        luaL_argexpected(L,
            t == LUA_TNUMBER || t == LUA_TSTRING ||
            t == LUA_TFUNCTION || t == LUA_TTABLE,
            ARG_REPL, "string/function/table");

        repl_type = t;
        max_s = luaL_optinteger(L, ARG_REPL + 1, (int)srcl + 1);
        n = 0;
        src_off = 0;
        end_off = 0;

        // Check anchor and strip from pattern on stack
        if (*p == '^')
        {
            is_anchor = true;
            lua_pushlstring(L, p + 1, lp - 1);
            lua_replace(L, ARG_PATTERN);
        }
        else
        {
            is_anchor = false;
        }

        // Truncate: keep source, pattern, replacement
        lua_settop(L, ARG_REPL);

        // Push yield-safe string buffer, then wire buffer
        lstrbuf_push(L);
        lua_newuserdatatagged(L, sizeof(MatchStateWire), UTAG_OPAQUE_BUFFER);  // wire at STACK_WIRE
    }

    // Re-read string data (fresh pointers after potential yield/resume)
    size_t srcl, lp;
    const char* src_str = lua_tolstring(L, ARG_SOURCE, &srcl);
    const char* p = lua_tolstring(L, ARG_PATTERN, &lp);
    int match_end_off = -1;
    buf = (lua_YieldSafeStrBuf*)lua_touserdatatagged(L, STACK_STRBUF, UTAG_STRBUF);

    MatchState ms;
    prepstate(&ms, L, src_str, srcl, p, lp);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(MATCH_CALL);
    YIELD_DISPATCH(REPL_CALL);
    YIELD_DISPATCH_END();

    while (n < max_s)
    {
        lua_settop(L, STACK_WIRE);
        reprepstate(&ms);

        YIELD_HELPER(L, MATCH_CALL, match_end_off = iterative_match_helper(L, slots,
            &ms, p, lp, src_off, STACK_WIRE));

        if (match_end_off >= 0)
        {
            end_off = match_end_off;
            int call_nargs;
            {
                const char* match_start = src_str + src_off;
                const char* match_end = src_str + end_off;
                int nlevels = (ms.level == 0) ? 1 : ms.level;
                ++n;

                // Ensure stack headroom for replacement processing.
                lua_checkstack(L, nlevels + 2);

                if (repl_type == LUA_TFUNCTION)
                {
                    lua_pushvalue(L, ARG_REPL);
                    for (int i = 0; i < nlevels; i++)
                        push_onecapture(&ms, i, match_start, match_end);
                    call_nargs = nlevels;
                }
                else if (repl_type == LUA_TTABLE)
                {
                    push_onecapture(&ms, 0, match_start, match_end);
                    // Not yieldable, but callTMres already calls the
                    // interrupt handler if __index is a function.
                    lua_gettable(L, ARG_REPL);
                }
                else
                {
                    // String replacement: process % escapes, add fragments to buffer.
                    size_t repl_len;
                    const char* news = lua_tolstring(L, ARG_REPL, &repl_len);
                    size_t run_start = 0;
                    for (size_t i = 0; i < repl_len; i++)
                    {
                        if (news[i] != L_ESC)
                            continue;
                        // Flush literal run before this escape
                        if (i > run_start)
                            strbuf_append_mem(L, buf, news + run_start, i - run_start);
                        i++;
                        if (!isdigit(uchar(news[i])))
                        {
                            if (news[i] != L_ESC)
                                luaL_error(L, "invalid use of '%c' in replacement string", L_ESC);
                            strbuf_append_char(L, buf, news[i]);
                        }
                        else if (news[i] == '0')
                        {
                            strbuf_append_mem(L, buf, src_str + src_off, end_off - src_off);
                        }
                        else
                        {
                            int cap_idx = news[i] - '1';
                            if (cap_idx >= nlevels)
                                luaL_error(L, "invalid capture index %%%d", cap_idx + 1);
                            push_onecapture(&ms, cap_idx, match_start, match_end);
                            strbuf_addvalue(L, buf);
                        }
                        run_start = i + 1;
                    }
                    // Flush trailing literal run
                    if (run_start < repl_len)
                        strbuf_append_mem(L, buf, news + run_start, repl_len - run_start);
                    goto gsub_advance;
                }
            }
            if (repl_type == LUA_TFUNCTION)
                YIELD_CALL(L, call_nargs, 1, REPL_CALL);
            // Function/table result: nil or false -> keep original match text
            if (!lua_toboolean(L, -1))
            {
                lua_pop(L, 1);
                strbuf_append_mem(L, buf, src_str + src_off, end_off - src_off);
            }
            else if (!lua_isstring(L, -1))
                luaL_error(L, "invalid replacement value (a %s)", luaL_typename(L, -1));
            else
                strbuf_addvalue(L, buf);

        gsub_advance:
            if (end_off > src_off)
                src_off = end_off;
            else if (src_off < (int)srcl)
            {
                strbuf_append_char(L, buf, src_str[src_off]);
                ++src_off;
            }
            else
                break;
        }
        else
        {
            if (src_off < (int)srcl)
            {
                strbuf_append_char(L, buf, src_str[src_off]);
                ++src_off;
            }
            else
                break;
        }
        if (is_anchor)
            break;
    }

    // Add remaining suffix and push result
    // Truncate stack to STACK_STRBUF to clean up leftover captures from the
    // last iteration, ensuring ci->top has room for the result push.
    lua_settop(L, STACK_STRBUF);
    strbuf_append_mem(L, buf, src_str + src_off, srcl - src_off);
    strbuf_tostring_inplace(L, STACK_STRBUF, true);

    lua_pushinteger(L, n);
    return 2;
}

// }======================================================
