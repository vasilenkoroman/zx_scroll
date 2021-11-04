#include <iostream>
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

#define LOG_INFO
//#define LOG_DEBUG

static const int totalTicksPerFrame = 71680;

static const uint8_t DEC_SP_CODE = 0x3b;
static const uint8_t LD_BC_CODE = 1;

static const int lineSize = 32;
static const int kScrollDelta = 1;
static const int kColorScrollDelta = 1;
static const int kMinDelay = 78;
static const int kPagesForData = 4;
static const int kSetPageTicks = 18;
static const int kPageNumPrefix = 0x50;
static const int kMinOffscreenBytes = 4; // < It contains at least 4 writted bytes to the screen

// Pages 0,1, 3,4
static const uint16_t rastrCodeStartAddrBase = 0xc000;

// Page 6
static const uint16_t kColorDataStartAddr = 0xc000;

enum Flags
{
    interlineRegisters = 1,     //< Experimental. Keep registers between lines.
    verticalCompressionL = 2,   //< Skip drawing data if it exists on the screen from the previous step.
    verticalCompressionH = 4,   //< Skip drawing data if it exists on the screen from the previous step.
    oddVerticalCompression = 8, //< can skip odd drawing bytes.
    inverseColors = 16,         //< Try to inverse data blocks for better compression.
    skipInvisibleColors = 32,   //< Don't draw invisible colors.
    optimizeLineEdge = 128,     //< merge two line borders with single SP moving block.
    updateViaHl = 512,
    OptimizeMcTicks = 1024
};

static const int kJpFirstLineDelay = 10;
static const int kLineDurationInTicks = 224;
static const int kRtMcContextSwitchDelay = 72; // multicolor to rastr context switch delay
static const int kTicksOnScreenPerByte = 4;

/**
 * The last drawing line is imageHeight-1. But the last drawing line is the imageHeight-1 + kmaxDescriptorOffset
 * Because of descriptor number calculation code doesn't do "line%imageHeight" for perfomance reason. So, just create a bit more descriptors
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

        minX = sameBytesCount->at(y * 32);

#if 0
        // remove left and rigt edge
        for (int x = 0; x < 32; ++x)
        {
            if (line[x] == nextLine[x])
                ++minX;
            else
                break;
        }
#endif

        for (int x = 31; x >= 0; --x)
        {
            //if (line[x] == nextLine[x])
            if (sameBytesCount->at(y * 32 + x))
                --maxX;
            else
                break;
        }
    }
};

void serialize(std::vector<uint8_t>& data, uint16_t value)
{
    data.push_back((uint8_t)value);
    data.push_back(value >> 8);
}

inline bool isHiddenData(uint8_t* colorBuffer, int x, int y)
{
    if (!colorBuffer)
        return false;
    const uint8_t colorData = colorBuffer[x + y * 32];
    return (colorData & 7) == ((colorData >> 3) & 7);
}

inline bool isHiddenData(const std::vector<bool>* hiddenData, int x, int y)
{
    if (!hiddenData || hiddenData->empty())
        return false;
    return hiddenData->at(x + y * 32);
}

struct CompressedData
{
    std::vector<int8_t> sameBytesCount;
    std::vector<CompressedLine> data;
    int flags = 0;

public:

    int ticks(int from = 0, int count = -1) const
    {
        int result = 0;
        int to = count == -1 ? data.size() : from + count;
        for (int i = from; i < to; ++i)
            result += data[i].drawTicks;
        return result;
    }

    int size(int from = 0, int count = -1) const
    {
        int result = 0;
        int to = count == -1 ? data.size() : from + count;
        for (int i = from; i < to; ++i)
            result += data[i].data.size();
        return result;
    }
};

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

template <int N>
bool compressLine(
    const Context& context,
    CompressedLine& result,
    std::array<Register16, N>& registers,
    int x);

template <int N>
inline bool makeChoise(
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

    return compressLine(context, result, registers, x + 2);
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
void updateTransitiveRegUsage(T& data)
{
    int size = data.size();
    for (int lineNum = 0; lineNum < data.size(); ++lineNum)
    {
        CompressedLine& line = data[lineNum];

        uint8_t selfRegMask = line.regUsage.selfRegMask;
        auto before = line.regUsage.regUseMask;
        for (int j = 1; j <= 8; ++j)
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
    bool success = false;
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

    std::vector<TryN> extraCompressOptions =
    {
        {0, context, std::make_shared<RegistersN>(registers)},  //< regular
        {oddVerticalCompression, context, std::make_shared<RegistersN>(registers)},
        {oddVerticalCompression | updateViaHl, context, std::make_shared<RegistersN>(registers)}
    };

    int bestTicks = std::numeric_limits<int>::max();
    TryN* bestTry = nullptr;
    for (auto& option : extraCompressOptions)
    {
        if (!useUpdateViaHlTry && (option.extraFlags & updateViaHl))
            continue;

        option.context.flags |= option.extraFlags;
        option.line.flags = option.context.flags;
        if (!(option.context.flags & oddVerticalCompression))
            option.context.minX = option.context.minX & ~1;
        option.success = compressLine(option.context, option.line, *option.registers,  /*x*/ option.context.minX);
        if (option.success && option.line.drawTicks <= bestTicks)
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
        if (auto f = findRegister8(registers, 'f'))
            f->value.reset(); //< Interline registers is not supported for 'f'
    }
    line.inputAf = std::make_shared<Register16>(context.af);
    return true;
}

static Register16 sp("sp");

template <int N>
void choiseNextRegister(
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

        if (!makeChoise(context, newLine, regCopy, regIndex, word, x))
            continue;

        bool useNextLine = choisedLine.data.empty() || newLine.drawTicks <= choisedLine.drawTicks;
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
                reg.push(result);
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
        else if (reg.l.hasValue(byte))
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

    if (newX < context.maxX
        && hasByteFromExistingRegisterExceptHl(context, line, registers, context.buffer[index]))
    {
        return true;
    }
    return false;
}

template <int N>
bool compressLine(
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
            result.spPosHint = result.data.size();
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
                    result.spPosHint = result.data.size();
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
        if (x < 31 && x < context.maxX && loadWordFromExistingRegister(context, result, registers, word, x))
        {
            if (awaitingLoadFromHl)
                abort();
            x += 2;
            continue;
        }

        if (awaitingLoadFromHl
            && loadByteFromExistingRegisterExceptHl(context, result, registers, context.buffer[index], x))
        {
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

        if (registers[0].isEmpty())
        {
            if (result.isAltReg != registers[0].isAlt)
                result.exx();
            registers[0].updateToValue(result, word, registers, context.af);
            registers[0].push(result);
            x += 2;
            continue;
        }

        CompressedLine choisedLine;
        auto chosedRegisters = registers;

        choiseNextRegister(choisedLine, chosedRegisters, context, result, registers, word, x);

        if ((context.flags & oddVerticalCompression) && choisedLine.data.empty() && x % 2 == 0)
        {
            // try to reset flags for the rest of the line
            Context contextCopy(context);
            contextCopy.flags &= ~oddVerticalCompression;
            choiseNextRegister(choisedLine, chosedRegisters, contextCopy, result, registers, word, x);
            choisedLine.lastOddRepPosition = x;
        }

        if (choisedLine.data.empty())
            return false;

        if (choisedLine.spPosHint >= 0)
            result.spPosHint = result.data.size() + choisedLine.spPosHint;

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

std::vector<CompressedLine> compressLines(const Context& context, const std::vector<int>& lines)
{
    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
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


    updateTransitiveRegUsage(result);
    return result;
}

std::future<std::vector<CompressedLine>> compressLinesAsync(const Context& context, const std::vector<int>& lines)
{
    return std::async(
        [context, lines]()
        {
            return compressLines(context, lines);
        }
    );
}

std::vector<bool> removeInvisibleColors(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    std::vector<bool> result;
    bool moveDown = flags & verticalCompressionL;
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

Register16 findBestByte(uint8_t* buffer, int imageHeight, const std::vector<int8_t>* sameBytesCount = nullptr, int* usageCount = nullptr)
{
    Register16 af("af");
    std::map<uint8_t, int> byteCount;
    for (int y = 0; y < 24; ++y)
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

CompressedData compressRastr(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight, std::vector<int8_t> sameBytesCount)
{
    std::vector<bool> maskColor;
    if (flags & skipInvisibleColors)
        maskColor = removeInvisibleColors(flags, buffer, colorBuffer, imageHeight);

    CompressedData result = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);
    result.sameBytesCount = sameBytesCount;

    if (!(flags & inverseColors))
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

void finilizeLine(
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

        if (pushLine.spPosHint >= 0)
            result.spPosHint = result.data.size() + pushLine.spPosHint;
        result += pushLine;
    }


    // Calculate line min and max draw delay (point 0 is the ray in the begin of a line)

    if (context.y == 16)
    {
        int gg = 4;
    }

    std::vector<Register16> registers = {
        Register16("bc"), Register16("de"), Register16("hl"),
        Register16("bc'"), Register16("de'"), Register16("hl'")
    };
    int extraDelay = 0;
    Z80Parser parser;
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
            const auto inputReg = Z80Parser::findRegByItsPushOpCode(info.outputRegisters, command.opCode);
            if (!inputReg)
                return false;

            int rightBorder = 32;
            static const int kInitialSpOffset = 16;
            int rayTicks = (kInitialSpOffset + info.spOffset) * kTicksOnScreenPerByte;
            if (info.ticks < rayTicks)
            {
                int delay = rayTicks - info.ticks;
                extraDelay = std::max(extraDelay, delay);
            }
            else
            {
                maxRayDelay = std::min(maxRayDelay, rayTicks + 224 - info.ticks);
            }

            return false;
        });
    result.mcStats.min = extraDelay;
    if (maxRayDelay < result.mcStats.max)
        result.mcStats.max = maxRayDelay;

    if (result.mcStats.min > result.mcStats.max)
    {
        std::cerr << "Something wrong. Multicolor line #" << context.y << ". Wrong mcStats. min= " << result.mcStats.min << " is bigger than max=" << result.mcStats.min << std::endl;
        //abort();
    }
}

int getDrawTicks(const CompressedLine& pushLine)
{
    int drawTicks = pushLine.drawTicks;
    if (pushLine.data.last() == kExAfOpCode)
        drawTicks -= 4;
    return drawTicks;
}

CompressedLine  compressMultiColorsLine(Context context)
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

    // 2.4 start compressor with prepared register values

    if (context.y == 11)
    {
        int gg = 4;
    }


    CompressedLine pushLine;
    auto regCopy = registers6;
    success = compressLine(context, pushLine, regCopy,  /*x*/ context.minX);
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

    if (!success)
    {
        std::cerr << "ERROR: unexpected error during compression multicolor line " << context.y
            << " (something wrong with oddVerticalCompression flag). It should not be! Just a bug." << std::endl;
        abort();
    }


    if (drawTicks > t2)
    {
        // TODO: implement me
        std::cerr << "ERROR: Line " << context.y << ". Not enough " << drawTicks - t2 << " ticks for multicolor" << std::endl;
        //assert(0);
        //abort();
    }

    pushLine.mcStats.max = t2 - drawTicks;
    finilizeLine(context, result, loadLine, pushLine);
    result.inputAf = std::make_shared<Register16>(context.af);

    if (result.spPosHint >= 0)
    {
        uint16_t* value = (uint16_t*)(result.data.buffer() + result.spPosHint + 1);
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

BorrowResult borrowIndexRegForLine(int line, CompressedData& compressedData)
{
    BorrowResult result;
    const int imageHeight = compressedData.data.size();
    auto& dstLine = compressedData.data[line];
    int borrowLine = line;
    for (int i = 0; i < imageHeight; ++i)
    {
        borrowLine = (borrowLine+1) % imageHeight;
        auto& srcLine = compressedData.data[borrowLine];

        if (srcLine.mcStats.lendIx.has_value())
            break;

        int dt = dstLine.drawTicks - srcLine.drawTicks;
        if (dt < 20)
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
            result.drawTicks -= 6;
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
        maxTicks = std::max(line.drawTicks, maxTicks);
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        auto& line = compressedData.data[i];
        if (line.drawTicks == maxTicks)
        {
            BorrowResult borrowRes = borrowIndexRegForLine(i, compressedData);
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

        // try to reduce max tics, next line will be drawn later
        int available = nextLine.mcStats.max - nextLine.mcStats.pos;
        if (available > 0
            && nextLine.mcStats.pos < maxMcPhaseForLine(compressedData)
            && line.mcStats.virtualTicks - nextLine.mcStats.virtualTicks > 1)
        {
            --line.mcStats.virtualTicks;
            ++nextLine.mcStats.pos;
            ++nextLine.mcStats.virtualTicks;
            return true;
        }
    }
    return false;
}

int alignMulticolorTimings(int flags, CompressedData& compressedData)
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
    std::cout << "INFO: max multicolor ticks line #" << maxTicksLine << ", ticks=" << maxTicks << ". Min ticks line #" << minTicksLine << ", ticks=" << minTicks << std::endl;
    std::cout << "INFO: align multicolor to ticks to " << maxTicks << " losed ticks=" << maxTicks * imageHeight - regularTicks << std::endl;


    if (flags & OptimizeMcTicks)
    {
        // 1. Sink LD IX,nn, LD IY,nn from fast lines to slow lines
#if 0
        while (borrowIndexRegStep(compressedData));
        maxTicks = 0;
        for (int i = 0; i < imageHeight; ++i)
        {
            auto& line = compressedData.data[i];
            line.mcStats.virtualTicks = line.drawTicks;
            if (line.drawTicks > maxTicks)
                maxTicks = line.drawTicks;
        }
#endif
        // 2. Use advanced ray model. MC line drawing could start when ray already inside line in case of enough ticks for drawing
        while (rebalanceStep(compressedData));
    }


    int maxVirtualTicks = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
        maxVirtualTicks = std::max(line.mcStats.virtualTicks, maxVirtualTicks);
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        if (i == 20)
        {
            int gg = 4;
        }
        const auto& line = compressedData.data[i];
        if (line.mcStats.pos < line.mcStats.min)
            maxVirtualTicks = std::max(maxVirtualTicks, line.mcStats.virtualTicks + (line.mcStats.min - line.mcStats.pos));
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        int endLine = (i + 23) % imageHeight;
        std::cout << i << ": " << compressedData.data[i].mcStats.pos  << "[" << compressedData.data[i].mcStats.min << ".." << compressedData.data[i].mcStats.max
            << "], ticks=" << compressedData.data[i].drawTicks << ", virtualTicks=" << compressedData.data[i].mcStats.virtualTicks << std::endl;
    }

    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
        if (line.mcStats.virtualTicks < maxVirtualTicks && maxVirtualTicks - line.mcStats.virtualTicks < 4)
        {
            maxVirtualTicks += 4;
            break;
        }
    }
    std::cout << "INFO: reduce maxTicks after rebalance: " << maxVirtualTicks << " reduce losed ticks to=" << maxVirtualTicks * imageHeight - regularTicks << std::endl;

    // 1. Calculate extra delay for begin of the line if it draw too fast
    int i = 0;
    for (auto& line : compressedData.data)
    {
        if (i == 22)
        {
            int gg = 4;
        }


        if (line.mcStats.pos < line.mcStats.min)
        {
            int beginDelay = line.mcStats.min - line.mcStats.pos;
            if (beginDelay > 0)
            {
                // Put delay to the begin only instead begin/end delay if possible.
                int endLineDelay = maxVirtualTicks - beginDelay - line.mcStats.virtualTicks;
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
                abort();
            }
        }
        ++i;
    }


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
    return maxVirtualTicks;
}

CompressedData compressMultiColors(uint8_t* buffer, int imageHeight)
{
    // Shuffle source data according to the multicolor drawing:
    // 32  <------------------16
    //                        16 <------------------ 0

    std::vector<uint8_t> shufledBuffer(imageHeight * 32);
    uint8_t* src = buffer;
    uint8_t* dst = shufledBuffer.data();

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

    auto suffledPtr = shufledBuffer.data();

    struct Context context;
    context.scrollDelta = 1;
    context.flags = verticalCompressionL;
    context.imageHeight = imageHeight;
    context.buffer = shufledBuffer.data();
    std::vector<int8_t> sameBytesCount = createSameBytesTable(context.flags, shufledBuffer.data(), /*maskColors*/ nullptr, imageHeight);
    context.sameBytesCount = &sameBytesCount;
    context.borderPoint = 16;

    int count1;
    context.af = findBestByte(suffledPtr, imageHeight, &sameBytesCount, &count1);
    context.af.isAltAf = true;

    CompressedData compressedData;
    for (int y = 0; y < imageHeight; y++)
    {
        context.y = y;
        auto line = compressMultiColorsLine(context);
        /*
        if (!success)
        {
            // Slow mode uses more ticks in general, but draw line under the ray on 34 ticks faster.
            line = compressMultiColorsLine(linePtr, nextLinePtr, generatedCodeAddress, &success, true );
            // Slow mode currently uses 6 registers and it is not enough 12 more ticks without compression.
            // If it fail again it need to use more registers. It need to add AF/AF' at this case.
            assert(success);
        }
        */
        compressedData.data.push_back(line);
    }

    compressedData.sameBytesCount = sameBytesCount;
    return compressedData;
}

CompressedData  compressColors(uint8_t* buffer, int imageHeight, const Register16& af2)
{
    int flags = verticalCompressionH | interlineRegisters | optimizeLineEdge;
    std::vector<int8_t> sameBytesCount = createSameBytesTable(flags, buffer, /*maskColors*/ nullptr, imageHeight / 8);

    Context context;
    context.scrollDelta = kScrollDelta;
    context.flags = flags;
    context.imageHeight = imageHeight / 8;
    context.buffer = buffer;
    context.sameBytesCount = &sameBytesCount;
    //context.af = af2;
    std::vector<int> lines;
    for (int y = 0; y < imageHeight / 8; y++)
    {
        lines.push_back(y);
    }

    CompressedData compressedData;
    compressedData.data = compressLines(context, lines);
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

    std::vector<uint8_t> preambula; //< Z80 code to load initial registers and JP command to the lineStart
    Z80CodeInfo codeInfo;           //< Descriptor data info.

    Z80CodeInfo omitedDataInfo;     //< Information about skipped start block (it's included directly to the descriptor preambula).
    std::vector<uint8_t> endBlock;  //< Information about replaced end block (JP IX command).

    int extraDelay = 0;             //< Alignment delay when serialize preambula.
    int startSpDelta = 0;           //< Addition commands to decrement SP included to preambula.
    Register16 af{ "af" };
    std::optional<uint16_t> updatedHlValue;

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
        int lineBank) const
    {
        CompressedLine preambula = dataLine.getSerializedUsedRegisters(af);
        auto [registers, omitedTicksFromMainCode, _] = mergedPreambulaInfo(preambula, serializedData, relativeOffsetToStart, kJpIxCommandLen);
        auto outRegs = getSerializedRegisters(registers, af);
        auto result = outRegs.drawTicks - omitedTicksFromMainCode;
        //if (lineBank % 2 == 1)
        result += kSetPageTicks;
        return result;
    }

    void replaceLoadWithHlToSp(
        std::vector<uint8_t>& firstCommands,
        std::vector<Register16>& registers)
    {
        /** LD(HL), reg8 command after the descriptor.It refer to the current HL = SP value, but HL is not set when start to exec descriptor.
         * Replace it to:
         * PUSH REG16
         * INC SP
        */
        int regIndex = firstCommands[0] - 0x70;
        const auto usedInHlReg = findRegisterByIndex(registers, regIndex, &af.h);
        bool canUseMethod1 = regIndex % 2 == 0 || regIndex == 7;
        if (!canUseMethod1 && usedInHlReg->name != 'a')
        {
            // If high and low parts of register are same, can still use method1 (it more fast)
            const auto highReg8 = findRegisterByIndex(registers, regIndex - 1, &af.h);
            if (highReg8 && highReg8->hasValue(*usedInHlReg->value))
                canUseMethod1 = true;
        }

        --startSpDelta;
        if (canUseMethod1)
        {
            // High register: b, d, h, a
            // Lose 10 ticks at descriptor header
            // Insert new commands after loading regs (use exising value)
            std::cout << "Replace LD (HL) To PUSH REG16 for descriptor header. Lose 10 ticks" << std::endl;
            firstCommands[0] = 0xc5 + 16*(regIndex/2); // PUSH XX
            firstCommands.insert(firstCommands.begin() + 1, 0x33); // INC SP
        }
        else
        {
            // Lose 17 ticks at descriptor header
            // Insert new commands before loading regs (load one more value to reg)
            std::cout << "Replace LD (HL) To PUSH REG16 for descriptor header. Lose 17 ticks" << std::endl;
            firstCommands.erase(firstCommands.begin());
            preambula.push_back(0x06); // LD B, X
            preambula.push_back(*usedInHlReg->value); // LD B, X
            preambula.push_back(0xc5); // PUSH BC
            preambula.push_back(0x33); // INC SP
        }
    }

    void serializeOmitedData(
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        int omitedDataSize,
        int extraDelay)
    {
        const uint16_t lineStartOffset = lineStartPtr - codeOffset;
        CompressedLine origPreambula = codeInfo.regUsage.getSerializedUsedRegisters(codeInfo.inputRegisters, af);

        auto [registers, omitedTicks, omitedBytes] = mergedPreambulaInfo(origPreambula, serializedData, lineStartOffset, omitedDataSize);

        auto firstCommands = Z80Parser::getCode(serializedData.data() + lineStartOffset, omitedDataSize);

        const int firstCommandsSize = firstCommands.size();
        omitedDataInfo = Z80Parser::parseCode(
            af, codeInfo.inputRegisters, firstCommands,
            0, firstCommands.size(), 0);

        firstCommands.erase(firstCommands.begin(), firstCommands.begin() + omitedBytes);

        auto regs = getSerializedRegisters(registers, af);
        if (extraDelay > 0)
            regs = updateRegistersForDelay(regs, extraDelay, af, /*testsmode*/ false).first;

        if (startSpDelta > 0 && !firstCommands.empty() && firstCommands[0] >= 0x70 && firstCommands[0] <= 0x77)
            replaceLoadWithHlToSp(firstCommands, registers);


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
        data.push_back(0x3e);            // LD A, #50 + page_number
        data.push_back(kPageNumPrefix + pageNum);  // LD A, #50 + page_number
        data.push_back(0xd3);            // OUT (#fd), A
        data.push_back(0xfd);            // OUT (#fd), A
        addToPreambule(data);
    }

    void makePreambulaForMC(
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        const CompressedLine* line,
        int pageNum, int bankNum)
    {
        //if (bankNum % 2 == 1)
        serializeSetPageCode(pageNum);

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

        serializeOmitedData(serializedData, codeOffset, kJpIxCommandLen, extraDelay);
    }

    void makePreambulaForOffscreen(
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

        serializeOmitedData(serializedData, codeOffset, kJpIxCommandLen - descriptorsDelta, 0);
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
    uint16_t addressBegin = 0;
    uint16_t endLineJpAddr = 0;
    uint16_t moveSpDelta = 0;
    uint16_t moveSpBytePos = 0;
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

#if 0
std::vector<JpIxDescriptor> createWholeFrameJpIxDescriptors(
    const std::vector<LineDescriptor>& descriptors)
{
    std::array<std::vector<JpIxDescriptor>, 4> jpIxDescriptors;

    int imageHeight = descriptors.size();
    const int bankSize = imageHeight / 8;
    const int colorsHeight = imageHeight / 8;

    // . Create delta for JP_IX when shift to 1 line
    for (int screenLine = 0; screenLine < imageHeight; ++screenLine)
    {
        int line = screenLine % imageHeight;
        for (int i : { 1, 2, 0 })
        {
            int l = (line + i * 64) % imageHeight;

            JpIxDescriptor d;
            d.pageNum = descriptors[l].pageNum;
            if (i == 1)
                d.line = line; //< restore off rastr mid on update

            d.address = descriptors[l].rastrForOffscreen.lineEndPtr;
            d.originData = descriptors[l].rastrForOffscreen.endBlock;
            jpIxDescriptors[d.pageNum].push_back(d);
            if (i == 0)
                d.line = line;
        }

        // 3 rastr for MC descriptors
        for (int i = 0; i < 3; ++i)
        {
            int l = (line + i * 64) % imageHeight;

            JpIxDescriptor d;
            d.pageNum = descriptors[l].pageNum;
            if (i == 2)
                d.line = line; //< write MC top on update

            if (i == 2)
            {
                // Shift to the next frame
                int line1 = line > 0 ? line - 1 : imageHeight - 1;
                int l1 = (line1 + i * 64) % imageHeight;
                d.address = descriptors[l1].rastrForMulticolor.lineEndPtr;
                d.originData = descriptors[l1].rastrForMulticolor.endBlock;

            }
            else
            {
                d.address = descriptors[l].rastrForMulticolor.lineEndPtr;
                d.originData = descriptors[l].rastrForMulticolor.endBlock;
            }
            jpIxDescriptors[d.pageNum].push_back(d);
        }
    }

    std::vector<JpIxDescriptor> result;
    for (const auto& descriptors: jpIxDescriptors)
    {
        for (const auto& d : descriptors)
            result.push_back(d);
    }

    return result;
}
#endif

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

int getRastrCodeStartAddr(int imageHeight)
{
    return rastrCodeStartAddrBase + jpixTableSize(imageHeight) / kPagesForData;
}

int serializeMainData(
    const CompressedData& data,
    const CompressedData& multicolorData,
    std::vector<LineDescriptor>& descriptors,
    const std::string& inputFileName,
    uint16_t reachDescriptorsBase,
    int flags,
    int mcDrawTicks)
{
    std::vector<uint8_t> mainMemSerializedDescriptors;
    std::array<std::vector<uint8_t>, kPagesForData> inpageSerializedDescriptors;

    using namespace std;
    const int imageHeight = data.data.size();
    const int rastrCodeStartAddr = getRastrCodeStartAddr(imageHeight);

    std::array<ofstream, kPagesForData> mainDataFiles;
    std::array<ofstream, kPagesForData> inpageDescrDataFiles;
    for (int i = 0; i < kPagesForData; ++i)
    {
        std::string fileName = inputFileName + ".main" + std::to_string(i);
        mainDataFiles[i].open(fileName, std::ios::binary);
        if (!mainDataFiles[i].is_open())
        {
            std::cerr << "Can not write destination file " << fileName << std::endl;
            return -1;
        }

        fileName = inputFileName + ".reach_descriptor" + std::to_string(i);
        inpageDescrDataFiles[i].open(fileName, std::ios::binary);
        if (!inpageDescrDataFiles[i].is_open())
        {
            std::cerr << "Can not write destination file " << fileName << std::endl;
            return -1;
        }
    }

    ofstream reachDescriptorFile;
    std::string reachDescriptorFileName = inputFileName + ".mt_and_rt_reach.descriptor";
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
        const static int kMinBytesForOffscreen = 6;

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
        descriptor.rastrForMulticolor.makePreambulaForMC(serializedData[descriptor.pageNum], rastrCodeStartAddr, &dataLine, descriptor.pageNum, lineBank);
        descriptor.rastrForOffscreen.makePreambulaForOffscreen(serializedData[descriptor.pageNum], rastrCodeStartAddr, descriptorsDelta);

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
    std::string colorDataFileName = inputFileName + ".color";
    colorDataFile.open(colorDataFileName, std::ios::binary);
    if (!colorDataFile.is_open())
    {
        std::cerr << "Can not write color file" << std::endl;
        return -1;
    }

    ofstream descriptorFile;
    std::string descriptorFileName = inputFileName + ".color_descriptor";
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

        // Currently color routing draws 24 lines at once.
        // So, for colorHeight=24 JP_IX it will over same line that we are enter.
        // Make filler to prevent it. It is perfomance loss, but I expect more big images ( > 192 lines) for release.
        if (imageHeight == 24)
        {
            serializedData.push_back(0); // NOP, NOP
            serializedData.push_back(0);
        }
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

        if (imageHeight == 24)
        {
            descriptor.addressBegin += 2; //< Avoid LD A,0 filler if exists.
        }

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

    for (const auto& descriptor : descriptors)
    {
        descriptorFile.write((const char*)&descriptor.addressEnd, sizeof(descriptor.addressEnd));
        descriptorFile.write((const char*)&descriptor.addressBegin, sizeof(descriptor.addressBegin));
    }

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());
    colorDataFile.write((const char*)serializedDescriptors.data(), serializedDescriptors.size());

    return serializedData.size() + serializedDescriptors.size();
}

void serializeAsmFile(
    const std::string& inputFileName,
    const CompressedData& rastrData,
    const CompressedData& multicolorData,
    int rastrFlags,
    int firstLineDelay)
{
    using namespace std;

    ofstream phaseFile;
    std::string phaseFileName = inputFileName + ".asm";
    phaseFile.open(phaseFileName);
    if (!phaseFile.is_open())
    {
        std::cerr << "Can not write asm file" << std::endl;
        return;
    }

    //phaseFile << "RASTR_REG_A               EQU    " << (unsigned) *rastrData.af.h.value << std::endl;
    phaseFile << "COLOR_REG_AF2             EQU    " << multicolorData.data[0].inputAf->value16() << std::endl;
    phaseFile << "FIRST_LINE_DELAY          EQU    " << firstLineDelay << std::endl;
    phaseFile << "MULTICOLOR_DRAW_PHASE     EQU    " << multicolorData.data[22].mcStats.pos << std::endl;
    phaseFile << "UNSTABLE_STACK_POS        EQU    "
        << ((rastrFlags & optimizeLineEdge) ? 1 : 0)
        << std::endl;
}

int serializeMultiColorData(
    const CompressedData& data,
    const std::string& inputFileName, uint16_t codeOffset)
{
    using namespace std;

    ofstream colorDataFile;
    std::string colorDataFileName = inputFileName + ".multicolor";
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
        lineOffset.push_back(serializedData.size());

        const auto& line = data.data[y];
        line.serialize(serializedData);

        // add JP XX command in the end of a line
        serializedData.push_back(0xc3);
        serializedData.push_back(0x00);
        serializedData.push_back(0x00);
    }

#if 0
    // Resolve LD HL, PREV_LINE_ADDR
    for (int y = 0; y < colorImageHeight; ++y)
    {
        int prevLineOffset = y > 0 ? lineOffset[y - 1] : lineOffset[lineOffset.size() - 1];
        uint16_t* ldHlPtr = (uint16_t*)(ldHlOffset[y] + serializedData.data());
        *ldHlPtr = prevLineOffset + codeOffset;
    }
#endif

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    // serialize multicolor descriptors

    std::vector<MulticolorDescriptor> descriptors;
    for (int d = -1; d < colorImageHeight + 23; ++d)
    {
        const int srcLine = d >= 0 ? d % colorImageHeight : colorImageHeight-1;

        MulticolorDescriptor descriptor;
        const auto& line = data.data[srcLine];
        const uint16_t lineAddressPtr = lineOffset[srcLine] + codeOffset;
        descriptor.addressBegin = lineAddressPtr;

        if (line.spPosHint >= 0)
        {
            if (line.data.buffer()[line.spPosHint] != 0x31)
            {
                std::cerr << "Invalid spPos hint. Some bug!";
                abort();
            }
            descriptor.moveSpBytePos = lineAddressPtr + line.spPosHint + 1;
            uint8_t value8 = line.data.buffer()[line.spPosHint + 1];
            descriptor.moveSpDelta = value8;
        }
        descriptor.endLineJpAddr = lineAddressPtr + line.data.size() + 1; // Line itself doesn't contains JP XX
        descriptors.push_back(descriptor);
    }

    ofstream file;
    std::string fileName = inputFileName + ".mc_descriptors";
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


    ofstream spDeltaFile, offRastrFile, mcRastrFile;
    {
        std::string fileName = inputFileName + ".sp_delta.descriptors";
        spDeltaFile.open(fileName, std::ios::binary);
        if (!spDeltaFile.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }
    {
        std::string fileName = inputFileName + ".off_rastr.descriptors";
        offRastrFile.open(fileName, std::ios::binary);
        if (!offRastrFile.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }
    {
        std::string fileName = inputFileName + ".mc_rastr.descriptors";
        mcRastrFile.open(fileName, std::ios::binary);
        if (!mcRastrFile.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

    for (int i = 0; i < imageHeight + kmaxDescriptorOffset; ++i)
    {
        int line = i % imageHeight;
        const auto& descriptor = descriptors[line];
        mcRastrFile.write((const char*)&descriptor.rastrForMulticolor.descriptorLocationPtr, 2);
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
    if (data.data.size() == 24)
        result += 23 * 8; //< Image height 192 has additional filler NOP: NOP for color lines.

    // End line contains JP <first line> command. If drawing is stoppeed on the end line this command is not executed.
    int endLine = (lineNum + 24) % imageHeight;
    if (endLine == 0)
        result -= kJpFirstLineDelay;

    return result;
}

int getTicksChainForMc(
    int line,
    int mcLineLen,
    const CompressedData& multicolor)
{
    if (line == 192 * 2 - 1)
    {
        int gg = 1;
    }

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

    int totalTicks = kLineDurationInTicks * 8;
    int rastrTicksFor8Lines = totalTicks - mcLineLen;

    return result + rastrTicksFor8Lines * 24;
}


int serializeTimingData(
    const std::vector<LineDescriptor>& descriptors,
    const std::vector<ColorDescriptor>& colorDescriptors,
    const CompressedData& data,
    const CompressedData& color,
    const CompressedData& multicolor,
    const std::string& inputFileName,
    int flags,
    int mcLineLen)
{
    using namespace std;

    const int imageHeight = data.data.size();
    const int colorHeight = imageHeight / 8;


    ofstream timingDataFile;
    std::string timingDataFileName = inputFileName + ".timings";
    timingDataFile.open(timingDataFileName, std::ios::binary);
    if (!timingDataFile.is_open())
    {
        std::cerr << "Can not write timing file" << std::endl;
        return -1;
    }
    int worseLineTicks = std::numeric_limits<int>::max();
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


        int mcTicks = getTicksChainForMc(line, mcLineLen, multicolor);
        if (line % 8 == 0)
        {
            ticks += mcTicks - mcLineLen * 24; //< Take into accout delay from the last MC line

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
                ticks += mcTicks;

            ticks += kLineDurationInTicks;  //< Draw next frame faster in  1 lines
        }

        int kZ80CodeDelay = 2951 - 168;
        if (line % 8 == 0)
        {
            kZ80CodeDelay += 2864 - 16 + 2325;
            if (line == 0)
                kZ80CodeDelay += 4;
        }
        else
        {
            if (line % 2 == 1)
                kZ80CodeDelay += 2;
            if (line % 8 == 1)
                kZ80CodeDelay += 2; //< end offscreen drawing direct jump(10) instead of ex de,hl: jp hl
        }

        ticks += kZ80CodeDelay;
        if (flags & optimizeLineEdge)
            ticks += 10 * 23; // LD SP, XX in each line

        int freeTicks = totalTicksPerFrame - ticks;
        if (freeTicks < kMinDelay)
        {
            std::cout << "WARNING: Low free ticks. line #" << line << ". free=" << freeTicks << ". minDelay=" << kMinDelay
                << ". Not enough " << kMinDelay - freeTicks << " ticks. "
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
        worseLineTicks = std::min(worseLineTicks, freeTicks);
        const uint16_t freeTicks16 = (uint16_t)freeTicks;
        timingDataFile.write((const char*)&freeTicks16, sizeof(freeTicks16));

    }
    std::cout << "worse line free ticks=" << worseLineTicks << ". ";
    if (kMinDelay - worseLineTicks > 0)
        std::cout << "Not enough ticks:" << kMinDelay - worseLineTicks;
    std::cout << std::endl;

    return firstLineDelay;
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
        std::string jpIxDescriptorFileName = inputFileName + ".jpix" + std::to_string(i);
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


    ofstream updateJpixHelper;
    {
        std::string fileName = inputFileName + ".update_jpix_helper";
        updateJpixHelper.open(fileName, std::ios::binary);
        if (!updateJpixHelper.is_open())
        {
            std::cerr << "Can not write destination file" << fileName << std::endl;
            return -1;
        }
    }

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

        updateJpixHelper.write((const char*)&address, sizeof(uint16_t));
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

int main(int argc, char** argv)
{
    using namespace std;

    ifstream fileIn;

    if (argc < 4)
    {
        std::cerr << "Usage: scroll_image_compress <file_name> [<file_name>] <out_file_name> <.sld file name>";
        return -1;
    }

    int fileCount = argc - 3;
    int imageHeight = 192 * fileCount;

    std::vector<uint8_t> buffer(fileCount * 6144);
    std::vector<uint8_t> colorBuffer(fileCount * 768);

    uint8_t* bufferPtr = buffer.data();
    uint8_t* colorBufferPtr = colorBuffer.data();

    for (int i = 1; i <= fileCount; ++i)
    {
        std::string inputFileName = argv[i];
        fileIn.open(inputFileName, std::ios::binary);
        if (!fileIn.is_open())
        {
            std::cerr << "Can not read source file " << inputFileName << std::endl;
            return -1;
        }

        fileIn.read((char*)bufferPtr, 6144);
        fileIn.read((char*)colorBufferPtr, 768);
        inverseImageIfNeed(bufferPtr, colorBufferPtr);

        bufferPtr += 6144;
        colorBufferPtr += 768;
        fileIn.close();
    }

    const std::string outputFileName = argv[argc - 2];
    const std::string sldFileName = argv[argc - 1];

    int codeOffset = parseSldFile(sldFileName);

    deinterlaceBuffer(buffer);
    writeTestBitmap(256, imageHeight, buffer.data(), outputFileName + ".bmp");

    mirrorBuffer8(buffer.data(), imageHeight);
    mirrorBuffer8(colorBuffer.data(), imageHeight / 8);

    int flags = verticalCompressionL | interlineRegisters | skipInvisibleColors | optimizeLineEdge | OptimizeMcTicks; // | inverseColors;

    const auto t1 = std::chrono::system_clock::now();

    std::vector<bool> maskColor;
    if (flags & skipInvisibleColors)
        maskColor = removeInvisibleColors(flags, buffer.data(), colorBuffer.data(), imageHeight);
    std::vector<int8_t> rastrSameBytes = createSameBytesTable(flags, buffer.data(), &maskColor, imageHeight);

    CompressedData multicolorData = compressMultiColors(colorBuffer.data(), imageHeight / 8);
    int alignedMcTicks = alignMulticolorTimings(flags, multicolorData);

    CompressedData data = compressRastr(flags, buffer.data(), colorBuffer.data(), imageHeight, rastrSameBytes);
    CompressedData colorData = compressColors(colorBuffer.data(), imageHeight, *multicolorData.data[0].inputAf);


    const auto t2 = std::chrono::system_clock::now();

    std::cout << "compression time= " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;

    static const int uncompressedTicks = 21 * 16 * imageHeight;
    static const int uncompressedColorTicks = uncompressedTicks / 8;
    std::cout << "uncompressed ticks: " << uncompressedTicks << " compressed ticks: "
        << data.ticks() << ", ratio: " << (data.ticks() / float(uncompressedTicks))
        << ", data size:" << data.size() << std::endl;

    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " compressed color ticks: "
        << colorData.ticks() << ", ratio: " << colorData.ticks() / (float)uncompressedColorTicks << std::endl;
    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " multi color ticks: "
        << multicolorData.ticks() << ", ratio: " << multicolorData.ticks() / (float)uncompressedColorTicks << std::endl;
    std::cout << "total ticks: " << data.ticks() + colorData.ticks() + multicolorData.ticks() << std::endl;

    // put JP to the latest line for every bank
    for (int bank = 0; bank < 8; ++bank)
    {
        int bankSize = imageHeight / 8;
        int line = bank * bankSize + bankSize - 1;
        int firstLineInBank = bank * bankSize;

        int pageNum = lineNumToPageNum(line, imageHeight);
        //int firstLineOffset = data.size(0, firstLineInBank);
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

    int mainDataSize = serializeMainData(data, multicolorData, descriptors, outputFileName, codeOffset, flags, alignedMcTicks);
    serializeMultiColorData(multicolorData, outputFileName, codeOffset + mainDataSize);

    // put JP to the latest line of colors
    colorData.data[colorData.data.size() - 1].jp(kColorDataStartAddr);
    int colorDataSize = serializeColorData(colorDescriptors, colorData, outputFileName, kColorDataStartAddr);

    serializeRastrDescriptors(descriptors, outputFileName);
    serializeJpIxDescriptors(descriptors, outputFileName);

    int firstLineDelay = serializeTimingData(descriptors, colorDescriptors, data, colorData, multicolorData, outputFileName, flags, alignedMcTicks);
    serializeAsmFile(outputFileName, data, multicolorData, flags, firstLineDelay);
    return 0;
}
