#include "inverse_colors_helper.h"

#include <sstream>
#include <fstream>

std::string toString(InverseResult value)
{
    switch (value)
    {
        case InverseResult::left:
            return "L";
        case InverseResult::right:
            return "R";
        case InverseResult::both:
            return "B";
        case InverseResult::none:
            return "-";
        default:
            return "?";
    }
}

InverseResult inverseResultFromString(const std::string& value)
{
    if (value == "L")
        return InverseResult::left;
    if (value == "R")
        return InverseResult::right;
    if (value == "B")
        return InverseResult::both;
    if (value == "-")
        return InverseResult::none;

    return InverseResult::notProcessed;
}

std::map<Position, InverseResult> loadInverseColorsState(
    const std::string& fileName,
    int imageHeight)
{
    std::map<Position, InverseResult> result;
    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < 32; x+=2)
            result[{y,x}] = InverseResult::notProcessed;
    }

    std::ifstream stream;
    stream.open(fileName);

    std::string word;
    while (stream >> word)
    {
        if (word.size() != 7)
        {
            std::string message = std::string("Invalid word ") + word + ". Please recreate or fix file";
            throw std::exception(message.c_str());
        }

        int y = std::stoi(word.substr(0, 2));
        int x = std::stoi(word.substr(3, 2));
        auto value = inverseResultFromString(word.substr(6, 1));
        result[{y,x}] = value;
    }

    return result;
}

std::string toStr(int value)
{
    if (value < 10)
        return std::string("0") + std::to_string(value);
    return std::to_string(value);
}

void saveInverseColorsState(
    const std::string& fileName,
    const std::map<Position, InverseResult>& data)
{
    std::string buffer;

    std::ofstream stream;
    stream.open(fileName, std::ios::trunc);

    for (const auto& [key, value]: data)
    {
        stream << toStr(key.y) << "," << toStr(key.x) << ":" << toString(value);
        if (key.x < 30)
            stream << " ";
        else
            stream << "\n";
    }
    stream.flush();
}
