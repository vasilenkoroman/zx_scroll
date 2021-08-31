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

#define TEMP_FOR_TEST_COLORS 1

static const int totalTicksPerFrame = 71680;
static const int kCodeOffset = 0x5e00;

static const uint8_t DEC_SP_CODE = 0x3b;
static const uint8_t LD_BC_CODE = 1;

static const int lineSize = 32;
static const int kScrollDelta = 1;
static const int kColorScrollDelta = 1;

// flags
static const int interlineRegisters = 1; //< Experimental. Keep registers between lines.
static const int verticalCompressionL = 2; //< Skip drawing data if it exists on the screen from the previous step.
static const int verticalCompressionH = 4; //< Skip drawing data if it exists on the screen from the previous step.
static const int oddVerticalCompression = 8; //< can skip odd drawing bytes.
static const int inverseColors = 16;
static const int forceToUseExistingRegisters = 32;


static const int skipInvisibleColors = 32;
static const int kJpFirstLineDelay = 10;
static const int kLineDurationInTicks = 224;
static const int kRtMcContextSwitchDelay = 86 + 10; // context switch + ld hl in the end of the multicolor line
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
    std::vector<int>* sameBytesCount = nullptr;
    int y = 0;
    int maxX = 31;
    int minX = 0;
    int lastOddRepPosition = 32;
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


void inversBlock(uint8_t* buffer, int x, int y)
{
    uint8_t* ptr = buffer + y * 8 * 32 + x;
    for (int k = 0; k < 8; ++k)
    {
        ptr[0] = ~ptr[0];
        ptr += 32;
    }
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
        success = reg.l.updateToValue(result, (uint8_t)word, registers);
    else if (canAvoidSecond)
        success = reg.h.updateToValue(result, word >> 8, registers);
    else
        success = reg.updateToValue(result, word, registers);
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
    const Context& context,
    CompressedLine& line,
    std::array<Register16, N>& registers)
{
    CompressedLine line1, line2;
    auto registers1 = registers;
    auto registers2 = registers;

    line1.flags = context.flags;
    bool success1 = compressLine(context, line1, registers1,  /*x*/ context.minX);
    Context context2 = context;
    context2.flags |= oddVerticalCompression;
    line2.flags = context2.flags;
    bool success2 = compressLine(context2, line2, registers2,  /*x*/ context.minX);

    bool useSecondLine = success2 && line2.drawTicks < line1.drawTicks;
    if (useSecondLine)
        line = line2;
    else
        line = line1;

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

            bool valueOk;
            if (canAvoidFirst && canAvoidSecond)
                valueOk = true;
            else if (canAvoidFirst)
                valueOk = reg.l.hasValue((uint8_t)word);
            else if (canAvoidSecond)
                valueOk = reg.h.hasValue(word >> 8);
            else
                valueOk = reg.hasValue16(word);

            if (valueOk)
            {
                if (result.isAltReg != reg.isAlt)
                    result.exx();
                reg.push(result);
                return true;
            }
        }
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
        if (!(context.flags & oddVerticalCompression) || x > context.lastOddRepPosition)
            verticalRepCount &= ~1;

        uint16_t* buffer16 = (uint16_t*) (context.buffer + index);
        uint16_t word = *buffer16;
        word = swapBytes(word);

        assert(x < context.maxX+1);
        if (x == 31 && !verticalRepCount) //< 31, not maxX here
            return false;

        // Decrement stack if line has same value from previous step (vertical compression)
        // Up to 4 bytes is more effetient to decrement via 'DEC SP' call.
        if (verticalRepCount > 4 && !result.isAltReg)
        {
            if (auto hl = findRegister(registers, "hl"))
            {
                hl->updateToValue(result, -verticalRepCount, registers);
                hl->addSP(result);
                if (auto f = findRegister8(registers, 'f'))
                    f->value.reset();
                sp.loadFromReg16(result, *hl);
                x += verticalRepCount;
                continue;
            }
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
            registers[0].updateToValue(result, word, registers);
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
        result += choisedLine;
        result.isAltReg = choisedLine.isAltReg;
        if (context.flags & oddVerticalCompression)
            result.lastOddRepPosition = x;
        result.lastOddRepPosition = choisedLine.lastOddRepPosition;
        registers = chosedRegisters;

        return true;
    }
    if (result.isAltReg)
        result.exx();
    if (result.isAltAf)
        result.exAf();

    if (context.flags & oddVerticalCompression)
        result.lastOddRepPosition = context.maxX;
    return true;
}

std::future<std::vector<CompressedLine>> compressLinesAsync(const Context& context, const std::vector<int>& lines)
{
    return std::async(
        [context, lines]()
        {
            //std::array<Register16, 2> registers = { Register16("bc"), Register16("de")};
            std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
            //std::array<Register16, 4> registers = { Register16("bc"), Register16("de"), Register16("hl"), Register16("af") };
            std::vector<CompressedLine> result;

            for (const auto line : lines)
            {
                Context ctx = context;
                ctx.y = line;

                CompressedLine line;
                auto registers1 = registers;
                compressLineMain(ctx, line, registers1);
                result.push_back(line);
                if (context.flags & interlineRegisters)
                    registers = registers1;
            }
            updateTransitiveRegUsage(result);
            return result;
        }
    );
}

std::vector<bool> removeInvisibleColors(int flags, uint8_t* buffer, uint8_t* colorBuffer, int imageHeight, uint8_t bestByte)
{
    std::vector<bool> result;
    bool moveDown = flags & verticalCompressionL;
    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            bool isHidden = isHiddenData(colorBuffer, x, y);
            result.push_back(isHidden);
            if (isHidden)
            {
                uint8_t* ptr = buffer + y * 32 + x;
                for (int i = 0; i < 8; ++i)
                {
                    *ptr = bestByte;
                    ptr += 32;
                }
            }
        }
    }

    return result;
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

    std::vector<int> wordsCount(65536);
    uint16_t* buffer16 = (uint16_t*)buffer;
    for (int i = 0; i < 16 * imageHeight; ++i)
        ++wordsCount[buffer16[i]];

    int bestWordCounter = 0;
    for (int i = 0; i < 65536; ++i)
    {
        if (wordsCount[i] > bestWordCounter)
        {
            af.setValue((uint16_t) i);
            bestWordCounter = wordsCount[i];
        }
    }
    std::cout << "best byte = " << (int) *af.h.value << " best word=" << af.value16() << std::endl;
    std::vector<bool> maskColor;
    if (flags & skipInvisibleColors)
        maskColor = removeInvisibleColors(flags, buffer, colorBuffer, imageHeight, *a.value);
    std::vector<int> sameBytesCount = createSameBytesTable(flags, buffer, &maskColor, imageHeight);


    CompressedData result = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);
    if (!(flags & inverseColors))
        return result;

    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            std::cout << "Check block " << y << ":" << x  << std::endl;

            int lineNum = y * 8;

            inversBlock(buffer, x, y);
            auto candidateLeft = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

            inversBlock(buffer, x+1, y);
            auto candidateBoth = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);

            inversBlock(buffer, x, y);
            auto candidateRight = compressImageAsync(flags, buffer, &maskColor, &sameBytesCount, imageHeight);
            inversBlock(buffer, x + 1, y);

            const int resultTicks = result.ticks();
            if (candidateLeft.ticks() < resultTicks
                && candidateLeft.ticks() < candidateBoth.ticks()
                && candidateLeft.ticks() < candidateRight.ticks())
            {
                result = candidateLeft;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;
                inversBlock(buffer, x, y);
            }
            else if (candidateBoth.ticks() < resultTicks
                && candidateBoth.ticks() < candidateLeft.ticks()
                && candidateBoth.ticks() < candidateRight.ticks())
            {
                result = candidateBoth;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;
                inversBlock(buffer, x, y);
                inversBlock(buffer, x + 1, y);
            }
            else if (candidateRight.ticks() < resultTicks
                && candidateRight.ticks() < candidateLeft.ticks()
                && candidateRight.ticks() < candidateBoth.ticks())
            {
                result = candidateRight;
                std::cout << "reduce ticks to = " << result.ticks() << std::endl;
                inversBlock(buffer, x + 1, y);
            }
        }
    }

    return result;
}

#if 0
CompressedLine  compressMultiColorsLine(uint16_t* buffer, uint16_t* nextLine, uint16_t generatedCodeMemAddress, bool* success, bool useSlowMode)
{
    /*
     *      Screen in words [0..15]
     *       0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
     *      -------------------------------------------------
     *      |1 |1 |1 |2 |2 |2 |2 |2 |2 |2 |2 |3 |3 |3 |3 |3 |
     *      -------------------------------------------------
     *      Drawing step (1..3): 3, 8, 5 words
     *
     *      Input params: SP should point to the lineStart + 6 (3 word)
     */

    *success = true;

    CompressedLine line;


    std::vector<Register16> registers = {Register16("bc"), Register16("de"), Register16("hl")};
    std::vector<Register16> registers1 = {Register16("bc'"), Register16("de'"), Register16("hl'")};

    Register16& bc(registers[0]);
    Register16& de(registers[1]);
    Register16& hl(registers[2]);
    Register16& bc1(registers1[0]);
    Register16& de1(registers1[1]);
    Register16& hl1(registers1[2]);

    Register8& l(hl.l);
    Register8 a('a');


    Register16 sp("sp", 0);
    int trailingDecSp = 0;

    int updateSpAddress1 = 0;
    int updateSpAddress2 = 0;
    if (useSlowMode)
    {
        hl.loadFromReg16(line, sp);
        l.setBit(line, 4); //< l = l + 16 (8 words).
        hl.poke(line, generatedCodeMemAddress); //< Need to update value later, after exact address will be discovered
        updateSpAddress1 = line.data.size() - 2;

        a.loadX(line, 10); //< 5 words
        a.add(line, l);
        l.loadFromReg(line, a);
        hl.poke(line, generatedCodeMemAddress); //< Need to update value later, after exact address will be discovered
        updateSpAddress2 = line.data.size() - 2;
        hl.reset();
    }

    auto pushIfNeed =
        [&](auto* reg)
        {
            if (!reg)
            {
                sp.dec(line, 2);
                trailingDecSp += 2;
            }
            else
            {
                reg->push(line);
                trailingDecSp = 0;
            }
        };

    auto choiseRegister =
        [&](Register16& reg, int number, std::vector<Register16>& registers) -> Register16*
        {
            if (nextLine && buffer[number] == nextLine[number])
                return nullptr;
            const auto word = buffer[number];
            for (auto& reg16: registers)
            {
                if (reg16.hasValue16(word))
                    return &reg16;
            }

            reg.updateToValue(line, buffer[number], registers);
            trailingDecSp = 0;
            return &reg;
        };

    auto removeTrailingDecSp = [&]()
    {
        for (int i = 0; i < trailingDecSp; ++i)
        {
            assert(line.data.last() == DEC_SP_CODE);
            line.data.pop_back();
            line.drawTicks -= 6;
        }
        trailingDecSp = 0;
    };


    // Step 0 (before drawing).

    auto selBc = choiseRegister(bc, 6, registers);
    auto selDe = choiseRegister(de, 7, registers);
    auto selHl = choiseRegister(hl, 8, registers);
    line.exx();
    auto selDe1 = choiseRegister(de1, 4, registers1);
    auto selBc1 = choiseRegister(bc1, 5, registers1);

    auto selHl1 = choiseRegister(hl1, 0, registers1);

    // Done preparing data in 64 ticks

    // <--------------- Step 1: Start drawing here.
    // Wait 3 words before start pusing data (24 ticks).

    // Push 3 slow words (63 ticks).
    int startDrawingTick = line.drawTicks;

    // This function should be started to execute on prelineTicks ticks before current drawing line.
    line.prelineTicks = startDrawingTick - 24;

    pushIfNeed(selHl1);
    selHl1 = choiseRegister(hl1, 1, registers1);

    pushIfNeed(selHl1);
    selHl1 = choiseRegister(hl1, 2, registers1);

    pushIfNeed(selHl1);

    const int trailingCopy = trailingDecSp;
    removeTrailingDecSp();

    if (useSlowMode)
    {
        sp.loadXX(line, 0);
        // Replace command offset to real value here. 'LD (xx), HL' generated above is replacing constant for 'LD SP,XX' in previous line.
        line.replaceWord(updateSpAddress1, generatedCodeMemAddress + line.data.size() - 2);
    }
    else
    {
        // Move stack to 3 + 8 == 11 words in 27 ticks.
        hl1.loadXX(line, (3 + 8) * 2 - trailingCopy);
        hl1.addSP(line);
        sp.loadFromReg16(line,hl1);
    }

    selHl1 = choiseRegister(hl1, 3, registers1);


    while (line.drawTicks - startDrawingTick < 64)
        line.nop(); //< Wait to 8 more words is free for drawing

    // Done drawing in: 63 + 27 = 90 ticks
    // Ray on tick: 24 + 90 = 114, 11 more words is free to draw. Need 8 words. Extra ticks to compress is 26 before this line.
    // Step 2. Draw 8 more words
    pushIfNeed(selHl1);
    pushIfNeed(selDe1);
    pushIfNeed(selBc1);

    line.exx();
    trailingDecSp = 0;

    pushIfNeed(selBc);
    pushIfNeed(selDe);
    pushIfNeed(selHl);

    selBc = choiseRegister(bc, 9, registers);
    pushIfNeed(selBc);
    selBc = choiseRegister(bc, 10, registers);
    pushIfNeed(selBc);

    // Move stack to 8 + 5 == 13 words
    const int16_t spDelta = (8 + 5) * 2 - trailingDecSp;
    removeTrailingDecSp();

    // Done drawing 8 more words in: 112 ticks
    // Ray on tick: 114 + 112 = 226. Ticks rest: -2 (line rest)  + 11 * 8(drawn ticks) = 86

    if (useSlowMode)
    {
        // Replace command offset to real value here. 'LD (xx), HL' generated above is replacing constant for 'LD SP,XX' in previous line.
        sp.loadXX(line, 0);
        line.replaceWord(updateSpAddress2, generatedCodeMemAddress + line.data.size() - 2);
    }
    else
    {
        // Have enough ticks here to update SP in the regular way
        hl.loadXX(line, spDelta);
        hl.addSP(line);
        sp.loadFromReg16(line, hl);
    }
    // Ticks spend on changing Sp = 10 + 11 + 6 = 27. spend ticks=253.  Ticks rest = 59

    int ticksSpend = line.drawTicks - startDrawingTick;
    int ticksAllowed = 207;
    if (useSlowMode)
        ticksAllowed += 17 * 2;
    if (ticksSpend > ticksAllowed)
    {
        *success = false;
        return line;
    }

    // Step 3. Draw rest 5 words.
    for (int i = 11; i < 16; ++i)
    {
        selBc = choiseRegister(bc, i, registers);
        pushIfNeed(selBc);
    }
    // Done drawing 5 more words in: 105 ticks

    // Done drawing line. Total ticks: 305 (drawing) + 64 (prepare data) = 364 ticks

    removeTrailingDecSp();

    return line;
}
#endif

void finilizeLine(
    const Context& context,
    CompressedLine& result,
    const CompressedLine& loadLine,
    const CompressedLine& pushLine)
{
    Register16 hl("hl");
    Register16 sp("sp");
    if (context.minX >= 5)
    {
        hl.loadXX(result, context.minX);
        hl.addSP(result);
        sp.loadFromReg16(result, hl);
    }
    else
    {
        sp.decValue(result, context.minX);
    }
    result += loadLine;
    int preloadTicks = result.drawTicks;

    // TODO: optimize drawOffset ticks here. Current version is the most simple, but slower solution
    int extraDelay = result.drawOffsetTicks - preloadTicks;
    const auto delay = Z80Parser::genDelay(extraDelay);
    result.push_front(delay);
    result.drawTicks += extraDelay;

    result += pushLine;
    //result.jpIx();
}

CompressedLine  compressMultiColorsLine(Context context)
{
    CompressedLine result;
    static const int kBorderTime = kLineDurationInTicks - 128;
    static const int kStackMovingTime = 27;

    uint8_t* line = context.buffer + context.y * 32;
    int nextLineNum = (context.y + context.scrollDelta) % context.imageHeight;
    uint8_t* nextLine = context.buffer + nextLineNum * 32;

    // remove left and rigt edge
    for (int x = 0; x < 32; ++x)
    {
        if (line[x] == nextLine[x])
            ++context.minX;
        else
            break;
    }
    for (int x = 31; x >= 0; --x)
    {
        if (line[x] == nextLine[x])
            --context.maxX;
        else
            break;
    }

    int t1 = kBorderTime + (context.minX + 31 - context.maxX) * 4; // write whole line at once limit (no intermediate stack moving)
    int t2 = kLineDurationInTicks - kStackMovingTime; // write whole line in 2 tries limit

    //try 1. Use 3 registers, no intermediate stack correction, use default compressor

    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl")};
    CompressedLine line1;
    compressLineMain(context, line1, registers);

#if 0
    CompressedLine loadLine;
    CompressedLine pushLine;

    line1.splitPreLoadAndPush(&loadLine, &pushLine);
    if (pushLine.drawTicks <= t1)
    {
        // Ray should run some bytes on the current line before we start to overwrite it.
        // At this case ray goes from 32 to minX (backward direction from drawing point of view)
        result.drawOffsetTicks = (32 - context.minX) * kTicksOnScreenPerByte;
        finilizeLine(context, result, loadLine, pushLine);
        return result;
    }
#endif

    //try 2. Prepare input registers manually, use 'oddVerticalRep' information from auto compressor

    // 2.1 Create used words map (skip holes)

    struct PositionInfo
    {
        int position = 0;
        uint16_t value = 0;
        int useCount = 0;
    };

    std::vector<PositionInfo> positionsToUse;
    std::set<uint16_t> uniqueWords;

    context.lastOddRepPosition = line1.lastOddRepPosition;
    int x = context.minX;
    int exxPos = -1;
    while (x <= context.maxX)
    {
        const int index = context.y * 32 + x;
        int verticalRepCount = context.sameBytesCount ? context.sameBytesCount->at(index) : 0;
        verticalRepCount = std::min(verticalRepCount, context.maxX - x + 1);
        if (x > context.lastOddRepPosition)
            verticalRepCount &= ~1;
        if (verticalRepCount)
        {
            x += verticalRepCount;
            continue;
        }
        uint16_t* ptr = (uint16_t*) (context.buffer + index);
        uint16_t word = *ptr;
        word = swapBytes(word);
        PositionInfo data{ x, word, 1 };
        uniqueWords.insert(word);
        if (uniqueWords.size() > 3 && exxPos == -1)
            exxPos = positionsToUse.size();
        positionsToUse.push_back(data);

        x += 2;
    }
    assert(x < 32);

    // 2.3 fill register values

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

    std::array<Register16, 3> regMain = { Register16("bc"), Register16("de"), Register16("hl") };
    std::array<Register16, 3> regAlt = { Register16("bc'"), Register16("de'"), Register16("hl'") };
    CompressedLine loadLine;

    auto loadRegisters =
        [&](auto& registers, int startPos, int endPos)
        {
            int regIndex = 0;
            for (int i = startPos; i < endPos; ++i)
            {
                uint16_t value = positionsToUse[i].value;
                if (!hasSameValue(registers, value))
                {
                    registers[regIndex].updateToValue(loadLine, value, registers);
                    ++regIndex;
                    if (regIndex == 3)
                        break;
                }
            }
        };

    loadRegisters(regAlt, exxPos, positionsToUse.size());
    loadLine.exx();
    loadRegisters(regMain, 0, exxPos);

    // 2.4 start compressor with prepared register values

    std::array<Register16, 6> registers6 = { regMain[0], regMain[1], regMain[2], regAlt[0], regAlt[1], regAlt[2] };
    context.flags |= oddVerticalCompression | forceToUseExistingRegisters;
    CompressedLine pushLine;
    bool success = compressLine(context, pushLine, registers6,  /*x*/ context.minX);

    if (pushLine.drawTicks <= t1)
    {
        result.drawOffsetTicks = (32 - context.minX) * kTicksOnScreenPerByte;
        finilizeLine(context, result, loadLine, pushLine);
        return result;
    }

    // 3. Try to split line on 2 segments
    if (pushLine.drawTicks <= t2)
    {
        // TODO: implement me
        std::cerr << "Not finished yet. need add more code here. Two segment multicolor" << std::endl;
        assert(0);
        abort();
    }

    // 4. Split line on 3 segments. It enough for any line
    // TODO: implement me. See commented implementation compressMultiColorsLine. It need to finish it.
    std::cerr << "Not finished yet. need add more code here. Three segment multicolor" << std::endl;
    assert(0);
    abort();


    return result;
}

CompressedData compressMultiColors(uint8_t* buffer, int imageHeight)
{
    // TODO: fill it
    static const int generatedCodeAddress = 0;

    struct Context context;
    context.scrollDelta = 1;
    context.flags = verticalCompressionL;
    context.imageHeight = imageHeight;
    context.buffer = buffer;
    std::vector<int> sameBytesCount = createSameBytesTable(context.flags, buffer, /*maskColors*/ nullptr, imageHeight);
    context.sameBytesCount = &sameBytesCount;

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

    // Align duration for multicolors

    int maxTicks = 0;
    for (const auto& line : compressedData.data)
        maxTicks = std::max(maxTicks, line.drawTicks);

    // TODO: consider to avoid +4 here. In case some line need be aligned to [1..3] ticks it is possible
    // to modify line commands itself.
    for (auto& line : compressedData.data)
    {
        if (maxTicks - line.drawTicks < 4)
        {
            maxTicks += 4;
            break;
        }
    }

    for (auto& line: compressedData.data)
    {
        const int endLineDelay = maxTicks - line.drawTicks;
        const auto delayCode = Z80Parser::genDelay(endLineDelay);
        line.append(delayCode);
        line.drawTicks += endLineDelay;
    }

    return compressedData;
}

CompressedData  compressColors(uint8_t* buffer, int imageHeight)
{
    CompressedData compressedData;
    int flags = verticalCompressionH; // | interlineRegisters;
    std::vector<int> sameBytesCount = createSameBytesTable(flags, buffer, /*maskColors*/ nullptr, imageHeight / 8);

    for (int y = 0; y < imageHeight / 8; y ++)
    {
        std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl")};

        Context context;
        context.scrollDelta = kScrollDelta;
        context.flags = flags;
        context.imageHeight = imageHeight / 8;
        context.buffer = buffer;
        context.y = y;
        context.sameBytesCount = &sameBytesCount;
        CompressedLine line;
        bool success = compressLineMain(context, line, registers);
        compressedData.data.push_back(line);
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
    
    Z80CodeInfo startDataInfo;      //< Information about skipped start block (it's included directly to the descriptor preambula).
    std::vector<uint8_t> endBlock;  //< Information about replaced end block (JP IX command).

    int extraDelay = 0;             //< Alignment delay when serialize preambula.
    int startSpDelta = 0;           //< Addition commands to decrement SP included to preambula.

    void setEndBlock(const uint8_t* ptr)
    {
        endBlock.push_back(*ptr++);
        endBlock.push_back(*ptr);
    }

    int ticksWithLoadingRegs() const
    {
        auto regs = codeInfo.regUsage.getSerializedUsedRegisters(codeInfo.inputRegisters);
        return codeInfo.ticks + regs.drawTicks;
    }

    void makePreambula(const std::vector<uint8_t>& serializedData, int codeOffset)
    {
        /*
         * In whole frame JP ix there is possible that first bytes of the line is 'broken' by JP iX command
         * So, descriptor point not to the line begin, but some line offset (2..4 bytes) and it repeat first N bytes of the line
         * directly in descriptor preambula. Additionally, preambula contains alignment delay and correction for SP register if need.
         */

        if (extraDelay > 0)
            addToPreambule(Z80Parser::genDelay(extraDelay));

        if (startSpDelta > 0)
            serializeSpDelta(startSpDelta);

        const uint16_t lineStartOffset = lineStartPtr - codeOffset;
        auto regs = codeInfo.regUsage.getSerializedUsedRegisters(codeInfo.inputRegisters);
        regs.serialize(preambula);

        const auto firstCommands = Z80Parser::getCode(serializedData.data() + lineStartOffset, kJpIxCommandLen);
        addToPreambule(firstCommands);
        startDataInfo = Z80Parser::parseCode(firstCommands);
        if (!startDataInfo.hasJump)
            addJpIx(lineStartPtr + firstCommands.size());
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
    for (int i = 0; i < 64 + 8; ++i)
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
        Z80Parser::optimizePreambula(serializedData, relativeOffsetToStart, lockedBlocks);
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
        descriptor.rastrForMulticolor.codeInfo = parser.parseCodeToTick(
            *dataLine.inputRegisters,
            serializedData,
            relativeOffsetToStart, relativeOffsetToEnd,
            codeOffset,
            ticksRest - linePreambulaTicks - 4);  // Reserver 4 more ticks because delay<4 is not possible

        int relativeOffsetToMid = descriptor.rastrForMulticolor.codeInfo.endOffset;

        parser.optimizePreambula(serializedData, relativeOffsetToMid, lockedBlocks);
        descriptor.rastrForOffscreen.codeInfo = parser.parseCodeToTick(
            descriptor.rastrForMulticolor.codeInfo.outputRegisters,
            serializedData,
            relativeOffsetToMid, relativeOffsetToEnd,
            codeOffset, std::numeric_limits<int>::max());

        assert(descriptor.rastrForMulticolor.codeInfo.spDelta
            + descriptor.rastrForOffscreen.codeInfo.spDelta == 256);

        // align timing for RastrForMulticolor part
        ticksRest -= descriptor.rastrForMulticolor.ticksWithLoadingRegs();
        descriptor.rastrForMulticolor.extraDelay = ticksRest;

        descriptor.rastrForMulticolor.lineStartPtr = relativeOffsetToStart  + codeOffset;
        descriptor.rastrForMulticolor.lineEndPtr = relativeOffsetToMid + codeOffset;
        descriptor.rastrForMulticolor.makePreambula(serializedData, codeOffset);

        descriptor.rastrForOffscreen.lineStartPtr = descriptor.rastrForMulticolor.lineEndPtr;
        descriptor.rastrForOffscreen.lineEndPtr = relativeOffsetToEnd + codeOffset;
        descriptor.rastrForOffscreen.startSpDelta = descriptor.rastrForMulticolor.codeInfo.spDelta;
        descriptor.rastrForOffscreen.makePreambula(serializedData, codeOffset);

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
            serializedData.push_back(0x3e);
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

        file.write((const char*)&descriptor.rastrForMulticolor.descriptorLocationPtr, 2);
        file.write((const char*)&descriptor.rastrForOffscreen.descriptorLocationPtr, 2);
    }
}

int getTicksChainFor64Line(
    const std::vector<LineDescriptor>& descriptors,
    int screenLineNum)
{
    static const int kReturnDelay = 8;
    static const int k8LinesFuncDelay = 66;
    static const int kCorrectSpDelay = 27;

    int result = 0;
    const int imageHeight = descriptors.size();
    screenLineNum = screenLineNum % imageHeight;

    const int bankSize = imageHeight / 8;

    for (int b = 0; b < 8; ++b)
    {
        int lineNumber = (screenLineNum + b) % imageHeight;
        const auto& d = descriptors[lineNumber];

        int sum = 0;
        auto info = Z80Parser::parseCode(d.rastrForOffscreen.preambula);
        sum += info.ticks;
        sum -= d.rastrForOffscreen.startDataInfo.ticks; //< These ticks are ommited to execute after jump to descriptor.
        sum += d.rastrForOffscreen.codeInfo.ticks;
        sum += kReturnDelay;
        sum += k8LinesFuncDelay;

        result += sum;

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
        if (data.data.size() == 24)
            result += 7; //< Image height 192 has additional filler LD A, 0 for color lines.
    }

    // End line contains JP <first line> command. If drawing is stoppeed on the end line this command is not executed.
    int endLine = (lineNum + 24) % imageHeight;
    if (endLine == 0)
        result -= kJpFirstLineDelay;

    return result;
}

int serializeTimingData(
    const std::vector<LineDescriptor>& descriptors,
    const CompressedData& data, const CompressedData& color, const std::string& inputFileName)
{
    static const int kOffscreenRastrFuncDelay = 92;

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

    for (int line = 0; line < imageHeight; ++line)
    {
        // offscreen rastr
        int ticks = kOffscreenRastrFuncDelay;
        for (int i = 0; i < 3; ++i)
            ticks += getTicksChainFor64Line(descriptors, line + i * 64);

        // colors
        ticks += getColorTicksForWholeFrame(color, (line+7)/ 8);
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
        static const int kZ80CodeDelay = 1512;
        ticks += kZ80CodeDelay;

        uint16_t freeTicks = totalTicksPerFrame - ticks;
        timingDataFile.write((const char*)&freeTicks, sizeof(freeTicks));
    }

    return 0;
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

int main(int argc, char** argv)
{
    using namespace std;

    ifstream fileIn;

    if (argc < 3)
    {
        std::cerr << "Usage: scroll_image_compress <file_name> [<file_name>] <out_file_name>";
        return -1;
    }

    int fileCount = argc - 2;
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
        bufferPtr += 6144;
        colorBufferPtr += 768;
        fileIn.close();
    }

    const std::string outputFileName = argv[argc - 1];

    deinterlaceBuffer(buffer);
    writeTestBitmap(256, imageHeight, buffer.data(), outputFileName + ".bmp");

    mirrorBuffer8(buffer.data(), imageHeight);
    mirrorBuffer8(colorBuffer.data(), imageHeight / 8);

    int flags = verticalCompressionL | interlineRegisters; // | skipInvisibleColors; // | inverseColors;

    const auto t1 = std::chrono::system_clock::now();

    CompressedData multicolorData = compressMultiColors(colorBuffer.data(), imageHeight / 8);
    // Multicolor data displaying from top to bottom
    //std::reverse(multicolorData.data.begin(), multicolorData.data.end());

    CompressedData colorData = compressColors(colorBuffer.data(), imageHeight);

    CompressedData data = compress(flags, buffer.data(), colorBuffer.data(), imageHeight);
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
        data.data[line].jp(firstLineOffset + kCodeOffset);
    }

    std::vector<LineDescriptor> descriptors;

    int multicolorTicks = multicolorData.data[0].drawTicks;
    int mainDataSize = serializeMainData(data, multicolorData, descriptors, outputFileName, kCodeOffset, flags, multicolorTicks);

    // put JP to the latest line of colors
    colorData.data[colorData.data.size() - 1].jp(kCodeOffset + mainDataSize);
    int colorDataSize = serializeColorData(colorData, outputFileName, kCodeOffset + mainDataSize);
    serializeMultiColorData(multicolorData, outputFileName, kCodeOffset + mainDataSize + colorDataSize);

    serializeRastrDescriptors(descriptors, outputFileName);
    serializeJpIxDescriptors(descriptors, outputFileName);

    serializeTimingData(descriptors, data, colorData, outputFileName);


    return 0;
}
