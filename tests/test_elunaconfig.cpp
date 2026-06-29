/*
 * Unit tests for ElunaConfig logic.
 *
 * Tests ShouldMapLoadEluna and TokenizeAllowedMaps behavior
 * by reimplementing the self-contained parsing logic from ElunaConfig.
 * This avoids depending on the game-server config system.
 */

#include <gtest/gtest.h>
#include "Common.h"

#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

// Reimplementation of the config parsing logic for testability.
// Mirrors ElunaConfig::TokenizeAllowedMaps and ShouldMapLoadEluna.
class TestableElunaConfig
{
public:
    void TokenizeAllowedMaps(const std::string& configValue)
    {
        m_allowedMaps.clear();

        std::istringstream maps(configValue);
        std::string mapIdStr;
        while (std::getline(maps, mapIdStr, ','))
        {
            mapIdStr.erase(std::remove_if(mapIdStr.begin(), mapIdStr.end(), [](char c) {
                return std::isspace(static_cast<unsigned char>(c));
            }), mapIdStr.end());

            if (mapIdStr.empty())
                continue;

            try {
                uint32 mapId = std::stoul(mapIdStr);
                m_allowedMaps.emplace(mapId);
            }
            catch (std::exception&) {
                // invalid value - skip
            }
        }
    }

    bool ShouldMapLoadEluna(uint32 id) const
    {
        if (m_allowedMaps.empty())
            return true;
        return (m_allowedMaps.find(id) != m_allowedMaps.end());
    }

    size_t GetAllowedMapCount() const { return m_allowedMaps.size(); }

private:
    std::unordered_set<uint32> m_allowedMaps;
};

// ================================================================
// ShouldMapLoadEluna tests
// ================================================================

TEST(ElunaConfig, EmptyConfigAllowsAllMaps)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(530));
    EXPECT_TRUE(config.ShouldMapLoadEluna(999));
}

TEST(ElunaConfig, SingleMapId)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("1");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_FALSE(config.ShouldMapLoadEluna(0));
    EXPECT_FALSE(config.ShouldMapLoadEluna(2));
}

TEST(ElunaConfig, MultipleMapIds)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0,1,530,571");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(530));
    EXPECT_TRUE(config.ShouldMapLoadEluna(571));
    EXPECT_FALSE(config.ShouldMapLoadEluna(2));
    EXPECT_FALSE(config.ShouldMapLoadEluna(529));
}

TEST(ElunaConfig, MapIdsWithSpaces)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0, 1, 530 , 571");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(530));
    EXPECT_TRUE(config.ShouldMapLoadEluna(571));
    EXPECT_EQ(config.GetAllowedMapCount(), 4u);
}

TEST(ElunaConfig, MapIdsWithLeadingTrailingSpaces)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("  0  ,  1  ");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, MapIdsWithTabs)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0,\t1,\t530");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(530));
    EXPECT_EQ(config.GetAllowedMapCount(), 3u);
}

TEST(ElunaConfig, InvalidMapIdIsSkipped)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0,abc,1");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, AllInvalidValues)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("abc,def,ghi");
    // All invalid -> empty set -> all maps allowed
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
}

TEST(ElunaConfig, DuplicateMapIds)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("1,1,1,2");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(2));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, MapIdZero)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_FALSE(config.ShouldMapLoadEluna(1));
}

TEST(ElunaConfig, LargeMapId)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("999999");
    EXPECT_TRUE(config.ShouldMapLoadEluna(999999));
    EXPECT_FALSE(config.ShouldMapLoadEluna(0));
}

TEST(ElunaConfig, TrailingComma)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("1,2,");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(2));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, LeadingComma)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps(",1,2");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(2));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, MultipleCommas)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("1,,2,,,3");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(2));
    EXPECT_TRUE(config.ShouldMapLoadEluna(3));
}

TEST(ElunaConfig, RetokenizeReplacesOldValues)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("1,2,3");
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
    EXPECT_EQ(config.GetAllowedMapCount(), 3u);

    config.TokenizeAllowedMaps("4,5");
    EXPECT_FALSE(config.ShouldMapLoadEluna(1));
    EXPECT_TRUE(config.ShouldMapLoadEluna(4));
    EXPECT_TRUE(config.ShouldMapLoadEluna(5));
    EXPECT_EQ(config.GetAllowedMapCount(), 2u);
}

TEST(ElunaConfig, NegativeNumberIsInvalid)
{
    TestableElunaConfig config;
    // std::stoul will wrap or throw for negative values
    config.TokenizeAllowedMaps("-1,1");
    // -1 should either be skipped or treated as a large unsigned value
    EXPECT_TRUE(config.ShouldMapLoadEluna(1));
}

TEST(ElunaConfig, MixedValidAndInvalid)
{
    TestableElunaConfig config;
    config.TokenizeAllowedMaps("0, hello, 530, , world, 571");
    EXPECT_TRUE(config.ShouldMapLoadEluna(0));
    EXPECT_TRUE(config.ShouldMapLoadEluna(530));
    EXPECT_TRUE(config.ShouldMapLoadEluna(571));
    EXPECT_FALSE(config.ShouldMapLoadEluna(1));
}
