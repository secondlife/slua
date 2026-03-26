#include <cstring>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

#include "lgc.h"
#include "lgcgraph.h"

#include "lfunc.h"
#include "lstring.h"
#include "ltable.h"
#include "ludata.h"
#include "lbuffer.h"
#include "llsl.h"
#include "lstrbuf.h"
#include "ltm.h"

struct Node
{
    GCObject* gco;
    uint8_t tt;
    uint8_t memcat;
    size_t size;
    std::optional<std::string> name;
    std::optional<std::string> type_name;
    bool is_free = false;
};

struct Edge
{
    GCObject* src;
    GCObject* dst;
    std::optional<std::string> name;
};

struct EnumContext
{
    lua_State *L;
    std::unordered_map<void*, Node> nodes = {};
    std::vector<Edge> edges = {};
};


static std::string gen_node_id(const Node &node)
{
    return "obj_" + std::to_string((size_t)node.gco);
}

static std::string escape_byte_string(const char *val)
{
    std::stringstream esc_val;
    auto len = strlen(val);
    for (size_t i=0; i<len; ++i)
    {
        auto char_val = val[i];
        if (char_val == '\\')
        {
            esc_val << "\\\\";
        }
        else if (char_val == '"')
        {
            esc_val << "\\\"";
        }
        else if (char_val >= 0x20 && char_val <= 0x7F)
        {
            esc_val << char_val;
        }
        else
        {
            esc_val << "\\\\x";
            esc_val << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)char_val;
        }
    }
    return esc_val.str();
}

static std::optional<std::string> gconame(GCObject *gco)
{
    switch (gco->gch.tt)
    {
    case LUA_TSTRING:
        return std::string(getstr(gco2ts(gco)));
    case LUA_TFUNCTION:
    {
        Closure* cl = gco2cl(gco);
        if (cl->isC)
        {
            if (cl->c.debugname)
                return std::string(cl->c.debugname);
        }
        else
        {
            Proto* p = cl->l.p;
            char buf[256];
            if (p->source && p->debugname)
                snprintf(buf, sizeof(buf), "%s@%s:%d", getstr(p->debugname), getstr(p->source), p->linedefined);
            else if (p->debugname)
                snprintf(buf, sizeof(buf), "%s:%d", getstr(p->debugname), p->linedefined);
            else
                return std::nullopt;
            return std::string(buf);
        }
        return std::nullopt;
    }
    case LUA_TPROTO:
    {
        Proto* p = gco2p(gco);
        char buf[256];
        if (p->source && p->debugname)
            snprintf(buf, sizeof(buf), "proto %s@%s:%d", getstr(p->debugname), getstr(p->source), p->linedefined);
        else if (p->debugname)
            snprintf(buf, sizeof(buf), "proto %s:%d", getstr(p->debugname), p->linedefined);
        else
            return std::nullopt;
        return std::string(buf);
    }
    case LUA_TTHREAD:
    {
        lua_State* th = gco2th(gco);
        if (th->ci > th->base_ci)
        {
            Closure* f = ci_func(th->ci);
            if (f && !f->isC && f->l.p->debugname)
            {
                char buf[256];
                Proto* p = f->l.p;
                if (p->source)
                    snprintf(buf, sizeof(buf), "thread at %s@%s:%d", getstr(p->debugname), getstr(p->source), p->linedefined);
                else
                    snprintf(buf, sizeof(buf), "thread at %s:%d", getstr(p->debugname), p->linedefined);
                return std::string(buf);
            }
        }
        return std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

// Resolve the typeof() name for a GCObject. Returns nullopt if the result
// is the same as the raw type name (no custom __type metamethod).
static std::optional<std::string> gco_typeof(lua_State *L, GCObject *gco)
{
    // Only types below LUA_T_COUNT can have metatables with __type overrides.
    // GC-internal types (proto, upval, deadkey) would be out of bounds.
    if (gco->gch.tt >= LUA_T_COUNT)
        return std::nullopt;

    TValue tv;
    tv.tt = gco->gch.tt;
    tv.value.gc = gco;

    const char *type_name = luaT_objtypename(L, &tv);
    if (strcmp(type_name, luaT_typenames[gco->gch.tt]) == 0)
        return std::nullopt;
    return std::string(type_name);
}

static std::string generate_node_attrs(const Node &node, const char* mode)
{
    std::stringstream ss;

    // Need to output ID as string due to ints not existing in JSON, they're really floats,
    // and our pointers are likely 64-bit.
    ss << "\"id\": \"" << (size_t)node.gco << "\"";
    ss << ", \"type\": \"" << luaT_typenames[node.tt] << "\"";
    ss << ", \"name\": \"" << (node.name.has_value() ? escape_byte_string(node.name->c_str()) : gen_node_id(node)) << "\"";
    ss << ", \"fixed\": " << (isfixed(node.gco) ? "true" : "false");

    const char *color = "unknown";
    if (isblack(node.gco))
        color = "black";
    else if (isgray(node.gco))
        color = "gray";
    else if (iswhite(node.gco))
        color = "white";

    ss << ", \"color\": \"" << color << "\"";
    ss << ", \"memcat\": " << (int)node.memcat;
    ss << ", \"size\": " << node.size;

    if (node.type_name.has_value())
        ss << ", \"type_name\": \"" << escape_byte_string(node.type_name->c_str()) << "\"";

    if (strcmp(mode, "user") == 0)
        ss << ", \"free\": " << (node.is_free ? "true" : "false");

    return ss.str();
}

static std::string generate_edge_attrs(const Edge &edge)
{
    std::stringstream ss;

    ss << "\"src\": \"" << (size_t)edge.src << "\"";
    ss << ", \"dst\": \"" << (size_t)edge.dst << "\"";
    if (edge.name.has_value())
        ss << ", \"name\": \"" << escape_byte_string(edge.name->c_str()) << "\"";
    else
        ss << ", \"name\": null";

    return ss.str();
}

static void write_graph_json(EnumContext &ctx, const char *out, const char *mode)
{
    std::ofstream ss {out, std::ios::out | std::ios::binary};
    LUAU_ASSERT(ss.is_open());

    ss << "{\n";
    ss << "  \"mode\": \"" << mode << "\",\n";
    ss << "  \"nodes\": [\n    ";
    bool first = true;
    for (const auto &node_iter : ctx.nodes)
    {
        if (!first)
            ss << ",\n    ";
        ss << "{" << generate_node_attrs(node_iter.second, mode) << "}";
        first = false;
    }
    ss << "\n  ],\n";

    ss << "  \"edges\": [\n    ";
    first = true;
    for (const auto &edge : ctx.edges)
    {
        if (!first)
            ss << ",\n    ";
        ss << "{" << generate_edge_attrs(edge) << "}";
        first = false;
    }
    ss << "\n  ]\n";

    ss << "}\n";

    ss.close();
}

void luaX_graphheap(lua_State *L, const char *out)
{
    EnumContext ctx;
    ctx.L = L;

    luaC_enumheap(
        L, &ctx,
        [](void* ctx, void* gco, uint8_t tt, uint8_t memcat, size_t size, const char* name) {
            EnumContext& context = *(EnumContext*)ctx;

            LUAU_ASSERT(gco);

            auto *casted_gco = ((GCObject *)gco);
            if (name == nullptr)
            {
                if (casted_gco->gch.tt == LUA_TSTRING)
                {
                    name = getstr(&casted_gco->ts);
                }
            }

            std::optional<std::string> name_str = name ? std::optional(std::string(name)) : std::nullopt;
            context.nodes[gco] = Node{casted_gco, tt, memcat, size, std::move(name_str), gco_typeof(context.L, casted_gco)};
        },
        [](void* ctx, void* src, void* dst, const char* edge_name) {
            EnumContext& context = *(EnumContext*)ctx;
            std::optional<std::string> name_str = edge_name ? std::optional<std::string>(edge_name) : std::nullopt;
            context.edges.push_back(Edge{(GCObject *)src, (GCObject *)dst, std::move(name_str)});
        }
    );

    write_graph_json(ctx, out, "global");
}

// User-thread-rooted graph: BFS from a user thread, only visiting objects with
// user memcats. Mirrors the traversal logic in lgctraverse.cpp but also records
// edges for graph visualization.

struct GraphContext
{
    EnumContext enum_ctx;
    std::queue<GCObject*> queue;
    std::unordered_set<void*> visited;
    const lua_OpaqueGCObjectSet *free_objects;
};

static void graph_add_node(GraphContext *ctx, GCObject *gco)
{
    bool is_free = ctx->free_objects && ctx->free_objects->count(gco);
    ctx->enum_ctx.nodes[gco] = Node{
        gco,
        gco->gch.tt,
        gco->gch.memcat,
        luaC_calclogicalgcosize(gco),
        gconame(gco),
        gco_typeof(ctx->enum_ctx.L, gco),
        is_free
    };
}

static void graph_enqueue(GraphContext *ctx, GCObject *from, GCObject *obj, const char *edge_name)
{
    if (!obj)
        return;

    // Same gating as lgctraverse.cpp's enqueueobj: user memcat, or thread with user activememcat
    bool eligible_thread = (obj->gch.tt == LUA_TTHREAD && gco2th(obj)->activememcat >= LUA_FIRST_USER_MEMCAT);
    if (!eligible_thread && obj->gch.memcat < LUA_FIRST_USER_MEMCAT)
        return;

    if (from)
    {
        std::optional<std::string> name_str = edge_name ? std::optional<std::string>(edge_name) : std::nullopt;
        ctx->enum_ctx.edges.push_back(Edge{from, obj, std::move(name_str)});
    }

    if (ctx->visited.insert(obj).second)
    {
        ctx->queue.push(obj);
    }
}

static void graph_enqueue_indexed(GraphContext *ctx, GCObject *from, GCObject *obj, const char *prefix, int index)
{
    if (!obj)
        return;

    bool eligible_thread = (obj->gch.tt == LUA_TTHREAD && gco2th(obj)->activememcat >= LUA_FIRST_USER_MEMCAT);
    if (!eligible_thread && obj->gch.memcat < LUA_FIRST_USER_MEMCAT)
        return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%s[%d]", prefix, index);

    std::optional<std::string> name_str{std::string(buf)};
    ctx->enum_ctx.edges.push_back(Edge{from, obj, std::move(name_str)});

    if (ctx->visited.insert(obj).second)
    {
        ctx->queue.push(obj);
    }
}

static void graph_traverse_table(GraphContext *ctx, GCObject *from, LuaTable *h)
{
    if (h->metatable)
        graph_enqueue(ctx, from, obj2gco(h->metatable), "metatable");

    for (int i = 0; i < h->sizearray; ++i)
    {
        if (iscollectable(&h->array[i]))
            graph_enqueue_indexed(ctx, from, gcvalue(&h->array[i]), "array", i);
    }

    if (h->node != &luaH_dummynode)
    {
        int sizenode = 1 << h->lsizenode;
        for (int i = 0; i < sizenode; ++i)
        {
            const LuaNode& n = h->node[i];
            if (!ttisnil(&n.val))
            {
                // Use string key value as edge name when available
                const char *key_name = nullptr;
                if (ttisstring(&n.key))
                    key_name = svalue(&n.key);

                if (iscollectable(&n.key))
                {
                    if (key_name)
                    {
                        char buf[128];
                        snprintf(buf, sizeof(buf), "[%s] (key)", key_name);
                        graph_enqueue(ctx, from, gcvalue(&n.key), buf);
                    }
                    else
                        graph_enqueue_indexed(ctx, from, gcvalue(&n.key), "key", i);
                }
                if (iscollectable(&n.val))
                {
                    if (key_name)
                    {
                        char buf[128];
                        snprintf(buf, sizeof(buf), ".%s", key_name);
                        graph_enqueue(ctx, from, gcvalue(&n.val), buf);
                    }
                    else
                        graph_enqueue_indexed(ctx, from, gcvalue(&n.val), "val", i);
                }
            }
        }
    }
}

static void graph_traverse_closure(GraphContext *ctx, GCObject *from, Closure *cl)
{
    graph_enqueue(ctx, from, obj2gco(cl->env), "env");

    if (cl->isC)
    {
        for (int i = 0; i < cl->nupvalues; ++i)
        {
            if (iscollectable(&cl->c.upvals[i]))
                graph_enqueue_indexed(ctx, from, gcvalue(&cl->c.upvals[i]), "upvalue", i);
        }
    }
    else
    {
        graph_enqueue(ctx, from, obj2gco(cl->l.p), "proto");

        for (int i = 0; i < cl->nupvalues; ++i)
        {
            if (iscollectable(&cl->l.uprefs[i]))
                graph_enqueue_indexed(ctx, from, gcvalue(&cl->l.uprefs[i]), "upvalue", i);
        }
    }
}

static void graph_traverse_udata(GraphContext *ctx, GCObject *from, Udata *u)
{
    if (u->metatable)
        graph_enqueue(ctx, from, obj2gco(u->metatable), "metatable");

    switch (u->tag)
    {
    case UTAG_LLEVENTS:
        graph_enqueue(ctx, from, obj2gco(((lua_LLEvents*)&u->data)->handlers_tab), "handlers");
        break;
    case UTAG_LLTIMERS:
        graph_enqueue(ctx, from, obj2gco(((lua_LLTimers*)&u->data)->timers_tab), "timers");
        break;
    case UTAG_UUID:
        graph_enqueue(ctx, from, obj2gco(((lua_LSLUUID*)&u->data)->str), "str");
        break;
    default:
        break;
    }
}

static void graph_traverse_thread(GraphContext *ctx, GCObject *from, lua_State *th)
{
    graph_enqueue(ctx, from, obj2gco(th->gt), "globals");

    for (StkId o = th->stack; o < th->top; ++o)
    {
        if (iscollectable(o))
            graph_enqueue_indexed(ctx, from, gcvalue(o), "stack", (int)(o - th->stack));
    }

    for (UpVal *uv = th->openupval; uv; uv = uv->u.open.threadnext)
        graph_enqueue(ctx, from, obj2gco(uv), "openupval");
}

static void graph_traverse_proto(GraphContext *ctx, GCObject *from, Proto *p)
{
    if (p->source)
        graph_enqueue(ctx, from, obj2gco(p->source), "source");
    if (p->debugname)
        graph_enqueue(ctx, from, obj2gco(p->debugname), "debugname");

    for (int i = 0; i < p->sizek; ++i)
    {
        if (iscollectable(&p->k[i]))
            graph_enqueue_indexed(ctx, from, gcvalue(&p->k[i]), "const", i);
    }

    for (int i = 0; i < p->sizeupvalues; ++i)
    {
        if (p->upvalues[i])
            graph_enqueue_indexed(ctx, from, obj2gco(p->upvalues[i]), "upvaluename", i);
    }

    for (int i = 0; i < p->sizep; ++i)
    {
        if (p->p[i])
            graph_enqueue_indexed(ctx, from, obj2gco(p->p[i]), "subproto", i);
    }

    for (int i = 0; i < p->sizelocvars; ++i)
    {
        if (p->locvars[i].varname)
            graph_enqueue_indexed(ctx, from, obj2gco(p->locvars[i].varname), "localname", i);
    }
}

static void graph_traverse_upval(GraphContext *ctx, GCObject *from, UpVal *uv)
{
    if (iscollectable(uv->v))
        graph_enqueue(ctx, from, gcvalue(uv->v), "value");
}

static void graph_traverse(GraphContext *ctx, GCObject *o)
{
    switch (o->gch.tt)
    {
    case LUA_TSTRING:
        break;
    case LUA_TTABLE:
        graph_traverse_table(ctx, o, gco2h(o));
        break;
    case LUA_TFUNCTION:
        graph_traverse_closure(ctx, o, gco2cl(o));
        break;
    case LUA_TUSERDATA:
        graph_traverse_udata(ctx, o, gco2u(o));
        break;
    case LUA_TTHREAD:
        graph_traverse_thread(ctx, o, gco2th(o));
        break;
    case LUA_TBUFFER:
        break;
    case LUA_TPROTO:
        graph_traverse_proto(ctx, o, gco2p(o));
        break;
    case LUA_TUPVAL:
        graph_traverse_upval(ctx, o, gco2uv(o));
        break;
    default:
        LUAU_ASSERT(!"Unknown object type in graph_traverse");
    }
}

void luaX_graphuserheap(lua_State *L, const char *out, const lua_OpaqueGCObjectSet *free_objects)
{
    GraphContext ctx;
    ctx.enum_ctx.L = L;
    ctx.free_objects = free_objects;

    // Always emit the root thread regardless of memcat
    GCObject *root = obj2gco(L);
    ctx.visited.insert(root);
    ctx.queue.push(root);

    while (!ctx.queue.empty())
    {
        GCObject *current = ctx.queue.front();
        ctx.queue.pop();

        graph_add_node(&ctx, current);
        graph_traverse(&ctx, current);
    }

    write_graph_json(ctx.enum_ctx, out, "user");
}
