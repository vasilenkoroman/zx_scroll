#pragma once

#include <array>
#include <set>
#include <iostream>
#include <string>
#include <vector>

struct CompressedData;

enum class Role
{
    regularBottom,
    regularTop,
    nextBottom,
    nextTop
};

struct TimingsInfo
{
    int mc = 0;
    Role role{};

    bool operator<(const TimingsInfo& other) const
    {
        if (mc != other.mc)
            return mc < other.mc;
        return (int)role < (int)other.role;
    }
};


using McToRastrInfo = std::array<std::vector<std::set<TimingsInfo>>, 8>;

McToRastrInfo calculateTimingsTable(int imageHeight, bool showLog);

int getMainMcTicks(const McToRastrInfo& info, const CompressedData& multicolor, int lineNum);
int getAltMcTicks(const McToRastrInfo& info, const CompressedData& multicolor, int lineNum);
