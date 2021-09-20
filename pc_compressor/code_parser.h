#pragma once

#include <vector>
#include <utility>
#include <functional>

#include "registers.h"

static const int kJpIxCommandLen = 2;
static const uint8_t kDecSpCode = 0x3b;
static const uint8_t kAddHlSpCode = 0x39;
static const uint8_t kLdSpHlCode = 0xf9;
static const uint8_t kExAfOpCode = 0x08;

struct z80Command
{
    int size = 0;
    int ticks = 0;
    uint8_t opCode = 0;
    uint16_t data = 0;
    uint16_t ptr = 0;
};

struct Z80CodeInfo
{
    const uint8_t* bufferBegin = nullptr;
    const uint8_t* bufferEnd = nullptr;

    int startOffset = 0;
    int endOffset = 0;
    int ticks = 0;
    int16_t spOffset = 0;
    int iySpOffset = 16;
    //std::optional<int> spDeltaOnFirstPush;
    std::vector<Register16> inputRegisters;
    std::vector<Register16> outputRegisters;
    RegUsageInfo regUsage;
    bool hasJump = false;
    std::vector<z80Command> commands;
};

class Z80Parser
{
    using BreakCondition = std::function<bool(const Z80CodeInfo&, const z80Command&)>;

public:
    static z80Command parseCommand(const uint8_t* ptr);

    static Z80CodeInfo parseCode(const Register16& af, const std::vector<uint8_t>& serializedData);
    static Z80CodeInfo parseCode(const Register16& af, const uint8_t* buffer, int size);

    static Z80CodeInfo parseCode(
        const Register16& af,
        const std::vector<Register16>& inputRegisters,
        const std::vector<uint8_t>& serializedData,
        int startOffset,
        int endOffset,
        uint16_t codeOffset,
        BreakCondition breakCondition = nullptr);

    static Z80CodeInfo parseCode(
        const Register16& af,
        const std::vector<Register16>& inputRegisters,
        const uint8_t* serializedData,
        const int serializedDataSize,
        int startOffset,
        int endOffset,
        uint16_t codeOffset,
        BreakCondition breakCondition = nullptr);

    // Return first bytes from the buffer. Round it up to op code size.
    static std::vector<uint8_t> getCode(const uint8_t* buffer, int requestedOpCodeSize);

    static std::vector<uint8_t> genDelay(int ticks);

    static const Register16* findRegByItsPushOpCode(const std::vector<Register16>& registers, uint8_t pushOpCode);

    /**
     * This function swap first two commands in the Z80 code if need.
     * It allows to make preambula more short. For example:
     * PUSH BC: LD HL,XX will generate 4 bytes preambula, but 3 bytes if swap commands.
     */
    static int swap2CommandIfNeed(
        std::vector<uint8_t>& serializedData,
        int startOffset,
        std::vector<std::pair<int, int>>& lockedBlocks);

    static int removeTrailingStackMoving(Z80CodeInfo& codeInfo, int maxCommandToRemove = std::numeric_limits<int>::max());
    static uint16_t removeTrailingCommands(Z80CodeInfo& codeInfo, int commandToRemove);
    static int removeStartStackMoving(Z80CodeInfo& codeInfo);

    static void serializeAddSpToFront(CompressedLine& line, int value);
    static void serializeAddSpToBack(CompressedLine& line, int value);

    static std::pair<RegUsageInfo, int> selfRegUsageInFirstCommands(
        const std::vector<z80Command>& command,
        std::vector<Register16>& registers,
        const Register16& af);
};

