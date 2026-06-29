#ifndef _OBJECT_GUID_STUB_H
#define _OBJECT_GUID_STUB_H

#include "Common.h"
#include <functional>

class ObjectGuid
{
public:
    ObjectGuid() : _guid(0) {}
    explicit ObjectGuid(uint64 guid) : _guid(guid) {}

    bool operator==(ObjectGuid const& other) const { return _guid == other._guid; }
    bool operator!=(ObjectGuid const& other) const { return _guid != other._guid; }
    bool operator<(ObjectGuid const& other) const { return _guid < other._guid; }

    uint64 GetRawValue() const { return _guid; }

private:
    uint64 _guid;
};

namespace std
{
    template<>
    struct hash<ObjectGuid>
    {
        size_t operator()(ObjectGuid const& guid) const
        {
            return hash<uint64>()(guid.GetRawValue());
        }
    };
}

#endif // _OBJECT_GUID_STUB_H
