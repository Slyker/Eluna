#ifndef _ELUNA_UTIL_H
#define _ELUNA_UTIL_H

#include "Common.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Log.h"
#include "QueryResult.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

typedef std::vector<uint8> BytecodeBuffer;

class Unit;
class WorldObject;
struct FactionTemplateEntry;

namespace ElunaUtil
{
    uint32 GetCurrTime();
    uint32 GetTimeDiff(uint32 oldMSTime);

    void EncodeData(const unsigned char* data, size_t input_length, std::string& output);
    unsigned char* DecodeData(const char* data, size_t *output_length);
};

#endif
