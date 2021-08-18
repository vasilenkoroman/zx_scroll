#pragma once

#include <vector>
#include "registers.h"

struct z80Command
{
    int size = 0;
    int ticks = 0;
    uint8_t usedReg = 0;
    uint8_t selfReg = 0;
};

struct ParseResult
{
    uint8_t* address = nullptr;
    int ticks = 0;
    int spDelta = 0;
    std::vector<Register16> inputRegisters;
    std::vector<Register16> registers;
    uint16_t lastPushAddress = 0;
    int lastPushTicks = 0;
    RegUsageInfo info;
};

class Z80Parser
{
public:
    static z80Command parseCommand(const uint8_t* ptr);

    ParseResult parseCodeToTick(
        const std::vector<Register16>& inputRegisters,
        const std::vector<uint8_t>& serializedData,
        int startOffset,
        int endOffset,
        uint16_t codeOffset, int ticks);
};
