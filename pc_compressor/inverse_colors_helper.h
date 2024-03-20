#pragma once

#include <string>
#include <map>

struct Position
{
    int y = 0;
    int x = 0;

    bool operator<(const Position& other) const
    {
        if (y != other.y)
            return y < other.y;
        return x < other.x;
    }
};

enum class InverseResult
{
    notProcessed,
    none,
    left,
    right,
    both
};


std::map<Position, InverseResult> loadInverseColorsState(
    const std::string& fileName,
    int imageHeight);

void saveInverseColorsState(
    const std::string& fileName,
    const std::map<Position, InverseResult>& data);

std::string toString(InverseResult value);
