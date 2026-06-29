/*
 * Unit tests for LuaEvent state management.
 *
 * Tests LuaEvent struct initialization, SetState transitions, and
 * GenerateDelay behavior. These are self-contained struct operations
 * that don't require the full EventMgr infrastructure.
 */

#include <gtest/gtest.h>
#include "Common.h"

// Reproduce the minimal types from ElunaEventMgr.h to avoid
// pulling in the full game-server header chain.

enum LuaEventState : uint8
{
    LUAEVENT_STATE_RUN,
    LUAEVENT_STATE_ABORT,
    LUAEVENT_STATE_ERASE,
};

// Minimal stub for urand (normally provided by the emulator)
static uint32 urand(uint32 min, uint32 max)
{
    if (min == max)
        return min;
    return min + (static_cast<uint32>(rand()) % (max - min + 1));
}

struct LuaEvent
{
    LuaEvent(int _funcRef, uint32 _min, uint32 _max, uint32 _repeats)
        : min(_min), max(_max), delay(0), repeats(_repeats),
          funcRef(_funcRef), state(LUAEVENT_STATE_RUN) { }

    void SetState(LuaEventState _state)
    {
        if (state != LUAEVENT_STATE_ERASE)
            state = _state;
    }

    void GenerateDelay()
    {
        delay = urand(min, max);
    }

    uint32 min;
    uint32 max;
    uint32 delay;
    uint32 repeats;
    int funcRef;
    LuaEventState state;
};

// ================================================================
// Construction tests
// ================================================================

TEST(LuaEvent, DefaultStateIsRun)
{
    LuaEvent evt(1, 100, 200, 5);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_RUN);
}

TEST(LuaEvent, FieldsInitialized)
{
    LuaEvent evt(42, 100, 500, 10);
    EXPECT_EQ(evt.funcRef, 42);
    EXPECT_EQ(evt.min, 100u);
    EXPECT_EQ(evt.max, 500u);
    EXPECT_EQ(evt.delay, 0u);
    EXPECT_EQ(evt.repeats, 10u);
}

TEST(LuaEvent, ZeroRepeatsIsInfinite)
{
    LuaEvent evt(1, 0, 0, 0);
    EXPECT_EQ(evt.repeats, 0u);
}

// ================================================================
// SetState tests
// ================================================================

TEST(LuaEventSetState, RunToAbort)
{
    LuaEvent evt(1, 0, 0, 0);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_RUN);
    evt.SetState(LUAEVENT_STATE_ABORT);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ABORT);
}

TEST(LuaEventSetState, RunToErase)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.SetState(LUAEVENT_STATE_ERASE);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);
}

TEST(LuaEventSetState, AbortToRun)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.SetState(LUAEVENT_STATE_ABORT);
    evt.SetState(LUAEVENT_STATE_RUN);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_RUN);
}

TEST(LuaEventSetState, AbortToErase)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.SetState(LUAEVENT_STATE_ABORT);
    evt.SetState(LUAEVENT_STATE_ERASE);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);
}

TEST(LuaEventSetState, EraseIsTerminal)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.SetState(LUAEVENT_STATE_ERASE);

    // Once erased, state cannot change
    evt.SetState(LUAEVENT_STATE_RUN);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);

    evt.SetState(LUAEVENT_STATE_ABORT);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);
}

TEST(LuaEventSetState, SetSameStateTwice)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.SetState(LUAEVENT_STATE_ABORT);
    evt.SetState(LUAEVENT_STATE_ABORT);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ABORT);
}

TEST(LuaEventSetState, FullTransitionCycle)
{
    LuaEvent evt(1, 0, 0, 0);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_RUN);

    evt.SetState(LUAEVENT_STATE_ABORT);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ABORT);

    evt.SetState(LUAEVENT_STATE_RUN);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_RUN);

    evt.SetState(LUAEVENT_STATE_ERASE);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);

    // Terminal - nothing changes
    evt.SetState(LUAEVENT_STATE_RUN);
    EXPECT_EQ(evt.state, LUAEVENT_STATE_ERASE);
}

// ================================================================
// GenerateDelay tests
// ================================================================

TEST(LuaEventDelay, FixedDelay)
{
    LuaEvent evt(1, 500, 500, 0);
    evt.GenerateDelay();
    EXPECT_EQ(evt.delay, 500u);
}

TEST(LuaEventDelay, DelayInRange)
{
    LuaEvent evt(1, 100, 200, 0);
    for (int i = 0; i < 100; i++)
    {
        evt.GenerateDelay();
        EXPECT_GE(evt.delay, 100u);
        EXPECT_LE(evt.delay, 200u);
    }
}

TEST(LuaEventDelay, ZeroMinMax)
{
    LuaEvent evt(1, 0, 0, 0);
    evt.GenerateDelay();
    EXPECT_EQ(evt.delay, 0u);
}

TEST(LuaEventDelay, RegeneratesNewValues)
{
    LuaEvent evt(1, 0, 1000000, 0);
    evt.GenerateDelay();
    uint32 first = evt.delay;

    // With a range of 0-1000000, getting the same value twice is extremely unlikely
    bool gotDifferent = false;
    for (int i = 0; i < 100; i++)
    {
        evt.GenerateDelay();
        if (evt.delay != first)
        {
            gotDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(gotDifferent);
}
