#include <iostream>
#include <fstream>
#include <vector>
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

static const int totalTicksPerFrame = 71680;
static const int kCodeOffset = 0x5b00 + 768;

static const uint8_t DEC_SP_CODE = 0x3b;
static const uint8_t LD_BC_CODE = 1;

static const int lineSize = 32;
static const int kScrollDelta = 1;
static const int kColorScrollDelta = 1;

static const int interlineRegisters = 1; //< Experimental. Keep registers between lines.
static const int verticalCompressionL = 2; //< Skip drawing data if it exists on the screen from the previous step.
static const int verticalCompressionH = 4; //< Skip drawing data if it exists on the screen from the previous step.
static const int oddVerticalCompression = 8; //< can skip odd drawing bytes.
static const int inverseColors = 16;
static const int skipInvisibleColors = 32;
static const int kJpFirstLineDelay = 10;

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
};

void serialize(std::vector<uint8_t>& data, uint16_t value)
{
    data.push_back((uint8_t)value);
    data.push_back(value >> 8);
}

bool isHiddenData(uint8_t* colorBuffer, int x, int y)
{
    if (!colorBuffer)
        return false;
    const uint8_t colorData = colorBuffer[x + y * 32];
    return (colorData & 7) == ((colorData >> 3) & 7);
}

bool isHiddenData(const std::vector<bool>* hiddenData, int x, int y)
{
    if (!hiddenData || hiddenData->empty())
        return false;
    return hiddenData->at(x + y * 32);
}

template <int N>
Register16* findRegister(std::array<Register16, N>& registers, const std::string& name)
{
    for (auto& reg : registers)
    {
        if (reg.name() == name)
            return &reg;
    }
    return nullptr;
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
        uint8_t selfRegMask = line.selfRegMask;
        auto before = line.regUseMask;
        for (int j = 1; j <= 8; ++j)
        {
            int nextLineNum = (lineNum + j) % size;
            CompressedLine& nextLine = data[nextLineNum];

            selfRegMask |= nextLine.selfRegMask;
            uint8_t additionalUsage = nextLine.regUseMask & ~selfRegMask;
            line.regUseMask |= additionalUsage;
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

    bool success1 = compressLine(context, line1, registers1,  /*x*/ context.minX);
    Context context2 = context;
    context2.flags |= oddVerticalCompression;
    bool success2 = compressLine(context2, line2, registers2,  /*x*/ context.minX);

    bool useSecondLine = success2 && line2.drawTicks < line1.drawTicks;
    if (useSecondLine)
        line = line2;
    else
        line = line1;

    for (const auto& reg16 : registers)
        line.inputRegisters.push_back(reg16);

    if (useSecondLine)
        registers = registers2;
    else
        registers = registers1;

    for (const auto& reg16 : registers)
        line.outputRegisters.push_back(reg16);

    return true;
}

template <int N>
bool compressLine(
    const Context& context,
    CompressedLine&  result,
    std::array<Register16, N>& registers,
    int x)
{
    static Register16 sp("sp");

    while (x <= context.maxX)
    {
        int verticalRepCount = context.sameBytesCount ? context.sameBytesCount->at(context.y * 32 + x) : 0;
        verticalRepCount = std::min(verticalRepCount, context.maxX - x + 1);
        if (!(context.flags & oddVerticalCompression))
            verticalRepCount &= ~1;

        uint16_t* buffer16 = (uint16_t*) (context.buffer + context.y * 32 + x);
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
                if (!result.isAltAf)
                    ;// result.exAf();
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
        bool isChoised = false;

        bool canAvoidFirst = isHiddenData(context.maskColor, x, context.y / 8);
        bool canAvoidSecond = isHiddenData(context.maskColor, x + 1, context.y / 8);

        if (x < context.maxX)
        {
            for (auto& reg : registers)
            {
                bool valueOk;
                if (canAvoidFirst && canAvoidSecond)
                    valueOk = true;
                else if (canAvoidFirst)
                    valueOk = reg.l.hasValue((uint8_t)word);
                else if (canAvoidSecond)
                    valueOk = reg.h.hasValue(word >> 8);
                else
                    valueOk = reg.hasValue16(word);

                if (result.isAltReg == reg.isAlt && valueOk)
                {
                    reg.push(result);
                    x += 2;
                    isChoised = true;
                    break;
                }
            }
            if (isChoised)
                continue;
        }

        // Decrement stack if line has same value from previous step (vertical compression)
        if (verticalRepCount > 0)
        {
            sp.dec(result, verticalRepCount);
            x += verticalRepCount;
            continue;
        }

        for (auto& reg : registers)
        {
            if (result.isAltReg == reg.isAlt && reg.isEmpty() && reg.h.name == 'b')
            {
                reg.updateToValue(result, word, registers);
                reg.push(result);
                x += 2;
                isChoised = true;
                break;
            }
        }
        if (isChoised)
            continue;

        CompressedLine choisedLine;
        auto chosedRegisters = registers;
        for (int regIndex = 0; regIndex < registers.size(); ++regIndex)
        {
            auto regCopy = registers;

            CompressedLine newLine;
            newLine.isAltReg = result.isAltReg;
            newLine.regUseMask = result.regUseMask;
            newLine.selfRegMask = result.selfRegMask;

            bool successChoise = makeChoise(context, newLine, regCopy, regIndex, word, x);
            if (successChoise && (choisedLine.data.empty() || newLine.drawTicks <= choisedLine.drawTicks))
            {
                chosedRegisters = regCopy;
                choisedLine = newLine;
            }
        }

        if ((context.flags & oddVerticalCompression) && choisedLine.data.empty() && x % 2 == 0)
        {
            // try to reset flags for the rest of the line
            Context contextCopy(context);
            contextCopy.flags &= ~oddVerticalCompression;
            for (int regIndex = 0; regIndex < registers.size(); ++regIndex)
            {
                auto regCopy = registers;
				
                CompressedLine newLine;
                newLine.isAltReg = result.isAltReg;
                newLine.regUseMask = result.regUseMask;
                newLine.selfRegMask = result.selfRegMask;

                bool successChoise = makeChoise(contextCopy, newLine, regCopy, regIndex, word, x);
                if (successChoise && (choisedLine.data.empty() || newLine.drawTicks < choisedLine.drawTicks))
                {
                    chosedRegisters = regCopy;
                    choisedLine = newLine;
                }
            }
        }

        if (choisedLine.data.empty())
            return false;
        result += choisedLine;
        result.isAltReg = choisedLine.isAltReg;
        registers = chosedRegisters;
        return true;
    }
    if (result.isAltReg)
        result.exx();
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
CompressedLine  compressRealtimeColorsLine(uint16_t* buffer, uint16_t* nextLine, uint16_t generatedCodeMemAddress, bool* success, bool useSlowMode)
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

CompressedLine  compressMultiColorsLine2(Context context)
{
    CompressedLine result;
    static const int kLineLength = 224;
    static const int kBorderTime = kLineLength - 128;
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

    std::set<uint8_t> uniqueBytes;
    std::set<uint16_t> uniqueWords;
    std::vector<uint16_t> allWords;
    for (int x = context.minX; x <= context.maxX;)
    {
        if (line[x] == nextLine[x])
        {
            ++x;
            continue;
        }
        uniqueBytes.insert(line[x]);
        uniqueBytes.insert(line[x+1]);
        uint16_t* word = (uint16_t*)(line + x);
        uniqueWords.insert(*word);
        allWords.push_back(*word);
        x += 2;
    }
    std::cout << "unique bytes=" << uniqueBytes.size() << ". uniq words=" << uniqueWords.size() << ", all words=" << allWords.size() << std::endl;

    int t1 = kBorderTime + (context.minX + 31 - context.maxX) * 4; // write whole line at once limit (no intermidiate stack moving)
    int t2 = kLineLength - kStackMovingTime; // write whole line in 2 tries limit

    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
    compressLineMain(context, result, registers);
    result.jpIx();
    return result;
}

CompressedData compressRealTimeColors(uint8_t* buffer, int imageHeight)
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
        auto line = compressMultiColorsLine2(context);
        /*
        if (!success)
        {
            // Slow mode uses more ticks in general, but draw line under the ray on 34 ticks faster.
            line = compressRealtimeColorsLine(linePtr, nextLinePtr, generatedCodeAddress, &success, true );
            // Slow mode currently uses 6 registers and it is not enough 12 more ticks without compression.
            // If it fail again it need to use more registers. It need to add AF/AF' at this case.
            assert(success);
        }
        */
        compressedData.data.push_back(line);
    }

    return compressedData;
}

void calculateMulticolorTimings(const CompressedData& data, const CompressedData & colorData, int flags)
{
    // Start drawing: color since line 0, rastr since line 128
    const int imageHeight = data.data.size();
    const int kTotalMulticolorTicks = 224 * 8;

    static const int kMainLoopDelay = 134;
    static const int kBlockRetDelay = 8; // JP IX
    
    for (int colorLine = 0; colorLine < 24; ++colorLine)
    {
        const int rastrLine = (128 + colorLine * 4) % imageHeight;

        int colorTicks = colorData.ticks(colorLine, 1);
        int rastrTicks = data.ticks(rastrLine, 4);
        int totalTicks = colorTicks + rastrTicks;

        if (flags & interlineRegisters)
        {
            const auto preambula = data.data[rastrLine].getSerializedUsedRegisters();
            totalTicks += preambula.drawTicks;
        }
        totalTicks += kJpFirstLineDelay + kMainLoopDelay + kBlockRetDelay*2;

        //std::cout << "line #" << colorLine << ", ticks: " << totalTicks << ", rest: " 
        //    << kTotalMulticolorTicks - totalTicks << std::endl;
    }
}

CompressedData  compressColors(uint8_t* buffer, int imageHeight)
{
    CompressedData compressedData;
    int flags = verticalCompressionL | interlineRegisters;
    std::vector<int> sameBytesCount = createSameBytesTable(flags, buffer, /*maskColors*/ nullptr, imageHeight);

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
struct LineDescriptor
{
    uint16_t addressBegin = 0;
    //uint16_t addressEnd = 0;
};
struct JpIxDescriptor
{
    uint16_t address = 0;
    uint16_t originData = 0;
};

#pragma pack(pop)

void resolveJumpToNextBankInGap(
    uint16_t offset,
    std::vector<uint8_t>& serializedData,
    const CompressedData& data,
    std::vector<int> lineOffsetWithPreambula)
{
    // resolve JP to the next bank in Gaps
    int imageHeight = data.data.size();
    for (int line = 0; line < imageHeight; ++line)
    {
        const int bankSize = imageHeight / 8;

        int bankNum = line / bankSize;
        int lineNumInBank = line - bankNum * bankSize;

        int lineNumInNextBank = lineNumInBank - 7;
        int nextBank = bankNum + 1;
        if (nextBank > 7)
        {
            nextBank = 0;
            ++lineNumInNextBank;
        }
        if (lineNumInNextBank < 0)
            lineNumInNextBank += bankSize;

        int nextLine = nextBank * bankSize + lineNumInNextBank;

        uint16_t currentLineOffset = lineOffsetWithPreambula[line];
        uint16_t nextLineOffset = lineOffsetWithPreambula[nextLine];

        auto preambula = data.data[line].getSerializedUsedRegisters();

        uint16_t gapOffset = currentLineOffset + preambula.data.size() + data.data[line].data.size();
        uint16_t* gapPtr = (uint16_t*) (serializedData.data() + gapOffset + 3);

        *gapPtr = nextLineOffset + offset;
    }
}

std::vector<JpIxDescriptor> createWholeFrameJpIxDescriptors(
    uint16_t baseOffset,
    std::vector<uint8_t>& serializedData,
    std::vector<int> lineOffset)
{

    std::vector<JpIxDescriptor> descriptors;

    int imageHeight = lineOffset.size();
    const int bankSize = imageHeight / 8;

    // . Create delta for JP_IX when shift to 1 line
    for (int screenLine = 0; screenLine < imageHeight + 8; ++screenLine)
    {
        int line = screenLine % imageHeight;

        int bankNum = line % 8;
        int lineNumInBank = line / 8;

        for (int i = 1; i <= 3; ++i)
        {
            int lineEndInBank = lineNumInBank + i * 8;
            if (lineEndInBank > bankSize)
                lineEndInBank -= bankSize;
            
            int l = lineEndInBank + bankNum * bankSize;

            JpIxDescriptor d;
            int relativeOffset = l < imageHeight ? lineOffset[l] : serializedData.size();
            if (lineEndInBank == bankSize)
            {
                // There is additional JP 'first bank line'  command at the end of latest line in bank.
                // overwrite this command (if exists) instead of first bytes of the next line in bank.
                relativeOffset -= 3;
            }

            d.address = relativeOffset + baseOffset;
            uint16_t* ptr = (uint16_t*) (serializedData.data() + relativeOffset);
            d.originData = *ptr;
            descriptors.push_back(d);
        }
    }
    return descriptors;
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

int serializeMainData(const CompressedData& data, const std::string& inputFileName, uint16_t codeOffset, int flags)
{
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

    /**
     * File format:
     * vector<LineDescriptor>
    */

    ofstream lineDescriptorFile;
    std::string lineDescriptorFileName = inputFileName + ".main_descriptor";
    lineDescriptorFile.open(lineDescriptorFileName, std::ios::binary);
    if (!lineDescriptorFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    ofstream jpIxDescriptorFile;
    std::string jpIxDescriptorFileName = inputFileName + ".jpix";
    jpIxDescriptorFile.open(jpIxDescriptorFileName, std::ios::binary);
    if (!jpIxDescriptorFile.is_open())
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

    std::vector<uint8_t> reachDescriptors;
    std::vector<LineDescriptor> descriptors;

    const int bankSizeInLines = imageHeight / 8;
    const int reachDescriptorsBase = codeOffset + serializedData.size();
    std::vector<uint16_t> reachDescriptorOffset;
    for (int d = 0; d < imageHeight + 191; ++d)
    {
        const int srcLine = d % imageHeight;

        LineDescriptor descriptor;
        int lineBank = srcLine % 8;
        int lineInBank = srcLine / 8;
        int lineNum = bankSizeInLines * lineBank + lineInBank;

        const auto& line = data.data[lineNum];
        if (d < imageHeight)
        {
            const uint16_t lineAddress = lineOffset[lineNum] + codeOffset;

            // create reach descriptor witch preambula(optional)
            descriptor.addressBegin = reachDescriptorsBase + reachDescriptors.size();
            reachDescriptorOffset.push_back(descriptor.addressBegin);
            if (flags & interlineRegisters)
            {
                auto preambula = line.getSerializedUsedRegisters();
                preambula.serialize(reachDescriptors);
            }

            // The first two bytes of the each line can be overwritted by JP_IX command
            // Copy these bytes to the descriptor and skip them in JP command
            static const int kJpIxCommandLen = 2;
            std::vector<uint8_t> firstCommands = line.getFirstCommands(kJpIxCommandLen);
            reachDescriptors.insert(reachDescriptors.end(), firstCommands.begin(), firstCommands.end());

            reachDescriptors.push_back(0xc3); // JP XXXX
            serialize(reachDescriptors, lineAddress + firstCommands.size());
        }
        else
        {
            descriptor.addressBegin = reachDescriptorOffset[srcLine];
        }

        descriptors.push_back(descriptor);
    }

    for (const auto& descriptor: descriptors)
        lineDescriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    mainDataFile.write((const char*)serializedData.data(), serializedData.size());
    mainDataFile.write((const char*)reachDescriptors.data(), reachDescriptors.size());

    // serialize Jp Ix descriptors

    std::vector<JpIxDescriptor> jpIxDescr = createWholeFrameJpIxDescriptors(
        codeOffset, serializedData, lineOffset);
    jpIxDescriptorFile.write((const char*)jpIxDescr.data(), jpIxDescr.size() * sizeof(JpIxDescriptor));


    return serializedData.size() + reachDescriptors.size() + jpIxDescr.size();
}

int serializeColorData(const CompressedData& data, const std::string& inputFileName, uint16_t codeOffset)
{
    using namespace std;

    ofstream colorDataFile;
    std::string colorDataFileName = inputFileName + ".color";
    colorDataFile.open(colorDataFileName, std::ios::binary);
    if (!colorDataFile.is_open())
    {
        std::cerr << "Can not write color file" << std::endl;
        return -1;
    }

    ofstream colorDescriptorFile;

    std::string colorDescriptorFileName = inputFileName + ".color_descriptor";
    colorDescriptorFile.open(colorDescriptorFileName, std::ios::binary);
    if (!colorDescriptorFile.is_open())
    {
        std::cerr << "Can not write color destination file" << std::endl;
        return -1;
    }

    // serialize main data
    int imageHeight = data.data.size();

    int size = 0;
    std::vector<uint8_t> serializedData; // (data.size() + data.data.size * 16);
    std::vector<int> lineOffsetWithPreambula;
    for (int y = 0; y < imageHeight; ++y)
    {
        const auto& line = data.data[y];

        lineOffsetWithPreambula.push_back(serializedData.size());
        const auto loadRegPreambula = line.getSerializedUsedRegisters();
        loadRegPreambula.serialize(serializedData);
        line.serialize(serializedData);
    }

    // serialize descriptors
    std::vector<LineDescriptor> descriptors;
    for (int d = 0; d < imageHeight + 16; ++d)
    {
        const int srcLine = d % imageHeight;

        LineDescriptor descriptor;
        const auto& line = data.data[srcLine];
        const uint16_t lineAddress = lineOffsetWithPreambula[srcLine] + codeOffset;
        descriptor.addressBegin = lineAddress;
        descriptors.push_back(descriptor);
    }

    for (const auto& descriptor: descriptors)
        colorDescriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    return serializedData.size();
    return size;
}

int serializeMultiColorData(const CompressedData& data, const std::string& inputFileName, uint16_t codeOffset)
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

    ofstream colorDescriptorFile;

    std::string colorDescriptorFileName = inputFileName + ".multicolor_descriptor";
    colorDescriptorFile.open(colorDescriptorFileName, std::ios::binary);
    if (!colorDescriptorFile.is_open())
    {
        std::cerr << "Can not write color destination file" << std::endl;
        return -1;
    }

    // serialize main data
    int imageHeight = data.data.size();

    int size = 0;
    std::vector<uint8_t> serializedData; // (data.size() + data.data.size * 16);
    std::vector<int> lineOffsetWithPreambula;
    for (int y = 0; y < imageHeight; ++y)
    {
        const auto& line = data.data[y];
        lineOffsetWithPreambula.push_back(serializedData.size());
        line.serialize(serializedData);
    }

    // serialize descriptors
    std::vector<LineDescriptor> descriptors;
    for (int srcLine = 0; srcLine < imageHeight; ++srcLine)
    {
        LineDescriptor descriptor;
        const auto& line = data.data[srcLine];
        const uint16_t lineAddress = lineOffsetWithPreambula[srcLine] + codeOffset;
        descriptor.addressBegin = lineAddress;
        descriptors.push_back(descriptor);
    }

    for (const auto& descriptor : descriptors)
        colorDescriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    colorDataFile.write((const char*)serializedData.data(), serializedData.size());

    return serializedData.size();
    return size;
}

int getTicksChainFor64Line(const CompressedData& data, int screenLineNum)
{
    static const int kEnterDelay = 4;
    static const int kReturnDelay = 8;

    int result = 0;
    const int imageHeight = data.data.size();
    screenLineNum = screenLineNum % imageHeight;

    const int bankSize = imageHeight / 8;
    int bankNum = screenLineNum % 8;
    int lineInBank = screenLineNum  / 8;

    for (int b = 0; b < 8; ++b)
    {
        bool firstLineInBank = true;
        int lineInBankCur = lineInBank;
        for (int i = 0; i < 8; ++i)
        {
            const auto& line = data.data[lineInBankCur + bankNum * bankSize];
            if (firstLineInBank)
                result += line.getSerializedUsedRegisters().drawTicks;
            result += line.drawTicks;
            firstLineInBank = false;
            lineInBankCur = nextLineInBank(lineInBankCur, imageHeight);
        }
        // The last line in every bank contains "JP firstLine" command. It is 10 ticks.
        // In case of it is the latest drawing line, this command is overwritten by JP IX,
        // this command is not need to take into account.
        if (lineInBankCur == 0)
            result -= kJpFirstLineDelay;

        ++bankNum;
        if (bankNum > 7)
        {
            bankNum = 0;
            lineInBank = nextLineInBank(lineInBank, imageHeight);
        }
    }
    result += kEnterDelay + kReturnDelay;
    return result;
}

int getColorTicksChainFor8Line(const CompressedData& data, int lineNum)
{
    static const int kEnterDelay = 4;
    static const int kReturnDelay = 8;

    int result = 0;
    const int imageHeight = data.data.size();
    lineNum = lineNum % imageHeight;

    for (int i = 0; i < 8; ++i)
    {
        const auto& line = data.data[lineNum];
        if (i == 0)
            result += line.getSerializedUsedRegisters().drawTicks;
        result += line.drawTicks;
        lineNum = (lineNum + 1) % imageHeight;
    }
    result += kEnterDelay + kReturnDelay;
    return result;
}

int serializeTimingData(const CompressedData& data, const CompressedData& color, const std::string& inputFileName)
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

    for (int line = 0; line < imageHeight; ++line)
    {
        int ticks = 0;
        for (int i = 0; i < 3; ++i)
        {
            ticks += getTicksChainFor64Line(data, line + i * 64);
            //ticks += getColorTicksChainFor8Line(color, line/8 + i * 8);
        }

        uint16_t freeTicks = totalTicksPerFrame - ticks;
        timingDataFile.write((const char*)&freeTicks, sizeof(freeTicks));
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

    int flags = verticalCompressionL | interlineRegisters | skipInvisibleColors; // | inverseColors;

    const auto t1 = std::chrono::system_clock::now();

    CompressedData realTimeColor = compressRealTimeColors(colorBuffer.data(), imageHeight / 8);
    serializeMultiColorData(realTimeColor, outputFileName, 0);

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
        << realTimeColor.ticks() << ", ratio: " << realTimeColor.ticks() / (float) uncompressedColorTicks << std::endl;
    std::cout << "total ticks: " << data.ticks() + colorData.ticks() +  realTimeColor.ticks() << std::endl;

    std::deque<int> ticksSum;
    int last4LineTicks = 0;
    int last4LineTicksMax = 0;
    int worseLine = 0;
    for (int i = 0; i < 8; ++i)
    {
        for (int y = 0; y < data.data.size(); y += 8)
        {
            int line = y + i;
            ticksSum.push_back(data.data[line].drawTicks);
            last4LineTicks += data.data[line].drawTicks;
            if (ticksSum.size() > 4)
            {
                last4LineTicks -= ticksSum.front();
                ticksSum.pop_front();
            }
            if (last4LineTicks > last4LineTicksMax)
            {
                last4LineTicksMax = last4LineTicks;
                worseLine = line;
            }
        }
    }
    std::cout << "max 4 line ticks: " << last4LineTicksMax << " worseLine=" << worseLine << std::endl;

    int maxColorTicks = 0;
    for (int i = 0; i < 24; ++i)
    {
        maxColorTicks = std::max(maxColorTicks, realTimeColor.data[i].drawTicks);
    }
    std::cout << "max realtime color ticks: " << maxColorTicks << std::endl;

    //moveLoadBcFirst(data);
    //moveLoadBcFirst(colorData);

    //interleaveData(data);

    // put JP to the latest line for every bank
    for (int bank = 0; bank < 8; ++bank)
    {
        int bankSize = imageHeight / 8;
        int line = bank * bankSize + bankSize - 1;
        int firstLineOffset = data.size(0, bank * bankSize);
        data.data[line].jp(firstLineOffset + kCodeOffset);
    }


    int mainDataSize = serializeMainData(data, outputFileName, kCodeOffset, flags);


    serializeColorData(colorData, outputFileName, kCodeOffset + mainDataSize);
    serializeTimingData(data, colorData, outputFileName);

    calculateMulticolorTimings(data, colorData, flags);

    return 0;
}
