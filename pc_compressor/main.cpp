#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <map>
#include <optional>
#include <cassert>
#include <functional>
#include <string>
#include <deque>
#include <chrono>
#include <future>

static const int imageHeight = 192;
static const int zxScreenSize = 6144;
uint8_t DEC_SP_CODE = 0x3b;

static const int interlineRegisters = 1; //< Experimental. Keep registers between lines.
static const int verticalCompressionL = 2; //< Skip drawing data if it exists on the screen from the previous step.
static const int verticalCompressionH = 4; //< Skip drawing data if it exists on the screen from the previous step.
static const int oddVerticalCompression = 8; //< can skip odd drawing bytes.
static const int inverseColors = 16;

class ZxData
{
    uint8_t m_buffer[64];
    int m_size = 0;

public:
    inline bool empty() const { return m_size == 0; }

    uint8_t last() const { return m_buffer[m_size]; }

    inline void push_back(uint8_t value)
    {
        assert(m_size < sizeof(m_buffer));
        m_buffer[m_size++] = value;
    }

    inline void pop_back()
    {
        --m_size;
    }

    inline int size() const { return m_size;  }
    inline const uint8_t* data() const { return m_buffer; }

    inline void append(const ZxData& other)
    {
        memcpy(m_buffer + m_size, other.m_buffer, other.m_size);
        m_size += other.m_size;
    }
};


struct CompressedLine
{

    CompressedLine()
    {
    }

    void exx()
    {
        data.push_back(0xd9);
        isAltReg = !isAltReg;
        drawTicks += 4;
    }

    void nop()
    {
        data.push_back(0x00);
        drawTicks += 4;
    }

    void replaceWord(int offset, uint16_t word)
    {
        uint16_t* ptr = (uint16_t*)(data.data() + offset);
        *ptr = word;
    }

    void operator +=(const CompressedLine& other)
    {
        data.append(other.data);
        drawTicks += other.drawTicks;
    }

    ZxData data;
    int drawTicks = 0;
    bool isAltReg = false;
    int prelineTicks = 0;

};

struct CompressedData
{
    std::vector<CompressedLine> data;


    void replace(int lineNum, int count, std::vector<CompressedLine>::iterator srcData)
    {
        for (int i = lineNum; i < lineNum + count; ++i)
        {
            data[i] = *srcData;
            srcData++;
        }
    }

    int ticks(int from = 0, int count = -1) const
    {
        int result = 0;
        int to = count == -1 ? data.size() : from + count;
        for (int i = from; i < to; ++i)
            result += data[i].drawTicks;
        return result;
    }

    int size() const
    {
        int result = 0;
        for (const auto& line : data)
            result += line.data.size();
        return result;
    }
};

static const int registersCount = 3;
class Register16;
using Registers = std::array<Register16, registersCount>;

class Register8
{
    int calculateIndex() const
    {
        if (name == 'b')
            return 0;
        else if (name == 'c')
            return 1;
        else if (name == 'd')
            return 2;
        else if (name == 'e')
            return 3;
        else if (name == 'h')
            return 4;
        else if (name == 'l')
            return 5;
        else if (name == 's') //< SP
            return 6;
        else if (name == 'a')
            return 7;
        else if (name == 'p')
            return -1; //< SP

        assert(0);
        return 0;
    }
public:

    char name;
    std::optional<uint8_t> value;
    int reg8Index;

    Register8(const char name): name(name)
    {
        reg8Index = calculateIndex();
    }

    inline bool hasValue(uint8_t byte) const
    {
        return value && *value == byte;
    }

    inline bool isEmpty() const
    {
        return !value.has_value();
    }

    void add(CompressedLine& line, const Register8& reg)
    {
        assert(name == 'a');
        line.data.push_back(0x80 + reg.reg8Index); //< ADD a, reg8
        line.drawTicks += 4;
    }

    void loadX(CompressedLine& line, uint8_t byte)
    {
        value = byte;
        line.drawTicks += 7;
        line.data.push_back(uint8_t(0x06 + 0x10 * reg8Index));
        line.data.push_back(byte);
    }

    void loadFromReg(CompressedLine& line, const Register8& reg)
    {
        value = reg.value;
        line.drawTicks += 4;

        const uint8_t data = 0x40 + reg8Index * 8 + reg.reg8Index;
        line.data.push_back(data);
    }

    void incValue(CompressedLine& line)
    {
        value = *value + 1;
        line.drawTicks += 4;

        const uint8_t data = 0x04 + 8 * reg8Index;
        line.data.push_back(data);
    }

    void decValue(CompressedLine& line)
    {
        value = *value - 1;
        line.drawTicks += 4;

        const uint8_t data = 0x05 + 8 * reg8Index;
        line.data.push_back(data);
    }

    void setBit(CompressedLine& line, uint8_t bit)
    {
        assert(bit < 8);
        line.data.push_back(0xcb);
        line.data.push_back(0xc0 + reg8Index + bit * 8);
        line.drawTicks += 8;
    }

    void updateToValue(CompressedLine& line, uint8_t byte, const std::vector<Register8>& registers)
    {
        for (const auto& reg: registers)
        {
            if (reg.hasValue(byte))
            {
                loadFromReg(line, reg);
                return;
            }
        }

        if (isEmpty())
            loadX(line, byte);
        else if (*value == byte - 1)
            decValue(line);
        else if (*value == byte + 1)
            incValue(line);
        else
            loadX(line, byte);
    }

    void updateToValue(CompressedLine& line, uint8_t byte, const Registers& registers16, const Register8& a);
};

class Register16
{
public:

    Register8 h;
    Register8 l;
    bool isAlt = false;

    Register16(const std::string& name, std::optional<uint16_t> value = std::nullopt):
        h(name[0]),
        l(name[1])
    {
        isAlt = name.length() > 2 && name[2] == '\'';
        if (value.has_value())
        {
            h.value = *value >> 8;
            l.value = *value % 256;
        }
    }

    std::string name() const
    {
        return std::string() + h.name + l.name;
    }

    inline int reg16Index() const
    {
        return h.reg8Index;
    }


    void reset()
    {
        h.value.reset();
        l.value.reset();
    }

    void load(CompressedLine& line, const Register16& reg)
    {
        if (h.name == 'h' && reg.h.name == 's')
            line.data.push_back(0x39); //< LD HL, SP
        else if (h.name == 's' && reg.h.name == 'h')
            line.data.push_back(0xf9); //< LD SP, HL
        else
            assert(0);
        line.drawTicks += 6;
    }

    void loadXX(CompressedLine& line, uint16_t value)
    {
        h.value = value >> 8;
        l.value = (uint8_t) value;
        line.drawTicks += 10;
        line.data.push_back(uint8_t(0x01 + 0x8 * reg16Index()));
        line.data.push_back(*l.value);
        line.data.push_back(*h.value);
    }

    void addSP(CompressedLine& line)
    {
        if (h.name != 'h')
            assert(0);
        h.value.reset();
        l.value.reset();
        line.data.push_back(0x39);
        line.drawTicks += 11;
    }

    inline bool isEmpty() const
    {
        return h.isEmpty() || l.isEmpty();
    }

    inline bool hasValue16(uint16_t data) const
    {
        if (!h.value || !l.value)
            return false;
        uint16_t value = (*h.value << 8) + *l.value;
        return value == data;
    }

    inline uint16_t value16() const
    {
        return *h.value * 256 + *l.value;
    }


    inline bool hasValue8(uint8_t value) const
    {
        return h.hasValue(value) || l.hasValue(value);
    }

    void push(CompressedLine& line) const
    {
        assert(isAlt == line.isAltReg);
        line.data.push_back(0xc5 + 0x10 * reg16Index());
        line.drawTicks += 11;
    }

    void updateToValue(CompressedLine& line, uint16_t value,
        const Registers& registers, const Register8& a = Register8('a'))
    {
        if (hasValue16(value))
            return;

        const uint8_t hiByte = value >> 8;
        const uint8_t lowByte = (uint8_t) value;

        if (h.hasValue(hiByte))
        {
          l.updateToValue(line, lowByte, registers, a);
        }
        else if (l.hasValue(lowByte))
        {
          h.updateToValue(line, hiByte, registers, a);
        }
        else if (!isEmpty() && value16() == value + 1)
        {
            inc(line);
        }
        else if (!isEmpty() && value16() == value - 1)
        {
            dec(line);
        }
        else
        {
            const Register8* regH = nullptr;
            const Register8* regL = nullptr;

            for (const auto& reg16: registers)
            {
                if (reg16.h.hasValue(hiByte))
                    regH = &reg16.h;
                else if (reg16.h.hasValue(lowByte))
                    regL = &reg16.h;
                else if (reg16.l.hasValue(hiByte))
                    regH = &reg16.l;
                else if (reg16.l.hasValue(lowByte))
                    regL = &reg16.l;
            }
            if (a.hasValue(hiByte))
                regH = &a;
            else if (a.hasValue(lowByte))
                regL = &a;

            if (regH && regL)
            {
                h.loadFromReg(line, *regH);
                l.loadFromReg(line, *regL);
            }
            else
            {
                loadXX(line, value);
            }
        }
    }

    void dec(CompressedLine& line, int repeat = 1)
    {
        if(!isEmpty())
        {
            const auto newValue = value16() - repeat;
            h.value = newValue >> 8;
            l.value = newValue % 256;
        }

        for (int i = 0; i < repeat; ++i)
        {
            line.data.push_back(0x0b + reg16Index() * 8); //< dec sp
            line.drawTicks += 6;
        }
    }

    void inc(CompressedLine& line, int repeat = 1)
    {
        if(!isEmpty())
        {
            const uint16_t newValue = value16() + repeat;
            h.value = newValue >> 8;
            l.value = newValue % 256;
        }

        for (int i = 0; i < repeat; ++i)
        {
            line.data.push_back(0x03 + reg16Index() * 8); //< dec sp
            line.drawTicks += 6;
        }
    }

    void poke(CompressedLine& line, uint16_t address)
    {
        assert(h.name == 'h');
        line.data.push_back(0x22); //< LD (**), HL
        line.data.push_back(*l.value);
        line.data.push_back(*h.value);
        line.drawTicks += 16;
    }
};

void Register8::updateToValue(CompressedLine& line, uint8_t byte, const Registers& registers16, const Register8& a)
{
    for (const auto& reg16: registers16)
    {
        if (reg16.h.hasValue(byte))
        {
            loadFromReg(line, reg16.h);
            return;
        }
        else if (reg16.l.hasValue(byte))
        {
            loadFromReg(line, reg16.l);
            return;
        }
    }
    if (a.hasValue(byte))
    {
        loadFromReg(line, a);
        return;
    }

    if (isEmpty())
        loadX(line, byte);
    else if (*value == byte - 1)
        decValue(line);
    else if (*value == byte + 1)
        incValue(line);
    else
        loadX(line, byte);
}

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


void deinterlaceBuffer(uint8_t* buffer)
{
    uint8_t tempBuffer[zxScreenSize];
    static const int lineSize = 32;

    for (int srcY = 0; srcY < 192; ++ srcY)
    {
        int dstY = (srcY % 8) * 8 + (srcY % 64) / 8 + (srcY / 64) * 64;
        memcpy(tempBuffer + dstY * lineSize, buffer + srcY * lineSize, lineSize); //< Copy screen line
    }
    memcpy(buffer, tempBuffer, zxScreenSize);
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

int sameVerticalBytes(int flags, uint8_t buffer[zxScreenSize], int x, int y, int maxY)
{
    int result = 0;
    if (!(flags & (verticalCompressionL | verticalCompressionH)))
        return 0;

    uint8_t* ptr = buffer + y * 32 + x;
    for (; x < 32; ++x)
    {
        uint8_t currentByte = *ptr;
        if (y > 0 && (flags & verticalCompressionH))
        {
            uint8_t upperByte = *(ptr-32);
            if (upperByte != currentByte)
                return result;
        }
        if (y < maxY && (flags & verticalCompressionL))
        {
            uint8_t lowerByte = *(ptr+32);
            if (lowerByte != currentByte)
                return result;
        }
        ++result;
        ++ptr;
    };
    return result;
}

int sameVerticalWorlds(uint8_t buffer[zxScreenSize], int x, int y)
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

int sameVerticalBytesOnRow(uint8_t buffer[zxScreenSize], int y)
{
    if (y == 191 || y == 0)
        return 0;

    int result = 0;
    for (int x = 0; x < 32; ++x)
    {
        uint8_t currentByte = buffer[y * 32 + x];
        uint8_t upperByte = buffer[(y - 1) * 32 + x];
        uint8_t lowerByte = buffer[(y + 1) * 32 + x];
        if (upperByte == currentByte && lowerByte == currentByte)
            ++result;
    };
    return result;
}

Register16* findRegister(Registers& registers, const std::string& name)
{
    for (auto& reg: registers)
    {
        if (reg.name() == name)
            return &reg;
    }
    return nullptr;
}

void compressLine(
    CompressedLine&  result,
    int flags,
    int maxY,
    uint8_t buffer[zxScreenSize],
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success);

CompressedLine makeChoise(
    int regIndex,
    uint16_t word,
    int flags,
    int maxY,
    uint8_t buffer[zxScreenSize],
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success)
{
    CompressedLine result;

    Register16& reg = registers[regIndex];
    reg.updateToValue(result, word, registers, a);
    reg.push(result);
    compressLine(result, flags, maxY, buffer, registers, a, y, x + 2, success);
    return result;
}

void compressLine(
    CompressedLine&  result,
    int flags,
    int maxY,
    uint8_t buffer[zxScreenSize],
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success)
{
    static Register16 sp("sp");
    *success = true;

    int verticalRepCount =  sameVerticalBytes(flags, buffer, x, y, maxY);
    if (!(flags & oddVerticalCompression))
        verticalRepCount &= ~1;

    while (x < 32)
    {
        uint16_t* buffer16 = (uint16_t*) (buffer + y * 32 + x);
        const uint16_t word = *buffer16;
        const uint8_t highByte = word >> 8;
        const uint8_t lowByte = (uint8_t) word;

        assert(x < 32);
        if (x == 31 && !verticalRepCount)
        {
            *success = false;
            return;
        }

        // Decrement stack if line has same value from previous step (vertical compression)
        // Up to 4 bytes is more effetient to decrement via 'DEC SP' call.
        if (verticalRepCount > 4)
        {
            auto hl = findRegister(registers, "hl");
            hl->updateToValue(result, -verticalRepCount, registers, a);
            hl->addSP(result);
            sp.load(result, *hl);
            x += verticalRepCount;
            continue;
        }
        else if (verticalRepCount > 2)
        {
            sp.dec(result, verticalRepCount);
            x += verticalRepCount;
            continue;
        }

        // push existing 16 bit value.
        bool isChoised = false;
        for (auto& reg: registers)
        {
            if (reg.hasValue16(word))
            {
                reg.push(result);
                x += 2;
                isChoised = true;
                break;
            }
        }
        if (isChoised)
            continue;

        // Decrement stack if line has same value from previous step (vertical compression)
        if (verticalRepCount > 0)
        {
            sp.dec(result, verticalRepCount);
            x += verticalRepCount;
            continue;
        }

        for (auto& reg: registers)
        {
            if (reg.isEmpty())
            {
                reg.updateToValue(result, word, registers, a);
                reg.push(result);
                x += 2;
                isChoised = true;
                break;
            }
        }
        if (isChoised)
            continue;

        CompressedLine choisedLine;
        Registers chosedRegisters = registers;
        for (int regIndex = 0; regIndex < registers.size(); ++regIndex)
        {
            Registers regCopy = registers;
            auto newLine = makeChoise(regIndex, word, flags, maxY,  buffer, regCopy, a, y, x, success);
            if (choisedLine.data.empty() || newLine.drawTicks < choisedLine.drawTicks)
            {
                chosedRegisters = regCopy;
                choisedLine = newLine;
            }
        }
        result += choisedLine;
        registers = chosedRegisters;
        return;
    }

}

std::future<CompressedLine> compressLineAsync(int flags, uint8_t buffer[zxScreenSize], const Register8& a, int line)
{
    return std::async(
        [flags, buffer, &a, line]()
        {
            Registers registers1 = {Register16("bc"), Register16("de"), Register16("hl")};
            Registers registers2 = registers1;

            bool success;
            CompressedLine line1, line2;
            compressLine(line1, flags | oddVerticalCompression, imageHeight,
                buffer, registers1, a, line, 0, &success);

            compressLine(line2, flags, imageHeight,
                buffer, registers2, a, line, 0, &success);
            if (success && line2.data.size() < line1.data.size())
                return line2;
            else
                return line1;
        }
    );
}

CompressedData compressLinesAsync(int flags, uint8_t buffer[zxScreenSize], const Register8& a, int y, int count)
{
    CompressedData compressedData;

    std::vector<std::future<CompressedLine>> compressors(count);
    for (int i = 0; i < count; ++i)
        compressors[i] = compressLineAsync(flags, buffer, a, y++);

    for (auto& compressor: compressors)
        compressedData.data.push_back(compressor.get());

    return compressedData;
}

CompressedData compress(int flags, uint8_t buffer[zxScreenSize])
{
    // Detect the most common byte in image
    std::vector<uint8_t> bytesCount(256);
    for (int i = 0; i < 6144; ++i)
        ++bytesCount[buffer[i]];

    Register8 a('a');
    int bestCounter = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (bytesCount[i] > bestCounter)
        {
            a.value = (uint8_t)i;
            bestCounter = bytesCount[i];
        }
    }

    CompressedData result = compressLinesAsync(flags, buffer, a, 0, imageHeight);
    if (!(flags & inverseColors))
        return result;

    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            std::cout << "Check block " << y << ":" << x  << std::endl;

            int lineNum = y * 8;
            int count = lineNum < imageHeight - 8 ? 9 : 8;

            inversBlock(buffer, x, y);
            auto candidateLeft = compressLinesAsync(flags, buffer, a, lineNum, count);

            inversBlock(buffer, x+1, y);
            auto candidateBoth = compressLinesAsync(flags, buffer, a, lineNum, count);

            inversBlock(buffer, x, y);
            auto candidateRight = compressLinesAsync(flags, buffer, a, lineNum, count);
            inversBlock(buffer, x + 1, y);

            const int resultTicks = result.ticks(lineNum, count);
            if (candidateLeft.ticks() < resultTicks
                && candidateLeft.ticks() < candidateBoth.ticks()
                && candidateLeft.ticks() < candidateRight.ticks())
            {
                result.replace(lineNum, count, candidateLeft.data.begin());
                inversBlock(buffer, x, y);
            }
            else if (candidateBoth.ticks() < resultTicks
                && candidateBoth.ticks() < candidateLeft.ticks()
                && candidateBoth.ticks() < candidateRight.ticks())
            {
                result.replace(lineNum, count, candidateBoth.data.begin());
                inversBlock(buffer, x, y);
                inversBlock(buffer, x + 1, y);
            }
            else if (candidateRight.ticks() < resultTicks
                && candidateRight.ticks() < candidateLeft.ticks()
                && candidateRight.ticks() < candidateBoth.ticks())
            {
                result.replace(lineNum, count, candidateRight.data.begin());
                inversBlock(buffer, x + 1, y);
            }
        }
    }
    return result;
}

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


    Registers registers = {Register16("bc"), Register16("de"), Register16("hl")};
    Registers registers1 = {Register16("bc'"), Register16("de'"), Register16("hl'")};

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
        hl.load(line, sp);
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
        [&](Register16& reg, int number, Registers& registers) -> Register16*
        {
            if (nextLine && buffer[number] == nextLine[number])
                return nullptr;
            const auto word = buffer[number];
            for (auto& reg16 : registers)
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
        sp.load(line,hl1);
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
        sp.load(line, hl);
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

CompressedData compressRealTimeColors(uint8_t* buffer, int maxY)
{
    // TODO: fill it
    static const int generatedCodeAddress = 0;

    CompressedData compressedData;
    for (int y = 0; y < maxY; y ++)
    {
        uint16_t* linePtr = (uint16_t*)(buffer + y * 32);
        uint16_t* nextLinePtr = y < maxY -1 ? linePtr + 16 : nullptr;
        bool success;
        auto line = compressRealtimeColorsLine(linePtr, nextLinePtr, generatedCodeAddress, &success, false /* slow mode*/ );
        if (!success)
        {
            // Slow mode uses more ticks in general, but draw line under the ray on 34 ticks faster.
            line = compressRealtimeColorsLine(linePtr, nextLinePtr, generatedCodeAddress, &success, true /* slow mode*/ );
            // Slow mode currently uses 6 registers and it is not enough 12 more ticks without compression.
            // If it fail again it need to use more registers. It need to add AF/AF' at this case.
            assert(success);
        }
        compressedData.data.push_back(line);
    }

    return compressedData;
}

CompressedData  compressColors(uint8_t* buffer)
{
    CompressedData compressedData;

    Register8 a('a');
    Register16 bc("bc");
    Register16 de("de");
    Register16 hl("hl");
    Registers registers = {bc, de, hl};

    for (int y = 0; y < 24; y ++)
    {
        bool success;
        CompressedLine line;
        compressLine(line, 0 /*flags*/, 24 /*maxY*/,
            buffer, registers, a, y, 0, &success);
        compressedData.data.push_back(line);
    }

    return compressedData;
}

int main(int argc, char** argv)
{
    using namespace std;

    ifstream fileIn;

    if (argc < 2)
    {
        std::cerr << "Usage: scroll_image_compress <file_name>";
        return -1;
    }

    std::string inputFileName = argv[1];
    fileIn.open(inputFileName, std::ios::binary);
    if (!fileIn.is_open())
    {
        std::cerr << "Can not read source file" << std::endl;
        return -1;
    }

    uint8_t buffer[zxScreenSize];
    uint8_t colorBuffer[768];

    fileIn.read((char*) buffer, sizeof(buffer));
    deinterlaceBuffer(buffer);
    writeTestBitmap(256, 192, buffer, inputFileName + ".bmp");

    fileIn.read((char*) colorBuffer, sizeof(colorBuffer));

    ofstream mainDataFile;
    std::string mainDataFileName = inputFileName + ".main";
    mainDataFile.open(mainDataFileName, std::ios::binary);
    if (!mainDataFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    ofstream lineDescriptorFile;
    std::string lineDescriptorFileName = inputFileName + ".descriptor";
    lineDescriptorFile.open(lineDescriptorFileName, std::ios::binary);
    if (!lineDescriptorFile.is_open())
    {
        std::cerr << "Can not write destination file" << std::endl;
        return -1;
    }

    int flags = verticalCompressionH | verticalCompressionL | inverseColors; // | interlineRegisters

    const auto t1 = std::chrono::system_clock::now();
    auto data = compress(flags, buffer);
    const auto t2 = std::chrono::system_clock::now();

    std::cout << "compression time= " <<  std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;


    auto colorData = compressColors(colorBuffer);
    auto realTimeColor = compressRealTimeColors(colorBuffer, 24);

    static const int uncompressedTicks = 21 * 6144 / 2;
    static const int uncompressedColorTicks = 21 * 768 / 2;
    static const int loadTicks = 10 * 6144 / 2;
    static const int saveTicks = 11 * 6144 / 2;
    std::cout << "uncompressed ticks: " << uncompressedTicks << " compressed ticks: "
        << data.ticks() << ", ratio: " << (data.ticks() / float(uncompressedTicks))
        << ", data size:" << data.size() << std::endl;

    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " compressed color ticks: "
        << colorData.ticks() << ", ratio: " << colorData.ticks() / (float) uncompressedColorTicks << std::endl;
    std::cout << "uncompressed color ticks: " << uncompressedColorTicks << " realtime color ticks: "
        << realTimeColor.ticks() << ", ratio: " << realTimeColor.ticks() / (float) uncompressedColorTicks << std::endl;
    std::cout << "total ticks: " << data.ticks() + colorData.ticks() +  realTimeColor.ticks() << std::endl;


    std::deque<int> ticksSum;
    int last4LineTicks = 0;
    int last4LineTicksMax = 0;
    for (int i = 0; i < 8; ++i)
    {
        for (int y = 0; y < 192; y += 8)
        {
            int line = y + i;
            ticksSum.push_back(data.data[i].drawTicks);
            last4LineTicks += data.data[i].drawTicks;
            if (ticksSum.size() > 4)
            {
                last4LineTicks -= ticksSum.front();
                ticksSum.pop_front();
            }
            last4LineTicksMax = std::max(last4LineTicksMax, last4LineTicks);
        }
    }
    std::cout << "max 4 line ticks: " << last4LineTicksMax << std::endl;

    int maxColorTicks = 0;
    for (int i = 0; i < 24; ++i)
    {
        maxColorTicks = std::max(maxColorTicks, realTimeColor.data[i].drawTicks);
    }
    std::cout << "max realtime color ticks: " << maxColorTicks << std::endl;

#pragma pack(push)
#pragma pack(1)
    struct LineDescriptor
    {
        uint16_t offset = 0;  //< Line offset in bytes.
        uint8_t next8Len = 0; //< length of this line and next 7 lines.
        uint8_t next4Len = 0; //< length of this line and next 3 lines.
    };
#pragma pack(pop)

    for (int lineNum = 0; lineNum < imageHeight; ++lineNum)
    {
        LineDescriptor descriptor;
        int bank = lineNum % 8;
        int numberInBank = lineNum / 8;
        int linesBefore = imageHeight / 8 * bank + numberInBank;
        for (int i = 0; i < linesBefore; ++i)
            descriptor.offset += data.data[i].data.size();

        for (int i = 0; i < 8; ++i)
        {
            if (lineNum + i < imageHeight)
            {
                const auto& line = data.data[lineNum + i];
                descriptor.next8Len += line.data.size();
                if ( i < 4)
                    descriptor.next4Len += line.data.size();
            }
        }
        lineDescriptorFile.write((const char*) &descriptor, sizeof(descriptor));
    }

    for (int y = 0; y < data.data.size(); ++y)
    {
        const auto& line = data.data[y];
        mainDataFile.write((const char*) line.data.data(), line.data.size());
    }

    return 0;
}
