#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <utility>
#include <array>
#include <map>
#include <cassert>
#include <functional>
#include <string>
#include <deque>
#include <chrono>
#include <future>
#include <set>

#include "compressed_line.h"
#include "registers.h"
#include "code_parser.h"
#include "timings_helper.h"
#include "inverse_colors_helper.h"

#define LOG_INFO
//#define LOG_DEBUG

static const int kMinWorseTiming = -100000;
static const int kDefaultCodeOffset = 29900;
static const int totalTicksPerFrame = 71680;

static const uint8_t DEC_SP_CODE = 0x3b;
static const uint8_t LD_BC_CODE = 1;

static const int lineSize = 32;
static const int kScrollDelta = 1;
static const int kColorScrollDelta = 1;
static const int kMinDelay = 59;
static const int kPagesForData = 4;
static const int kSetPageTicks = 0;
static const int kPageNumPrefix = 0x50;
static const int kMinOffscreenBytes = 8; // < It contains at least 8 writted bytes to the screen
static const int kThirdSpPartSize = 4;
static const int kCallPlayerDelay = 18-10-4;

// Pages 0,1, 3,4
static const uint16_t rastrCodeStartAddrBase = 0xc000;

// Page 6
static const uint16_t kColorDataStartAddr = 0xc000;

enum Flags
{
    interlineRegisters = 1,     //< Keep registers between lines.
    verticalCompressionL = 2,   //< Skip drawing data if it exists on the screen from the previous step.
    verticalCompressionH = 4,   //< Skip drawing data if it exists on the screen from the previous step.
    oddVerticalCompression = 8, //< can skip odd drawing bytes.
    inverseColors = 16,         //< Experimental. Try to inverse data blocks for better compression.
    skipInvisibleColors = 32,   //< Don't draw invisible colors.
    optimizeLineEdge = 128,     //< merge two line borders with single SP moving block.
    updateViaHl = 512,          //< use ld(hl),reg8 after stack moving.
    OptimizeMcTicks = 1024,     //< Allow multicolor lines became shorter or longer than 224t in case of timing of the current line allows it.
    threeStackPos = 2048,       //< Draw multicolor line in 3 pieces with two intermediate stack moving.
    twoRastrDescriptors = 4096, //< Generate 3 descriptor tables instead of 1 to reduce wait ticks. More memory usage.
    updateColorData = 8192     //< Change color attribute to the same paper/inc in case of rastr data has only 0x00 or 0xff.
};

static const int kJpFirstLineDelay = 10;
static const int kLineDurationInTicks = 224;
static const int kRtMcContextSwitchDelay = 62 + 19; // multicolor to rastr context switch delay
static const int kTicksOnScreenPerByte = 4;

/**
 * The last drawing line is imageHeight-1. But the last drawing line is the imageHeight-1 + kmaxDescriptorOffset
 * Because of descriptor number calculation code doesn't do "line%imageHeight" for performance reason. So, just create a bit more descriptors
 */
static const int kmaxDescriptorOffset = 128 + 7;

struct Context
{
    int scrollDelta = 0;
    int flags = 0;
    int imageHeight = 0;
    uint8_t* buffer = nullptr;
    std::vector<bool>* maskColor = nullptr;
    std::vector<int8_t>* sameBytesCount = nullptr;
    int y = 0;
    int maxX = 31;
    int minX = 0;
    int lastOddRepPosition = 32;
    int borderPoint = 0;
    Register16 af{ "af" };

    void removeEdge()
    {
        uint8_t* line = buffer + y * 32;
        int nextLineNum = (y + scrollDelta) % imageHeight;
        uint8_t* nextLine = buffer + nextLineNum * 32;

        minX += sameBytesCount->at(y * 32 + minX);

        for (int x = maxX; x >= 0; --x)
        {
            if (sameBytesCount->at(y * 32 + x))
                --maxX;
            else
                break;
        }
    }
};

struct Labels
{
    int mt_and_rt_reach_descriptor = 0;
    int multicolor = 0;
};

void serialize(std::vector<uint8_t>& data, uint16_t value)
{
    data.push_back((uint8_t)value);
    data.push_back(value >> 8);
}

__forceinline bool isHiddenData(const uint8_t* colorBuffer, int x, int y)
{
    if (!colorBuffer)
        return false;
    const uint8_t colorData = colorBuffer[x + y * 32];
    return (colorData & 7) == ((colorData >> 3) & 7);
}

__forceinline bool isHiddenData(const std::vector<bool>* hiddenData, int x, int y)
{
    if (!hiddenData || hiddenData->empty())
        return false;
    return hiddenData->at(x + y * 32);
}

void writeTestBitmap(int w, int h, uint8_t* buffer, const std::string& bmpFileName)
{
    unsigned char* img = NULL;
    int filesize = 54 + 3 * w * h;  //w is your image width, h is image height, both int

    img = (unsigned char*)malloc(3 * w * h);
    memset(img, 0, 3 * w * h);

    for (int x = 0; x < w; ++x)
    {
        for (int y = 0; y < h; y++)
        {

            uint8_t data = buffer[y * 32 + x / 8];
            uint8_t pixelMask = 0x80 >> (x % 8);
            uint8_t pixel = (data & pixelMask) ? 0 : 0xff;

            int offset = (x + y * w) * 3;
            img[offset + 0] = pixel;
            img[offset + 1] = pixel;
            img[offset + 2] = pixel;
        }
    }

    unsigned char bmpfileheader[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0 };
    unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };
    unsigned char bmppad[3] = { 0,0,0 };

    memcpy(bmpfileheader + 2, &filesize, 4);
    memcpy(bmpinfoheader + 4, &w, 4);
    memcpy(bmpinfoheader + 8, &h, 4);

    std::ofstream imageFile;
    imageFile.open(bmpFileName, std::ios::binary);

    imageFile.write((char*)&bmpfileheader, sizeof(bmpfileheader));
    imageFile.write((char*)&bmpinfoheader, sizeof(bmpinfoheader));
    for (int i = 0; i < h; i++)
    {
        imageFile.write((char*)img + w * (h - i - 1) * 3, 3 * w);
        int bytesRest = 4 - (w * 3) % 4;
        imageFile.write((char*)&bmppad, bytesRest % 4);
    }

    free(img);
}

void mirrorBuffer8(uint8_t* buffer, int imageHeight)
{
    int dataSize = lineSize * imageHeight;

    uint8_t* bufferEnd = buffer + dataSize - 1;
    while (buffer < bufferEnd)
    {
        uint8_t value = *buffer;
        *buffer = *bufferEnd;
        *bufferEnd = value;

        --bufferEnd;
        ++buffer;
    }
}

void mirrorBuffer16(uint16_t* buffer, int imageHeight)
{
    int dataSize = lineSize * imageHeight;

    uint16_t* bufferEnd = buffer + dataSize / 2 - 1;
    while (buffer < bufferEnd)
    {
        uint16_t value = *buffer;
        *buffer = *bufferEnd;
        *bufferEnd = value;

        --bufferEnd;
        ++buffer;
    }
}

void deinterlaceBuffer(std::vector<uint8_t>& buffer)
{
    int imageHeight = buffer.size() / 32;
    std::vector<uint8_t> tempBuffer(buffer.size());
    for (int srcY = 0; srcY < imageHeight; ++srcY)
    {
        int dstY = (srcY % 8) * 8 + (srcY % 64) / 8 + (srcY / 64) * 64;
        memcpy(tempBuffer.data() + dstY * lineSize,
            buffer.data() + srcY * lineSize, lineSize); //< Copy screen line
    }
    memcpy(buffer.data(), tempBuffer.data(), imageHeight * 32);
}

void inverseColorBlock(uint8_t* colorBuffer, int x, int y)
{
    auto ptr = colorBuffer + y * 32 + x;
    int inc = *ptr & 7;
    int paper = (*ptr >> 3) & 7;
    *ptr = (*ptr & 0xc0) + (inc << 3) + paper;
}

void inversBlock(uint8_t* buffer, uint8_t* colorBuffer, int x, int y)
{
    uint8_t* ptr = buffer + y * 8 * 32 + x;
    for (int k = 0; k < 8; ++k)
    {
        ptr[0] = ~ptr[0];
        ptr += 32;
    }

    inverseColorBlock(colorBuffer, x, y);
}

int sameVerticalWorlds(uint8_t* buffer, int x, int y)
{
    if (y == 191 || y == 0)
        return 0;
    const uint16_t* buffer16 = (uint16_t*)buffer;

    int result = 0;
    for (; x < 16; ++x)
    {
        uint16_t currentWord = buffer16[y * 16 + x];
        uint16_t upperWord = buffer16[(y - 1) * 16 + x];
        uint16_t lowerWord = buffer16[(y + 1) * 16 + x];
        if (upperWord != currentWord || lowerWord != currentWord)
            return result;
        ++result;
    };
    return result;
}


template <int N, int Nested = 0>
__forceinline bool compressLine(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    int x);

template <int N, int Nested>
__forceinline bool makeChoise(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    int regIndex,
    uint16_t word,
    int x)
{
    Register16& reg = registers[regIndex];
    if (reg.h.name != 'i' && reg.isAlt != result.isAltReg)
        result.exx();

    // Word in network byte order here (swap bytes call before)
    bool canAvoidFirst = isHiddenData(context.maskColor, x, context.y / 8);
    bool canAvoidSecond = isHiddenData(context.maskColor, x + 1, context.y / 8);

    bool success;
    if (canAvoidFirst && canAvoidSecond)
        success = true;
    else if (canAvoidFirst)
        success = reg.l.updateToValue(result, (uint8_t)word, registers, context.af);
    else if (canAvoidSecond)
        success = reg.h.updateToValue(result, word >> 8, registers, context.af);
    else
        success = reg.updateToValue(result, word, registers, context.af);
    if (!success)
        return false;

    reg.push(result);

    return compressLine<N, (Nested+1)%4>(context, result, registers, x + 2);
}

uint16_t swapBytes(uint16_t word)
{
    uint8_t* ptr = (uint8_t*)&word;
    return ((uint16_t)ptr[0] << 8) + ptr[1];
}

/**
 * Some line can declare it doesn't use some reg at all. But it can be used in the next line.
 * So, check thus transitive references and mark Register as used.
*/

template <typename T>
void updateTransitiveRegUsage(T& data, int maxDepth)
{
    int size = data.size();
    for (int lineNum = 0; lineNum < data.size(); ++lineNum)
    {
        CompressedLine& line = data[lineNum];

        uint8_t selfRegMask = line.regUsage.selfRegMask;
        auto before = line.regUsage.regUseMask;
        for (int j = 1; j < maxDepth; ++j)
        {
            int nextLineNum = (lineNum + j) % size;
            CompressedLine& nextLine = data[nextLineNum];

            selfRegMask |= nextLine.regUsage.selfRegMask;
            uint8_t additionalUsage = nextLine.regUsage.regUseMask & ~selfRegMask;
            line.regUsage.regUseMask |= additionalUsage;
        }
#ifdef VERBOSE
        if (line.regUseMask != before)
            std::cout << "line #" << lineNum << " extend reg us mask from " << (int)before << " to " << (int)line.regUseMask << std::endl;
#endif
    }
}

template <int N>
struct CompressTry
{
    int extraFlags = 0;
    Context context;
    std::shared_ptr<std::array<Register16, N>> registers;
    CompressedLine line;
    std::future<bool> success;
};

template <int N>
bool compressLineMain(
    Context& context,
    CompressedLine& line,
    std::array<Register16, N>& registers,
    bool useUpdateViaHlTry = true)
{
    using TryN = CompressTry<N>;
    using RegistersN = std::array<Register16, N>;

    std::array<TryN, 3> extraCompressOptions =
    {
        TryN{0, context, std::make_shared<RegistersN>(registers)},  //< regular
        TryN{oddVerticalCompression, context, std::make_shared<RegistersN>(registers)},
        TryN{oddVerticalCompression | updateViaHl, context, std::make_shared<RegistersN>(registers)}
    };

    int bestTicks = std::numeric_limits<int>::max();
    const TryN* bestTry = nullptr;
    for (auto& option : extraCompressOptions)
    {
        if (option.extraFlags & updateViaHl)
        {
            if (!useUpdateViaHlTry)
                continue;
            if (!hasHlTries(option.context, option.line, /*x*/ option.context.minX))
                continue;
        }

        option.context.flags |= option.extraFlags;
        option.line.flags = option.context.flags;
        if (!(option.context.flags & oddVerticalCompression))
            option.context.minX = option.context.minX & ~1;

        option.success = std::async(
                [&option]()
                {
                    return compressLine<N, 0>(option.context, option.line, *option.registers,  /*x*/ option.context.minX);
                });
    }
    for (auto& option : extraCompressOptions)
    {
        if (!option.success.valid())
            continue;

        option.success.wait();
        if (option.success.get() && option.line.drawTicks <= bestTicks)
        {
            bestTry = &option;
            bestTicks = option.line.drawTicks;
        }
    }

    line = bestTry->line;
    line.inputRegisters = std::make_shared<std::vector<Register16>>();
    for (const auto& reg16 : registers)
        line.inputRegisters->push_back(reg16);
    //context = bestTry->context;
    context = bestTry->context;
    if (context.flags & interlineRegisters)
    {
        registers = *bestTry->registers;

        // Interline registers is not supported for 'af'.
        // To common 'a' value across all lines context.af is used.
        if (auto af = findRegister(registers, "af"))
            af->reset();
    }
    line.inputAf = std::make_shared<Register16>(context.af);
    return true;
}

static Register16 sp("sp");

template <int N, int Nested>
__forceinline void choiseNextRegister(
    CompressedLine& choisedLine,
    std::array<Register16, N>& chosedRegisters,
    const Context& context,
    const CompressedLine& currentLine,
    const std::array<Register16, N>& registers,
    const uint16_t word,
    const int x)
{
    int choisedIndex = -1;
    for (int regIndex = 0; regIndex < registers.size(); ++regIndex)
    {
        auto regCopy = registers;

        CompressedLine newLine;
        newLine.isAltReg = currentLine.isAltReg;
        newLine.isAltAf = currentLine.isAltAf;
        newLine.regUsage = currentLine.regUsage;

        if (!makeChoise<N, Nested>(context, newLine, regCopy, regIndex, word, x))
            continue;

        bool useNextLine = choisedLine.data.empty() || newLine.drawTicks < choisedLine.drawTicks;
        if (useNextLine)
        {
            chosedRegisters = regCopy;
            choisedLine = newLine;
            choisedIndex = regIndex;
        }
    }
}

template <int N>
bool loadWordFromExistingRegister(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    const uint16_t word, const int x)

{
    bool canAvoidFirst = isHiddenData(context.maskColor, x, context.y / 8);
    bool canAvoidSecond = isHiddenData(context.maskColor, x + 1, context.y / 8);

    if (canAvoidFirst && canAvoidSecond)
    {
        context.af.push(result);
        return true;
    }

    for (int run = 0; run < 2; ++run)
    {
        for (auto& reg : registers)
        {
            bool condition = result.isAltReg == reg.isAlt;
            if (run == 1)
                condition = !condition;
            if (!condition)
                continue;

            if (reg.hasValue16(word, canAvoidFirst, canAvoidSecond))
            {
                if (result.isAltReg != reg.isAlt)
                    result.exx();
                reg.push(result, canAvoidFirst, canAvoidSecond);
                return true;
            }
        }
    }

    if (context.af.hasValue16(word, canAvoidFirst, canAvoidSecond))
    {
        if (context.af.isAltAf != result.isAltAf)
            result.exAf();
        context.af.push(result);
        return true;
    }

    return false;
}

template <int N>
bool hasWordFromExistingRegisterExceptHl(
    const Context& context,
    const CompressedLine& line,
    std::array<Register16, N>& registers,
    const uint16_t word, const int x)

{
    bool canAvoidFirst = isHiddenData(context.maskColor, x, context.y / 8);
    bool canAvoidSecond = isHiddenData(context.maskColor, x + 1, context.y / 8);

    for (int run = 0; run < 2; ++run)
    {
        for (auto& reg : registers)
        {
            bool condition = line.isAltReg == reg.isAlt;
            if (run == 1)
                condition = !condition;
            if (!condition)
                continue;
            if (reg.h.name == 'h')
                continue;
            if (reg.hasValue16(word, canAvoidFirst, canAvoidSecond))
            {
                return true;
            }
        }
    }

    if (context.af.hasValue16(word, canAvoidFirst, canAvoidSecond))
    {
        return true;
    }

    return false;
}

template <int N>
bool loadByteFromExistingRegisterExceptHl(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    const uint8_t byte, const int x)

{
    for (auto& reg : registers)
    {
        if (result.isAltReg != reg.isAlt)
            continue;
        if (reg.h.hasValue(byte))
        {
            reg.h.pushViaHL(result);
            return true;
        }
        else if (reg.l.hasValue(byte))
        {
            reg.l.pushViaHL(result);
            return true;
        }
    }
    if (context.af.h.hasValue(byte) && result.isAltAf == context.af.isAltAf)
    {
        context.af.h.pushViaHL(result);
        return true;
    }

    return false;
}

bool loadConstByte(
    const Context& context,
    CompressedLine& result,
    const uint8_t byte, const int x)
{
    z80Command lodHlNn;
    lodHlNn.size = 2;
    lodHlNn.opCode = 0x36;
    lodHlNn.data = byte;
    lodHlNn.ticks = 10;

    result.appendCommand(lodHlNn);

    return false;
}

template <int N>
bool hasByteFromExistingRegisterExceptHl(
    const Context& context,
    const CompressedLine& line,
    std::array<Register16, N>& registers,
    const uint8_t byte)

{
    for (auto& reg : registers)
    {
        if (reg.h.name == 'h')
            continue;

        if (line.isAltReg != reg.isAlt)
            continue;
        if (reg.h.hasValue(byte))
        {
            return true;
        }
        else if (reg.l.hasValue(byte) && reg.l.name != 'f')
        {
            return true;
        }
    }
    if (context.af.isAltAf == line.isAltAf && context.af.h.hasValue(byte))
    {
        return true;
    }

    return false;
}

template <int N>
bool willWriteByteViaHl(
    const Context& context,
    const CompressedLine& line,
    std::array<Register16, N>& registers,
    int x,
    int verticalRepCnt)
{
    int newX = x + verticalRepCnt;
    if (newX > 31)
        return false;

    int index = context.y * 32 + x + verticalRepCnt;
    bool canUseOddPos = (context.flags & oddVerticalCompression) && newX < context.lastOddRepPosition;
    if (!canUseOddPos)
        return false;

    if (context.sameBytesCount->at(index))
        return false;

    if (newX < 31 & newX < context.maxX)
    {
        uint16_t* buffer16 = (uint16_t*)(context.buffer + index);
        uint16_t word = *buffer16;
        word = swapBytes(word);
        if (x < context.maxX && hasWordFromExistingRegisterExceptHl(context, line, registers, word, newX))
            return false;
    }

    if (newX < context.maxX)
        //&& hasByteFromExistingRegisterExceptHl(context, line, registers, context.buffer[index]))
    {
        return true;
    }
    return false;
}

template <int N, int Nested>
__forceinline bool compressLine(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    int x)
{
    bool awaitingLoadFromHl = false;

    while (x <= context.maxX)
    {
        bool canUseOddPos = (context.flags & oddVerticalCompression) && x < context.lastOddRepPosition;

        const int index = context.y * 32 + x;
        int verticalRepCount = context.sameBytesCount ? context.sameBytesCount->at(index) : 0;
        verticalRepCount = std::min(verticalRepCount, context.maxX - x + 1);
        if (verticalRepCount)
        {
            if (x + verticalRepCount == 31)
                --verticalRepCount;
            else
                if (!canUseOddPos)
                    verticalRepCount &= ~1;
        }

        if (context.borderPoint && x == context.borderPoint)
        {
            result.spPosHints[0] = result.data.size();
            if (verticalRepCount > 0)
                x += verticalRepCount;
            sp.loadXX(result, 32 - (x - context.borderPoint));
            if (verticalRepCount > 0)
                continue;
        }

        uint16_t* buffer16 = (uint16_t*)(context.buffer + index);
        uint16_t word = *buffer16;
        word = swapBytes(word);

        assert(x < context.maxX + 1);

        if (context.borderPoint && x < context.borderPoint)
        {
            if (x == context.borderPoint - 1 && !verticalRepCount)
                return false;
            if (x + verticalRepCount >= context.borderPoint)
            {
                x += verticalRepCount;
                if (x  > context.borderPoint)
                {
                    result.spPosHints[0] = result.data.size();
                    sp.loadXX(result, 32 - (x - context.borderPoint));
                }
                continue;
            }
        }

        // Decrement stack if line has same value from previous step (vertical compression)
        // Up to 4 bytes is more effetient to decrement via 'DEC SP' call.
        if (verticalRepCount > 4)
        {

            if (auto hl = findRegister(registers, "hl", result.isAltReg))
            {
                if (result.isAltAf)
                    result.exAf();

                int spDelta = -verticalRepCount;
                if (context.flags & updateViaHl)
                {
                    if (willWriteByteViaHl(context, result, registers, x, verticalRepCount))
                    {
                        --spDelta;
                        awaitingLoadFromHl = true;
                    }
                }

                hl->updateToValue(result, spDelta, registers, context.af);
                hl->addSP(result);
                if (auto f = findRegister8(registers, 'f'))
                    f->value.reset();
                sp.loadFromReg16(result, *hl);
                x += verticalRepCount;
                continue;
            }
        }

        // push existing 16 bit value.
        bool loadExistingChecked = false;
        if (x < context.maxX
            && (x % 2 == 0 || verticalRepCount > 1 && (x + verticalRepCount) % 2 == 0))
        {
            if (loadWordFromExistingRegister(context, result, registers, word, x))
            {
                if (awaitingLoadFromHl)
                    abort();
                x += 2;
                continue;
            }
            loadExistingChecked = true;
        }

        // Decrement stack if line has same value from previous step (vertical compression)
        if (verticalRepCount & 1)
        {
            sp.decValue(result, verticalRepCount);
            x += verticalRepCount;
            if (awaitingLoadFromHl)
                abort();
            continue;
        }

        // push existing 16 bit value.
        if (x < 31 && x < context.maxX && !loadExistingChecked && loadWordFromExistingRegister(context, result, registers, word, x))
        {
            if (awaitingLoadFromHl)
                abort();
            x += 2;
            continue;
        }

        if (awaitingLoadFromHl)
        {
            bool loaded = loadByteFromExistingRegisterExceptHl(context, result, registers, context.buffer[index], x);
            if (!loaded)
                loadConstByte(context, result, context.buffer[index], x);
            ++x;
            awaitingLoadFromHl = false;
            continue;
        }

        if (awaitingLoadFromHl)
            abort();

        if (x == 31 && !verticalRepCount) //< 31, not maxX here
            return false;


        // Decrement stack if line has same value from previous step (vertical compression)
        if (verticalRepCount > 0)
        {
            sp.decValue(result, verticalRepCount);
            x += verticalRepCount;
            continue;
        }

        CompressedLine choisedLine;
        auto chosedRegisters = registers;

        choiseNextRegister<N, Nested>(choisedLine, chosedRegisters, context, result, registers, word, x);

        if ((context.flags & oddVerticalCompression) && choisedLine.data.empty() && x % 2 == 0)
        {
            // try to reset flags for the rest of the line
            Context contextCopy(context);
            contextCopy.flags &= ~oddVerticalCompression;
            choiseNextRegister<N, Nested>(choisedLine, chosedRegisters, contextCopy, result, registers, word, x);
            choisedLine.lastOddRepPosition = x;
        }

        if (choisedLine.data.empty())
            return false;

        if (choisedLine.spPosHints[0] >= 0)
            result.spPosHints[0] = result.data.size() + choisedLine.spPosHints[0];

        result += choisedLine;
        result.isAltReg = choisedLine.isAltReg;
        result.isAltAf = choisedLine.isAltAf;
        result.lastOddRepPosition = choisedLine.lastOddRepPosition;
        registers = chosedRegisters;

        return true;
    }

    if (context.flags & oddVerticalCompression)
        result.lastOddRepPosition = context.maxX;
    if (result.isAltAf)
        result.exAf();
    return true;
}

bool hasHlTries(
    const Context& context,
    CompressedLine& result,
    int x)
{
    while (x < context.maxX)
    {
        const int index = context.y * 32 + x;
        int verticalRepCount = context.sameBytesCount->at(index);
        if (verticalRepCount > 4)
            return true;
        x += std::max(1, verticalRepCount);
    }
    return false;
}

template <int N>
std::vector<CompressedLine> compressLines(
    std::array<Register16, N>& registers,
    const Context & context, const std::vector<int> & lines, int transitiveDepth)
{

    std::vector<CompressedLine> result;
    for (const auto line : lines)
    {
        Context ctx = context;
        ctx.y = line;

        if (ctx.flags & optimizeLineEdge)
            ctx.removeEdge();

        CompressedLine line;
        auto registers1 = registers;

        if ((ctx.flags & optimizeLineEdge) && !result.empty())
        {
            auto& prevLine = *result.rbegin();
            // Real moving can be 1 bytes less if there is no oddCompression flag and minX is odd.
            int expectedStackMovingAtStart = ctx.minX + (32 - prevLine.maxX);
            if (expectedStackMovingAtStart >= 5)
            {
                auto hl = findRegister(registers1, "hl");
                hl->reset();
            }
        }

        compressLineMain(ctx, line, registers1);
        if (context.flags & interlineRegisters)
            registers = registers1;


        if (ctx.flags & optimizeLineEdge)
        {
            Z80Parser parser;
            auto info = parser.parseCode(
                context.af,
                *line.inputRegisters,
                line.data.buffer(), line.data.size(),
                0, line.data.size(), 0);

            line.minX = ctx.minX;
            line.maxX = ctx.minX - info.spOffset;

            if (!result.empty())
            {
                auto& prevLine = *result.rbegin();
                int stackMovingAtStart = ctx.minX + (32 - prevLine.maxX);

                if (stackMovingAtStart >= 5 && line.minX > 0)
                {
                    Z80Parser::serializeAddSpToFront(line, stackMovingAtStart);
                    line.stackMovingAtStart = stackMovingAtStart;
                }
                else if (stackMovingAtStart > 0)
                {
                    Z80Parser::serializeAddSpToFront(line, line.minX);
                    Z80Parser::serializeAddSpToBack(prevLine, stackMovingAtStart - line.minX);
                    line.stackMovingAtStart = line.minX;
                }
            }
        }

        result.push_back(line);
    }

    if (context.flags & optimizeLineEdge)
    {
        auto& line = result[0];
        auto& prevLine = *result.rbegin();

        int stackMovingAtStart = line.minX + (32 - prevLine.maxX);
        if (stackMovingAtStart >= 5 && line.minX > 0)
        {
            Z80Parser::serializeAddSpToFront(line, stackMovingAtStart);
            line.stackMovingAtStart = stackMovingAtStart;
        }
        else if (stackMovingAtStart > 0)
        {
            Z80Parser::serializeAddSpToFront(line, line.minX);
            Z80Parser::serializeAddSpToBack(prevLine, stackMovingAtStart - line.minX);
            line.stackMovingAtStart = line.minX;
        }
    }


    updateTransitiveRegUsage(result, transitiveDepth);
    return result;
}

std::vector<CompressedLine> compressLines(const Context& context, const std::vector<int>& lines, int transitiveDepth)
{
    std::array<Register16, 3> registers = { Register16("hl"), Register16("de"), Register16("bc") };
    return compressLines(registers, context, lines, transitiveDepth);
}

std::future<std::vector<CompressedLine>> compressLinesAsync(const Context& context, const std::vector<int>& lines)
{
    return std::async(
        [context, lines]()
        {
            return compressLines(context, lines, /*maxTransitiveDepth*/ 8);
        }
    );
}

std::vector<bool> removeInvisibleColors(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    std::vector<bool> result;
    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            bool isHidden = isHiddenData(colorBuffer, x, y);
            result.push_back(isHidden);
        }
    }

    return result;
}

#if 1
void cleanupColors(uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    for (int y = 1; y < imageHeight; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            uint8_t* colorPtr = colorBuffer + y * 32 + x;
            uint8_t* prevColorPtr = colorBuffer + (y - 1) * 32 + x;
            uint8_t* rastrPtr = buffer + y * 8 * 32 + x;
            bool isOnlyInc = true;
            bool isOnlyPaper = true;
            for (int k = 0; k < 8; ++k)
            {
                if (rastrPtr[k * 32] != 255)
                    isOnlyInc = false;
                if (rastrPtr[k * 32] != 0)
                    isOnlyPaper = false;
            }

            uint8_t prevInc = prevColorPtr[0] & 0x7;
            uint8_t currentInc = colorPtr[0] & 0x7;
            uint8_t prevPaper = (prevColorPtr[0] & 0x38) >> 3;
            uint8_t currentPaper = (colorPtr[0] & 0x38) >> 3;

            if (isOnlyPaper)
            {
                if (prevInc != currentInc)
                    colorPtr[0] = (colorPtr[0] & ~7) + prevInc;
                else
                {
                    colorPtr[0] = colorPtr[0] & ~7;
                    //colorPtr[0] = (colorPtr[0] & ~7) + currentPaper;
                    int gg = 4;
                }
            }
            else if (isOnlyInc)
            {
                if (prevPaper != currentPaper)
                {
                    colorPtr[0] = (colorPtr[0] & ~0x38) + (prevPaper << 3);
                }
                else
                {
                    //colorPtr[0] = (colorPtr[0] & ~0x38) + (currentInc << 3);
                    colorPtr[0] = colorPtr[0] & ~0x38;
                    int gg = 4;
                }

            }
        }
    }
}
#else
void makePaperAndIncSameIfNeed(uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    for (int y = 0; y < imageHeight; y += 8)
    {
        for (int x = 0; x < 32; ++x)
        {
            uint8_t* ptr = buffer + y * 32 + x;
            bool sameIncAndPaper = ptr[0] == 0 || ptr[0] == 0xff;
            for (int i = 0; i < 256; i += 32)
            {
                if (ptr[i] != ptr[0])
                {
                    sameIncAndPaper = false;
                    break;
                }
            }
            if (sameIncAndPaper)
            {
                int colorLine = y / 8;
                int prevLine = colorLine > 0 ? colorLine - 1 : imageHeight / 8 - 1;
                int nextLine = (colorLine + 1) % (imageHeight / 8);
                uint8_t* colorPtr = colorBuffer + colorLine * 32 + x;

                //if (colorPtr[0] == colorBuffer[prevLine * 32 + x] || colorPtr[0] == colorBuffer[nextLine * 32 + x])
                //    continue;

                uint8_t inc = colorPtr[0] & 0x7;
                uint8_t paper = (colorPtr[0] >> 3) & 0x7;
                if (inc != paper)
                {
                    if (ptr[0] == 0xff)
                    {
                        paper = inc;
                        colorPtr[0] &= ~0x3f;
                        colorPtr[0] |= inc + (paper << 3);
                    }
                    else
                    {
                        inc = paper;
                        colorPtr[0] &= ~0x3f;
                        colorPtr[0] |= inc + (paper << 3);
                    }
                }
            }
        }
    }
}
#endif

Register16 findBestByte(uint8_t* buffer, int imageHeight, const std::vector<int8_t>* sameBytesCount = nullptr, int* usageCount = nullptr)
{
    Register16 af("af");
    std::map<uint8_t, int> byteCount;
    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < 32;)
        {
            int index = y * 32 + x;
            if (sameBytesCount)
            {
                int reps = sameBytesCount->at(index);
                if (reps > 0)
                {
                    x += reps;
                    continue;
                }
            }

            ++byteCount[buffer[index]];
            ++x;
        }
    }

    int bestByteCounter = 0;
    for (const auto& [byte, count] : byteCount)
    {
        if (count > bestByteCounter)
        {
            bestByteCounter = count;
            af.h.value = byte;
        }
    }
    if (usageCount)
        *usageCount = bestByteCounter;
    af.l.value = af.h.value;
    return af;
}

CompressedData compressImageAsync(int flags, uint8_t* buffer, std::vector<bool>* maskColors,
    std::vector<int8_t>* sameBytesCount, int imageHeight)
{
    CompressedData compressedData;


    Context context;
    context.scrollDelta = kScrollDelta;
    context.flags = flags;
    context.imageHeight = imageHeight;
    context.buffer = buffer;
    context.maskColor = maskColors;
    context.sameBytesCount = sameBytesCount;
    //context.af = findBestByte(buffer, imageHeight, context.sameBytesCount);

    std::vector<std::future<std::vector<CompressedLine>>> compressors(8);

    for (int i = 0; i < 8; ++i)
    {
        int pageNum = i / 2;
        if (pageNum >= 2)
            ++pageNum;
        context.af.h.value = kPageNumPrefix + pageNum;
        std::vector<int > lines;
        for (int y = 0; y < imageHeight; y += 8)
            lines.push_back(y + i);
        compressors[i] = compressLinesAsync(context, lines);
    }
    for (auto& compressor : compressors)
    {
        auto compressedLines = compressor.get();
        compressedData.data.insert(compressedData.data.end(),
            compressedLines.begin(), compressedLines.end());
    }
    return compressedData;
}


int sameVerticalBytes(int flags, int scrollDelta, const uint8_t* buffer,
    std::vector<bool>* maskColor, int x, int y, int imageHeight)
{
    int result = 0;
    if (!(flags & (verticalCompressionL | verticalCompressionH)))
        return 0;

    const uint8_t* ptr = buffer + y * 32 + x;
    for (; x < 32; ++x)
    {
        uint8_t currentByte = *ptr;
        if (!isHiddenData(maskColor, x, y / 8))
        {
            if (flags & verticalCompressionH)
            {
                int nextLine = y - scrollDelta;
                if (nextLine < 0)
                    nextLine += imageHeight;
                auto ptr = buffer + nextLine * 32 + x;
                if (*ptr != currentByte)
                    return result;

                if (isHiddenData(maskColor, x, nextLine / 8))
                    return result; //< There is any byte in invisible color. draw again.
            }
            if (flags & verticalCompressionL)
            {
                int nextLine = (y + scrollDelta) % imageHeight;
                auto ptr = buffer + nextLine * 32 + x;
                if (*ptr != currentByte)
                    return result;

                if (isHiddenData(maskColor, x, nextLine / 8))
                    return result; //< There is any byte in invisible color. draw again.
            }
        }
        ++result;
        ++ptr;
    };
    return result;
}

std::vector<int8_t> createSameBytesTable(int flags, const uint8_t* buffer,
    std::vector<bool>* maskColor, int imageHeight)
{
    std::vector<int8_t> result;
    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < 32; ++x)
            result.push_back(sameVerticalBytes(flags, kScrollDelta, buffer, maskColor, x, y, imageHeight));
    }
    return result;
}


void inverseImageIfNeed(uint8_t* buffer, uint8_t* colorBuffer)
{
    // TODO: not checked yet
    return;

    auto af = findBestByte(buffer, 192);
    if (af.h.value == 255)
    {
        for (int i = 0; i < 6144; ++i)
        {
            *buffer = *buffer ^ 255;
            ++buffer;
        }
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 32; ++x)
            {
                inverseColorBlock(colorBuffer, x, y);
            }
        }
    }
}

int sameBytesWithNextBlock(int flags, uint8_t* buffer, int x, int y, int imageHeight)
{
    uint8_t* ptr = buffer + y * 32 + x;
    int result = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (ptr[0] == ptr[1])
            ++result;
        ptr += 32;
    }

    return result;
}

CompressedData compressRastr(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    std::vector<bool> maskColor;
    if (flags & skipInvisibleColors)
        maskColor = removeInvisibleColors(flags, buffer, colorBuffer, imageHeight);

    std::vector<int8_t> sameBytesCount = createSameBytesTable(flags, buffer, &maskColor, imageHeight);
    CompressedData result = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);
    result.sameBytesCount = sameBytesCount;


    if (!(flags & inverseColors))
        return result;

    return result;

    std::cout << "rastr ticks = " << result.ticks() << std::endl;

    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            std::cout << "Check block " << y << ":" << x << std::endl;

            int lineNum = y * 8;

            inversBlock(buffer, colorBuffer, x, y);
            auto candidateLeft = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

            inversBlock(buffer, colorBuffer, x + 1, y);
            auto candidateBoth = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

            inversBlock(buffer, colorBuffer, x, y);
            auto candidateRight = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);
            inversBlock(buffer, colorBuffer, x + 1, y);

            const int resultTicks = result.ticks();
            if (candidateLeft.ticks() < resultTicks
                && candidateLeft.ticks() < candidateBoth.ticks()
                && candidateLeft.ticks() < candidateRight.ticks())
            {
                result = candidateLeft;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl; colorBuffer,
                    inversBlock(buffer, colorBuffer, x, y);
            }
            else if (candidateBoth.ticks() < resultTicks
                && candidateBoth.ticks() < candidateLeft.ticks()
                && candidateBoth.ticks() < candidateRight.ticks())
            {
                result = candidateBoth;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;
                inversBlock(buffer, colorBuffer, x, y);
                inversBlock(buffer, colorBuffer, x + 1, y);
            }
            else if (candidateRight.ticks() < resultTicks
                && candidateRight.ticks() < candidateLeft.ticks()
                && candidateRight.ticks() < candidateBoth.ticks())
            {
                result = candidateRight;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;
                inversBlock(buffer, colorBuffer, x + 1, y);
            }
        }
    }
    return result;
}

void finalizeLine(
    const Context& context,
    CompressedLine& result,
    const CompressedLine& loadLine,
    const CompressedLine& pushLine)
{
    Register16 sp("sp");
    Register16 iy("iy");

    // Left part is exists
    if (context.minX > 0)
    {
        int delta = context.minX < 16 ? context.minX : context.minX - 32;
        Z80Parser::serializeAddSpToFront(result, delta);
    }

    result += loadLine;
    int preloadTicks = result.drawTicks;

    //result.maxDrawDelayTicks = pushLine.maxDrawDelayTicks;
    result.mcStats.max = (64 - preloadTicks) + pushLine.mcStats.max;

    {
        Z80Parser parser;
        std::vector<Register16> registers = {
            Register16("bc"), Register16("de"), Register16("hl"),
            Register16("bc'"), Register16("de'"), Register16("hl'")
        };

        for (int i = 0; i < pushLine.spPosHints.size(); ++i)
        {
            if (pushLine.spPosHints[i] >= 0)
                result.spPosHints[i] = result.data.size() + pushLine.spPosHints[i];
        }
        result += pushLine;
    }


    // Calculate line min and max draw delay (point 0 is the ray in the begin of a line)

    std::vector<Register16> registers = {
        Register16("bc"), Register16("de"), Register16("hl"),
        Register16("bc'"), Register16("de'"), Register16("hl'")
    };
    Z80Parser parser;
    int extraDelay = std::numeric_limits<int>::min();
    int maxRayDelay = std::numeric_limits<int>::max();
    auto timingInfo = parser.parseCode(
        context.af,
        registers,
        result.data.buffer(),
        result.data.size(),
        /* start offset*/ 0,
        /* end offset*/ result.data.size(),
        /* codeOffset*/ 0,
        [&](const Z80CodeInfo& info, const z80Command& command)
        {
            if (!Z80Parser::isPushOpCode(command.opCode))
                return false;

            int rightBorder = 32;
            static const int kInitialSpOffset = 16;
            int rayTicks = (kInitialSpOffset + info.spOffset) * kTicksOnScreenPerByte;

            int delay = rayTicks - info.ticks;
            extraDelay = std::max(extraDelay, delay);

            if (info.ticks >= rayTicks)
            {
                delay = rayTicks + 224 - 8 - (info.ticks + command.ticks);
                if (info.spOffset == 2 && context.maxX < 31)
                    delay += 8; //< Last byte of a line is not updated. Can override it under ray.
                maxRayDelay = std::min(maxRayDelay, delay);
            }

            return false;
        });

    result.mcStats.min = extraDelay;
    if (maxRayDelay < result.mcStats.max)
        result.mcStats.max = maxRayDelay;
}

int getDrawTicks(const CompressedLine& pushLine)
{
    int drawTicks = pushLine.drawTicks;
    if (pushLine.data.last() == kExAfOpCode)
        drawTicks -= 4;
    return drawTicks;
}

void addSameByteToTable(std::vector<int8_t>& sameBytesCount, int y, int x)
{
    int leftBorder = x;
    while (leftBorder > 0 && sameBytesCount[y * 32 + leftBorder - 1] > 0)
        --leftBorder;
    int rightBorder = x;
    while (rightBorder < 31 && sameBytesCount[y * 32 + rightBorder + 1] > 0)
        ++rightBorder;

    int count = rightBorder - leftBorder + 1;
    for (int i = leftBorder; i <= rightBorder; ++i)
    {
        sameBytesCount[y * 32 + i] = count;
        --count;
    }
}

void swapRegs(Register16& a, Register16& b)
{
    uint16_t tmp = a.value16();
    a.setValue(b.value16());
    b.setValue(tmp);
}

std::tuple<CompressedLine, std::array<Register16, 6>> makeLoadLine(
    const Context& context, const CompressedLine& line1)
{
    auto hasSameValue =
        [](const auto& registers, uint16_t word)
    {
        for (auto& reg : registers)
        {
            if (reg.hasValue16(word))
                return true;
        }
        return false;
    };

    std::array<Register16, 6> registers6 = {
        Register16("bc"), Register16("de"), Register16("hl"),
        Register16("bc'"), Register16("de'"), Register16("hl'")
    };

    CompressedLine loadLineAlt;
    CompressedLine loadLineMain;
    Z80Parser parser;
    auto info = parser.parseCode(
        context.af,
        *line1.inputRegisters,
        line1.data.buffer(),
        line1.data.size(),
        /* start offset*/ 0,
        /* end offset*/ line1.data.size(),
        /* codeOffset*/ 0,
        [&](const Z80CodeInfo& info, const z80Command& command)
        {
            const auto inputReg = Z80Parser::findRegByItsPushOpCode(info.outputRegisters, command.opCode);
            if (!inputReg)
                return false;

            if (!hasSameValue(registers6, inputReg->value16()))
            {
                for (int i = 0; i < registers6.size(); ++i)
                {
                    if (registers6[i].isEmpty())
                    {
                        auto& line = i < 3 ? loadLineMain : loadLineAlt;
                        if (context.minX == 0)
                        {
                            // It takes 64 ticks to load 6 registers, dont compress here because it needs 64 ticks for ray
                            registers6[i].loadXX(line, inputReg->value16());
                        }
                        else
                        {
                            auto prevRegs = i < 3
                                ? std::array<Register16, 3> { registers6[0], registers6[1], registers6[2] }
                            : std::array<Register16, 3> { registers6[3], registers6[4], registers6[5] };
                            registers6[i].updateToValue(line, inputReg->value16(), prevRegs, context.af);
                        }
                        break;
                    }
                }
            }

            return false; //< Don't break.
        });

    CompressedLine loadLine;
    loadLine += loadLineAlt;
    loadLine.exx();
    loadLine += loadLineMain;

    return { loadLine, registers6 };
}

CompressedLine  compressMultiColorsLine(Context srcContext)
{
    /*
     *      Screen in words [0..15]
     *       0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
     *      -------------------------------------------------
     *      |1 |1 |1 |1 |1 |1 |1 |1 |2 |2 |2 |2 |2 |2 |2 |2 |
     *      -------------------------------------------------
     *      Drawing step (1..2): 8, 8 words
     *
     *      Input params: SP should point to the lineStart + 16 (8 word),
     *      IY point to the line end.
     *      For the most complicated lines it need 3 interval (0..8, 8..13, 13..16)
     *      but it take more time in general for preparation.
     */

    CompressedLine result;
    Context context = srcContext;

    std::vector<int8_t> sameBytesCopy;
    if (context.flags & threeStackPos)
    {
        sameBytesCopy = *context.sameBytesCount;
        for (int x = 16; x < 16 + kThirdSpPartSize; ++x)
            addSameByteToTable(*context.sameBytesCount, context.y, x);
    }

    context.removeEdge(); //< Fill minX, maxX

    //try 1. Use 3 registers, no intermediate stack correction, use default compressor

    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
    CompressedLine line1;

    // This code calculate expected position for register values.
    // It could fail with this flag. It need to update calculator if use it with this flag.
    bool success = compressLineMain(context, line1, registers, /*updateViaHlTry*/ false);
    if (!success)
    {
        std::cerr << "Can't compress multicolor line " << context.y << " It should not be. Some bug." << std::endl;
        abort();
    }

    context.lastOddRepPosition = line1.lastOddRepPosition;
    context.flags = line1.flags;

    if (!(context.flags & oddVerticalCompression))
        context.minX &= ~1;

    //try 2. Prepare input registers manually, use 'oddVerticalRep' information from auto compressor


    auto [loadLine, registers6] = makeLoadLine(context, line1);

    // 2.4 start compressor with prepared register values


    CompressedLine pushLine;
    auto regCopy = registers6;
    success = compressLine<regCopy.size(), 0>(context, pushLine, regCopy,  /*x*/ context.minX);
    if (!success)
    {
        std::cerr << "ERROR: unexpected error during compression multicolor line " << context.y
            << " (something wrong with oddVerticalCompression flag). It should not be! Just a bug." << std::endl;
        abort();
    }


    //static const int kBorderTime = kLineDurationInTicks - 128;
    //int t1 = kBorderTime + (context.minX + 31 - context.maxX) * 4; // write whole line at once limit (no intermediate stack moving)
    int t2 = kLineDurationInTicks; // write whole line in 2 tries limit
    t2 += (31 - context.maxX) * kTicksOnScreenPerByte;
    int drawTicks = getDrawTicks(pushLine);
    pushLine.mcStats.max = t2 - drawTicks;

    if (!(context.flags & Flags::threeStackPos))
    {
        if (drawTicks > t2)
        {
            #ifdef DEBUG
                std::cout << "DEBUG: Line " << context.y << ". Not enough " << drawTicks - t2 << " ticks for 2 stack pos. Switching to 3 stack" << std::endl;
            #endif
            srcContext.flags |= Flags::threeStackPos;
            return compressMultiColorsLine(srcContext);
        }
    }
    else
    {

        if (drawTicks > t2)
        {
            std::cerr << "ERROR: Line " << context.y << ". Not enough " << drawTicks - t2 << " ticks for first 2 piece in 3 piece mode. " << std::endl;
            //abort();
        }

        // add 3-th piece
        *context.sameBytesCount = sameBytesCopy;
        pushLine.spPosHints[1] = pushLine.data.size();
        sp.loadXX(pushLine, 32);

        Context context3 = context;
        context3.minX = 16;
        context3.maxX = context3.minX + kThirdSpPartSize - 1;
        context3.borderPoint = -1;
        context3.removeEdge();
        context3.flags &= ~oddVerticalCompression;
        // Updating minX not supported now. Only maxX is supported here. It is because save ticks in non MC mode while preparing MC drawing.
        context3.minX = 16;

        if (pushLine.isAltReg)
        {
            swapRegs(regCopy[0], regCopy[3]);
            swapRegs(regCopy[1], regCopy[4]);
            swapRegs(regCopy[2], regCopy[5]);
        }

        CompressedLine line3;
        success = compressLineMain(context3, line3, regCopy, /*updateViaHlTry*/ false);
        if (!success)
        {
            std::cerr << "Can't compress multicolor line " << context.y << " It should not be. Some bug." << std::endl;
            abort();
        }

        pushLine += line3;
        int drawTicks2 = getDrawTicks(pushLine);

        int t3 = t2 + 48;
        if (drawTicks2 > t3)
        {
            std::cerr << "ERROR: Line " << context.y << ". Not enough " << drawTicks2 - t3 << " ticks in 3 piece mode. " << std::endl;
            //abort();
        }
        pushLine.mcStats.max = std::min(pushLine.mcStats.max, t3 - drawTicks2);
    }


    finalizeLine(context, result, loadLine, pushLine);
    result.inputAf = std::make_shared<Register16>(context.af);

    if (result.spPosHints[0] >= 0)
    {
        uint16_t* value = (uint16_t*)(result.data.buffer() + result.spPosHints[0] + 1);
        if (*value > 32 || *value < 1)
        {
            std::cerr << "Unsupported spPos value " << *value << std::endl;
            abort();
        }
    }

    return result;
}


Register16 findBestWord(uint8_t* buffer, int imageHeight, const std::vector<int>& sameBytesCount, int flags, int* usageCount)
{
    Register16 af("af");
    std::map<uint16_t, int> wordCount;
    for (int y = 0; y < 24; ++y)
    {
        for (int x = 0; x < 31;)
        {
            int index = y * 32 + x;
            int reps = sameBytesCount[index];
            if (flags & oddVerticalCompression)
                reps &= ~1;
            if (reps > 0)
            {
                x += reps;
                continue;
            }

            uint16_t* buffer16 = (uint16_t*)(buffer + index);
            uint16_t word = *buffer16;
            word = swapBytes(word);
            ++wordCount[word];
            x += 2;
        }
    }

    *usageCount = 0;
    for (const auto& [word, count] : wordCount)
    {
        if (count > * usageCount)
        {
            *usageCount = count;
            af.setValue(word);
        }
    }

    return af;
}

void markByteAsSame(std::vector<int8_t>& rastrSameBytes, int y, int x)
{
    int left = x;
    int right = x;
    while (left > 0 && rastrSameBytes[y * 32 + left - 1])
        --left;
    while (right < 31 && rastrSameBytes[y * 32 + right + 1])
        ++right;
    int value = right - left + 1;
    for (int x = left; x <= right; ++x)
    {
        rastrSameBytes[y * 32 + x] = value;
        --value;
    }
}

int maxMcPhaseForLine(CompressedData& compressedData)
{
    static const int kRastrToMcSwitchDelay = 28;
    return kRastrToMcSwitchDelay + kMinOffscreenBytes * kTicksOnScreenPerByte;
}

struct BorrowResult
{
    Register16 ix{"ix"};
    Register16 iy{ "iy" };
    bool success = false;
    int borrowFrom = -1;
    int borrowTo = -1;
};

BorrowResult borrowIndexRegForLine(int line, CompressedData& compressedData, int maxTicks)
{
    BorrowResult result;
    const int imageHeight = compressedData.data.size();
    auto& dstLine = compressedData.data[line];
    int borrowLine = line;
    for (int i = 0; i < imageHeight; ++i)
    {
        //borrowLine = (borrowLine+1) % imageHeight;
        ++borrowLine;
        if (borrowLine >= imageHeight)
            break; //< Disable cycling. It need to put PUSH IX/IY to the MC descriptors for the first visible line to enable it.

        auto& srcLine = compressedData.data[borrowLine];

        if (srcLine.mcStats.lendIx.has_value())
            break;

        if (srcLine.mcStats.virtualTicks + 14 >= maxTicks)
            continue;
        result.borrowFrom = borrowLine;
        result.borrowTo = line;
        result.success = true;
        break;
    }

    return result;
}

bool repackLine(CompressedLine& line, BorrowResult* borrowedRegs)
{
    CompressedLine result = line;

    Z80Parser parser;

    auto replaceToIndexReg =
        [&](int offset)
        {
            uint8_t* ldRegPtr = result.data.buffer() + offset;
            uint16_t* prevValue = (uint16_t*)(ldRegPtr + 1);
            borrowedRegs->ix.setValue(*prevValue);
            result.data.erase(offset, 2);
            ldRegPtr[0] = 0xdd; // IX prefix
            ldRegPtr[1] = 0xe5; // PUSH IX
            result.drawTicks -= 6;
            result.mcStats.virtualTicks -= 6;
        };

    auto regUseMask =
        [&](int offset)
    {
        std::vector<Register16> registers = {
            Register16("bc"), Register16("de"), Register16("hl"),
            Register16("bc'"), Register16("de'"), Register16("hl'")
        };
        auto info = parser.parseCode(
            *line.inputAf,
            registers,
            line.data.buffer(), line.data.size(),
            offset, line.data.size(), 0);
        return info.regUsage.regUseMask;
    };

    std::vector<Register16> registers = {
        Register16("bc"), Register16("de"), Register16("hl"),
        Register16("bc'"), Register16("de'"), Register16("hl'")
    };

    bool success = false;
    auto info = parser.parseCode(
        *line.inputAf,
        registers,
        line.data.buffer(), line.data.size(),
        0, line.data.size(), 0);

    for (int i = info.commands.size() - 1; i > 0; --i)
    {
        if (info.commands[i].opCode == 0xc5 && info.commands[i - 1].opCode == 0x01)
        {
            // LD BC, XX: PUSH BC
            if ((regUseMask(info.commands[i].ptr + 1) & 3) == 0)
            {
                replaceToIndexReg(info.commands[i - 1].ptr);
                success = true;
                break;
            }
        }
        else if (info.commands[i].opCode == 0xd5 && info.commands[i - 1].opCode == 0x11)
        {
            // LD DE, XX: PUSH DE
            if ((regUseMask(info.commands[i].ptr + 1) & 0x0c) == 0)
            {
                replaceToIndexReg(info.commands[i - 1].ptr);
                success = true;
                break;
            }
        }
        else if (info.commands[i].opCode == 0xe5 && info.commands[i - 1].opCode == 0x21)
        {
            // LD HL, XX: PUSH HL
            if ((regUseMask(info.commands[i].ptr + 1) & 0x30) == 0)
            {
                replaceToIndexReg(info.commands[i-1].ptr);
                success = true;
                break;
            }
        }
    };

    if (success)
        line = result;

    return success;
}

bool borrowIndexRegStep(CompressedData& compressedData)
{
    const int imageHeight = compressedData.data.size();

    int maxTicks = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
        maxTicks = std::max(line.mcStats.virtualTicks, maxTicks);
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        auto& line = compressedData.data[i];
        if (line.mcStats.virtualTicks == maxTicks)
        {
            BorrowResult borrowRes = borrowIndexRegForLine(i, compressedData, maxTicks);
            if (borrowRes.success)
            {
                if (!repackLine(line, &borrowRes))
                    return false;

                auto& borrowLine = compressedData.data[borrowRes.borrowFrom];
                CompressedLine temp;
                if (!borrowRes.ix.isEmpty())
                    borrowRes.ix.loadXX(temp, borrowRes.ix.value16());
                if (!borrowRes.iy.isEmpty())
                    borrowRes.iy.loadXX(temp, borrowRes.iy.value16());
                borrowLine.push_front(temp.data);
                borrowLine.drawTicks += temp.drawTicks;
                borrowLine.mcStats.virtualTicks += temp.drawTicks;

                for (int j = borrowRes.borrowFrom; j != i;)
                {
                    compressedData.data[j].mcStats.lendIx = borrowRes.ix.value16();
                    --j;
                    if (j < 0)
                        j = imageHeight - 1;
                }

            }
            return borrowRes.success;
        }
    }
    return false;
}

bool rebalanceStep(CompressedData& compressedData)
{
    const int imageHeight = compressedData.data.size();

    for (int i = 0; i < imageHeight; ++i)
    {
        int nextIndex = i > 0 ? i - 1 : imageHeight - 1;
        auto& line = compressedData.data[i];
        auto& nextLine = compressedData.data[nextIndex];

        if (nextLine.mcStats.pos > nextLine.mcStats.max)
        {
            ++line.mcStats.virtualTicks;
            --nextLine.mcStats.virtualTicks;
            --nextLine.mcStats.pos;
            return true;
        }

        // try to reduce max tics, next line will be drawn later
        int available = nextLine.mcStats.max - nextLine.mcStats.pos;
        if (available > 0
            && nextLine.mcStats.pos < maxMcPhaseForLine(compressedData)
            && line.mcStats.virtualTicks - nextLine.mcStats.virtualTicks > 1)
        {
            --line.mcStats.virtualTicks;
            ++nextLine.mcStats.virtualTicks;
            ++nextLine.mcStats.pos;
            return true;
        }

    }
    return false;
}

bool alignTo4(CompressedLine& line)
{
    if (line.mcStats.virtualTicks % 4 == 0)
        return false;
    int delayTicks = 4 - (line.mcStats.virtualTicks % 4) + 4;
    const auto delay = Z80Parser::genDelay(delayTicks);
    line.append(delay);
    line.mcStats.virtualTicks += delayTicks;
    line.drawTicks += delayTicks;
    return true;

}

void align2Lines(CompressedData& multicolor, int l0, int l1)
{
    int t0 = multicolor.data[l0].mcStats.virtualTicks;
    int t1 = multicolor.data[l1].mcStats.virtualTicks;
    int dt = std::abs(t0 - t1);
    if (dt > 0 && dt < 4)
    {
        if (t1 > t0)
            alignTo4(multicolor.data[l1]);
        else
            alignTo4(multicolor.data[l0]);
    }

    t0 = multicolor.data[l0].mcStats.virtualTicks;
    t1 = multicolor.data[l1].mcStats.virtualTicks;
    dt = std::abs(t0 - t1);
    if (dt > 0 && dt < 4)
    {
        alignTo4(multicolor.data[l1]);
        alignTo4(multicolor.data[l0]);
    }
}

void align3Lines(CompressedData& multicolor, int l0, int l1, int l2)
{
    align2Lines(multicolor, l0, l1);
    align2Lines(multicolor, l1, l2);
}

void alignTo4(const McToRastrInfo& info, CompressedData& multicolor)
{
    const int imageHeight = multicolor.data.size();

    struct LineAndTicks
    {
        int lineNum = 0;
        int ticks = 0;

        bool operator<(const LineAndTicks& other) const
        {
            if (ticks != other.ticks)
                return ticks < other.ticks;
            return lineNum < other.lineNum;
        }

        bool operator==(const LineAndTicks& other)
        {
            return lineNum == other.lineNum && ticks == other.ticks;
        }

    };

    for (const auto& banks : info)
    {
        for (const auto& data: banks)
        {
            std::vector<LineAndTicks> lines;
            for (const auto& i: data)
                lines.push_back({ i.mc, multicolor.data[i.mc].mcStats.virtualTicks });
            std::sort(lines.begin(), lines.end());
            auto last = std::unique(lines.begin(), lines.end());
            lines.erase(last, lines.end());


            if (lines.size() > 3)
            {
                std::cerr << "Error while aligning MC ticks" << std::endl;
                abort();
            }
            else if (lines.size() == 3)
            {
                align3Lines(multicolor, lines[0].lineNum, lines[1].lineNum, lines[2].lineNum);
            }
            else if (lines.size() == 2)
            {
                align2Lines(multicolor, lines[0].lineNum, lines[1].lineNum);
            }
        }
    }
}

bool alignMulticolorTimings(const McToRastrInfo& info, int flags, CompressedData& compressedData, bool silenceMode)
{
    const int imageHeight = compressedData.data.size();

    // 1. Get lines stats
    int maxTicks = 0;
    int minTicks = std::numeric_limits<int>::max();
    int regularTicks = 0;
    int maxTicksLine = 0;
    int minTicksLine = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        auto& line = compressedData.data[i];
        line.mcStats.virtualTicks = line.drawTicks;
        if (line.drawTicks > maxTicks)
        {
            maxTicks = line.drawTicks;
            maxTicksLine = i;
        }
        if (line.drawTicks < minTicks)
        {
            minTicks = line.drawTicks;
            minTicksLine = i;
        }
        regularTicks += line.drawTicks;
    }

    if (!silenceMode)
    {
        std::cout << "INFO: max multicolor ticks line #" << maxTicksLine << ", ticks=" << maxTicks << ". Min ticks line #" << minTicksLine << ", ticks=" << minTicks << std::endl;
        std::cout << "INFO: align multicolor to ticks to " << maxTicks << " losed ticks=" << maxTicks * imageHeight - regularTicks << std::endl;
    }


    if (flags & OptimizeMcTicks)
    {
        // 2. Use advanced ray model. MC line drawing could start when ray already inside line in case of enough ticks for drawing
        while (rebalanceStep(compressedData));

#if LOG_DEBUG
        // 1. Sink LD IX,nn, LD IY,nn from fast lines to slow lines
        while (borrowIndexRegStep(compressedData));
        while (rebalanceStep(compressedData));
#endif

    }

    int maxVirtualTicks = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
        maxVirtualTicks = std::max(line.mcStats.virtualTicks, maxVirtualTicks);
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
        if (line.mcStats.pos < line.mcStats.min)
            maxVirtualTicks = std::max(maxVirtualTicks, line.mcStats.virtualTicks + (line.mcStats.min - line.mcStats.pos));
    }

#if LOG_DEBUG
    if (!silenceMode)
    {
        for (int i = 0; i < imageHeight; ++i)
        {
            int endLine = (i + 23) % imageHeight;
            std::cout << i << ": " << compressedData.data[i].mcStats.pos << "[" << compressedData.data[i].mcStats.min << ".." << compressedData.data[i].mcStats.max
                << "], ticks=" << compressedData.data[i].drawTicks << ", virtualTicks=" << compressedData.data[i].mcStats.virtualTicks << std::endl;
        }
    }
#endif

    for (int i = 0; i < imageHeight; ++i)
    {
        auto& line = compressedData.data[i];
        if (line.mcStats.virtualTicks < maxVirtualTicks && maxVirtualTicks - line.mcStats.virtualTicks < 4)
        {
            //auto result = Z80Parser::prolongCodeForDelay(line, maxVirtualTicks - line.mcStats.virtualTicks);
            maxVirtualTicks += 4;
            break;
        }
    }

    if (!silenceMode)
        std::cout << "INFO: reduce maxTicks after rebalance: " << maxVirtualTicks << " reduce losed ticks to=" << maxVirtualTicks * imageHeight - regularTicks << std::endl;

    int idealSum = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        int next = (i + 1) % imageHeight;
        idealSum += std::abs(compressedData.data[next].mcStats.virtualTicks - compressedData.data[i].mcStats.virtualTicks);
    }

    if (!silenceMode)
        std::cout << "INFO:  ideal losed ticks=" << idealSum << std::endl;

    // 1. Calculate extra delay for begin of the line if it draw too fast
    int i = 0;
    for (auto& line : compressedData.data)
    {
        if (line.mcStats.pos < line.mcStats.min)
        {
            int beginDelay = line.mcStats.min - line.mcStats.pos;
            if (beginDelay > 0)
            {
                // Put delay to the begin only instead begin/end delay if possible.
                int endLineDelay = 0;
                if ((flags & twoRastrDescriptors))
                {
                    int d = (line.mcStats.virtualTicks + beginDelay) % 4;
                    if (d != 0)
                        beginDelay += 4 - d;
                }
                else
                {
                    endLineDelay = maxVirtualTicks - beginDelay - line.mcStats.virtualTicks;
                }
                if (line.mcStats.max - line.mcStats.pos >= beginDelay + endLineDelay)
                    beginDelay += endLineDelay;
            }

            beginDelay = std::max(4, beginDelay);
            const auto delay = Z80Parser::genDelay(beginDelay);
            line.push_front(delay);
            line.mcStats.virtualTicks += beginDelay;
            line.drawTicks += beginDelay;
            if (beginDelay + line.mcStats.pos > line.mcStats.max)
            {
                std::cerr << "Error. pos > max after alignment at line " << i << ". pos=" << line.mcStats.pos << " . max=" << line.mcStats.max << ". dt=" << beginDelay << std::endl;
                return false;
            }
        }
        ++i;
    }

    if (flags & twoRastrDescriptors)
    {
        int prevTotalTicks = 0;
        for (int i = 0; i < imageHeight; ++i)
            prevTotalTicks += compressedData.data[i].mcStats.virtualTicks;

        alignTo4(info, compressedData);

        int totalTicks = 0;
        for (int i = 0; i < imageHeight; ++i)
            totalTicks += compressedData.data[i].mcStats.virtualTicks;
        if (!silenceMode)
            std::cout << "After align to 4 losed ticks=" << totalTicks - prevTotalTicks << std::endl;
    }
    else
    {
        // 2. Add delay to the end of lines to make them equal duration

        for (int y = 0; y < compressedData.data.size(); ++y)
        {
            auto& line = compressedData.data[y];

            int endLineDelay = maxVirtualTicks - line.mcStats.virtualTicks;
            const auto delayCode = Z80Parser::genDelay(endLineDelay);
            line.append(delayCode);
            line.drawTicks += endLineDelay;
            line.mcStats.virtualTicks += endLineDelay;
        }
    }

    return true;
}


void fillShuffledBuffer(uint8_t* shufledBuffer, uint8_t* buffer, int imageHeight)
{
    uint8_t* src = buffer;
    uint8_t* dst = shufledBuffer;

    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            dst[16] = src[0];
            dst++;
            src++;
        }
        for (int x = 0; x < 16; ++x)
        {
            dst[-16] = src[0];
            dst++;
            src++;
        }
    }
}

CompressedData compressMultiColors(int flags, uint8_t* buffer, int imageHeight, uint8_t* rastrBuffer)
{
    // Shuffle source data according to the multicolor drawing:
    // 32  <------------------16
    //                        16 <------------------ 0

    std::vector<uint8_t> shufledBuffer(imageHeight * 32);
    std::vector<uint8_t> bufferAlt(imageHeight * 32);
    std::vector<uint8_t> shufledBufferCopy(imageHeight * 32);
    std::vector<uint8_t> shufledBufferAlt(imageHeight * 32);

    fillShuffledBuffer(shufledBuffer.data(), buffer, imageHeight);

    if (flags & updateColorData)
    {
        // Can change color if all rastr pixels are 0x00 or 0xff
        memcpy(bufferAlt.data(), buffer, 32 * imageHeight);
        //makePaperAndIncSameIfNeed(rastrBuffer, bufferAlt.data(), imageHeight * 8);
        fillShuffledBuffer(shufledBufferAlt.data(), bufferAlt.data(), imageHeight);
        memcpy(shufledBufferCopy.data(), shufledBuffer.data(), imageHeight * 32);
    }

    auto suffledPtr = shufledBuffer.data();

    struct Context context;
    context.scrollDelta = 1;
    context.flags = verticalCompressionL;
    context.imageHeight = imageHeight;
    context.buffer = shufledBuffer.data();
    context.borderPoint = 16;


    std::vector<int8_t> sameBytesCount = createSameBytesTable(context.flags, shufledBuffer.data(), /*maskColors*/ nullptr, imageHeight);
    context.sameBytesCount = &sameBytesCount;

    int count1;
    //context.af = findBestByte(suffledPtr, imageHeight, &sameBytesCount, &count1);
    //context.af.isAltAf = true;

    CompressedData compressedData;
    for (int y = imageHeight-1; y >= 0; --y)
    {
        context.y = y;
        auto line = compressMultiColorsLine(context);

        if (flags & updateColorData)
        {
            memcpy(shufledBuffer.data() + y * 32, shufledBufferAlt.data() + y * 32, 32);
            sameBytesCount = createSameBytesTable(context.flags, shufledBuffer.data(), /*maskColors*/ nullptr, imageHeight);
            context.sameBytesCount = &sameBytesCount;

            auto lineAlt = compressMultiColorsLine(context);

            if (lineAlt.drawTicks <= line.drawTicks)
            {
                memcpy(buffer + y * 32, bufferAlt.data() + y * 32, 32);
                line = lineAlt;
            }
            else
            {
                memcpy(shufledBuffer.data() + y * 32, shufledBufferCopy.data() + y * 32, 32);
                sameBytesCount = createSameBytesTable(context.flags, shufledBuffer.data(), /*maskColors*/ nullptr, imageHeight);
                context.sameBytesCount = &sameBytesCount;
            }
        }

        compressedData.data.insert(compressedData.data.begin(), line);
        if (line.mcStats.min > line.mcStats.max)
        {
            std::cerr << "Something wrong. Multicolor line #" << context.y << ". Wrong mcStats. min= " << line.mcStats.min << " is bigger than max=" << line.mcStats.max << std::endl;
            //abort();
        }
    }

    if (flags & updateColorData)
    {
        context.y = imageHeight - 1;
        auto line = compressMultiColorsLine(context);
        compressedData.data[context.y] = line;
    }

    compressedData.sameBytesCount = sameBytesCount;
    return compressedData;
}

CompressedData  compressColors(uint8_t* buffer, int imageHeight)
{
    int flags = verticalCompressionH | interlineRegisters | optimizeLineEdge;
    std::vector<int8_t> sameBytesCount = createSameBytesTable(flags, buffer, /*maskColors*/ nullptr, imageHeight / 8);

    Context context;
    context.scrollDelta = kScrollDelta;
    context.flags = flags;
    context.imageHeight = imageHeight / 8;
    context.buffer = buffer;
    context.sameBytesCount = &sameBytesCount;
    //context.af.h = findBestByte(buffer, context.imageHeight, &sameBytesCount).h;

    std::vector<int> lines;
    for (int y = 0; y < imageHeight / 8; y++)
    {
        lines.push_back(y);
    }

    CompressedData compressedData;
#if 1
    //std::array<Register16, 4> registers = { Register16("bc"), Register16("de"), Register16("hl"), Register16("af") };
    std::array<Register16, 4> registers = { Register16("af"), Register16("hl"), Register16("de"), Register16("bc") };
    compressedData.data = compressLines(registers, context, lines, /*maxTransitiveDepth*/ 24);
#else
	compressedData.data = compressLines(context, lines, /*maxTransitiveDepth*/ 24);
#endif
    compressedData.sameBytesCount = sameBytesCount;
    compressedData.flags = flags;
    return compressedData;
}

void interleaveData(CompressedData& data)
{
    std::vector<CompressedLine> newData;
    for (int i = 0; i < 8; ++i)
    {
        for (int y = 0; y < data.data.size(); y += 8)
            newData.push_back(data.data[y + i]);
    }
    data.data = newData;
}

#pragma pack(push)
#pragma pack(1)

struct DescriptorState
{
    uint16_t descriptorLocationPtr = 0; //< The address where descriptor itself is serialized
    uint16_t lineStartPtr = 0;      //< Start draw lines here.
    uint16_t lineEndPtr = 0;        //< Stop draw lines here.

    std::optional<CompressedLine> expectedUsedRegisters;
    std::vector<uint8_t> preambula; //< Z80 code to load initial registers and JP command to the lineStart
    Z80CodeInfo codeInfo;           //< Descriptor data info.

    Z80CodeInfo omitedDataInfo;     //< Information about skipped start block (it's included directly to the descriptor preambula).
    std::vector<uint8_t> endBlock;  //< Information about replaced end block (JP IX command).

    int extraDelay = 0;             //< Alignment delay when serialize preambula.
    int startSpDelta = 0;           //< Addition commands to decrement SP included to preambula.
    Register16 af{ "af" };
    std::optional<uint16_t> updatedHlValue;

    const uint8_t* srcBuffer = 0;
    int srcLine = 0;

    int bottomMcOffset = 0;
    int topMcOffset = 0;
    int nextMcOffset = 0;

    void setEndBlock(const uint8_t* ptr)
    {
        endBlock.push_back(*ptr++);
        endBlock.push_back(*ptr);
    }

    void removeTrailingStackMoving(int maxCommandToRemove = std::numeric_limits<int>::max())
    {
        lineEndPtr -= Z80Parser::removeTrailingStackMoving(codeInfo, maxCommandToRemove);
    }

    std::pair<CompressedLine, bool> updateRegistersForDelay(const CompressedLine& serializedRegs, int delay, Register16& af, bool testMode)
    {
        if (delay == 0)
            return { serializedRegs, true };

        int origDelay = delay;

        if (delay > 3)
        {
            std::cerr << "Invalid delay. " << delay << "Allowed range is [0..3]" << std::endl;
            abort();
        }

        std::vector<Register16> emptyRegs = { Register16("bc"), Register16("de"), Register16("hl") };
        auto parsedRegs = Z80Parser::parseCode(
            af, emptyRegs,
            serializedRegs.data.buffer(), serializedRegs.data.size(),
            0, serializedRegs.data.size(),
            0);

        CompressedLine updatedLine;

        auto splitLoadXX =
            [&](const std::string& regName)
        {
            auto reg = findRegister(parsedRegs.outputRegisters, regName);
            auto reg8 = findRegisterByValue(parsedRegs.outputRegisters, *reg->h.value, &af.h);
            if (reg8 && (reg8->name == 'a' || reg8->reg8Index < reg->h.reg8Index))
            {
                reg->l.loadX(updatedLine, reg->l.value.value());
                reg->h.loadFromReg(updatedLine, *reg8);
                return true;
            }
            else
            {
                reg8 = findRegisterByValue(parsedRegs.outputRegisters, *reg->l.value, &af.h);
                if (reg8 && (reg8->name == 'a' || reg8->reg8Index < reg->l.reg8Index))
                {
                    reg->h.loadX(updatedLine, reg->h.value.value());
                    reg->l.loadFromReg(updatedLine, *reg8);
                    return true;
                }
            }
            return false;
        };

        if (delay == 3)
        {
            // Try to change LD REG8,REG8 to LD REG8, X
            for (const auto& command : parsedRegs.commands)
            {
                if (command.opCode >= 0x40 && command.opCode <= 0x6f && delay == 3)
                {
                    int regFromIndex = (command.opCode - 0x40) % 8;
                    int regToIndex = (command.opCode - 0x40) / 8;
                    auto regFrom = findRegisterByIndex(parsedRegs.outputRegisters, regFromIndex, &af.h);
                    auto regTo = findRegisterByIndex(parsedRegs.outputRegisters, regToIndex);
                    regTo->loadX(updatedLine, *regFrom->value);
                    delay = 0;
                }
                else
                {
                    updatedLine.appendCommand(command);
                }
            }
        }

        if (delay == 2)
        {
            // Try to change 2 LD REG8,REG8 to LD REG16, XX
            for (int i = 0; i < (int)parsedRegs.commands.size() - 1; ++i)
            {
                const auto& command = parsedRegs.commands[i];
                const auto& nextCommand = parsedRegs.commands[i + 1];
                if (command.opCode >= 0x40 && command.opCode <= 0x6f
                    && nextCommand.opCode >= 0x40 && nextCommand.opCode <= 0x6f
                    && delay == 2)
                {
                    int regFromIndex1 = (command.opCode - 0x40) % 8;
                    int regToIndex1 = (command.opCode - 0x40) / 8;

                    int regFromIndex2 = (nextCommand.opCode - 0x40) % 8;
                    int regToIndex2 = (nextCommand.opCode - 0x40) / 8;

                    auto regTo1 = findRegisterByIndex(parsedRegs.outputRegisters, regToIndex1);
                    auto regTo2 = findRegisterByIndex(parsedRegs.outputRegisters, regToIndex2);

                    if (regToIndex1 + 1 == regToIndex2 && regToIndex1 % 2 == 0)
                    {
                        auto regFrom1 = findRegisterByIndex(parsedRegs.outputRegisters, regFromIndex1, &af.h);
                        auto regFrom2 = findRegisterByIndex(parsedRegs.outputRegisters, regFromIndex2, &af.h);

                        const uint16_t word = ((uint16_t)*regFrom1->value << 8) + *regFrom2->value;

                        std::string reg16Name = std::string() + regTo1->name + regTo2->name;
                        auto reg16 = findRegister(parsedRegs.outputRegisters, reg16Name);
                        reg16->loadXX(updatedLine, word);
                        delay = 0;
                        ++i;
                    }
                    else
                    {
                        updatedLine.appendCommand(command);
                    }
                }
                else
                {
                    updatedLine.appendCommand(command);
                }
            }
        }

        if (delay)
        {
            // Split LD REG16, XX to   LD REG8, X : LD REG8, REG8
            std::set<uint8_t> availableValues;
            if (af.h.value)
                availableValues.insert(*af.h.value);
            for (const auto& command : parsedRegs.commands)
            {
                bool updated = false;
                if (delay > 0)
                {
                    switch (command.opCode)
                    {
                    case 0x01:  // LD BC, xx
                        updated = splitLoadXX("bc");
                        break;
                    case 0x11:  // LD DE, xx
                        updated = splitLoadXX("de");
                        break;
                    case 0x21:  // LD HL, xx
                        updated = splitLoadXX("hl");
                        break;
                    }
                    if (updated)
                        --delay;
                }
                if (!updated)
                    updatedLine.appendCommand(command);
            }
        }

        if (parsedRegs.ticks + origDelay != updatedLine.drawTicks || delay)
        {
            if (testMode)
                return { updatedLine, false };

            std::cerr << "Updating regs for extra delay " << origDelay << " failed. Some bug" << std::endl;
            abort();
        }

        auto parsedRegs2 = Z80Parser::parseCode(
            af, emptyRegs,
            updatedLine.data.buffer(), updatedLine.data.size(),
            0, updatedLine.data.size(),
            0);

        for (int i = 0; i < 3; ++i)
        {
            if (parsedRegs2.outputRegisters[i].h.value != parsedRegs.outputRegisters[i].h.value
                || parsedRegs2.outputRegisters[i].l.value != parsedRegs.outputRegisters[i].l.value)
            {
                std::cerr << "Updating regs for extra delay " << origDelay << " failed. Some bug" << std::endl;
                abort();
            }
        }

        return { updatedLine, true };
    }

    bool canProlongPreambulaFor(
        int delay,
        const CompressedLine& dataLine,
        std::vector<uint8_t> serializedData,
        int relativeOffsetToStart)
    {
        CompressedLine preambula = dataLine.getSerializedUsedRegisters(af);
        auto [registers, __1, __2] = mergedPreambulaInfo(preambula, serializedData, relativeOffsetToStart, kJpIxCommandLen);

        auto regs = getSerializedRegisters(registers, af);
        auto [updatedRegs, success] = updateRegistersForDelay(regs, delay, af, /*testMode*/ true);
        return success;
    }


    struct MergedPreambulaInfo
    {
        std::vector<Register16> outputRegisters; //< Preambula + omited registers final state.
        int omitedTicks = 0;                     //< Omited data ticks.
        int omitedSize = 0;                      //< Omited data bytes.
    };

    MergedPreambulaInfo mergedPreambulaInfo(
        const CompressedLine& preambula,
        std::vector<uint8_t> serializedData,
        int relativeOffsetToStart,
        int omitedDataSize) const
    {
        auto firstCommands = Z80Parser::getCode(serializedData.data() + relativeOffsetToStart, omitedDataSize);
        std::vector<Register16> emptyRegs = { Register16("bc"), Register16("de"), Register16("hl") };
        auto omitedDataInfo = Z80Parser::parseCode(
            af, emptyRegs, firstCommands,
            0, firstCommands.size(), 0);

        emptyRegs = { Register16("bc"), Register16("de"), Register16("hl") };
        auto [regUsage, commandCounter] = Z80Parser::selfRegUsageInFirstCommands(omitedDataInfo.commands, emptyRegs, af);

        int regUsageCommandsSize = 0;
        int regUsageCommandsTicks = 0;
        for (int i = 0; i < commandCounter; ++i)
        {
            regUsageCommandsSize += omitedDataInfo.commands[i].size;
            regUsageCommandsTicks += omitedDataInfo.commands[i].ticks;
        }

        // Load regs + first command together
        std::vector<uint8_t> preambulaWithFirstCommand;
        preambula.serialize(preambulaWithFirstCommand);
        preambulaWithFirstCommand.insert(preambulaWithFirstCommand.end(),
            firstCommands.begin(), firstCommands.begin() + regUsageCommandsSize);

        emptyRegs = { Register16("bc"), Register16("de"), Register16("hl") };
        auto newCodeInfo = Z80Parser::parseCode(
            af, emptyRegs, preambulaWithFirstCommand,
            0, preambulaWithFirstCommand.size(), 0);

        if (updatedHlValue)
        {
            auto hl = findRegister(newCodeInfo.outputRegisters, "hl");
            hl->setValue(*updatedHlValue);
        }

        return { newCodeInfo.outputRegisters, regUsageCommandsTicks, regUsageCommandsSize };
    }

    int expectedPreambulaTicks(
        const CompressedLine& dataLine,
        std::vector<uint8_t> serializedData,
        int relativeOffsetToStart,
        int lineBank)
    {
        expectedUsedRegisters = dataLine.getSerializedUsedRegisters(af);
        auto [registers, omitedTicksFromMainCode, _] = mergedPreambulaInfo(*expectedUsedRegisters, serializedData, relativeOffsetToStart, kJpIxCommandLen);
        auto outRegs = getSerializedRegisters(registers, af);
        auto result = outRegs.drawTicks - omitedTicksFromMainCode;
        //if (lineBank % 2 == 1)
        result += kSetPageTicks;
        return result;
    }

    void replaceLoadWithHlToSp(
        int imageHeight,
        uint8_t dataByte,
        std::vector<Register16>& registers)
    {
        if (startSpDelta % 32 < 2)
        {
            std::cerr << "Unsupported startSpDelta % 32 < 2. Just a bug. Please report an issue to developers." << std::endl;
            abort();
        }
        startSpDelta -= 2;

        CompressedLine replaceLdHlData;
        int prevSpDelta = startSpDelta - 1;

        int drawLine = srcLine + (startSpDelta / 32) * 8;
        drawLine = drawLine % imageHeight;

        const uint8_t* cur = srcBuffer + drawLine * 32 + startSpDelta % 32;
        uint16_t* buffer16 = (uint16_t*)(cur);
        uint16_t valueToRestore = buffer16[0];
        valueToRestore = swapBytes(valueToRestore);

        int regIndex = dataByte - 0x70;
        std::unique_ptr<Register16> r;
        if (regIndex / 2 == 0)
            r = std::make_unique<Register16>("bc");
        else if (regIndex / 2 == 1)
            r = std::make_unique<Register16>("de");
        else
            r = std::make_unique<Register16>("hl");
        r->loadXX(replaceLdHlData, valueToRestore);
        r->push(replaceLdHlData);

        replaceLdHlData.serialize(preambula);

        // optimize registers value if it contains same data
        if (auto r2 = findRegister(registers, r->name()))
        {
            if (r2->h.hasValue(*r->h.value))
                r2->h.value.reset();
            if (r2->l.hasValue(*r->l.value))
                r2->l.value.reset();
        }


        /** LD(HL), reg8 command after the descriptor.It refer to the current HL = SP value, but HL is not set when start to exec descriptor.
         * Replace it to:
         * PUSH REG16
         * INC SP
        */

    }

    void replaceLoadConstWithHlToSp(
        int imageHeight,
        std::vector<Register16>& registers)
    {
        if (startSpDelta % 32 < 2)
        {
            std::cerr << "Unsupported startSpDelta % 32 < 2. Just a bug. Please report an issue to developers." << std::endl;
            abort();
        }
        startSpDelta -= 2;

        CompressedLine replaceLdHlData;
        int prevSpDelta = startSpDelta - 1;

        int drawLine = srcLine + (startSpDelta / 32) * 8;
        drawLine = drawLine % imageHeight;

        const uint8_t* cur = srcBuffer + drawLine * 32 + startSpDelta % 32;
        uint16_t* buffer16 = (uint16_t*)(cur);
        uint16_t valueToRestore = buffer16[0];
        valueToRestore = swapBytes(valueToRestore);

        std::unique_ptr<Register16> r = std::make_unique<Register16>("hl");
        r->loadXX(replaceLdHlData, valueToRestore);
        r->push(replaceLdHlData);

        replaceLdHlData.serialize(preambula);

        // optimize registers value if it contains same data
        if (auto r2 = findRegister(registers, r->name()))
        {
            if (r2->h.hasValue(*r->h.value))
                r2->h.value.reset();
            if (r2->l.hasValue(*r->l.value))
                r2->l.value.reset();
        }


        /** LD(HL), nn command after the descriptor.It refer to the current HL = SP value, but HL is not set when start to exec descriptor.
         * Replace it to:
         * PUSH REG16
         * INC SP
        */

    }

    void serializeOmitedData(
        int imageHeight,
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        int omitedDataSize,
        int extraDelay)
    {
        const uint16_t lineStartOffset = lineStartPtr - codeOffset;
        CompressedLine usedRegisters = expectedUsedRegisters
            ? *expectedUsedRegisters
            : codeInfo.regUsage.getSerializedUsedRegisters(codeInfo.inputRegisters, af);

        auto [registers, omitedTicks, omitedBytes] = mergedPreambulaInfo(usedRegisters, serializedData, lineStartOffset, omitedDataSize);

        auto firstCommands = Z80Parser::getCode(serializedData.data() + lineStartOffset, omitedDataSize);

        const int firstCommandsSize = firstCommands.size();
        omitedDataInfo = Z80Parser::parseCode(
            af, codeInfo.inputRegisters, firstCommands,
            0, firstCommands.size(), 0);


        firstCommands.erase(firstCommands.begin(), firstCommands.begin() + omitedBytes);

        if (startSpDelta > 0)
        {
            const uint8_t* dataPtr = serializedData.data() + lineStartOffset;
            if (!firstCommands.empty() && z80Command::isLoadViaHl(firstCommands[0]))
            {
                uint8_t dataByte = firstCommands[0];
                firstCommands.erase(firstCommands.begin());
                replaceLoadWithHlToSp(imageHeight, dataByte, registers);
            }
            else if (firstCommands.empty() && z80Command::isLoadViaHl(dataPtr[0]))
            {
                replaceLoadWithHlToSp(imageHeight, dataPtr[0], registers);
                codeInfo.startOffset++;
                lineStartPtr++;
                codeInfo.ticks -= 7;
            }
            if (!firstCommands.empty() && z80Command::isLoadConstViaHl(firstCommands[0]))
            {
                firstCommands.erase(firstCommands.begin());
                firstCommands.erase(firstCommands.begin());
                replaceLoadConstWithHlToSp(imageHeight, registers);
            }
            else if (firstCommands.empty() && z80Command::isLoadConstViaHl(dataPtr[0]))
            {
                replaceLoadConstWithHlToSp(imageHeight, registers);
                codeInfo.startOffset+=2;
                lineStartPtr+=2;
                codeInfo.ticks -= 10;
            }
        }

        auto regs = getSerializedRegisters(registers, af);
        if (extraDelay > 0)
            regs = updateRegistersForDelay(regs, extraDelay, af, /*testsmode*/ false).first;

        regs.serialize(preambula);
        addToPreambule(firstCommands);

        if (!omitedDataInfo.hasJump)
            addJumpTo(lineStartPtr + firstCommandsSize);
    }

    void serializeSetPageCode(int pageNum)
    {
        if (pageNum >= 2)
            ++pageNum; //< Pages are: 0,1, 3,4
        std::vector<uint8_t> data;

        //data.push_back(0x3e);            // LD A, #50 + page_number
        //data.push_back(kPageNumPrefix + pageNum);  // LD A, #50 + page_number
        if (pageNum == 0)
        {
            data.push_back(0xdd);
            data.push_back(0x7c);
        }
        else if (pageNum == 1)
        {
            data.push_back(0xdd);
            data.push_back(0x7d);
        }
        else if (pageNum == 3)
        {
            data.push_back(0xfd);
            data.push_back(0x7d);
        }
        else
        {
            data.push_back(0xfd);
            data.push_back(0x7c);
        }


        data.push_back(0xd3);            // OUT (#fd), A
        data.push_back(0xfd);            // OUT (#fd), A
        addToPreambule(data);
    }

    void makePreambulaForMC(
        int imageHeight,
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        const CompressedLine* line,
        int pageNum, int bankNum,
        int mcTicksBottom, int mcTicksTop, int mcTicksNext)
    {
        //if (bankNum % 2 == 1)
        //serializeSetPageCode(pageNum);

        /*
         * In whole frame JP ix there is possible that first bytes of the line is 'broken' by JP iX command
         * So, descriptor point not to the line begin, but some line offset (2..4 bytes) and it repeat first N bytes of the line
         * directly in descriptor preambula. Additionally, preambula contains alignment delay and correction for SP register if need.
         */

        if (extraDelay > 3)
        {
            addToPreambule(Z80Parser::genDelay(extraDelay));
            extraDelay = 0;
        }

        serializeOmitedData(imageHeight, serializedData, codeOffset, kJpIxCommandLen, extraDelay);

        struct TicksInfo
        {
            int ticks = 0;
            int* role = nullptr;;

            bool operator<(const TicksInfo& other) const
            {
                return ticks < other.ticks;
            }
        };

        int maxTicks = std::max(std::max(mcTicksBottom, mcTicksTop), mcTicksNext);
        std::vector<TicksInfo> alignTicks;
        alignTicks.push_back({ mcTicksBottom, &bottomMcOffset });
        alignTicks.push_back({ mcTicksTop, &topMcOffset });
        alignTicks.push_back({ mcTicksNext, &nextMcOffset });
        std::sort(alignTicks.begin(), alignTicks.end());

        for (int i = alignTicks.size() - 1; i > 0; --i)
            alignTicks[i].ticks -= alignTicks[i-1].ticks;
        alignTicks[0].ticks = 0;

        std::vector<uint8_t> extraDelayData;
        while (!alignTicks.empty())
        {
            int minTicks = alignTicks[0].ticks;
            if (minTicks > 0)
            {
                auto mcAlignDelay = Z80Parser::genDelay(minTicks);
                extraDelayData.insert(extraDelayData.end(), mcAlignDelay.begin(), mcAlignDelay.end());
                for (const auto& info : alignTicks)
                    *info.role += mcAlignDelay.size();
            }
            alignTicks.erase(alignTicks.begin());
        }
        preambula.insert(preambula.begin(), extraDelayData.begin(), extraDelayData.end());
    }

    void makePreambulaForOffscreen(
        int imageHeight,
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        int descriptorsDelta)
    {
        if (codeInfo.commands.size() >= 3)
        {
            if (codeInfo.commands[1].opCode == kAddHlSpCode && codeInfo.commands[2].opCode == kLdSpHlCode)
            {
                // Remove LD HL, x: add HL, SP: LD HL, SP if exists
                int size = codeInfo.commands[0].size + codeInfo.commands[1].size + codeInfo.commands[2].size;
                auto p = Z80Parser::parseCode(
                    af,
                    codeInfo.inputRegisters,
                    serializedData,
                    codeInfo.startOffset, codeInfo.startOffset + size,
                    codeOffset);
                codeInfo.inputRegisters = p.outputRegisters;
                codeInfo.startOffset += size;
                lineStartPtr += size;
                startSpDelta -= p.spOffset;
                descriptorsDelta += size;
                codeInfo.ticks -= p.ticks;
            }
        }

        /*
         * In whole frame JP ix there is possible that first bytes of the line is 'broken' by JP iX command
         * So, descriptor point not to the line begin, but some line offset (2..4 bytes) and it repeat first N bytes of the line
         * directly in descriptor preambula. Additionally, preambula contains alignment delay and correction for SP register if need.
         */

        //if (startSpDelta > 0)
        //    serializeSpDelta(startSpDelta);

        serializeOmitedData(imageHeight, serializedData, codeOffset, kJpIxCommandLen - descriptorsDelta, 0);
    }

    void serialize(std::vector<uint8_t>& dst)
    {
        dst.insert(dst.end(), preambula.begin(), preambula.end());
    }

private:

    void serializeSpDelta(int delta)
    {
        CompressedLine data;

        Register16 hl("hl");
        Register16 sp("sp");
        hl.loadXX(data, -delta);
        hl.addSP(data);
        sp.loadFromReg16(data, hl);
        data.serialize(preambula);
    }

    void addJumpTo(uint16_t value)
    {
        preambula.push_back(0xc3); //< JP XXXX
        preambula.push_back((uint8_t)value);
        preambula.push_back(value >> 8);
    }


    void addToPreambule(const std::vector<uint8_t>& data)
    {
        preambula.insert(preambula.end(), data.begin(), data.end());
    }
};

struct LineDescriptor
{
    // From 0.. to the line middle. Used to draw rastr during multicolor
    DescriptorState rastrForMulticolor;
    // From the line middle to the end. Used to draw rastr during back ray
    DescriptorState rastrForOffscreen;
    int pageNum = 0;
    // Multicolor drawing code
};

struct MulticolorDescriptor
{
    uint16_t endLineJpAddr = 0;
    uint16_t moveSp2BytePos = 0;
    uint16_t moveSpDelta = -1;
    uint16_t moveSpBytePos = 0;
    uint16_t addressBegin = 0;
};

struct ColorDescriptor
{
    uint16_t addressBegin = 0;
    uint16_t addressEnd = 0;
    CompressedLine preambula;
};

struct JpIxDescriptor
{
    enum class Type
    {
        unknown,
        multicolor,
        offscreen,
        multicolor_restore,
        offscreen_restore
    };

    uint16_t address = 0;
    std::vector<uint8_t> originData;
    int pageNum = 0;
    int line = -1;
    Type type{Type::unknown};
};

struct JpIxLineData
{
    std::vector<JpIxDescriptor> write;
    std::vector<JpIxDescriptor> restore;
};

int lineNumToPageNum(int y, int height)
{
    int bankSize = height / 8;
    int bankNum = y / bankSize;
    return bankNum / 2;
}

#pragma pack(pop)

std::vector<JpIxLineData> createWholeFrameJpIxDescriptors(
    const std::vector<LineDescriptor>& descriptors)
{

    std::vector<JpIxLineData> result;

    const int imageHeight = descriptors.size();
    const int blocks64 = imageHeight / 64;
    const int bankSize = imageHeight / 8;
    const int colorsHeight = imageHeight / 8;

    // . Create delta for JP_IX when shift to 1 line
    for (int i = 0; i < 64; ++i)
    {
        int line = i;

        for (int i = 0; i < blocks64 + 2; ++i)
        {
            int l = (line + i * 64) % imageHeight;

            JpIxLineData data;

            JpIxDescriptor d;
            d.pageNum = descriptors[l].pageNum;
            d.line = l;

            d.type = JpIxDescriptor::Type::offscreen;
            d.address = descriptors[l].rastrForOffscreen.lineEndPtr;
            d.originData = descriptors[l].rastrForOffscreen.endBlock;
            data.write.push_back(d);

            d.type = JpIxDescriptor::Type::multicolor;
            d.address = descriptors[l].rastrForMulticolor.lineEndPtr;
            d.originData = descriptors[l].rastrForMulticolor.endBlock;
            data.write.push_back(d);

            l = (l + 8) % imageHeight;
            d.line = l;

            d.type = JpIxDescriptor::Type::multicolor_restore;
            d.address = descriptors[l].rastrForMulticolor.lineEndPtr;
            d.originData = descriptors[l].rastrForMulticolor.endBlock;
            data.restore.push_back(d);

            d.type = JpIxDescriptor::Type::offscreen_restore;
            d.address = descriptors[l].rastrForOffscreen.lineEndPtr;
            d.originData = descriptors[l].rastrForOffscreen.endBlock;
            data.restore.push_back(d);

            result.push_back(data);
        }
    }
    return result;
}


JpIxDescriptor findDescriptor(const std::vector<JpIxLineData>& fullJpix, int line, JpIxDescriptor::Type type)
{
    for (const auto& data: fullJpix)
    {
        for (const auto& d : data.write)
        {
            if (d.line == line && d.type == type)
                return d;
        }
        for (const auto& d : data.restore)
        {
            if (d.line == line && d.type == type)
                return d;
        }
    }
    abort();
    return JpIxDescriptor();
}

int nextLineInBank(int line, int imageHeight)
{
    const int bankSize = imageHeight / 8;
    int bankNum = line / bankSize;
    int lineNumInBank = line - bankNum * bankSize;

    if (lineNumInBank == bankSize - 1)
        return bankNum * bankSize;
    else
        return line + 1;
}

int jpixTableSize(int imageHeight)
{
    return (imageHeight / 64 + 2) * 64 * 12;
}

int getTimingsStartAddress(int imageHeight)
{
    int result = rastrCodeStartAddrBase + jpixTableSize(imageHeight) / kPagesForData;
    return result;
}

int getRastrCodeStartAddr(int imageHeight)
{
    static const int kTimingsAligner = 12;
    return getTimingsStartAddress(imageHeight) + imageHeight*2 + kTimingsAligner;
}

int serializeMainData(
    const uint8_t* srcBuffer,
    const CompressedData& data,
    const CompressedData& multicolorData,
    std::vector<LineDescriptor>& descriptors,
    const std::string& inputFileName,
    uint16_t reachDescriptorsBase,
    int flags,
    const McToRastrInfo& mcToRastrInfo)
{
    std::vector<uint8_t> mainMemSerializedDescriptors;
    std::array<std::vector<uint8_t>, kPagesForData> inpageSerializedDescriptors;

    int mcTicksBottom = multicolorData.data[0].mcStats.virtualTicks;
    int mcTicksTop = mcTicksBottom;
    int mcTicksNext = mcTicksBottom;

    using namespace std;
    const int imageHeight = data.data.size();
    const int rastrCodeStartAddr = getRastrCodeStartAddr(imageHeight);

    std::array<ofstream, kPagesForData> mainDataFiles;
    std::array<ofstream, kPagesForData> inpageDescrDataFiles;
    for (int i = 0; i < kPagesForData; ++i)
    {
        std::string fileName = inputFileName + "main" + std::to_string(i) + ".z80";
        mainDataFiles[i].open(fileName, std::ios::binary);
        if (!mainDataFiles[i].is_open())
        {
            std::cerr << "Can not write destination file " << fileName << std::endl;
            return -1;
        }

        fileName = inputFileName + "reach_descriptor" + std::to_string(i) + ".z80";
        inpageDescrDataFiles[i].open(fileName, std::ios::binary);
        if (!inpageDescrDataFiles[i].is_open())
        {
            std::cerr << "Can not write destination file " << fileName << std::endl;
            return -1;
        }
    }

    ofstream reachDescriptorFile;
    std::string reachDescriptorFileName = inputFileName + "mt_and_rt_reach_descriptor.z80";
    reachDescriptorFile.open(reachDescriptorFileName, std::ios::binary);
    if (!reachDescriptorFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    // serialize main data

    int size = 0;
    std::array<std::vector<uint8_t>, kPagesForData> serializedData;
    std::vector<int> lineOffset;
    for (int y = 0; y < imageHeight; ++y)
    {
        const auto& line = data.data[y];
        int pageNum = lineNumToPageNum(y, imageHeight);
        lineOffset.push_back(serializedData[pageNum].size());
        line.serialize(serializedData[pageNum]);
    }


    // serialize descriptors

    std::array<int, kPagesForData> inpageDescrOffsets;
    for (int i = 0; i < kPagesForData; ++i)
        inpageDescrOffsets[i] = rastrCodeStartAddr + serializedData[i].size();

    const int bankSize = imageHeight / 8;
    const int colorsHeight = bankSize;

    std::vector<std::pair<int, int>> lockedBlocks; //< Skip locked blocks in optimization. Just in case.

    for (int srcLine = 0; srcLine < imageHeight; ++srcLine)
    {
        int lineBank = srcLine % 8;
        int lineInBank = srcLine / 8;
        int lineNum = bankSize * lineBank + lineInBank;
        int relativeOffsetToStart = lineOffset[lineNum];

        // Do not swap DEC SP, LD REG, XX at this mode
        int pageNum = lineNumToPageNum(lineNum, imageHeight);
        Z80Parser::swap2CommandIfNeed(serializedData[pageNum], relativeOffsetToStart, lockedBlocks);
    }

    for (int d = 0; d < imageHeight; ++d)
    {
        const int srcLine = d % imageHeight;

        LineDescriptor descriptor;
        int lineBank = srcLine % 8;
        int lineInBank = srcLine / 8;
        int lineNum = bankSize * lineBank + lineInBank;

        if (flags & twoRastrDescriptors)
        {
            mcTicksBottom = getMcTicks(mcToRastrInfo, multicolorData, srcLine, Role::regularBottom);
            mcTicksTop = getMcTicks(mcToRastrInfo, multicolorData, srcLine, Role::regularTop);
            mcTicksNext = getMcTicks(mcToRastrInfo, multicolorData, srcLine, Role::nextBottom);
        }


        // Calculate timing for left/right parts in line.

        const auto& dataLine = data.data[lineNum];

        int lineEndInBank = lineInBank + 7;
        if (lineEndInBank >= bankSize)
            lineEndInBank -= bankSize;

        int fullLineEndNum = lineEndInBank + lineBank * bankSize;

        int relativeOffsetToStart = lineOffset[lineNum];

        const auto& endLine = data.data[fullLineEndNum];
        int relativeOffsetToEnd = lineOffset[fullLineEndNum] + endLine.data.size();

        if (lineEndInBank == bankSize - 1)
        {
            // There is additional JP 'first bank line'  command at the end of latest line in bank.
            // overwrite this command (if exists) instead of first bytes of the next line in bank.
            relativeOffsetToEnd -= 3;
        }

        // do Split
        const int mcDrawTicks = std::max(std::max(mcTicksBottom, mcTicksTop), mcTicksNext);

        int totalTicks = kLineDurationInTicks * 8;
        int ticksRest = totalTicks - mcDrawTicks;
        ticksRest -= kRtMcContextSwitchDelay;
        int linePreambulaTicks = 0;

        descriptor.rastrForMulticolor.af = *dataLine.inputAf;
        descriptor.rastrForOffscreen.af = *dataLine.inputAf;
        descriptor.rastrForMulticolor.updatedHlValue = dataLine.updatedHlValue();
        descriptor.pageNum = lineNumToPageNum(lineNum, imageHeight);

        if (flags & interlineRegisters)
        {
            linePreambulaTicks = descriptor.rastrForMulticolor.expectedPreambulaTicks(
                dataLine, serializedData[descriptor.pageNum], relativeOffsetToStart, lineBank);
        }
        ticksRest -= kJpFirstLineDelay; //< Jump from descriptor to the main code


        Z80Parser parser;
        int ticksLimit = ticksRest - linePreambulaTicks;
        int extraCommandsIncluded = 0;
        int descriptorsDelta = 0;

        // Aviod conflicts between MC/OFF descriptors if image packs too good.
        // Offscreen descriptor end should be at least 2 bytes later than MC descriptor end after removeTrailing stack moving.
        const static int kMinBytesForOffscreen = 7;

        descriptor.rastrForMulticolor.codeInfo = parser.parseCode(
            *dataLine.inputAf,
            *dataLine.inputRegisters,
            serializedData[descriptor.pageNum],
            relativeOffsetToStart, relativeOffsetToEnd,
            rastrCodeStartAddr,
            //[ticksLimit, &extraCommandsIncluded, &descriptorsDelta, &dataLine, &serializedData, &relativeOffsetToStart]
            [&]
        (const Z80CodeInfo& info, const z80Command& command)
            {
                int sum = info.ticks + command.ticks;
                bool success = sum == ticksLimit || sum < ticksLimit - 3;
                if (!success)
                {
                    if (sum < ticksLimit)
                    {
                        if (descriptor.rastrForMulticolor.canProlongPreambulaFor(
                            ticksLimit - sum,
                            dataLine,
                            serializedData[descriptor.pageNum],
                            relativeOffsetToStart))
                        {
                            return false; //< Continue
                        }
                    }

                    if (command.opCode == kDecSpCode || command.opCode == kAddHlSpCode || command.opCode == kLdSpHlCode)
                    {
                        ++extraCommandsIncluded;
                        descriptorsDelta += command.size;
                        return false; //< Continue
                    }
                }
                if (relativeOffsetToEnd > command.ptr && command.ptr >= relativeOffsetToEnd -  kMinBytesForOffscreen)
                {
                    #ifdef LOG_DEBUG
                        std::cout << "break due to size at line " << d << std::endl;
                    #endif
                    success = false;
                }
                if (info.spOffset > 32*8 - kMinOffscreenBytes)
                {
                    #ifdef LOG_DEBUG
                        std::cout << "break due to stack moving at line " << d << std::endl;
                    #endif
                    success = false;
                }
                return !success;
            });

        parser.swap2CommandIfNeed(serializedData[descriptor.pageNum],
            descriptor.rastrForMulticolor.codeInfo.endOffset, lockedBlocks);
        descriptor.rastrForOffscreen.codeInfo = parser.parseCode(
            *dataLine.inputAf,
            descriptor.rastrForMulticolor.codeInfo.outputRegisters,
            serializedData[descriptor.pageNum],
            descriptor.rastrForMulticolor.codeInfo.endOffset, relativeOffsetToEnd,
            rastrCodeStartAddr);

        descriptor.rastrForOffscreen.srcBuffer = srcBuffer; // +srcLine * 32;
        descriptor.rastrForOffscreen.srcLine = srcLine;


        int spDeltaSum = descriptor.rastrForMulticolor.codeInfo.spOffset + descriptor.rastrForOffscreen.codeInfo.spOffset + (dataLine.stackMovingAtStart - dataLine.minX);
        if (spDeltaSum < -256)
        {
            std::cerr << "Invalid SP delta sum " << spDeltaSum
                << " at line #" << d << ". It is bug! Expected exact value -256 or a bit above with flag optimizeLineEdge" << std::endl;
            abort();
        }


        descriptor.rastrForMulticolor.lineStartPtr = relativeOffsetToStart + rastrCodeStartAddr;
        descriptor.rastrForMulticolor.lineEndPtr = descriptor.rastrForMulticolor.codeInfo.endOffset + rastrCodeStartAddr;

        descriptor.rastrForOffscreen.lineStartPtr = descriptor.rastrForMulticolor.lineEndPtr;
        descriptor.rastrForOffscreen.lineEndPtr = relativeOffsetToEnd + rastrCodeStartAddr;
        descriptor.rastrForOffscreen.startSpDelta = -descriptor.rastrForMulticolor.codeInfo.spOffset;
        descriptor.rastrForOffscreen.startSpDelta -= dataLine.stackMovingAtStart - dataLine.minX;

        descriptor.rastrForMulticolor.removeTrailingStackMoving(extraCommandsIncluded);
        // align timing for RastrForMulticolor part

        ticksRest -= linePreambulaTicks + descriptor.rastrForMulticolor.codeInfo.ticks;
        descriptor.rastrForMulticolor.extraDelay = ticksRest;

        if (flags & optimizeLineEdge)
            descriptor.rastrForOffscreen.removeTrailingStackMoving();
        descriptor.rastrForMulticolor.makePreambulaForMC(imageHeight, serializedData[descriptor.pageNum], rastrCodeStartAddr, &dataLine, descriptor.pageNum, lineBank, mcTicksBottom, mcTicksTop, mcTicksNext);
        descriptor.rastrForOffscreen.makePreambulaForOffscreen(imageHeight, serializedData[descriptor.pageNum], rastrCodeStartAddr, descriptorsDelta);

        descriptor.rastrForMulticolor.descriptorLocationPtr = reachDescriptorsBase + mainMemSerializedDescriptors.size();
        descriptor.rastrForMulticolor.serialize(mainMemSerializedDescriptors);

        std::vector<uint8_t> tmp;
        descriptor.rastrForOffscreen.serialize(tmp);
        if (inpageDescrOffsets[descriptor.pageNum] + tmp.size() <= 65535)
        {
            descriptor.rastrForOffscreen.descriptorLocationPtr = inpageDescrOffsets[descriptor.pageNum];
            descriptor.rastrForOffscreen.serialize(inpageSerializedDescriptors[descriptor.pageNum]);
            inpageDescrOffsets[descriptor.pageNum] += tmp.size();
        }
        else
        {
            descriptor.rastrForOffscreen.descriptorLocationPtr = reachDescriptorsBase + mainMemSerializedDescriptors.size();
            descriptor.rastrForOffscreen.serialize(mainMemSerializedDescriptors);
        }

        descriptors.push_back(descriptor);
    }

    for (int d = 0; d < imageHeight; ++d)
    {
        int lineBank = d % 8;
        int lineInBank = d / 8;
        int lineNum = bankSize * lineBank + lineInBank;

        int pageNum = lineNumToPageNum(lineNum, imageHeight);
        auto& descriptor = descriptors[d];
        descriptor.rastrForMulticolor.setEndBlock(serializedData[pageNum].data()
            + descriptor.rastrForMulticolor.lineEndPtr - rastrCodeStartAddr);
        descriptor.rastrForOffscreen.setEndBlock(serializedData[pageNum].data()
            + descriptor.rastrForOffscreen.lineEndPtr - rastrCodeStartAddr);
    }

    for (int i = 0; i < kPagesForData; ++i)
    {
        mainDataFiles[i].write((const char*)serializedData[i].data(), serializedData[i].size());
        inpageDescrDataFiles[i].write((const char*)inpageSerializedDescriptors[i].data(), inpageSerializedDescriptors[i].size());
    }
    reachDescriptorFile.write((const char*)mainMemSerializedDescriptors.data(), mainMemSerializedDescriptors.size());

    int serializedSize = mainMemSerializedDescriptors.size();
    return serializedSize;
}

int serializeColorData(
    std::vector<ColorDescriptor>& descriptors,
    const CompressedData& colorData, const std::string& inputFileName, uint16_t codeOffset)
{
    using namespace std;

    int imageHeight = colorData.data.size();

    ofstream colorDataFile;
    std::string colorDataFileName = inputFileName + "color.z80";
    colorDataFile.open(colorDataFileName, std::ios::binary);
    if (!colorDataFile.is_open())
    {
        std::cerr << "Can not write color file" << std::endl;
        return -1;
    }

    ofstream descriptorFile;
    std::string descriptorFileName = inputFileName + "color_descriptor.dat";
    descriptorFile.open(descriptorFileName, std::ios::binary);
    if (!descriptorFile.is_open())
    {
        std::cerr << "Can not write color destination file" << std::endl;
        return -1;
    }

    // serialize color data

    std::vector<uint8_t> serializedData;
    std::vector<int> lineOffset;

    for (int i = 0; i < colorData.data.size(); ++i)
    {
        const auto& line = colorData.data[i];
        lineOffset.push_back(serializedData.size());
        line.serialize(serializedData);
    }

    const int reachDescriptorsBase = codeOffset + serializedData.size();
    std::vector<uint8_t> serializedDescriptors;

    // serialize color descriptors
    for (int d = 0; d <= imageHeight; ++d)
    {
        const int srcLine = d % imageHeight;
        const int endLine = (d + 24) % imageHeight;
        const auto& line = colorData.data[srcLine];

        ColorDescriptor descriptor;

        descriptor.addressBegin = lineOffset[srcLine] + codeOffset;

        Register16 af("af");
        if (colorData.flags & interlineRegisters)
        {
            if (d < imageHeight)
            {
                int omitedSize = 0;
                auto regs = line.getUsedRegisters();
                descriptor.preambula = getSerializedRegisters(regs, af);
                auto updatedHlValue = line.updatedHlValue();
                if (updatedHlValue)
                {
                    if (auto existingHl = findRegister(regs, "hl"))
                    {
                        std::cerr << "Reg HL should be null here" << std::endl;
                        abort();
                    }
                    omitedSize = 3;
                    Register16 hl("hl");
                    hl.loadXX(descriptor.preambula, *updatedHlValue);
                    descriptor.preambula.drawTicks -= 10; //< Don't count it becase it omit same LD HL, XX in the main data.
                }
                else if (imageHeight == 24)
                {
                    // Put first commands to the preambula
                    auto firstCommands = Z80Parser::getCode(line.data.buffer(), 2);
                    omitedSize = firstCommands.size();
                    descriptor.preambula.append(firstCommands);
                }

                descriptor.preambula.jp(descriptor.addressBegin + omitedSize);
                descriptor.addressBegin = reachDescriptorsBase + serializedDescriptors.size();
                descriptor.preambula.serialize(serializedDescriptors);
            }
            else
            {
                descriptor = descriptors[0];
            }
        }

        if (endLine == 0)
            descriptor.addressEnd = serializedData.size() - 3 + codeOffset;
        else
            descriptor.addressEnd = lineOffset[endLine] + codeOffset;

        descriptors.push_back(descriptor);
    }

    for (int i = descriptors.size() - 1; i >= 0; --i)
    {
        const auto& descriptor = descriptors[i];
        descriptorFile.write((const char*)&descriptor.addressBegin, sizeof(descriptor.addressBegin));
        descriptorFile.write((const char*)&descriptor.addressEnd, sizeof(descriptor.addressEnd));
    }

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());
    colorDataFile.write((const char*)serializedDescriptors.data(), serializedDescriptors.size());

    return serializedData.size() + serializedDescriptors.size();
}

template <typename... Values>
int fileSizeSum(const std::string& inputFileName, Values&&... values)
{
    int result = 0;
    const std::string data[]{ values... };
    for (const auto& name: data)
    {
        std::string fileName = inputFileName + name;
        std::ifstream fileIn;
        fileIn.open(fileName, std::ios::binary);
        if (!fileIn.is_open())
        {
            std::cerr << "Can not open source file " << fileName << std::endl;
            return -1;
        }

        fileIn.seekg(0, std::ios::end);
        int fileSize = fileIn.tellg();
        result += fileSize;
    }
    return result;
}

void serializeAsmFile(
    const std::string& inputFileName,
    const CompressedData& rastrData,
    const CompressedData& colorData,
    const CompressedData& multicolorData,
    int rastrFlags,
    const Labels& labels,
    bool hasPlayer)
{
    using namespace std;

    ofstream phaseFile;
    std::string phaseFileName = inputFileName + "labels.asm";
    phaseFile.open(phaseFileName);
    if (!phaseFile.is_open())
    {
        std::cerr << "Can not write asm file" << std::endl;
        return;
    }

    int colorHeight = multicolorData.data.size();
    int imageHeight = colorHeight * 8;

    phaseFile << "COLOR_REG_AF2             EQU    #" << std::hex;
    if (!colorData.data[0].inputAf->isEmpty())
        phaseFile << colorData.data[0].inputAf->value16();
    else
        phaseFile << 0;
    phaseFile << std::dec << std::endl;

    phaseFile << "FIRST_LINE_DELAY          EQU    " << multicolorData.data[colorHeight-1].mcStats.pos << std::endl;
    phaseFile << "UNSTABLE_STACK_POS        EQU    "
        << ((rastrFlags & optimizeLineEdge) ? 1 : 0)
        << std::endl;
    phaseFile << "RAM2_UNCOMPRESSED_SIZE   EQU    " << labels.mt_and_rt_reach_descriptor + labels.multicolor << std::endl;

    phaseFile << "RAM0_UNCOMPRESSED_SIZE   EQU    " <<
        fileSizeSum(inputFileName, "jpix0.dat", "timings0.dat", "main0.z80", "reach_descriptor0.z80") << std::endl;
    phaseFile << "RAM1_UNCOMPRESSED_SIZE   EQU    " <<
        fileSizeSum(inputFileName, "jpix1.dat", "timings1.dat", "main1.z80", "reach_descriptor1.z80") << std::endl;
    phaseFile << "RAM3_UNCOMPRESSED_SIZE   EQU    " <<
        fileSizeSum(inputFileName, "jpix2.dat", "timings2.dat", "main2.z80", "reach_descriptor2.z80") << std::endl;
    phaseFile << "RAM4_UNCOMPRESSED_SIZE   EQU    " <<
        fileSizeSum(inputFileName, "jpix3.dat", "timings3.dat", "main3.z80", "reach_descriptor3.z80") << std::endl;

    phaseFile << "HAS_PLAYER   EQU    " << (hasPlayer ? 1 : 0) << std::endl;
    phaseFile << "imageHeight   EQU    " << imageHeight << std::endl;
}

int serializeMultiColorData(
    const CompressedData& data,
    const std::string& inputFileName, uint16_t codeOffset)
{
    using namespace std;

    ofstream colorDataFile;
    std::string colorDataFileName = inputFileName + "multicolor.z80";
    colorDataFile.open(colorDataFileName, std::ios::binary);
    if (!colorDataFile.is_open())
    {
        std::cerr << "Can not write color file" << std::endl;
        return -1;
    }

    int colorImageHeight = data.data.size();

    int size = 0;
    std::vector<uint8_t> serializedData;
    std::vector<int> lineOffset;
    //std::vector<int> ldHlOffset;
    for (int y = 0; y < colorImageHeight; ++y)
    {
        int address = codeOffset + serializedData.size();
        if (address % 256 == 254)
        {
            serializedData.push_back(0x00); // align
            ++address;
        }
        if (address % 256 == 255)
            serializedData.push_back(0x00); // align


        // add LD SP command in the begin of a line
        serializedData.push_back(0x31);
        serializedData.push_back(0x00);
        serializedData.push_back(0x00);

        lineOffset.push_back(serializedData.size());
        auto& line = data.data[y];

        line.serialize(serializedData);

        // add JP XX command in the end of a line
        serializedData.push_back(0xc3);
        serializedData.push_back(0x00);
        serializedData.push_back(0x00);
    }

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    // serialize multicolor descriptors

    std::vector<MulticolorDescriptor> descriptors;
    for (int d = -1; d < colorImageHeight + 23; ++d)
    {
        const int srcLine = d >= 0 ? d % colorImageHeight : colorImageHeight-1;

        MulticolorDescriptor descriptor;
        const auto& line = data.data[srcLine];
        const uint16_t lineAddressPtr = lineOffset[srcLine] + codeOffset;
        descriptor.addressBegin = lineAddressPtr - 3; // extra LD SP before line

        if (line.spPosHints[1] >= 0)
        {
            descriptor.moveSp2BytePos = lineAddressPtr + line.spPosHints[1] + 1;
            if (line.spPosHints[0] == -1)
            {
                std::cerr << "Invalid spPos2 value. Missing spPos1 value!";
                abort();
            }
        }

        if (line.spPosHints[0] >= 0)
        {
            if (line.data.buffer()[line.spPosHints[0]] != 0x31)
            {
                std::cerr << "Invalid spPos hint. Some bug!";
                abort();
            }
            descriptor.moveSpBytePos = lineAddressPtr + line.spPosHints[0] + 1;
            uint16_t value16 = line.data.buffer()[line.spPosHints[0] + 1];
            descriptor.moveSpDelta = value16 - 32;
        }

        if (descriptor.moveSpDelta == 0)
        {
            std::swap(descriptor.moveSp2BytePos, descriptor.moveSpBytePos);
            descriptor.moveSpDelta = -4;
        }


        descriptor.endLineJpAddr = lineAddressPtr + line.data.size() + 1; // Line itself doesn't contains JP XX
        descriptors.push_back(descriptor);
    }

    ofstream file;
    std::string fileName = inputFileName + "mc_descriptors.dat";
    file.open(fileName, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    for (const auto& descriptor : descriptors)
    {
        file.write((const char*)&descriptor, sizeof(descriptor));
    }


    return serializedData.size();
}

int serializeRastrDescriptors(
    std::vector<LineDescriptor> descriptors,
    const std::string& inputFileName)
{
    using namespace std;
    int imageHeight = descriptors.size();


    ofstream spDeltaFile, offRastrFile, mcRastrFileBottom, mcRastrFileTop, mcRastrFileNext;
    {
        std::string fileName = inputFileName + "sp_delta_descriptors.dat";
        spDeltaFile.open(fileName, std::ios::binary);
        if (!spDeltaFile.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }
    {
        std::string fileName = inputFileName + "off_rastr_descriptors.dat";
        offRastrFile.open(fileName, std::ios::binary);
        if (!offRastrFile.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

    {
        std::string fileName = inputFileName + "mc_rastr_bottom_descriptors.dat";
        mcRastrFileBottom.open(fileName, std::ios::binary);
        if (!mcRastrFileBottom.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

    {
        std::string fileName = inputFileName + "mc_rastr_top_descriptors.dat";
        mcRastrFileTop.open(fileName, std::ios::binary);
        if (!mcRastrFileTop.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

    {
        std::string fileName = inputFileName + "mc_rastr_next_descriptors.dat";
        mcRastrFileNext.open(fileName, std::ios::binary);
        if (!mcRastrFileNext.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

    for (int i = 0; i < imageHeight + kmaxDescriptorOffset; ++i)
    {
        int line = i % imageHeight;
        const auto& descriptor = descriptors[line];

        uint16_t locationBottom = descriptor.rastrForMulticolor.descriptorLocationPtr + descriptor.rastrForMulticolor.bottomMcOffset;
        uint16_t locationTop = descriptor.rastrForMulticolor.descriptorLocationPtr + descriptor.rastrForMulticolor.topMcOffset;
        uint16_t locationNext = descriptor.rastrForMulticolor.descriptorLocationPtr + descriptor.rastrForMulticolor.nextMcOffset;

        mcRastrFileBottom.write((const char*) &locationBottom, 2);
        mcRastrFileTop.write((const char*) &locationTop, 2);
        mcRastrFileNext.write((const char*)&locationNext, 2);
    }

    for (int i = -7; i < imageHeight + kmaxDescriptorOffset; ++i)
    {
        int line = i < 0 ? i + imageHeight : i;
        line = line % imageHeight;
        const auto& descriptor = descriptors[line];
        offRastrFile.write((const char*)&descriptor.rastrForOffscreen.descriptorLocationPtr, 2);


        int nextLine = (line + 64) % imageHeight;
        const auto& nextDescriptor = descriptors[nextLine];

        uint8_t value1 = 256 - (uint8_t)descriptor.rastrForOffscreen.startSpDelta;
        uint8_t value2 = 256 - (uint8_t)nextDescriptor.rastrForOffscreen.startSpDelta;
        uint16_t value = value1 + 256 * value2;
        spDeltaFile.write((const char*) &value, 2);
    }
}

struct OffscreenTicks
{
    int preambulaTicks = 0;
    int payloadTicks = 0;
    int enterExitTicks = 0;
    std::vector<int> preambulaTicksDetails;

    int ticks() const
    {
        return preambulaTicks + payloadTicks + enterExitTicks;
    }

    OffscreenTicks& operator+=(const OffscreenTicks& other)
    {
        preambulaTicks += other.preambulaTicks;
        payloadTicks += other.payloadTicks;
        enterExitTicks += other.enterExitTicks;
        preambulaTicksDetails.insert(preambulaTicksDetails.end(),
            other.preambulaTicksDetails.begin(), other.preambulaTicksDetails.end());
        return *this;
    }

    void addPreambulaTicks(int value)
    {
        preambulaTicks += value;
        preambulaTicksDetails.push_back(value);
    }

    std::string getPreambulaDetail() const
    {
        std::string result;
        for (int value : preambulaTicksDetails)
        {
            if (!result.empty())
                result += ' ';
            result += std::to_string(value);
        }
        return result;
    }

};

OffscreenTicks getTicksChainFor64Line(
    const std::vector<LineDescriptor>& descriptors,
    int screenLineNum)
{
    const int imageHeight = descriptors.size();
    screenLineNum = screenLineNum % imageHeight;

    const int bankSize = imageHeight / 8;

    OffscreenTicks result;

    for (int b = 0; b < 8; ++b)
    {
        int lineNumber = (screenLineNum + b) % imageHeight;
        const auto& d = descriptors[lineNumber];

        //int sum = 0;
        auto info = Z80Parser::parseCode(d.rastrForOffscreen.af, d.rastrForOffscreen.preambula);
        result.addPreambulaTicks(info.ticks);
        result.payloadTicks -= d.rastrForOffscreen.omitedDataInfo.ticks; //< These ticks are ommited to execute after jump to descriptor.
        result.payloadTicks += d.rastrForOffscreen.codeInfo.ticks;
    }
    return result;
}

int getMcRastrTicksChainFor64Line(
    const std::vector<LineDescriptor>& descriptors,
    int screenLineNum, bool isNextFrame)
{
    const int imageHeight = descriptors.size();
    screenLineNum = screenLineNum % imageHeight;

    if (isNextFrame)
        screenLineNum = screenLineNum > 0 ? screenLineNum - 1 : imageHeight - 1; //< Go prevLine

    const int bankSize = imageHeight / 8;

    int result = 0;

    for (int b = 0; b < 8; ++b)
    {
        int lineNumber = (screenLineNum + b) % imageHeight;

        const auto& d = descriptors[lineNumber];

        auto preambula = d.rastrForMulticolor.preambula;
        preambula.erase(preambula.begin(), preambula.begin() +
            (isNextFrame ? d.rastrForMulticolor.nextMcOffset : d.rastrForMulticolor.bottomMcOffset));
        auto info = Z80Parser::parseCode(d.rastrForMulticolor.af, preambula);

        int r = 0;
        r += info.ticks;
        r -= d.rastrForMulticolor.omitedDataInfo.ticks; //< These ticks are ommited to execute after jump to descriptor.
        r += d.rastrForMulticolor.codeInfo.ticks;
        result += r;
    }
    return result;
}

int getColorTicksForWholeFrame(
    const std::vector<ColorDescriptor>& colorDescriptors,
    const CompressedData& data, int lineNum)
{
    int result = 0;
    const int imageHeight = data.data.size();

    for (int i = 0; i < 24; ++i)
    {
        int l = (lineNum + i) % imageHeight;
        const auto& line = data.data[l];
        result += line.drawTicks;
    }

    if (data.flags & interlineRegisters)
        result += colorDescriptors[lineNum].preambula.drawTicks;

    // End line contains JP <first line> command. If drawing is stoppeed on the end line this command is not executed.
    int endLine = (lineNum + 24) % imageHeight;
    if (endLine == 0)
        result -= kJpFirstLineDelay;

    return result;
}

int getTicksChainForMc(
    int line,
    const CompressedData& multicolor)
{
    int colorHeight = multicolor.data.size();

    int result = 192 * kLineDurationInTicks;
    int colorLine = line / 8;
    int prevColorLine = colorLine > 0 ? colorLine - 1 : colorHeight - 1;
    int lastColorLine = (prevColorLine + 24) % colorHeight;
    int dt = multicolor.data[prevColorLine].mcStats.pos - multicolor.data[lastColorLine].mcStats.pos;
    result += dt;
    return result;
}

int getMulticolorOnlyTicks(
    int mcLine,
    const CompressedData& multicolor)
{
    int colorHeight = multicolor.data.size();

    int result = 0;
    for (int i = 0; i < 24; ++i)
    {
        int line = (mcLine + i) % colorHeight;
        result += multicolor.data[line].drawTicks;
    }
    return result;
}

int getRealTicksChainForMc(
    int line,
    const CompressedData& multicolor)
{
    int colorHeight = multicolor.data.size();
    int mcLine = ((line + 0) / 8) % colorHeight;
    int result = 0;
    for (int i = 0; i < 24; ++i)
    {
        result += multicolor.data[mcLine].drawTicks;
        mcLine++;
        if (mcLine == colorHeight)
            mcLine = 0;
    }
    return result;
}

int prev_frame_line_23_overrun(
    const std::vector<LineDescriptor>& descriptors,
    const CompressedData& multicolor,
    int line)
{
    const int imageHeight = descriptors.size();
    const int colorHeight = imageHeight / 8;

    int prevLine = (line + 1) % imageHeight;

    int line23 = (prevLine + 127) % imageHeight;

    const auto& d = descriptors[line23];

    auto preambula = d.rastrForMulticolor.preambula;
    preambula.erase(preambula.begin(), preambula.begin() + d.rastrForMulticolor.nextMcOffset);
    auto info = Z80Parser::parseCode(d.rastrForMulticolor.af, preambula);

    int r = 0;
    r += info.ticks;
    r -= d.rastrForMulticolor.omitedDataInfo.ticks; //< These ticks are ommited to execute after jump to descriptor.
    r += d.rastrForMulticolor.codeInfo.ticks;

    int multicolorNum = line / 8;
    if (multicolorNum < 0)
        multicolorNum = colorHeight - 1;
    int mcTicks = multicolor.data[multicolorNum].mcStats.virtualTicks;
    int dt = mcTicks + r + kRtMcContextSwitchDelay - kLineDurationInTicks * 8;

#ifdef define LOG_DEBUG
    std::cout << "line=" << line << " 23 overrun=" << dt << std::endl;
#endif

    return dt;
}

std::vector<int> getEffectDelayInternalForRun1(int imageHeight)
{
    std::vector<int> result;

    // Effect1 delay
    result.push_back(3718+10);
    for (int i = imageHeight/4 - 35 - 1; i > 0; --i)
        result.push_back(51);

    // Last step Effect1 delay
    result.push_back(71 + 1441);

    // Effect2 delay
    for (int i = 1; i < 31; ++i)
    {
        if (i % 4 != 3)
            result.push_back(1441);
        else
            result.push_back(1441 - 6 + 79);
    }
    result.push_back(1441 - 6 + 79 - 6 + 36);

    // Effect3 delay
    result.push_back(3140);

    // Effect3_2 delay
    result.push_back(6536+11);
    result.push_back(6567+10);

    // Effect 4

    while (result.size() < imageHeight / 2)
    {
        result.push_back(6162 + 11);
        for (int k = 0; k < 3; ++k)
        {
            result.push_back(6107 + 11);
            result.push_back(6109 + 11);
        }
        result.push_back(6170 + 10);
    }


    return result;
}

std::vector<int> getEffectDelayInternalForRun2(int imageHeight)
{
    std::vector<int> result;
    while (result.size() < imageHeight / 2)
    {
        result.push_back(6162 + 11);
        for (int k = 0; k < 3; ++k)
        {
            result.push_back(6107+11);
            result.push_back(6109+11);
        }
        result.push_back(6170+10);
    }

    return result;
}

std::vector<int> getEffectDelayInternalForRun(int runNumber, int imageHeight)
{
    if (runNumber == 1)
        return getEffectDelayInternalForRun1(imageHeight);
    else if (runNumber == 2)
        return getEffectDelayInternalForRun2(imageHeight);
    return std::vector<int>();
}

int effectRegularStepDelay(
    int rastrFlags,
    const std::vector<LineDescriptor>& descriptors,
    const std::vector<ColorDescriptor>& colorDescriptors,
    const CompressedData& data,
    const CompressedData& color,
    const CompressedData& multicolor,
    int runNumber,
    int line,
    std::vector<int>* effectDelay)
{
    switch (line % 8)
    {
        case 0:
            return 0;
        case 1:
        case 3:
        case 5:
        case 7:
        {
            if (runNumber == 0 || runNumber == 3)
                return 11;

            // 7-th bank page here with logo
            int colorTicks = getColorTicksForWholeFrame(colorDescriptors, color, (line + 7) / 8);
            int result = 0;
            result -= colorTicks;
            result -= 42; // Call draw color ticks
            result -= getMulticolorOnlyTicks(line/8, multicolor);
            result -= (kRtMcContextSwitchDelay - 72) * 24; // In rastr only mode context swithing is faster
            result += 205+77+18+21-22+4+29+10 + 33; // page 7 branch itself is longer
            result += 23 * 2;
            result -= 8;
            result += 20 * 2;
            result += 17 * 2;
            result += 2;
            result += 20 + 17 + 26 + 26;
            result += 10;
            result += *effectDelay->rbegin();
            effectDelay->pop_back();

            return result;
        }
        case 2:
        case 4:
        case 6:
            return 11;
    }
}

int serializeTimingDataForRun(
    const std::vector<LineDescriptor>& descriptors,
    const std::vector<ColorDescriptor>& colorDescriptors,
    const CompressedData& data,
    const CompressedData& color,
    const CompressedData& multicolor,
    int flags,
    const std::vector<int>& musicTimings,
    int& worseMcLineTicks,
    int& worseLineTicks,
    int& worseLineNum,
    int& minSpecialTicks,
    std::vector<uint16_t>& outputData,
    int runNumber,
    bool silenceMode)
{
    using namespace std;

    const int imageHeight = data.data.size();
    const int colorHeight = imageHeight / 8;
    bool hasPlayer = !musicTimings.empty();

    std::vector<int> effectDelay = getEffectDelayInternalForRun(runNumber, imageHeight);

    int firstLineDelay = 0;
    for (int line = 0; line < imageHeight; ++line)
    {
        // offscreen rastr
        OffscreenTicks offscreenTicks;
        for (int i = 0; i < 3; ++i)
            offscreenTicks += getTicksChainFor64Line(descriptors, line + i * 64);

        // colors
        int ticks = 0;
        ticks += offscreenTicks.ticks();
        int colorTicks = getColorTicksForWholeFrame(colorDescriptors, color, (line + 7) / 8);
        ticks += colorTicks;

        int mcTicks = getTicksChainForMc(line, multicolor);
        int mcOverhead = mcTicks - kLineDurationInTicks * 192;

        if (line % 8 == 0)
        {
            int mcRastrTicks = kRtMcContextSwitchDelay * 24;
            for (int i = 0; i < 3; ++i)
                mcRastrTicks += getMcRastrTicksChainFor64Line(descriptors, line + i * 64, i == 2);
            mcRastrTicks += mcOverhead; //< Shift next frame drawing to the new MC virtual ticks phase.

            ticks += mcRastrTicks;

            // Update MC phase for line
            int curMcLine = (line / 8) % colorHeight;
            int curMcLastLine = (curMcLine+23) % colorHeight;
            int nextMcLastLine = curMcLastLine > 0 ? curMcLastLine - 1 : colorHeight - 1;
            ticks -= multicolor.data[nextMcLastLine].mcStats.pos - multicolor.data[curMcLastLine].mcStats.pos;

            ticks -= kLineDurationInTicks * 7; //< Draw next frame longer in  6 lines
        }
        else
        {
            if (line % 8 == 7)
                ticks += kLineDurationInTicks * 192;
            else
            {
                ticks += mcTicks;
                ticks += prev_frame_line_23_overrun(descriptors, multicolor, line);
            }
            ticks += kLineDurationInTicks;  //< Draw next frame faster in  1 lines

            if (!hasPlayer)
                ticks -= 7;
        }

        int kZ80CodeDelay = 2309;

        if (line % 8 == 0)
        {
            kZ80CodeDelay += 7404;
            if (line == 0)
            {
                kZ80CodeDelay += 47;
                if (hasPlayer)
                    kZ80CodeDelay += 68;
            }
        }

        // offscreen drawing branches has different length
        switch (line % 8)
        {
            case 1:
                kZ80CodeDelay -= 4;
                break;
            case 2:
                kZ80CodeDelay -= 3; // SET_NEXT_STEP_SHORT here
                break;
            case 3:
                kZ80CodeDelay -= 6;
                break;
            case 4:
                kZ80CodeDelay -= 4;
                break;
            case 5:
                kZ80CodeDelay -= 10;
                break;
            case 6:
                kZ80CodeDelay -= 20;
                break;
            case 7:
                kZ80CodeDelay -= 17;
                kZ80CodeDelay -= 3;    // SET_NEXT_STEP_SHORT here
                break;
        }
        int specialTicks =  effectRegularStepDelay(
            flags,
            descriptors,
            colorDescriptors,
            data,
            color,
            multicolor,
            runNumber, line,
            &effectDelay);
        kZ80CodeDelay += specialTicks;


        ticks += kZ80CodeDelay;
        if (flags & optimizeLineEdge)
            ticks += 10 * 23; // LD SP, XX in each line

        if (!musicTimings.empty())
        {
            int playerDelay = kCallPlayerDelay;
            int inversedLine = (imageHeight - line) % imageHeight;
            if (musicTimings.size() > inversedLine)
                playerDelay += musicTimings[inversedLine];
            ticks += playerDelay;
        }

        int freeTicks = totalTicksPerFrame - ticks;
        const int minDelay = kMinDelay;
        if (freeTicks < minDelay && !silenceMode)
        {
            std::cout << "WARNING: Low free ticks. line #" << line << ". free=" << freeTicks << ". minDelay=" << minDelay
                << ". Not enough " << minDelay - freeTicks << " ticks. "
                << "Color=" << colorTicks
                << ". preambula=" << offscreenTicks.preambulaTicks
                << ". offRastr=" << offscreenTicks.payloadTicks
                << std::endl;
        }
        else
        {
#ifdef LOG_DEBUG
            std::cout << "INFO: Ticks rest at line #" << line << ". free=" << freeTicks
                << " color=" << colorTicks
                << ". preambula=" << offscreenTicks.preambulaTicks << ".\t" << offscreenTicks.getPreambulaDetail()
                << ". off rastr=" << offscreenTicks.payloadTicks
                << std::endl;
#endif
        }
        if (freeTicks < worseLineTicks)
        {
            worseLineTicks = freeTicks;
            worseLineNum = line + runNumber * imageHeight;
        }
        if (line % 8 == 0)
        {
            if (freeTicks < worseMcLineTicks)
                worseMcLineTicks = freeTicks;
        }
        const uint16_t freeTicks16 = uint16_t(std::max(kMinDelay, freeTicks));
        outputData.push_back(freeTicks16);

        if (specialTicks != 0 && specialTicks != 11)
            minSpecialTicks = std::min(freeTicks, minSpecialTicks);

    }

    return firstLineDelay;
}

int serializeTimingData(
    const std::vector<LineDescriptor>& descriptors,
    const std::vector<ColorDescriptor>& colorDescriptors,
    const CompressedData& data,
    const CompressedData& color,
    const CompressedData& multicolor,
    const std::string& inputFileName,
    int flags,
    std::vector<int> musicTimings,
    bool silenceMode)
{
    const int imageHeight = data.data.size();

    // Align music timings by imageHeight
    int pages = 4;
    if (!musicTimings.empty())
    {
        if (musicTimings.size() > imageHeight * 4)
        {
            musicTimings.resize(imageHeight * 4);
        }
        else
        {
            int index = 0;
            while (musicTimings.size() < imageHeight * 4)
                musicTimings.push_back(index++);
            if (index == imageHeight * 4)
                index = 0;
        }
    }


    int worseLineTicks = std::numeric_limits<int>::max();
    int worseMcLineTicks = std::numeric_limits<int>::max();

    int worseLineNum = 0;
    int minSpecialTicks = std::numeric_limits<int>::max();
    std::vector<uint16_t> delayTicks;
    for (int runNumber = 0; runNumber < pages; ++runNumber)
    {
        std::vector<int> musicPage;
        if (!musicTimings.empty())
            musicPage = std::vector<int>(musicTimings.begin() + runNumber * imageHeight, musicTimings.begin() + (runNumber + 1) * imageHeight);

        serializeTimingDataForRun(
            descriptors,
            colorDescriptors,
            data,
            color,
            multicolor,
            flags,
            musicPage,
            worseMcLineTicks,
            worseLineTicks,
            worseLineNum,
            minSpecialTicks,
            delayTicks,
            runNumber,
            silenceMode);
    }

    if (!silenceMode)
    {
        std::cout << "worse ticks for effects=" << minSpecialTicks << std::endl;
        std::cout << "worse MC line free ticks=" << worseMcLineTicks << std::endl;
        std::cout << "worse line free ticks=" << worseLineTicks << " at pos=" << worseLineNum << ". ";
    }
    const int minDelay = kMinDelay;
    if (worseLineTicks < minDelay && !silenceMode)
    {
        std::cout << "Not enough ticks:" << minDelay - worseLineTicks;
        std::cout << std::endl;
    }

    std::vector<std::vector<int>> musicPages;
    musicPages.resize(4);
    for (auto& pageData : musicPages)
        pageData.resize(imageHeight + 6);

    // Interleave timings data to 4 pages
    int index = 0;
    for (int run= 0; run < musicPages.size(); ++run)
    {
        for (int i = 0; i < imageHeight; ++i)
        {
            int page = (i % 8) / 2;
            int index = i + run*2;
            musicPages[page][index] = delayTicks[i + run*imageHeight];
        }
    }

    uint16_t v = musicPages[0][0];
    for (int i = 0; i < 3; ++i)
        musicPages[0][i*2] = musicPages[0][(i+1)*2];
    musicPages[0][3*2] = v;

    using namespace std;
    for (int page = 0; page < musicPages.size(); ++page)
    {
        ofstream timingDataFile;
        std::string timingDataFileName = inputFileName + std::string("timings") + std::to_string(page) + ".dat";
        timingDataFile.open(timingDataFileName, std::ios::binary);
        if (!timingDataFile.is_open())
        {
            std::cerr << "Can not write timing file " << timingDataFileName << std::endl;
            return -1;
        }

        for (int i = 0; i < musicPages[page].size(); ++i)
        {
            uint16_t freeTicks16 = musicPages[page][i];
            timingDataFile.write((const char*)&freeTicks16, sizeof(freeTicks16));
        }
    }

    return worseLineTicks;
}

int serializeJpIxDescriptors(
    const std::vector<LineDescriptor>& descriptors,
    const std::string& inputFileName)
{
    using namespace std;

    const int imageHeight = descriptors.size();
    std::array<ofstream, kPagesForData> jpIxDescriptorFiles;
    for (int i = 0; i < kPagesForData; ++i)
    {
        std::string jpIxDescriptorFileName = inputFileName + "jpix" + std::to_string(i) + ".dat";
        jpIxDescriptorFiles[i].open(jpIxDescriptorFileName, std::ios::binary);
        if (!jpIxDescriptorFiles[i].is_open())
        {
            std::cerr << "Can not write destination file" << std::endl;
            return -1;
        }
    }
    const auto jpIxDescr = createWholeFrameJpIxDescriptors(descriptors);
    for (int i = 0; i < jpIxDescr.size(); ++i)
    {
        const auto& d = jpIxDescr[i];
        const int page = d.write[0].pageNum;
        for (int i = 0; i < 2; ++i)
        {
            const auto data = d.restore[i];
            jpIxDescriptorFiles[page].write((const char*)&data.address, sizeof(uint16_t));
            jpIxDescriptorFiles[page].write((const char*)data.originData.data(), data.originData.size());
        }
        for (int i = 0; i < 2; ++i)
            jpIxDescriptorFiles[page].write((const char*)&d.write[i].address, sizeof(uint16_t));

    }


#ifdef CREATE_JPIX_HELPER
    ofstream updateJpixHelper;
    {
        std::string fileName = inputFileName + "update_jpix_helper.dat";
        updateJpixHelper.open(fileName, std::ios::binary);
        if (!updateJpixHelper.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }
#endif

    for (unsigned i = 0; i < imageHeight; ++i)
    {
        if (i % 8 > 1)
            continue; //< Same pages have same data

        int recordSize = 12;
        int banks = (imageHeight / 64) + 2;
        int fullRecordSize = recordSize * banks;

        int outerNum = i / 64;
        int innerNum = i % 64;
        innerNum = innerNum & ~6;
        innerNum = (innerNum >> 2) + (i & 1);

        uint16_t offset = innerNum * fullRecordSize + outerNum * recordSize;
        uint16_t address = rastrCodeStartAddrBase + offset;
#ifdef  CREATE_JPIX_HELPER
        updateJpixHelper.write((const char*)&address, sizeof(uint16_t));
#endif
        address += rastrCodeStartAddrBase;
    }


    return 0;
}

std::vector<std::string> parsedSldLIne(const std::string& line)
{
    size_t prevPos = 0;
    size_t pos = 0;
    std::vector<std::string> result;
    std::string delimiter = "|";
    while ((pos = line.find(delimiter, prevPos)) != std::string::npos)
    {
        std::string token = line.substr(prevPos, pos - prevPos);
        result.push_back(token);
        prevPos = pos + delimiter.length();
    }
    if (prevPos < line.size())
        result.push_back(line.substr(prevPos));
    return result;
}

int parseSldFile(const std::string& sldFileName)
{
    using namespace std;
    ifstream file;
    file.open(sldFileName);
    if (!file.is_open())
    {
        std::cerr << "Can not read sld file " << sldFileName << std::endl;
        return -1;
    }
    string line;
    while (getline(file, line))
    {
        auto params = parsedSldLIne(line);
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i] == "generated_code" && i >= 2)
            {
                int result = std::stoi(params[i - 2]);
                return result;
            }
        }
    }
    return -1;
}

__forceinline bool ends_with(std::string const& value, std::string const& ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


void runCompressor(const std::string& outFileName)
{
    const auto command = outFileName + "compress.bat";
    system(command.data());
}

std::vector<uint8_t> interlaceBuffer(const std::vector<uint8_t>& buffer)
{
    int height = buffer.size() / 32;
    int parts = height / 64;
    std::vector<uint8_t> result;
    for (int k = 0; k < parts; ++k)
    {
        for (int y2 = 0; y2 < 8; ++y2)
        {
            for (int y1 = 0; y1 < 64; y1 += 8)
            {
                auto itr = buffer.begin() + (y2 + y1 + k*64) * 32;
                result.insert(result.end(), itr, itr + 32);
            }
        }
    }
    return result;
}

std::vector<uint8_t> getDeltaForFirstScreen(std::vector<uint8_t> buffer, int screenHeight, int direction,
    const std::vector<uint8_t>* colorData,
    const std::vector<LineDescriptor>* descriptors)
{
    std::vector<uint8_t> result;

    int imageHeight = buffer.size() / 32;


    for (int y = imageHeight - screenHeight; y < imageHeight; ++y)
    {
        const uint8_t* curLine = buffer.data() + y * 32;

        int prevLineNum = y + direction;
        if (prevLineNum < 0)
            prevLineNum = imageHeight - 1;
        else if (prevLineNum == imageHeight)
            prevLineNum = 0;
        const uint8_t* prevLine = buffer.data() + prevLineNum * 32;

        for (int x = 0; x < 32; ++x)
        {
            bool forceToSave = false; // colorData&& isHiddenData(colorData->data(), x, y / 8);

            if (colorData && isHiddenData(colorData->data(), x, y / 8))
                forceToSave = true;

            if (screenHeight == 192 && y < imageHeight - screenHeight + 64)
            {
                int yDelta = y - (imageHeight - screenHeight);
                int bank = 7 - (yDelta % 8);

                auto d = descriptors->at(128 + bank).rastrForOffscreen;
                int offset = (7 - yDelta/8) * 32 + (31 - x);
                if (offset < d.startSpDelta)
                    forceToSave = true;
            }
#if 1
            forceToSave = true; // Save Whole screen instead of delta
#endif

            if (curLine[x] == prevLine[x] || forceToSave)
                result.push_back(curLine[x]);
            else
                result.push_back(0);
        }
    }

    return result;
}

int
    packAll(
        int flags,
        int codeOffset,
        std::vector<uint8_t>& buffer,
        std::vector<uint8_t>& colorBuffer,
        const std::vector<int>& musicTimings,
        const std::string& outputFileName,
        bool silenceMode)
{
    const int imageHeight = buffer.size() / 32;

    const auto t1 = std::chrono::system_clock::now();

    CompressedData multicolorData = compressMultiColors(flags, colorBuffer.data(), imageHeight / 8, buffer.data());

    const auto mcToRastrTimings = calculateTimingsTable(imageHeight, false);
    if (!alignMulticolorTimings(mcToRastrTimings, flags, multicolorData, silenceMode))
        return kMinWorseTiming;

    CompressedData colorData = compressColors(colorBuffer.data(), imageHeight);
    CompressedData data = compressRastr(flags, buffer.data(), colorBuffer.data(), imageHeight);

    const auto t2 = std::chrono::system_clock::now();

    if (!silenceMode)
        std::cout << "compression time= " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;

    static const int uncompressedTicks = 21 * 16 * imageHeight;
    static const int uncompressedColorTicks = uncompressedTicks / 8;
    if (!silenceMode)
    {
        std::cout << "uncompressed ticks: " << uncompressedTicks << " compressed ticks: "
            << data.ticks() << ", ratio: " << (data.ticks() / float(uncompressedTicks))
            << ", data size:" << data.size() << std::endl;

        std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " compressed color ticks: "
            << colorData.ticks() << ", ratio: " << colorData.ticks() / (float)uncompressedColorTicks << std::endl;
        std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " multi color ticks: "
            << multicolorData.ticks() << ", ratio: " << multicolorData.ticks() / (float)uncompressedColorTicks << std::endl;
        std::cout << "total ticks: " << data.ticks() + colorData.ticks() + multicolorData.ticks() << std::endl;
    }

    // put JP to the latest line for every bank
    for (int bank = 0; bank < 8; ++bank)
    {
        int bankSize = imageHeight / 8;
        int line = bank * bankSize + bankSize - 1;
        int firstLineInBank = bank * bankSize;

        int pageNum = lineNumToPageNum(line, imageHeight);
        int firstLineOffset = 0;
        for (int i = 0; i < firstLineInBank; ++i)
        {
            int page = lineNumToPageNum(i, imageHeight);
            if (page == pageNum)
                firstLineOffset += data.size(i, 1);
        }

        data.data[line].jp(firstLineOffset + getRastrCodeStartAddr(imageHeight));
    }

    std::vector<LineDescriptor> descriptors;
    std::vector<ColorDescriptor> colorDescriptors;

    Labels labels;

    labels.mt_and_rt_reach_descriptor = serializeMainData(buffer.data(), data, multicolorData, descriptors, outputFileName, codeOffset, flags, mcToRastrTimings);
    labels.multicolor = serializeMultiColorData(multicolorData, outputFileName, codeOffset + labels.mt_and_rt_reach_descriptor);

    // put JP to the latest line of colors
    colorData.data[colorData.data.size() - 1].jp(kColorDataStartAddr);
    int colorDataSize = serializeColorData(colorDescriptors, colorData, outputFileName, kColorDataStartAddr);

    int worseTiming = serializeTimingData(descriptors, colorDescriptors, data, colorData, multicolorData, outputFileName, flags, musicTimings, silenceMode);

    serializeRastrDescriptors(descriptors, outputFileName);
    serializeJpIxDescriptors(descriptors, outputFileName);

    serializeAsmFile(outputFileName, data, colorData, multicolorData, flags, labels, !musicTimings.empty());

    auto colorBufferCopy = colorBuffer;
    mirrorBuffer8(colorBufferCopy.data(), imageHeight / 8);
    std::ofstream firstScreenFile;
    firstScreenFile.open(outputFileName + "first_screen.scr", std::ios::binary);

    auto deinterlacedRastr = buffer;
    mirrorBuffer8(deinterlacedRastr.data(), imageHeight);

    auto firstScreenDelta = getDeltaForFirstScreen(deinterlacedRastr, 192, -1, &colorBufferCopy, &descriptors);
    firstScreenDelta = interlaceBuffer(firstScreenDelta);
    firstScreenFile.write((const char*)firstScreenDelta.data(), firstScreenDelta.size());

    firstScreenDelta = getDeltaForFirstScreen(colorBufferCopy, 24, 1, nullptr, nullptr);
    firstScreenFile.write((const char*)firstScreenDelta.data(), firstScreenDelta.size());
    firstScreenFile.close();

    return worseTiming;
}

std::future<int> packAllAsync(
    int flags,
    int codeOffset,
    std::vector<uint8_t>& buffer,
    std::vector<uint8_t>& colorBuffer,
    const std::vector<int>& musicTimings,
    const std::string& outputFileName,
    bool silenceMode)
{
    return std::async(
        [&]()
        {
            return packAll(flags, codeOffset,
                buffer, colorBuffer, musicTimings, outputFileName, silenceMode);
        }
    );
}

void showHelp()
{
    std::cerr << "scroll_image_compress v.1.1";
    std::cerr << "Usage: scroll_image_compress \n";
    std::cerr << "    -i <file_name>    Argument can be repeated several times or several files can be provided via ';' delimiter\n";
    std::cerr << "    -o <out_folder_name> Folder with generated files for scroller.asm\n";
    std::cerr << "    [-csv <.csv file name>]  Optional parameter. Use audio player if provided.\n";
    std::cerr << "        csv file should contains music timings from psg_compressor. Parameter is optional. \n";
    std::cerr << "        Without this parameter it will be compiled in pure scroll mode.\n";
    std::cerr << "    [-sld <.sld file name>] Optional. Can read generated code start address from scroller.asm instead of constant.\n";
    std::cerr << "        For developer purpose only.\n";
    std::cerr << "    [-inv <temp_log_file>] Use inverse color addition step. It is very slow. \n";
    std::cerr << "        While it is compressing temporary file <temp_log_file> is used to continue after compressor restart.\n";
    std::cerr << "    [-inv2 <temp_log_file>] Same to -inv but continue to update file till min required ticks are reached. -inv uses existing data only.\n";
    std::cerr << "    [-inv3 <temp_log_file>] Same to -inv but continue to update file till end. -inv uses existing data only.\n";
    std::cerr << "\n";
    std::cerr << "Example: scroll_image_compress -i image1.scr;image2.scr -o ./generated_folder -csv music_timings.csv\n";
}

void addInputFiles(std::vector<std::string>* result, const std::string& value)
{
    std::string tmp;
    std::stringstream ss(value);
    while (getline(ss, tmp, ';'))
        result->push_back(tmp);
}

int main(int argc, char** argv)
{
    using namespace std;

    ifstream fileIn;

    std::string sldFileName;
    std::string musTimingsFileName;
    std::string outputFileName;
    std::vector<std::string> inputFileNames;
    std::string inverseColorsTmpFile;
    bool updateInverseColors = false;
    bool breakOnMinTicks = false;

    for (int i = 1; i < argc; i += 2)
    {
        std::string paramName = argv[i];
        if (i + 1 >= argc)
        {
            showHelp();
            return -1;
        }
        std::string paramValue = argv[i+1];

        if (paramName == "-i")
            addInputFiles(&inputFileNames, paramValue);
        else if (paramName == "-o")
            outputFileName = paramValue;
        else if (paramName == "-csv" || paramName == "-c")
            musTimingsFileName = paramValue;
        else if (paramName == "-sld" || paramName == "-s")
            sldFileName = paramValue;
        else if (paramName == "-inv")
            inverseColorsTmpFile = paramValue;
        else if (paramName == "-inv2" || paramName == "-inv3")
        {
            inverseColorsTmpFile = paramValue;
            updateInverseColors = true;
            breakOnMinTicks = paramName != "-inv3";
        }
        else
        {
            showHelp();
            return -1;
        }
    }

    std::vector<int> musicTimings;
    if (!musTimingsFileName.empty())
    {
        ifstream musFile;
        if (!musTimingsFileName.empty())
        {
            musFile.open(musTimingsFileName);
            if (!musFile.is_open())
            {
                std::cerr << "Can not open csv file '" << musTimingsFileName << "'with music timings" << std::endl;
                return -1;
            }
            std::string line;
            std::getline(musFile, line); //< skip CSV header
            while (1)
            {
                std::getline(musFile, line);
                if (line.empty())
                    break;
                auto pos = line.find(';');
                if (pos == std::string::npos)
                    continue;
                ++pos;
                auto pos2 = line.find(';', pos);
                if (pos2 == std::string::npos)
                    pos2 = line.size();
                std::string timingStr = line.substr(pos, pos2 - pos);
                musicTimings.push_back(std::stoi(timingStr));
            }
        }
    }

    if (!ends_with(outputFileName, "\\") && !ends_with(outputFileName, "/"))
        outputFileName += "/";

    int fileCount = inputFileNames.size();
    int imageHeight = 192 * fileCount;

    std::vector<uint8_t> buffer;
    std::vector<uint8_t> colorBuffer;

    for (const auto& inputFileName: inputFileNames)
    {
        fileIn.open(inputFileName, std::ios::binary);
        if (!fileIn.is_open())
        {
            std::cerr << "Can not read source file " << inputFileName << std::endl;
            return -1;
        }

        fileIn.seekg(0, ios::end);
        int fileSize = fileIn.tellg();
        fileIn.seekg(0, ios::beg);

        const int minBlockSize = 2048 + 2048 / 8;
        if (fileSize % minBlockSize || fileSize <= 0)
        {
            std::cerr << "Unexpected file size " << fileSize << ". Each block should be aligned at 64 lines" << std::endl;
        }

        int blocks = fileSize / minBlockSize;

        int oldRastrSize = buffer.size();
        buffer.resize(buffer.size() + blocks * 2048);
        uint8_t* rastrPtr = buffer.data() + oldRastrSize;
        fileIn.read((char*) rastrPtr, blocks * 2048);

        int oldColorSize = colorBuffer.size();
        colorBuffer.resize(colorBuffer.size() + blocks * 256);
        uint8_t* colorPtr = colorBuffer.data() + oldColorSize;
        fileIn.read((char*)colorPtr, blocks * 256);
        fileIn.close();
    }

    inverseImageIfNeed(buffer.data(), colorBuffer.data());

    int codeOffset = sldFileName.empty() ? kDefaultCodeOffset : parseSldFile(sldFileName);

    deinterlaceBuffer(buffer);
    writeTestBitmap(256, imageHeight, buffer.data(), outputFileName + "image.bmp");

    mirrorBuffer8(buffer.data(), imageHeight);
    mirrorBuffer8(colorBuffer.data(), imageHeight / 8);

//#define DEBUG_TIMINGS
#ifdef DEBUG_TIMINGS
    // Remove some flags for more easy debuging timings at ZX
    int flags = verticalCompressionL | interlineRegisters | skipInvisibleColors | updateColorData;
#else
    int flags = verticalCompressionL | interlineRegisters | skipInvisibleColors | optimizeLineEdge | twoRastrDescriptors | OptimizeMcTicks | updateColorData;
#endif

    cleanupColors(buffer.data(), colorBuffer.data(), imageHeight / 8);

    if (!inverseColorsTmpFile.empty())
        flags |= inverseColors;

    bool silenceMode = flags & inverseColors;

    if (silenceMode)
        std::cout << "Perform initial pack at pos [0, 0]" << std::endl;

    int bestWorseTiming = packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, false /*silenceMode*/);

    if (!(flags & inverseColors))
    {
        runCompressor(outputFileName);
        return 0;
    }

    std::cout << "Worse ticks = " << bestWorseTiming << std::endl;

    auto tmpOutputFolder = outputFileName + "tmp/";
    std::filesystem::create_directory(tmpOutputFolder);
    auto tmpOutputFolder1 = tmpOutputFolder + "tmp1/";
    auto tmpOutputFolder2 = tmpOutputFolder + "tmp2/";
    auto tmpOutputFolder3 = tmpOutputFolder + "tmp3/";
    std::filesystem::create_directory(tmpOutputFolder1);
    std::filesystem::create_directory(tmpOutputFolder2);
    std::filesystem::create_directory(tmpOutputFolder3);

    auto processedCells = loadInverseColorsState(inverseColorsTmpFile, imageHeight / 8);
    saveInverseColorsState(inverseColorsTmpFile, processedCells);

    const bool updateImidiatly = false; //updateInverseColors;

    bool hasSomeToRepack = false;
    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            Position pos{ y,x };
            if (processedCells[pos] != InverseResult::notProcessed && processedCells[pos] != InverseResult::none)
            {
                std::cout << "Check block " << y << ":" << x << std::endl;

                if (processedCells[pos] == InverseResult::left || processedCells[pos] == InverseResult::both)
                    inversBlock(buffer.data(), colorBuffer.data(), x, y);

                if (processedCells[pos] == InverseResult::right || processedCells[pos] == InverseResult::both)
                    inversBlock(buffer.data(), colorBuffer.data(), x + 1, y);

                if (updateImidiatly)
                {
                    int newTicks = packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, false /*silenceMode*/);
                    std::cout << "(" << toString(processedCells[pos]) << ") Improve worse ticks from " << bestWorseTiming << " to " << newTicks << std::endl;
                    if (newTicks <= bestWorseTiming)
                    {
                        std::cout << "================= Degradation! Rollback. ===================" << std::endl;
                        if (processedCells[pos] == InverseResult::left || processedCells[pos] == InverseResult::both)
                            inversBlock(buffer.data(), colorBuffer.data(), x, y);

                        if (processedCells[pos] == InverseResult::right || processedCells[pos] == InverseResult::both)
                            inversBlock(buffer.data(), colorBuffer.data(), x + 1, y);

                        processedCells[pos] = InverseResult::notProcessed;
                        saveInverseColorsState(inverseColorsTmpFile, processedCells);
                        hasSomeToRepack = true;
                    }
                    else
                    {
                        bestWorseTiming = newTicks;
                    }
                }
                else
                {
                    hasSomeToRepack = true;
                }
            }
        }
    }

    if (hasSomeToRepack)
    {
        int newTicks = packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, updateInverseColors /*silenceMode*/);
        saveInverseColorsState(inverseColorsTmpFile, processedCells);
        std::cout << "Improve worse ticks from "
            << bestWorseTiming << " to " << newTicks << " using previous pack data" << std::endl;
        bestWorseTiming = newTicks;
    }

    if (updateInverseColors)
    {
        for (int y = 0; y < imageHeight / 8; ++y)
        {
            for (int x = 0; x < 32; x += 2)
            {
                if (bestWorseTiming >= kMinDelay && breakOnMinTicks)
                    break;

                Position pos{y,x};

                int lineNum = y * 8;

                if (processedCells[pos] == InverseResult::notProcessed)
                {
                    std::cout << "Check block " << y << ":" << x << "   ";
                    const auto t1 = std::chrono::system_clock::now();

                    std::array<std::vector<uint8_t>,3> buffers;
                    std::array < std::vector<uint8_t>,3> colorBuffers;
                    for (int i = 0; i < 3; ++i)
                    {
                        buffers[i] = buffer;
                        colorBuffers[i] = colorBuffer;
                    }

                    inversBlock(buffers[0].data(), colorBuffers[0].data(), x, y);
                    std::future<int> candidateLeftF = packAllAsync(flags, codeOffset, buffers[0], colorBuffers[0], musicTimings, tmpOutputFolder1, silenceMode);

                    inversBlock(buffers[1].data(), colorBuffers[1].data(), x, y);
                    inversBlock(buffers[1].data(), colorBuffers[1].data(), x + 1, y);
                    std::future<int> candidateBothF = packAllAsync(flags, codeOffset, buffers[1], colorBuffers[1], musicTimings, tmpOutputFolder2, silenceMode);

                    inversBlock(buffers[2].data(), colorBuffers[2].data(), x + 1, y);
                    std::future<int> candidateRightF = packAllAsync(flags, codeOffset, buffers[2], colorBuffers[2], musicTimings, tmpOutputFolder3, silenceMode);

                    candidateLeftF.wait();
                    candidateBothF.wait();
                    candidateRightF.wait();

                    int candidateLeft = candidateLeftF.get();
                    int candidateBoth = candidateBothF.get();
                    int candidateRight = candidateRightF.get();


                    const auto t2 = std::chrono::system_clock::now();
                    std::cout << "time= " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;

                    if (candidateLeft > bestWorseTiming
                        && candidateLeft >= candidateBoth
                        && candidateLeft >= candidateRight)
                    {
                        std::cout << "(L) Improve worse ticks from " << bestWorseTiming << " to " << candidateLeft << std::endl;
                        processedCells[pos] = InverseResult::left;
                        inversBlock(buffer.data(), colorBuffer.data(), x, y);
                        bestWorseTiming = candidateLeft;
                        packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, silenceMode);
                    }
                    else if (candidateBoth > bestWorseTiming
                        && candidateBoth >= candidateLeft
                        && candidateBoth >= candidateRight)
                    {
                        std::cout << "(B) Improve worse ticks from " << bestWorseTiming << " to " << candidateBoth << std::endl;
                        processedCells[pos] = InverseResult::both;
                        inversBlock(buffer.data(), colorBuffer.data(), x, y);
                        inversBlock(buffer.data(), colorBuffer.data(), x + 1, y);
                        bestWorseTiming = candidateBoth;
                        packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, silenceMode);
                    }
                    else if (candidateRight > bestWorseTiming
                        && candidateRight >= candidateLeft
                        && candidateRight >= candidateBoth)
                    {
                        std::cout << "(R) Improve worse ticks from " << bestWorseTiming << " to " << candidateRight << std::endl;
                        processedCells[pos] = InverseResult::right;
                        inversBlock(buffer.data(), colorBuffer.data(), x + 1, y);
                        bestWorseTiming = candidateRight;
                        packAll(flags, codeOffset, buffer, colorBuffer, musicTimings, outputFileName, silenceMode);
                    }
                    else
                    {
                        processedCells[pos] = InverseResult::none;
                    }
                    saveInverseColorsState(inverseColorsTmpFile, processedCells);
                }
            }
        }
    }

    runCompressor(outputFileName);
    return 0;
}
