/*
 * Unit tests for the LuaVal class.
 *
 * Tests constructors, to_string, comparison operators, clone,
 * hash function, and Lua-state interaction (push/get/AsLuaVal).
 */

#include <gtest/gtest.h>
#include "LuaValue.h"

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

// ==================================================================
// Pure C++ tests (no Lua state needed)
// ==================================================================

TEST(LuaValConstruct, DefaultIsNil)
{
    LuaVal v;
    EXPECT_EQ(v.to_string(), "nil");
}

TEST(LuaValConstruct, FromString)
{
    LuaVal v(std::string("hello"));
    EXPECT_EQ(v.to_string(), "hello");
}

TEST(LuaValConstruct, FromEmptyString)
{
    LuaVal v(std::string(""));
    EXPECT_EQ(v.to_string(), "");
}

TEST(LuaValConstruct, FromBoolTrue)
{
    LuaVal v(true);
    EXPECT_EQ(v.to_string(), "true");
}

TEST(LuaValConstruct, FromBoolFalse)
{
    LuaVal v(false);
    EXPECT_EQ(v.to_string(), "false");
}

TEST(LuaValConstruct, FromDouble)
{
    LuaVal v(3.14);
    // std::to_string(3.14) produces "3.140000"
    EXPECT_EQ(v.to_string(), std::to_string(3.14));
}

TEST(LuaValConstruct, FromZero)
{
    LuaVal v(0.0);
    EXPECT_EQ(v.to_string(), std::to_string(0.0));
}

TEST(LuaValConstruct, FromNegativeDouble)
{
    LuaVal v(-42.5);
    EXPECT_EQ(v.to_string(), std::to_string(-42.5));
}

TEST(LuaValConstruct, FromMap)
{
    LuaVal::MapType m;
    m[LuaVal(std::string("key"))] = LuaVal(42.0);
    LuaVal v(m);
    std::string str = v.to_string();
    EXPECT_NE(str.find("key"), std::string::npos);
    EXPECT_NE(str.find("42"), std::string::npos);
}

TEST(LuaValConstruct, FromInitializerList)
{
    LuaVal v({ {LuaVal(std::string("a")), LuaVal(1.0)} });
    std::string str = v.to_string();
    EXPECT_NE(str.find("a"), std::string::npos);
}

// ==================================================================
// Equality operator tests
// ==================================================================

TEST(LuaValEquality, NilEqualsNil)
{
    LuaVal a;
    LuaVal b;
    EXPECT_TRUE(a == b);
}

TEST(LuaValEquality, SameString)
{
    LuaVal a(std::string("test"));
    LuaVal b(std::string("test"));
    EXPECT_TRUE(a == b);
}

TEST(LuaValEquality, DifferentStrings)
{
    LuaVal a(std::string("foo"));
    LuaVal b(std::string("bar"));
    EXPECT_FALSE(a == b);
}

TEST(LuaValEquality, SameDouble)
{
    LuaVal a(42.0);
    LuaVal b(42.0);
    EXPECT_TRUE(a == b);
}

TEST(LuaValEquality, DifferentDoubles)
{
    LuaVal a(1.0);
    LuaVal b(2.0);
    EXPECT_FALSE(a == b);
}

TEST(LuaValEquality, SameBool)
{
    LuaVal a(true);
    LuaVal b(true);
    EXPECT_TRUE(a == b);
}

TEST(LuaValEquality, DifferentBools)
{
    LuaVal a(true);
    LuaVal b(false);
    EXPECT_FALSE(a == b);
}

TEST(LuaValEquality, DifferentTypes)
{
    LuaVal nil;
    LuaVal str(std::string("hello"));
    LuaVal num(42.0);
    LuaVal b(true);

    EXPECT_FALSE(nil == str);
    EXPECT_FALSE(nil == num);
    EXPECT_FALSE(nil == b);
    EXPECT_FALSE(str == num);
    EXPECT_FALSE(str == b);
    EXPECT_FALSE(num == b);
}

// ==================================================================
// Less-than operator tests
// ==================================================================

TEST(LuaValLessThan, NilVsString)
{
    LuaVal nil;
    LuaVal str(std::string("a"));
    // variant ordering: monostate < string
    EXPECT_TRUE(nil < str);
    EXPECT_FALSE(str < nil);
}

TEST(LuaValLessThan, StringOrdering)
{
    LuaVal a(std::string("abc"));
    LuaVal b(std::string("abd"));
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(LuaValLessThan, DoubleOrdering)
{
    LuaVal a(1.0);
    LuaVal b(2.0);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(LuaValLessThan, SameValueNotLess)
{
    LuaVal a(5.0);
    LuaVal b(5.0);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(b < a);
}

// ==================================================================
// Copy / Move / Clone tests
// ==================================================================

TEST(LuaValCopy, CopyConstruct)
{
    LuaVal original(std::string("data"));
    LuaVal copy(original);
    EXPECT_TRUE(copy == original);
    EXPECT_EQ(copy.to_string(), "data");
}

TEST(LuaValCopy, CopyAssignment)
{
    LuaVal original(42.0);
    LuaVal copy;
    copy = original;
    EXPECT_TRUE(copy == original);
}

TEST(LuaValCopy, MoveConstruct)
{
    LuaVal original(std::string("moved"));
    LuaVal moved(std::move(original));
    EXPECT_EQ(moved.to_string(), "moved");
}

TEST(LuaValCopy, MoveAssignment)
{
    LuaVal original(99.0);
    LuaVal moved;
    moved = std::move(original);
    EXPECT_EQ(moved.to_string(), std::to_string(99.0));
}

TEST(LuaValClone, PrimitiveClone)
{
    LuaVal v(std::string("cloned"));
    LuaVal c = v.clone();
    EXPECT_TRUE(c == v);
    EXPECT_EQ(c.to_string(), "cloned");
}

TEST(LuaValClone, MapCloneIsDeepCopy)
{
    LuaVal::MapType m;
    m[LuaVal(std::string("key"))] = LuaVal(1.0);
    LuaVal original(m);
    LuaVal cloned = original.clone();

    // Clone produces a distinct shared_ptr (deep copy of map data)
    auto& originalMap = std::get<LuaVal::WrappedMap>(original.v);
    auto& clonedMap = std::get<LuaVal::WrappedMap>(cloned.v);
    EXPECT_NE(originalMap.get(), clonedMap.get());
    EXPECT_EQ(originalMap->size(), clonedMap->size());

    // Modifying the clone's map should not affect the original
    (*clonedMap)[LuaVal(std::string("new_key"))] = LuaVal(2.0);
    EXPECT_EQ(originalMap->size(), 1u);
    EXPECT_EQ(clonedMap->size(), 2u);
}

TEST(LuaValClone, ReferenceIsShallowCopy)
{
    LuaVal::MapType m;
    m[LuaVal(std::string("key"))] = LuaVal(1.0);
    LuaVal original(m);
    LuaVal ref = original.reference();

    // Modifying the ref's map SHOULD affect the original (shared_ptr)
    auto& refMap = std::get<LuaVal::WrappedMap>(ref.v);
    (*refMap)[LuaVal(std::string("new_key"))] = LuaVal(2.0);

    auto& originalMap = std::get<LuaVal::WrappedMap>(original.v);
    EXPECT_EQ(originalMap->size(), 2u);
}

// ==================================================================
// Hash tests
// ==================================================================

TEST(LuaValHash, SameValuesProduceSameHash)
{
    LuaVal a(std::string("test"));
    LuaVal b(std::string("test"));
    EXPECT_EQ(std::hash<LuaVal>{}(a), std::hash<LuaVal>{}(b));
}

TEST(LuaValHash, NilHash)
{
    LuaVal a;
    LuaVal b;
    EXPECT_EQ(std::hash<LuaVal>{}(a), std::hash<LuaVal>{}(b));
}

TEST(LuaValHash, BoolHash)
{
    LuaVal t(true);
    LuaVal f(false);
    // Different values should (likely) produce different hashes
    // Not guaranteed but practically true
    EXPECT_NE(std::hash<LuaVal>{}(t), std::hash<LuaVal>{}(f));
}

TEST(LuaValHash, DoubleHash)
{
    LuaVal a(3.14);
    LuaVal b(3.14);
    EXPECT_EQ(std::hash<LuaVal>{}(a), std::hash<LuaVal>{}(b));
}

TEST(LuaValHash, UsableInUnorderedMap)
{
    std::unordered_map<LuaVal, int> map;
    map[LuaVal(std::string("key1"))] = 1;
    map[LuaVal(std::string("key2"))] = 2;
    map[LuaVal(42.0)] = 3;
    map[LuaVal(true)] = 4;

    EXPECT_EQ(map[LuaVal(std::string("key1"))], 1);
    EXPECT_EQ(map[LuaVal(std::string("key2"))], 2);
    EXPECT_EQ(map[LuaVal(42.0)], 3);
    EXPECT_EQ(map[LuaVal(true)], 4);
}

// ==================================================================
// to_string_map tests
// ==================================================================

TEST(LuaValToStringMap, EmptyMap)
{
    LuaVal::MapType m;
    std::string str = LuaVal::to_string_map(&m);
    EXPECT_EQ(str, "[\n]");
}

TEST(LuaValToStringMap, SingleEntry)
{
    LuaVal::MapType m;
    m[LuaVal(std::string("name"))] = LuaVal(std::string("value"));
    std::string str = LuaVal::to_string_map(&m);
    EXPECT_NE(str.find("name"), std::string::npos);
    EXPECT_NE(str.find("value"), std::string::npos);
    EXPECT_NE(str.find("key:"), std::string::npos);
}

// ==================================================================
// Lua-state interaction tests
// ==================================================================

TEST(LuaValLuaInteraction, RegisterCreatesGlobal)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_getglobal(L, "LuaVal");
    EXPECT_TRUE(lua_istable(L, -1));
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, PushAndGetNil)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    LuaVal v;
    LuaVal::PushLuaVal(L, v);
    LuaVal* got = LuaVal::GetLuaVal(L, -1);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(*got == v);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, PushAndGetString)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    LuaVal v(std::string("hello_lua"));
    LuaVal::PushLuaVal(L, v);
    LuaVal* got = LuaVal::GetLuaVal(L, -1);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(*got == v);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, PushAndGetDouble)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    LuaVal v(123.456);
    LuaVal::PushLuaVal(L, v);
    LuaVal* got = LuaVal::GetLuaVal(L, -1);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(*got == v);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, PushAndGetBool)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    LuaVal v(true);
    LuaVal::PushLuaVal(L, v);
    LuaVal* got = LuaVal::GetLuaVal(L, -1);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(*got == v);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, PushAndGetMap)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    LuaVal v({ {LuaVal(std::string("x")), LuaVal(10.0)} });
    LuaVal::PushLuaVal(L, v);
    LuaVal* got = LuaVal::GetLuaVal(L, -1);
    ASSERT_NE(got, nullptr);
    auto* p = std::get_if<LuaVal::WrappedMap>(&got->v);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((*p)->size(), 1u);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaValFromNumber)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_pushnumber(L, 42.0);
    LuaVal v = LuaVal::AsLuaVal(L, -1);
    EXPECT_TRUE(v == LuaVal(42.0));
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaValFromString)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_pushstring(L, "test_string");
    LuaVal v = LuaVal::AsLuaVal(L, -1);
    EXPECT_TRUE(v == LuaVal(std::string("test_string")));
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaValFromBoolean)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_pushboolean(L, 1);
    LuaVal v = LuaVal::AsLuaVal(L, -1);
    EXPECT_TRUE(v == LuaVal(true));
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaValFromNil)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_pushnil(L);
    LuaVal v = LuaVal::AsLuaVal(L, -1);
    EXPECT_TRUE(v == LuaVal());
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaValFromTable)
{
    LuaStateGuard L;
    LuaVal::Register(L);
    lua_newtable(L);
    lua_pushstring(L, "key1");
    lua_pushnumber(L, 100);
    lua_settable(L, -3);
    lua_pushstring(L, "key2");
    lua_pushboolean(L, 0);
    lua_settable(L, -3);

    // Use absolute index since FromTable internally pushes nil,
    // which would shift a negative index.
    int tblIdx = lua_gettop(L);
    LuaVal v = LuaVal::AsLuaVal(L, tblIdx);
    auto* p = std::get_if<LuaVal::WrappedMap>(&v.v);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((*p)->size(), 2u);
    EXPECT_TRUE((*p)->at(LuaVal(std::string("key1"))) == LuaVal(100.0));
    EXPECT_TRUE((*p)->at(LuaVal(std::string("key2"))) == LuaVal(false));
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsObjectPushesCorrectType)
{
    LuaStateGuard L;
    LuaVal::Register(L);

    // nil
    LuaVal().asObject(L);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    // string
    LuaVal(std::string("hi")).asObject(L);
    EXPECT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "hi");
    lua_pop(L, 1);

    // number
    LuaVal(7.5).asObject(L);
    EXPECT_TRUE(lua_isnumber(L, -1));
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 7.5);
    lua_pop(L, 1);

    // bool
    LuaVal(false).asObject(L);
    EXPECT_TRUE(lua_isboolean(L, -1));
    EXPECT_EQ(lua_toboolean(L, -1), 0);
    lua_pop(L, 1);
}

TEST(LuaValLuaInteraction, AsLuaConvertsMapToTable)
{
    LuaStateGuard L;
    LuaVal::Register(L);

    LuaVal v({ {LuaVal(std::string("a")), LuaVal(1.0)}, {LuaVal(std::string("b")), LuaVal(2.0)} });
    v.asLua(L, 0);
    EXPECT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "a");
    EXPECT_TRUE(lua_isnumber(L, -1));
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 1.0);
    lua_pop(L, 1);

    lua_getfield(L, -1, "b");
    EXPECT_TRUE(lua_isnumber(L, -1));
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 2.0);
    lua_pop(L, 2);
}
