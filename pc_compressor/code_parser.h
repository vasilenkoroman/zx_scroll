#pragma once

#include <vector>
#include <utility>
#include "registers.h"

static const int kJpIxCommandLen = 2;

struct z80Command
{
    int size = 0;
    int ticks = 0;
    uint8_t opCode = 0;
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
    bool hasJump = false;
};

class Z80Parser
{
public:
    static z80Command parseCommand(const uint8_t* ptr);

    static Z80CodeInfo parseCode(const std::vector<uint8_t>& serializedData);
    static Z80CodeInfo parseCode(const uint8_t* buffer, int size);

    static Z80CodeInfo parseCodeToTick(
        const std::vector<Register16>& inputRegisters,
        const std::vector<uint8_t>& serializedData,
        int startOffset,
        int endOffset,
        uint16_t codeOffset, int maxTicks);

    // Return first bytes from the buffer. Round it up to op code size.
    static std::vector<uint8_t> getCode(const uint8_t* buffer, int requestedOpCodeSize);

    static std::vector<uint8_t> genDelay(int ticks);

    /**
     * This function swap first two commands in the Z80 code if need.
     * It allows to make preambula more short. For example: 
     * PUSH BC: LD HL,XX will generate 4 bytes preambula, but 3 bytes if swap commands.
     */
    static int optimizePreambula(
        std::vector<uint8_t>& serializedData,
        int startOffset,
        std::vector<std::pair<int, int>>& lockedBlocks);
};

