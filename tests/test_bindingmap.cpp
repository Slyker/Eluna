/*
 * Unit tests for BindingMap key types, hash_helper, and BindingMap operations.
 *
 * Tests EventKey, EntryKey, UniqueObjectKey equality/hash,
 * hash_helper::hash, and BindingMap Insert/Clear/Remove/HasBindingsFor.
 */

#include <gtest/gtest.h>
#include "BindingMap.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// Helper RAII wrapper for lua_State
struct LuaStateGuard
{
    lua_State* L;
    LuaStateGuard() : L(luaL_newstate()) { luaL_openlibs(L); }
    ~LuaStateGuard() { lua_close(L); }
    operator lua_State*() const { return L; }
};

// ================================================================
// hash_helper tests
// ================================================================

TEST(HashHelper, SingleIntValue)
{
    auto h1 = hash_helper::hash(42);
    auto h2 = hash_helper::hash(42);
    EXPECT_EQ(h1, h2);
}

TEST(HashHelper, DifferentIntValues)
{
    auto h1 = hash_helper::hash(1);
    auto h2 = hash_helper::hash(2);
    EXPECT_NE(h1, h2);
}

TEST(HashHelper, TwoValueCombination)
{
    auto h1 = hash_helper::hash(1, 2);
    auto h2 = hash_helper::hash(1, 2);
    EXPECT_EQ(h1, h2);
}

TEST(HashHelper, DifferentCombinations)
{
    auto h1 = hash_helper::hash(1, 2);
    auto h2 = hash_helper::hash(2, 1);
    EXPECT_NE(h1, h2);
}

TEST(HashHelper, ThreeValueCombination)
{
    auto h1 = hash_helper::hash(1, 2, 3);
    auto h2 = hash_helper::hash(1, 2, 3);
    EXPECT_EQ(h1, h2);
}

TEST(HashHelper, ThreeValueDifferent)
{
    auto h1 = hash_helper::hash(1, 2, 3);
    auto h2 = hash_helper::hash(1, 2, 4);
    EXPECT_NE(h1, h2);
}

enum TestEvent : int
{
    EVENT_A = 0,
    EVENT_B = 1,
    EVENT_C = 2,
};

TEST(HashHelper, EnumValue)
{
    auto h1 = hash_helper::hash(EVENT_A);
    auto h2 = hash_helper::hash(EVENT_A);
    EXPECT_EQ(h1, h2);
}

TEST(HashHelper, DifferentEnumValues)
{
    auto h1 = hash_helper::hash(EVENT_A);
    auto h2 = hash_helper::hash(EVENT_B);
    EXPECT_NE(h1, h2);
}

// ================================================================
// EventKey tests
// ================================================================

TEST(EventKey, EqualKeys)
{
    EventKey<int> a(1);
    EventKey<int> b(1);
    EXPECT_TRUE(std::equal_to<EventKey<int>>{}(a, b));
}

TEST(EventKey, UnequalKeys)
{
    EventKey<int> a(1);
    EventKey<int> b(2);
    EXPECT_FALSE(std::equal_to<EventKey<int>>{}(a, b));
}

TEST(EventKey, SameHash)
{
    EventKey<int> a(5);
    EventKey<int> b(5);
    EXPECT_EQ(std::hash<EventKey<int>>{}(a), std::hash<EventKey<int>>{}(b));
}

TEST(EventKey, DifferentHash)
{
    EventKey<int> a(5);
    EventKey<int> b(6);
    EXPECT_NE(std::hash<EventKey<int>>{}(a), std::hash<EventKey<int>>{}(b));
}

TEST(EventKey, WithEnum)
{
    EventKey<TestEvent> a(EVENT_A);
    EventKey<TestEvent> b(EVENT_A);
    EventKey<TestEvent> c(EVENT_B);
    EXPECT_TRUE(std::equal_to<EventKey<TestEvent>>{}(a, b));
    EXPECT_FALSE(std::equal_to<EventKey<TestEvent>>{}(a, c));
}

TEST(EventKey, UsableInUnorderedMap)
{
    std::unordered_map<EventKey<int>, std::string> map;
    map[EventKey<int>(1)] = "one";
    map[EventKey<int>(2)] = "two";
    EXPECT_EQ(map[EventKey<int>(1)], "one");
    EXPECT_EQ(map[EventKey<int>(2)], "two");
    EXPECT_EQ(map.count(EventKey<int>(3)), 0u);
}

// ================================================================
// EntryKey tests
// ================================================================

TEST(EntryKey, EqualKeys)
{
    EntryKey<int> a(1, 100);
    EntryKey<int> b(1, 100);
    EXPECT_TRUE(std::equal_to<EntryKey<int>>{}(a, b));
}

TEST(EntryKey, DifferentEventId)
{
    EntryKey<int> a(1, 100);
    EntryKey<int> b(2, 100);
    EXPECT_FALSE(std::equal_to<EntryKey<int>>{}(a, b));
}

TEST(EntryKey, DifferentEntry)
{
    EntryKey<int> a(1, 100);
    EntryKey<int> b(1, 200);
    EXPECT_FALSE(std::equal_to<EntryKey<int>>{}(a, b));
}

TEST(EntryKey, SameHash)
{
    EntryKey<int> a(1, 100);
    EntryKey<int> b(1, 100);
    EXPECT_EQ(std::hash<EntryKey<int>>{}(a), std::hash<EntryKey<int>>{}(b));
}

TEST(EntryKey, DifferentHash)
{
    EntryKey<int> a(1, 100);
    EntryKey<int> b(1, 200);
    EXPECT_NE(std::hash<EntryKey<int>>{}(a), std::hash<EntryKey<int>>{}(b));
}

TEST(EntryKey, UsableInUnorderedMap)
{
    std::unordered_map<EntryKey<int>, int> map;
    map[EntryKey<int>(1, 100)] = 10;
    map[EntryKey<int>(2, 200)] = 20;
    EXPECT_EQ(map[EntryKey<int>(1, 100)], 10);
    EXPECT_EQ(map[EntryKey<int>(2, 200)], 20);
    EXPECT_EQ(map.count(EntryKey<int>(1, 200)), 0u);
}

// ================================================================
// UniqueObjectKey tests
// ================================================================

TEST(UniqueObjectKey, EqualKeys)
{
    ObjectGuid g(12345);
    UniqueObjectKey<int> a(1, g, 42);
    UniqueObjectKey<int> b(1, g, 42);
    EXPECT_TRUE((std::equal_to<UniqueObjectKey<int>>{}(a, b)));
}

TEST(UniqueObjectKey, DifferentEventId)
{
    ObjectGuid g(12345);
    UniqueObjectKey<int> a(1, g, 42);
    UniqueObjectKey<int> b(2, g, 42);
    EXPECT_FALSE((std::equal_to<UniqueObjectKey<int>>{}(a, b)));
}

TEST(UniqueObjectKey, DifferentGuid)
{
    UniqueObjectKey<int> a(1, ObjectGuid(100), 42);
    UniqueObjectKey<int> b(1, ObjectGuid(200), 42);
    EXPECT_FALSE((std::equal_to<UniqueObjectKey<int>>{}(a, b)));
}

TEST(UniqueObjectKey, DifferentInstanceId)
{
    ObjectGuid g(12345);
    UniqueObjectKey<int> a(1, g, 42);
    UniqueObjectKey<int> b(1, g, 99);
    EXPECT_FALSE((std::equal_to<UniqueObjectKey<int>>{}(a, b)));
}

TEST(UniqueObjectKey, SameHash)
{
    ObjectGuid g(12345);
    UniqueObjectKey<int> a(1, g, 42);
    UniqueObjectKey<int> b(1, g, 42);
    EXPECT_EQ((std::hash<UniqueObjectKey<int>>{}(a)), (std::hash<UniqueObjectKey<int>>{}(b)));
}

TEST(UniqueObjectKey, DifferentHash)
{
    UniqueObjectKey<int> a(1, ObjectGuid(100), 42);
    UniqueObjectKey<int> b(1, ObjectGuid(200), 42);
    EXPECT_NE((std::hash<UniqueObjectKey<int>>{}(a)), (std::hash<UniqueObjectKey<int>>{}(b)));
}

// ================================================================
// BindingMap tests (using a real lua_State for ref management)
// ================================================================

TEST(BindingMap, InsertAndHasBindings)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    EventKey<int> key(1);
    map.Insert(key, ref, 0);
    EXPECT_TRUE(map.HasBindingsFor(key));
}

TEST(BindingMap, HasNoBindingsForUnknownKey)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);
    EXPECT_FALSE(map.HasBindingsFor(EventKey<int>(1)));
}

TEST(BindingMap, ClearByKey)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    EventKey<int> key(1);
    map.Insert(key, ref, 0);
    EXPECT_TRUE(map.HasBindingsFor(key));

    map.Clear(key);
    EXPECT_FALSE(map.HasBindingsFor(key));
}

TEST(BindingMap, ClearAll)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref1 = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref2 = luaL_ref(L, LUA_REGISTRYINDEX);

    map.Insert(EventKey<int>(1), ref1, 0);
    map.Insert(EventKey<int>(2), ref2, 0);

    EXPECT_TRUE(map.HasBindingsFor(EventKey<int>(1)));
    EXPECT_TRUE(map.HasBindingsFor(EventKey<int>(2)));

    map.Clear();

    EXPECT_FALSE(map.HasBindingsFor(EventKey<int>(1)));
    EXPECT_FALSE(map.HasBindingsFor(EventKey<int>(2)));
}

TEST(BindingMap, RemoveById)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    EventKey<int> key(1);
    uint64 id = map.Insert(key, ref, 0);

    EXPECT_TRUE(map.HasBindingsFor(key));
    map.Remove(id);
    EXPECT_FALSE(map.HasBindingsFor(key));
}

TEST(BindingMap, RemoveInvalidId)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    // Should not crash
    map.Remove(9999);
}

TEST(BindingMap, MultipleBindingsSameKey)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref1 = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref2 = luaL_ref(L, LUA_REGISTRYINDEX);

    EventKey<int> key(1);
    uint64 id1 = map.Insert(key, ref1, 0);
    map.Insert(key, ref2, 0);

    EXPECT_TRUE(map.HasBindingsFor(key));

    map.Remove(id1);
    // Should still have bindings from ref2
    EXPECT_TRUE(map.HasBindingsFor(key));
}

TEST(BindingMap, InsertReturnsUniqueIds)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref1 = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref2 = luaL_ref(L, LUA_REGISTRYINDEX);

    uint64 id1 = map.Insert(EventKey<int>(1), ref1, 0);
    uint64 id2 = map.Insert(EventKey<int>(1), ref2, 0);
    EXPECT_NE(id1, id2);
}

TEST(BindingMap, ClearOnEmptyMapDoesNotCrash)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);
    map.Clear(EventKey<int>(1));
    map.Clear();
}

TEST(BindingMap, HasBindingsForEmptyMap)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);
    EXPECT_FALSE(map.HasBindingsFor(EventKey<int>(0)));
}

// NOTE: PushRefsFor with shot-limited bindings (remainingShots > 0) has a
// pre-existing iterator invalidation bug in BindingMap.h.
// When a binding's shots reach zero, list.erase(i_prev) invalidates the
// pre-advanced iterator `i` (std::vector invalidates iterators at/after
// the erase point), causing undefined behavior on the next loop iteration.
// Shot-based PushRefsFor tests are intentionally omitted until this is fixed.

TEST(BindingMap, PushRefsForPermanent)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    EventKey<int> key(1);
    map.Insert(key, ref, 0);  // 0 shots = permanent

    for (int i = 0; i < 10; i++)
    {
        int top = lua_gettop(L);
        map.PushRefsFor(key);
        EXPECT_EQ(lua_gettop(L) - top, 1);
        lua_pop(L, 1);
    }
    EXPECT_TRUE(map.HasBindingsFor(key));
}

TEST(BindingMap, PushRefsForUnknownKey)
{
    LuaStateGuard L;
    BindingMap<EventKey<int>> map(L);

    int top = lua_gettop(L);
    map.PushRefsFor(EventKey<int>(999));
    EXPECT_EQ(lua_gettop(L), top);
}

TEST(BindingMap, WithEntryKey)
{
    LuaStateGuard L;
    BindingMap<EntryKey<int>> map(L);

    lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    EntryKey<int> key(1, 100);
    map.Insert(key, ref, 0);
    EXPECT_TRUE(map.HasBindingsFor(key));
    EXPECT_FALSE(map.HasBindingsFor(EntryKey<int>(1, 200)));
    EXPECT_FALSE(map.HasBindingsFor(EntryKey<int>(2, 100)));
}
