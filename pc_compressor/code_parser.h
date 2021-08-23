#pragma once

#include <vector>
#include "registers.h"

struct z80Command
{
    int size = 0;
    int ticks = 0;
};

struct Z80CodeInfo
{
    int startOffset = 0;
    int endOffset = 0;
    int ticks = 0;
    int spDelta = 0;
    std::vector<Register16> inputRegisters;
    std::vector<Register16> outputRegisters;
    RegUsageInfo regUsage;
};

class Z80Parser
{
public:
    static z80Command parseCommand(const uint8_t* ptr);

    Z80CodeInfo parseCodeToTick(
        const std::vector<Register16>& inputRegisters,
        const std::vector<uint8_t>& serializedData,
        int startOffset,
        int endOffset,
        uint16_t codeOffset, int maxTicks,
        bool parseToLastPush);

    // Return first bytes from the buffer. Round it up to op code size.
    static std::vector<uint8_t> getCode(const uint8_t* buffer, int requestedOpCodeSize);

    static std::vector<uint8_t> genDelay(int ticks);
};

