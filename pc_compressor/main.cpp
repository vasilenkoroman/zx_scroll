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

#define TEMP_FOR_TEST_COLORS 1

static const int totalTicksPerFrame = 71680;

static const uint8_t DEC_SP_CODE = 0x3b;
static const uint8_t LD_BC_CODE = 1;

static const int lineSize = 32;
static const int kScrollDelta = 1;
static const int kColorScrollDelta = 1;
static const int kMinDelay = 78;

enum Flags
{
    interlineRegisters = 1,     //< Experimental. Keep registers between lines.
    verticalCompressionL = 2,   //< Skip drawing data if it exists on the screen from the previous step.
    verticalCompressionH = 4,   //< Skip drawing data if it exists on the screen from the previous step.
    oddVerticalCompression = 8, //< can skip odd drawing bytes.
    inverseColors = 16,         //< Try to inverse data blocks for better compression.
    skipInvisibleColors = 32,   //< Don't draw invisible colors.
    hurryUpViaIY = 64,          //< Not enough ticks for multicolors. Use dec IY instead of dec SP during preparing registers to save 6 ticks during multicolor.
    optimizeLineEdge = 128      //< merge two line borders with single SP moving block.
};

static const int kJpFirstLineDelay = 10;
static const int kLineDurationInTicks = 224;
static const int kRtMcContextSwitchDelay = 97 + 10; // context switch + ld hl in the end of the multicolor line
static const int kTicksOnScreenPerByte = 4;
static const int kStackMovingTimeForMc = 10;

static const std::vector<uint8_t> kLdSpIy = {0xFD, 0xF9};

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
    std::vector<int>* sameBytesCount = nullptr;
    int y = 0;
    int maxX = 31;
    int minX = 0;
    int lastOddRepPosition = 32;
    int borderPoint = 0;
    Register16 af{"af"};

    void removeEdge()
    {
        uint8_t* line = buffer + y * 32;
        int nextLineNum = (y + scrollDelta) % imageHeight;
        uint8_t* nextLine = buffer + nextLineNum * 32;

        // remove left and rigt edge
        for (int x = 0; x < 32; ++x)
        {
            if (line[x] == nextLine[x])
                ++minX;
            else
                break;
        }
        for (int x = 31; x >= 0; --x)
        {
            if (line[x] == nextLine[x])
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
    std::vector<CompressedLine> data;
    Register16 af{"af"};
    int mcDrawPhase = 0;  //< Negative value means draw before ray start line.

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
    unsigned char *img = NULL;
    int filesize = 54 + 3*w*h;  //w is your image width, h is image height, both int

    img = (unsigned char *) malloc(3 * w * h);
    memset(img, 0, 3 * w * h);

    for(int x = 0; x < w; ++x)
    {
        for(int y = 0; y < h; y++)
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

    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    unsigned char bmppad[3] = {0,0,0};

    memcpy(bmpfileheader + 2, &filesize, 4);
    memcpy(bmpinfoheader + 4, &w, 4);
    memcpy(bmpinfoheader + 8, &h, 4);

    std::ofstream imageFile;
    imageFile.open(bmpFileName, std::ios::binary);

    imageFile.write((char*) &bmpfileheader, sizeof(bmpfileheader));
    imageFile.write((char*) &bmpinfoheader, sizeof(bmpinfoheader));
    for(int i = 0; i < h; i++)
    {
        imageFile.write((char*) img + w * (h-i-1)*3 , 3 * w);
        int bytesRest = 4 - (w * 3) % 4;
        imageFile.write((char*) &bmppad, bytesRest % 4);
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
    for (int srcY = 0; srcY < imageHeight; ++ srcY)
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
    const uint16_t* buffer16 = (uint16_t*) buffer;

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
            std::cout << "line #" << lineNum << " extend reg us mask from " << (int) before << " to " << (int ) line.regUseMask << std::endl;
#endif
    }
}

template <int N>
bool compressLineMain(
    Context& context,
    CompressedLine& line,
    std::array<Register16, N>& registers)
{
    CompressedLine line1, line2;
    auto registers1 = registers;
    auto registers2 = registers;

    line1.flags = context.flags;
    bool success1 = compressLine(context, line1, registers1,  /*x*/ context.minX & ~1);
    Context context2 = context;
    context2.flags |= oddVerticalCompression;
    line2.flags = context2.flags;
    bool success2 = compressLine(context2, line2, registers2,  /*x*/ context.minX);

    if (!success1)
    {
        std::cerr << "Cant compress line " << context.y << " because of oddRep position. It should not be!";
        abort();
    }

    bool useSecondLine = success2 && line2.drawTicks <= line1.drawTicks;
    if (useSecondLine)
    {
        line = line2;
    }
    else
    {
        context.minX &= ~1;
        line = line1;
    }

    line.inputRegisters = std::make_shared<std::vector<Register16>>();
    for (const auto& reg16 : registers)
        line.inputRegisters->push_back(reg16);

    if (context.flags & interlineRegisters)
    {
        if (useSecondLine)
            registers = registers2;
        else
            registers = registers1;

        if (auto f = findRegister8(registers, 'f'))
            f->value.reset(); //< Interline registers is not supported for 'f'
    }

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
bool compressLine(
    const Context& context,
    CompressedLine&  result,
    std::array<Register16, N>& registers,
    int x)
{
    while (x <= context.maxX)
    {
        const int index = context.y * 32 + x;
        int verticalRepCount = context.sameBytesCount ? context.sameBytesCount->at(index) : 0;
        verticalRepCount = std::min(verticalRepCount, context.maxX - x + 1);
        if (verticalRepCount)
        {
            if (x + verticalRepCount == 31)
                --verticalRepCount;
            else if (!(context.flags & oddVerticalCompression) || x >= context.lastOddRepPosition)
                verticalRepCount &= ~1;
        }

        if (x == context.borderPoint)
        {
            result.splitPosHint = result.data.size();
            if (verticalRepCount > 0 && (context.flags & hurryUpViaIY))
            {
                result.extraIyDelta = std::min(2, verticalRepCount);
                x += result.extraIyDelta;
                continue;
            }
        }

        uint16_t* buffer16 = (uint16_t*) (context.buffer + index);
        uint16_t word = *buffer16;
        word = swapBytes(word);

        assert(x < context.maxX+1);
        if (x == 31 && !verticalRepCount) //< 31, not maxX here
            return false;
        if (context.borderPoint && x < context.borderPoint)
        {
            if (x == context.borderPoint - 1 && !verticalRepCount)
                return false;
            if (x + verticalRepCount >= context.borderPoint)
            {
                const int prevVerticalRepCount = verticalRepCount;
                verticalRepCount -= context.borderPoint -x;
                x = context.borderPoint;
                if (verticalRepCount > 3)
                {
                    if (auto hl = findRegister(registers, "hl", result.isAltReg))
                    {
                        if (result.isAltAf)
                            result.exAf();
                        hl->updateToValue(result, 32 - prevVerticalRepCount, registers, context.af);
                        hl->addSP(result);
                        if (auto f = findRegister8(registers, 'f'))
                            f->value.reset();
                        sp.loadFromReg16(result, *hl);
                        x += verticalRepCount;
                        result.scf();
                    }
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
                hl->updateToValue(result, -verticalRepCount, registers, context.af);
                hl->addSP(result);
                if (auto f = findRegister8(registers, 'f'))
                    f->value.reset();
                sp.loadFromReg16(result, *hl);
                x += verticalRepCount;
                continue;
            }
        }


        // Decrement stack if line has same value from previous step (vertical compression)
        if (verticalRepCount & 1 )
        {
            sp.decValue(result, verticalRepCount);
            x += verticalRepCount;
            continue;
        }

        // push existing 16 bit value.
        if (x < context.maxX && loadWordFromExistingRegister(context, result, registers, word, x))
        {
            x += 2;
            continue;
        }

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

        if (choisedLine.splitPosHint >= 0)
            result.splitPosHint = result.data.size() + choisedLine.splitPosHint;

        result += choisedLine;
        result.isAltReg = choisedLine.isAltReg;
        result.isAltAf = choisedLine.isAltAf;
        result.lastOddRepPosition = choisedLine.lastOddRepPosition;
        if (choisedLine.extraIyDelta > 0)
            result.extraIyDelta = choisedLine.extraIyDelta;
        registers = chosedRegisters;

        return true;
    }

    if (context.flags & oddVerticalCompression)
        result.lastOddRepPosition = context.maxX;
    if (result.isAltAf)
        result.exAf();
    return true;
}

std::future<std::vector<CompressedLine>> compressLinesAsync(const Context& context, const std::vector<int>& lines)
{
    return std::async(
        [context, lines]()
        {
            std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
            std::vector<CompressedLine> result;
            for (const auto line: lines)
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

                        if (stackMovingAtStart >= 5)
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
                if (stackMovingAtStart >= 5)
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

Register16 findBestByte(uint8_t* buffer, int imageHeight, const std::vector<int>* sameBytesCount = nullptr, int* usageCount = nullptr)
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
    std::vector<int>* sameBytesCount, int imageHeight)
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
    context.af.h.value = 0;

    std::vector<std::future<std::vector<CompressedLine>>> compressors(8);

    for (int i = 0; i < 8; ++i)
    {

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
    compressedData.af = context.af;
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

std::vector<int> createSameBytesTable(int flags, const uint8_t* buffer,
    std::vector<bool>* maskColor, int imageHeight)
{
    std::vector<int> result;
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

CompressedData compress(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight)
{
    // Detect the most common byte in image
    std::vector<int> bytesCount(256);
    for (int i = 0; i < 32 * imageHeight; ++i)
        ++bytesCount[buffer[i]];

    Register8 a('a');
    Register16 af("af");
    int bestCounter = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (bytesCount[i] > bestCounter)
        {
            a.value = (uint8_t)i;
            bestCounter = bytesCount[i];
        }
    }

    std::vector<bool> maskColor;
    if (flags & skipInvisibleColors)
        maskColor = removeInvisibleColors(flags, buffer, colorBuffer, imageHeight);
    std::vector<int> sameBytesCount = createSameBytesTable(flags, buffer, &maskColor, imageHeight);

    CompressedData result = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

    if (!(flags & inverseColors))
        return result;

    std::cout << "rastr ticks = " << result.ticks() << std::endl;

    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            std::cout << "Check block " << y << ":" << x  << std::endl;

            int lineNum = y * 8;

            inversBlock(buffer, colorBuffer, x, y);
            auto candidateLeft = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

            inversBlock(buffer, colorBuffer, x+1, y);
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
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;colorBuffer,
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

    auto doAddIY =
        [&](int value)
        {
            iy.decValue(result, value);
        };


    // Left part is exists
    if (context.minX > 0)
    {
        uint16_t delta = context.minX < 16 ? context.minX : 16 - (context.minX - 16);
        Z80Parser::serializeAddSpToFront(result, delta);
    }
    if (pushLine.splitPosHint >= 0 && pushLine.extraIyDelta)
    {
        if (pushLine.extraIyDelta)
            doAddIY(pushLine.extraIyDelta);
    }

    result += loadLine;
    int preloadTicks = result.drawTicks;

    result.maxDrawDelayTicks = pushLine.maxDrawDelayTicks;
    result.maxMcDrawShift = (64 - preloadTicks) + pushLine.maxDrawDelayTicks;

    Z80Parser parser;
    std::vector<Register16> registers = { Register16("bc"), Register16("de"), Register16("hl") };

    // parse pushLine to find point for LD SP, IY

    auto info = parser.parseCode(
        context.af,
        registers,
        pushLine.data.buffer(),
        pushLine.data.size(),
        /* start offset*/ 0,
        /* end offset*/ pushLine.splitPosHint,
        /* codeOffset*/ 0
    );

    auto info2 = parser.parseCode(
        context.af,
        registers,
        pushLine.data.buffer(),
        pushLine.data.size(),
        /* start offset*/ info.endOffset,
        /* end offset*/ pushLine.data.size(),
        /* codeOffset*/ 0);

    result.append(pushLine.data.buffer(), info.endOffset);
    //result.drawTicks += info.ticks;

    if (pushLine.splitPosHint >= 0)
    {
        result.append(kLdSpIy);
        result.drawTicks += kStackMovingTimeForMc;
    }


    result.append(pushLine.data.buffer() + info.endOffset, pushLine.data.size() - info.endOffset);
    result.extraIyDelta = pushLine.extraIyDelta;
    result.drawTicks += pushLine.drawTicks;

}

int getDrawTicks(const CompressedLine& pushLine)
{
    int drawTicks = pushLine.drawTicks;
    if (pushLine.splitPosHint >= 0)
        drawTicks += kStackMovingTimeForMc;
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

    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl")};
    CompressedLine line1;

    bool success = compressLineMain(context, line1, registers);
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
            for (auto& reg: registers)
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
                            auto prevRegs =  i < 3
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

    if (drawTicks > t2 && pushLine.splitPosHint >= 0)
    {
        context.flags |= hurryUpViaIY;
        CompressedLine pushLine2;
        success = compressLine(context, pushLine2, registers6,  /*x*/ context.minX);

        int drawTicks2 = getDrawTicks(pushLine2);

        if (drawTicks2 < drawTicks)
        {
            pushLine = pushLine2;
            drawTicks = drawTicks2;
        }
        else
        {
            context.flags &= ~hurryUpViaIY;
        }
    }
    if (!success)
    {
        std::cerr << "ERROR: unexpected error during compression multicolor line " << context.y
            << " (something wrong with oddVerticalCompression flag). It should not be! Just a bug." << std::endl;
        abort();
    }


    if (drawTicks > t2)
    {
        // TODO: implement me
        std::cerr << "ERROR: Line " << context.y << ". Not enough " << drawTicks - t2  << " ticks for multicolor" << std::endl;
        //assert(0);
        //abort();
    }

    pushLine.maxDrawDelayTicks = t2 - drawTicks;
    finilizeLine(context, result, loadLine, pushLine);
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

void alignMulticolorTimings(CompressedData& compressedData)
{
    // Align duration for multicolors


     // 1. Calculate extra delay for begin of the line if it draw too fast

    const int imageHeight = compressedData.data.size();

    for (const auto& line : compressedData.data)
        compressedData.mcDrawPhase = std::min(compressedData.mcDrawPhase, line.maxMcDrawShift);

    // Put extra delay to the start if drawing ahead of ray
    int i = 0;
    for (auto& line : compressedData.data)
    {
        std::vector<Register16> registers = { Register16("bc"), Register16("de"), Register16("hl") };
        int extraDelay = 0;
        Z80Parser parser;

        auto timingInfo = parser.parseCode(
            compressedData.af,
            registers,
            line.data.buffer(),
            line.data.size(),
            /* start offset*/ 0,
            /* end offset*/ line.data.size(),
            /* codeOffset*/ 0,
            [&](const Z80CodeInfo& info, const z80Command& command)
            {
                const auto inputReg = Z80Parser::findRegByItsPushOpCode(info.outputRegisters, command.opCode);
                if (!inputReg)
                    return false;

                int rightBorder = 32;
                static const int kInitialSpOffset = 16;
                int rayTicks = (kInitialSpOffset + info.spOffset) * kTicksOnScreenPerByte;
                rayTicks -= compressedData.mcDrawPhase;
                if (info.ticks < rayTicks)
                {
                    int delay = rayTicks - info.ticks;
                    extraDelay = std::max(extraDelay, delay);
                }

                return false;
            });

        if (extraDelay > 0)
            extraDelay = std::max(4, extraDelay);
        if (extraDelay > line.maxDrawDelayTicks)
        {
            std::cerr << "Something wrong. Multicolor line #" << i << ". Extray delay " << extraDelay << " is bigger than maxDrawDelayTicks=" << line.maxDrawDelayTicks << std::endl;
            abort();
        }

        const auto delay = Z80Parser::genDelay(extraDelay);
        line.push_front(delay);
        line.drawTicks += extraDelay;
        ++i;
    }

    int maxTicks = 0;
    int minTicks = std::numeric_limits<int>::max();
    int regularTicks = 0;
    int maxTicksLine = 0;
    int minTicksLine = 0;
    int maxPhase = 0;
    for (int i = 0; i < imageHeight; ++i)
    {
        const auto& line = compressedData.data[i];
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

    // 2. Add delay to the end of lines to make them equal duration

    for (auto& line : compressedData.data)
    {
        if (maxTicks != line.drawTicks && maxTicks - line.drawTicks < 4)
        {
            maxTicks += 4;
            break;
        }
    }

#ifdef LOG_INFO
    std::cout << "INFO: max multicolor ticks line #" << maxTicksLine << ", ticks=" << maxTicks<<  ". Min ticks line #" << minTicksLine << ", ticks=" << minTicks << std::endl;

    std::cout << "INFO: align multicolor to ticks to " << maxTicks << " losed ticks=" << maxTicks * 24 - regularTicks << std::endl;
#endif

    for (auto& line : compressedData.data)
    {
        const int endLineDelay = maxTicks - line.drawTicks;
        if (endLineDelay >= 41 + 21)
        {
            // TODO: Can update rastr here to spend free ticks:
            // LD A, iyh
            // ADD a, #90
            // rlca
            // rlca
            // rlca
            // LD iyh, a
            // LD sp, iy
            // total 41 ticks

        }
        const auto delayCode = Z80Parser::genDelay(endLineDelay);
        line.append(delayCode);
        line.drawTicks += endLineDelay;
    }

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
    std::vector<int> sameBytesCount = createSameBytesTable(context.flags, shufledBuffer.data(), /*maskColors*/ nullptr, imageHeight);
    context.sameBytesCount = &sameBytesCount;
    context.borderPoint = 16;

    int count1;
    context.af = findBestByte(suffledPtr, imageHeight, &sameBytesCount, &count1);
    context.af.isAltAf = true;

    CompressedData compressedData;
    for (int y = 0; y < imageHeight; y ++)
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

    alignMulticolorTimings(compressedData);
    compressedData.af = context.af;
    return compressedData;
}

CompressedData  compressColors(uint8_t* buffer, int imageHeight, const Register16& af2)
{
    CompressedData compressedData;
    int flags = verticalCompressionH; // | interlineRegisters;
    std::vector<int> sameBytesCount = createSameBytesTable(flags, buffer, /*maskColors*/ nullptr, imageHeight / 8);

    for (int y = 0; y < imageHeight / 8; y ++)
    {
        std::array<Register16, 3> registers1 = { Register16("bc"), Register16("de"), Register16("hl")};
        std::array<Register16, 3> registers2 = registers1;

        Context context;
        context.scrollDelta = kScrollDelta;
        context.flags = flags;
        context.imageHeight = imageHeight / 8;
        context.buffer = buffer;
        context.y = y;
        context.sameBytesCount = &sameBytesCount;

        CompressedLine line1, line2;
        compressLineMain(context, line1, registers1);
        context.af = af2;
        compressLineMain(context, line2, registers2);
        if (line1.drawTicks < line2.drawTicks)
            compressedData.data.push_back(line1);
        else
            compressedData.data.push_back(line2);
    }
    updateTransitiveRegUsage(compressedData.data);
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
    Register16 af{"af"};

    void setEndBlock(const uint8_t* ptr)
    {
        endBlock.push_back(*ptr++);
        endBlock.push_back(*ptr);
    }

    void removeTrailingStackMoving(int maxCommandToRemove = std::numeric_limits<int>::max())
    {
        lineEndPtr -= Z80Parser::removeTrailingStackMoving(codeInfo, maxCommandToRemove);
    }

    int ticksWithLoadingRegs(const Register16& _af, const std::vector<uint8_t>& serializedData, int codeOffset) const
    {
        const uint16_t lineStartOffset = lineStartPtr - codeOffset;

        auto firstCommands = Z80Parser::getCode(serializedData.data() + lineStartOffset, kJpIxCommandLen);
        auto omitedDataInfo = Z80Parser::parseCode(
            af, codeInfo.inputRegisters, firstCommands,
            0, firstCommands.size(), 0);

        const auto firstCommand = omitedDataInfo.commands[0];
        auto regUsage = Z80Parser::regUsageByCommand(firstCommand);
        auto newCodeInfo = codeInfo;
        if (regUsage.selfRegMask)
        {
            // Join LD REG8, X from omited data directly to the serialized registers
            newCodeInfo.inputRegisters = omitedDataInfo.outputRegisters;
            newCodeInfo.regUsage.regUseMask |= regUsage.selfRegMask;
            newCodeInfo.regUsage.selfRegMask &= ~regUsage.selfRegMask;
            newCodeInfo.ticks -= firstCommand.ticks;
        }
        auto regs = newCodeInfo.regUsage.getSerializedUsedRegisters(newCodeInfo.inputRegisters);
        return newCodeInfo.ticks + regs.drawTicks;
    }


    void serializeOmitedData(const Register16& _af, const std::vector<uint8_t>& serializedData, int codeOffset,
        int omitedDataSize,
        std::optional<uint16_t> updatedHlValue)
    {
        const uint16_t lineStartOffset = lineStartPtr - codeOffset;

        auto firstCommands = Z80Parser::getCode(serializedData.data() + lineStartOffset, omitedDataSize);
        omitedDataInfo = Z80Parser::parseCode(
            af, codeInfo.inputRegisters, firstCommands,
            0, firstCommands.size(), 0);

        const int firstCommandsSize = firstCommands.size();
        if (!omitedDataInfo.commands.empty())
        {
            const auto firstCommand = omitedDataInfo.commands[0];
            auto regUsage = Z80Parser::regUsageByCommand(firstCommand);
            if (regUsage.selfRegMask)
            {
                // Join LD REG8, X from omited data directly to the serialized registers
                codeInfo.inputRegisters = omitedDataInfo.outputRegisters;
                codeInfo.regUsage.regUseMask |= regUsage.selfRegMask;
                codeInfo.regUsage.selfRegMask &= ~regUsage.selfRegMask;
                if (updatedHlValue)
                {
                    auto hl = findRegister(codeInfo.inputRegisters, "hl");
                    hl->setValue(*updatedHlValue);
                }

                firstCommands.erase(firstCommands.begin(), firstCommands.begin() + firstCommand.size);
            }
        }

        auto regs = codeInfo.regUsage.getSerializedUsedRegisters(codeInfo.inputRegisters);
        regs.serialize(preambula);
        addToPreambule(firstCommands);

        if (!omitedDataInfo.hasJump)
            addJpIx(lineStartPtr + firstCommandsSize);
    }

    void makePreambulaForMC(
        const Register16& _af,
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        const CompressedLine* line)
    {
        af = _af;
        std::optional<uint16_t> updatedHlValue;

        if (line->stackMovingAtStart != line->minX)
        {
            bool hasAddHlSp = line->stackMovingAtStart > 4;
            if (hasAddHlSp)
                updatedHlValue = -line->minX;
        }

        /*
         * In whole frame JP ix there is possible that first bytes of the line is 'broken' by JP iX command
         * So, descriptor point not to the line begin, but some line offset (2..4 bytes) and it repeat first N bytes of the line
         * directly in descriptor preambula. Additionally, preambula contains alignment delay and correction for SP register if need.
         */

        if (extraDelay > 0)
            addToPreambule(Z80Parser::genDelay(extraDelay));

        std::vector<uint8_t> firstCommands;
        serializeOmitedData(_af, serializedData, codeOffset, kJpIxCommandLen, updatedHlValue);
    }

    void makePreambulaForOffscreen(
        const Register16& _af,
        const std::vector<uint8_t>& serializedData,
        int codeOffset,
        int descriptorsDelta)
    {
        af = _af;

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

        if (startSpDelta > 0)
            serializeSpDelta(startSpDelta);

        serializeOmitedData(_af, serializedData, codeOffset, kJpIxCommandLen - descriptorsDelta, std::nullopt);
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

    void addJpIx(uint16_t value)
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
    // Multicolor drawing code
};

struct MulticolorDescriptor
{
    uint16_t addressBegin = 0;
};

struct ColorDescriptor
{
    uint16_t addressBegin = 0;
    uint16_t addressEnd = 0;
};

struct JpIxDescriptor
{
    uint16_t address = 0;
    std::vector<uint8_t> originData;
};

#pragma pack(pop)

std::vector<JpIxDescriptor> createWholeFrameJpIxDescriptors(
    const std::vector<LineDescriptor>& descriptors)
{

    std::vector<JpIxDescriptor> jpIxDescriptors;

    const int imageHeight = descriptors.size();
    const int blocks64 = imageHeight / 64;
    const int bankSize = imageHeight / 8;
    const int colorsHeight = imageHeight / 8;

    // . Create delta for JP_IX when shift to 1 line
    for (int i = 0; i < 64; ++i)
    {
        int line = i % 64;

        for (int i = 0; i < blocks64 + 2; ++i)
        {
            int l = (line + i * 64) % imageHeight;

            JpIxDescriptor d;
            d.address = descriptors[l].rastrForMulticolor.lineEndPtr;
            d.originData = descriptors[l].rastrForMulticolor.endBlock;
            jpIxDescriptors.push_back(d);

            d.address = descriptors[l].rastrForOffscreen.lineEndPtr;
            d.originData = descriptors[l].rastrForOffscreen.endBlock;
            jpIxDescriptors.push_back(d);
        }
    }
    return jpIxDescriptors;
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

int serializeMainData(
    const CompressedData& data,
    const CompressedData& multicolorData,
    std::vector<LineDescriptor>& descriptors,
    const std::string& inputFileName, uint16_t codeOffset, int flags,
    int mcDrawTicks)
{
    std::vector<uint8_t> serializedDescriptors;

    using namespace std;
    const int imageHeight = data.data.size();
    ofstream mainDataFile;
    std::string mainDataFileName = inputFileName + ".main";
    mainDataFile.open(mainDataFileName, std::ios::binary);
    if (!mainDataFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
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
    std::vector<uint8_t> serializedData;
    std::vector<int> lineOffset;
    for (int y = 0; y < imageHeight; ++y)
    {
        const auto& line = data.data[y];
        lineOffset.push_back(serializedData.size());
        line.serialize(serializedData);
    }


    // serialize descriptors

    const int bankSize = imageHeight / 8;
    const int colorsHeight = bankSize;

    const int reachDescriptorsBase = codeOffset + serializedData.size();

    std::vector<uint16_t> reachDescriptorOffset;
    std::vector<std::pair<int, int>> lockedBlocks; //< Skip locked blocks in optimization. Just in case.

    for (int d = 0; d < imageHeight; ++d)
    {
        const int srcLine = d % imageHeight;

        LineDescriptor descriptor;
        int lineBank = srcLine % 8;
        int lineInBank = srcLine / 8;
        int lineNum = bankSize * lineBank + lineInBank;
        const auto& dataLine = data.data[lineNum];
        int relativeOffsetToStart = lineOffset[lineNum];

        // Do not swap DEC SP, LD REG, XX at this mode
        Z80Parser::swap2CommandIfNeed(serializedData, relativeOffsetToStart, lockedBlocks);
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

        int lineEndInBank = lineInBank + 8;
        if (lineEndInBank > bankSize)
            lineEndInBank -= bankSize;
        int fullLineEndNum = lineEndInBank + lineBank * bankSize;

        int relativeOffsetToStart = lineOffset[lineNum];

        int relativeOffsetToEnd = fullLineEndNum < imageHeight
            ? lineOffset[fullLineEndNum]
            : serializedData.size();
        if (lineEndInBank == bankSize)
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
        if (flags & interlineRegisters)
            linePreambulaTicks = dataLine.getSerializedUsedRegisters().drawTicks;

        ticksRest -= kJpFirstLineDelay; //< Jump from descriptor to the main code


        Z80Parser parser;
        int ticksLimit = ticksRest - linePreambulaTicks;
        int extraCommandsIncluded = 0;
        int descriptorsDelta = 0;
        descriptor.rastrForMulticolor.codeInfo = parser.parseCode(
            data.af,
            *dataLine.inputRegisters,
            serializedData,
            relativeOffsetToStart, relativeOffsetToEnd,
            codeOffset,
            [ticksLimit, &extraCommandsIncluded, &descriptorsDelta](const Z80CodeInfo& info, const z80Command& command)
            {
                int sum = info.ticks + command.ticks;
                bool success = sum == ticksLimit || sum < ticksLimit - 3;
                if (!success)
                {
                    if (command.opCode == kDecSpCode || command.opCode == kAddHlSpCode || command.opCode == kLdSpHlCode)
                    {
                        ++extraCommandsIncluded;
                        descriptorsDelta += command.size;
                        return false; //< Continue
                    }
                }
                return !success;
            });


        parser.swap2CommandIfNeed(serializedData, descriptor.rastrForMulticolor.codeInfo.endOffset, lockedBlocks);
        descriptor.rastrForOffscreen.codeInfo = parser.parseCode(
            data.af,
            descriptor.rastrForMulticolor.codeInfo.outputRegisters,
            serializedData,
            descriptor.rastrForMulticolor.codeInfo.endOffset, relativeOffsetToEnd,
            codeOffset);

        int spDeltaSum = descriptor.rastrForMulticolor.codeInfo.spOffset + descriptor.rastrForOffscreen.codeInfo.spOffset + (dataLine.stackMovingAtStart - dataLine.minX);
        if (spDeltaSum < -256)
        {
            std::cerr << "Invalid SP delta sum " << spDeltaSum
                << " at line #" << d << ". It is bug! Expected exact value -256 or a bit above with flag optimizeLineEdge" << std::endl;
            abort();
        }


        descriptor.rastrForMulticolor.lineStartPtr = relativeOffsetToStart  + codeOffset;
        descriptor.rastrForMulticolor.lineEndPtr = descriptor.rastrForMulticolor.codeInfo.endOffset + codeOffset;

        descriptor.rastrForOffscreen.lineStartPtr = descriptor.rastrForMulticolor.lineEndPtr;
        descriptor.rastrForOffscreen.lineEndPtr = relativeOffsetToEnd + codeOffset;
        descriptor.rastrForOffscreen.startSpDelta = -descriptor.rastrForMulticolor.codeInfo.spOffset;
        descriptor.rastrForOffscreen.startSpDelta -= dataLine.stackMovingAtStart - dataLine.minX;

        descriptor.rastrForMulticolor.removeTrailingStackMoving(extraCommandsIncluded);
        // align timing for RastrForMulticolor part
        ticksRest -= descriptor.rastrForMulticolor.ticksWithLoadingRegs(data.af, serializedData, codeOffset);
        descriptor.rastrForMulticolor.extraDelay = ticksRest;

        if (flags & optimizeLineEdge)
            descriptor.rastrForOffscreen.removeTrailingStackMoving();

        descriptor.rastrForMulticolor.makePreambulaForMC(data.af, serializedData, codeOffset, &dataLine);
        descriptor.rastrForOffscreen.makePreambulaForOffscreen(data.af, serializedData, codeOffset, descriptorsDelta);

        descriptor.rastrForMulticolor.descriptorLocationPtr = reachDescriptorsBase + serializedDescriptors.size();
        descriptor.rastrForMulticolor.serialize(serializedDescriptors);

        descriptor.rastrForOffscreen.descriptorLocationPtr = reachDescriptorsBase + serializedDescriptors.size();
        descriptor.rastrForOffscreen.serialize(serializedDescriptors);

        descriptors.push_back(descriptor);
    }

    for (auto& descriptor: descriptors)
    {
        descriptor.rastrForMulticolor.setEndBlock(serializedData.data() + descriptor.rastrForMulticolor.lineEndPtr - codeOffset);
        descriptor.rastrForOffscreen.setEndBlock(serializedData.data() + descriptor.rastrForOffscreen.lineEndPtr - codeOffset);
    }

    mainDataFile.write((const char*)serializedData.data(), serializedData.size());
    reachDescriptorFile.write((const char*)serializedDescriptors.data(), serializedDescriptors.size());

    int serializedSize = serializedData.size() + serializedDescriptors.size();
    return serializedSize;
}

int serializeColorData(const CompressedData& colorData, const std::string& inputFileName, uint16_t codeOffset)
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

    int size = 0;
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
            serializedData.push_back(0x26); // LD H, 0 as an filler
            serializedData.push_back(0);
        }
        line.serialize(serializedData);
    }

    // serialize color descriptors
    std::vector<ColorDescriptor> descriptors;
    for (int d = 0; d <= imageHeight; ++d)
    {
        const int srcLine = d % imageHeight;
        const int endLine = (d + 24) % imageHeight;

        ColorDescriptor descriptor;

        descriptor.addressBegin = lineOffset[srcLine] + codeOffset;
        if (imageHeight == 24)
            descriptor.addressBegin += 2; //< Avoid LD A,0 filler if exists.

        if (endLine == 0)
            descriptor.addressEnd = serializedData.size() - 3 + codeOffset;
        else
            descriptor.addressEnd = lineOffset[endLine] + codeOffset;

        descriptors.push_back(descriptor);
    }

    for (const auto& descriptor: descriptors)
        descriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    return serializedData.size();
    return size;
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

    phaseFile << "RASTR_REG_A               EQU    " << (unsigned) *rastrData.af.h.value << std::endl;
    phaseFile << "COLOR_REG_AF2             EQU    " << multicolorData.af.value16() << std::endl;
    phaseFile << "FIRST_LINE_DELAY          EQU    " << firstLineDelay << std::endl;
    phaseFile << "MULTICOLOR_DRAW_PHASE     EQU    " << multicolorData.mcDrawPhase << std::endl;
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
    std::vector<int> ldHlOffset;
    for (int y = 0; y < colorImageHeight; ++y)
    {
        lineOffset.push_back(serializedData.size());

        const auto& line = data.data[y];
        line.serialize(serializedData);

        // add LD HL, NEXT_LINE_ADDRESS command in the end of a line
        serializedData.push_back(0x21);
        ldHlOffset.push_back(serializedData.size());
        serializedData.push_back(0);
        serializedData.push_back(0);

        // add JP_IX command in the end of a line
        serializedData.push_back(0xdd);
        serializedData.push_back(0xe9);
    }

    // Resolve LD HL, PREV_LINE_ADDR
    for (int y = 0; y < colorImageHeight; ++y)
    {
        int prevLineOffset = y > 0 ? lineOffset[y - 1] : lineOffset[lineOffset.size()-1];
        uint16_t* ldHlPtr = (uint16_t*) (ldHlOffset[y] + serializedData.data());
        *ldHlPtr = prevLineOffset + codeOffset;
    }

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    // serialize multicolor descriptors

    std::vector<MulticolorDescriptor> descriptors;
    for (int d = 0; d < colorImageHeight + 23; ++d)
    {
        const int srcLine = d % colorImageHeight;

        MulticolorDescriptor descriptor;
        const auto& line = data.data[srcLine];
        const uint16_t lineAddressPtr = lineOffset[srcLine] + codeOffset;
        descriptor.addressBegin = lineAddressPtr;
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
        file.write((const char*)&descriptor, sizeof(descriptor));


    return serializedData.size();
}

int serializeRastrDescriptors(
    std::vector<LineDescriptor> descriptors,
    const std::string& inputFileName)
{
    using namespace std;
    int imageHeight = descriptors.size();


    ofstream file;
    std::string fileName = inputFileName + ".rastr.descriptors";
    file.open(fileName, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    for (int i = 0; i < imageHeight + kmaxDescriptorOffset; ++i)
    {
        int line = i % imageHeight;
        const auto& descriptor = descriptors[line];

        file.write((const char*)&descriptor.rastrForOffscreen.descriptorLocationPtr, 2);
        file.write((const char*)&descriptor.rastrForMulticolor.descriptorLocationPtr, 2);
    }
}

struct OffscreenTicks
{
    int preambulaTicks = 0;
    int payloadTicks = 0;
    int enterExitTicks = 0;

    int ticks() const
    {
        return preambulaTicks + payloadTicks + enterExitTicks;
    }

    OffscreenTicks& operator+=(const OffscreenTicks& other)
    {
        preambulaTicks += other.preambulaTicks;
        payloadTicks += other.payloadTicks;
        enterExitTicks += other.enterExitTicks;
        return *this;
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
        result.preambulaTicks += info.ticks;
        result.payloadTicks -= d.rastrForOffscreen.omitedDataInfo.ticks; //< These ticks are ommited to execute after jump to descriptor.
        result.payloadTicks += d.rastrForOffscreen.codeInfo.ticks;
    }
    return result;
}

int getColorTicksForWholeFrame(const CompressedData& data, int lineNum)
{
    int result = 0;
    const int imageHeight = data.data.size();

    for (int i = 0; i < 24; ++i)
    {
        int l  = (lineNum + i) % imageHeight;
        const auto& line = data.data[l];
        result += line.drawTicks;
    }
    if (data.data.size() == 24)
        result += 23 * 7; //< Image height 192 has additional filler LD A, 0 for color lines.

    // End line contains JP <first line> command. If drawing is stoppeed on the end line this command is not executed.
    int endLine = (lineNum + 24) % imageHeight;
    if (endLine == 0)
        result -= kJpFirstLineDelay;

    return result;
}

int serializeTimingData(
    const std::vector<LineDescriptor>& descriptors,
    const CompressedData& data, const CompressedData& color, const std::string& inputFileName,
    int flags)
{
    using namespace std;

    const int imageHeight = data.data.size();
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
        int colorTicks = getColorTicksForWholeFrame(color, (line + 7) / 8);
        ticks += colorTicks;

        if (line == 0)
            firstLineDelay = ticks;

        ticks += kLineDurationInTicks * 192; //< Rastr for multicolor + multicolor

        if (line % 8 == 0)
        {
            // Draw next frame longer in  6 lines
            ticks -= kLineDurationInTicks * 7;
        }
        else //if (line % 8 > 1)
        {
            // Draw next frame faster in one line ( 6 times)
            ticks += kLineDurationInTicks;
        }
        static const int kZ80CodeDelay = 3163;
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
                << ". preambula=" << offscreenTicks.preambulaTicks
                << ". off rastr=" << offscreenTicks.payloadTicks
                << std::endl;
            #endif
        }
        worseLineTicks = std::min(worseLineTicks, freeTicks);
        const uint16_t freeTicks16 = (uint16_t) freeTicks;
        timingDataFile.write((const char*)&freeTicks16, sizeof(freeTicks16));

        if (line == 0)
            firstLineDelay += freeTicks;
    }
    std::cout << "worse line free ticks=" << worseLineTicks << ". ";
    if (kMinDelay - worseLineTicks > 0)
        std::cout << "Not enough ticks:" << kMinDelay - worseLineTicks;
    std::cout <<  std::endl;

    return firstLineDelay;
}

int serializeJpIxDescriptors(
    const std::vector<LineDescriptor>& descriptors,
    const std::string& inputFileName)
{
    using namespace std;

    const int imageHeight = descriptors.size();
    ofstream jpIxDescriptorFile;
    std::string jpIxDescriptorFileName = inputFileName + ".jpix";
    jpIxDescriptorFile.open(jpIxDescriptorFileName, std::ios::binary);
    if (!jpIxDescriptorFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    std::vector<JpIxDescriptor> jpIxDescr = createWholeFrameJpIxDescriptors(descriptors);
    for (const auto& d: jpIxDescr)
    {
        jpIxDescriptorFile.write((const char*) &d.address, sizeof(uint16_t));
        jpIxDescriptorFile.write((const char*) d.originData.data(), d.originData.size());
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

        fileIn.read((char*) bufferPtr, 6144);
        fileIn.read((char*) colorBufferPtr, 768);
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

    int flags = verticalCompressionL | interlineRegisters | skipInvisibleColors | optimizeLineEdge; // | inverseColors;

    const auto t1 = std::chrono::system_clock::now();

    CompressedData data = compress(flags, buffer.data(), colorBuffer.data(), imageHeight);

    CompressedData multicolorData = compressMultiColors(colorBuffer.data(), imageHeight / 8);
    // Multicolor data displaying from top to bottom
    //std::reverse(multicolorData.data.begin(), multicolorData.data.end());

    CompressedData colorData = compressColors(colorBuffer.data(), imageHeight, multicolorData.af);

    const auto t2 = std::chrono::system_clock::now();

    std::cout << "compression time= " <<  std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;

    static const int uncompressedTicks = 21 * 16 * imageHeight;
    static const int uncompressedColorTicks = uncompressedTicks / 8;
    std::cout << "uncompressed ticks: " << uncompressedTicks << " compressed ticks: "
        << data.ticks() << ", ratio: " << (data.ticks() / float(uncompressedTicks))
        << ", data size:" << data.size() << std::endl;

    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " compressed color ticks: "
        << colorData.ticks() << ", ratio: " << colorData.ticks() / (float) uncompressedColorTicks << std::endl;
    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " multi color ticks(in progress): "
        << multicolorData.ticks() << ", ratio: " << multicolorData.ticks() / (float) uncompressedColorTicks << std::endl;
    std::cout << "total ticks: " << data.ticks() + colorData.ticks() + multicolorData.ticks() << std::endl;

    // put JP to the latest line for every bank
    for (int bank = 0; bank < 8; ++bank)
    {
        int bankSize = imageHeight / 8;
        int line = bank * bankSize + bankSize - 1;
        int firstLineOffset = data.size(0, bank * bankSize);
        data.data[line].jp(firstLineOffset + codeOffset);
    }

    std::vector<LineDescriptor> descriptors;

    int multicolorTicks = multicolorData.data[0].drawTicks;
    int mainDataSize = serializeMainData(data, multicolorData, descriptors, outputFileName, codeOffset, flags, multicolorTicks);

    // put JP to the latest line of colors
    colorData.data[colorData.data.size() - 1].jp(codeOffset + mainDataSize);
    int colorDataSize = serializeColorData(colorData, outputFileName, codeOffset + mainDataSize);
    serializeMultiColorData(multicolorData, outputFileName, codeOffset + mainDataSize + colorDataSize);

    serializeRastrDescriptors(descriptors, outputFileName);
    serializeJpIxDescriptors(descriptors, outputFileName);

    int firstLineDelay = serializeTimingData(descriptors, data, colorData, outputFileName, flags);
    serializeAsmFile(outputFileName, data, multicolorData, flags, firstLineDelay);
    return 0;
}
