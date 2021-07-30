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

static const int totalTicksPerFrame = 71680;
const int codeOffset = 0x5b00 + 1024;

uint8_t DEC_SP_CODE = 0x3b;
uint8_t LD_BC_CODE = 1;
uint8_t  IX_REG_PREFIX = 0xdd;
uint8_t  IY_REG_PREFIX = 0xfd;

static const int lineSize = 32;

static const int interlineRegisters = 1; //< Experimental. Keep registers between lines.
static const int verticalCompressionL = 2; //< Skip drawing data if it exists on the screen from the previous step.
static const int verticalCompressionH = 4; //< Skip drawing data if it exists on the screen from the previous step.
static const int oddVerticalCompression = 8; //< can skip odd drawing bytes.
static const int inverseColors = 16;

class ZxData
{
    uint8_t m_buffer[64 + 32];
    int m_size = 0;

public:

    uint8_t* buffer() { return m_buffer; }

    inline bool empty() const { return m_size == 0; }

    uint8_t last() const { return m_buffer[m_size-1]; }

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

class Register16;
using Registers = std::array<Register16, 3>;

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
        else if (name == 'h' || name == 'i')
            return 4;
        else if (name == 'l' || name == 'x' || name == 'y')
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
    bool isAlt = false;
    uint8_t indexRegPrefix = 0;

    Register8(const char name): name(name)
    {
        reg8Index = calculateIndex();
        if (name == 'x')
            indexRegPrefix = IX_REG_PREFIX;
        else if (name == 'y')
            indexRegPrefix = IY_REG_PREFIX;
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

        if (indexRegPrefix)
        {
            line.data.push_back(indexRegPrefix);
            line.drawTicks += 4;
        }

        line.drawTicks += 7;
        line.data.push_back(uint8_t(0x06 + reg8Index * 8));
        line.data.push_back(byte);
    }

    void loadFromReg(CompressedLine& line, const Register8& reg)
    {
        value = reg.value;

        if (indexRegPrefix)
        {
            line.data.push_back(indexRegPrefix);
            line.drawTicks += 4;
        }

        line.drawTicks += 4;
        const uint8_t data = 0x40 + reg8Index * 8 + reg.reg8Index;
        line.data.push_back(data);
    }

    void incValue(CompressedLine& line)
    {
        value = *value + 1;

        if (indexRegPrefix)
        {
            line.data.push_back(indexRegPrefix);
            line.drawTicks += 4;
        }

        line.drawTicks += 4;
        const uint8_t data = 0x04 + 8 * reg8Index;
        line.data.push_back(data);
    }

    void decValue(CompressedLine& line)
    {
        value = *value - 1;

        if (indexRegPrefix)
        {
            line.data.push_back(indexRegPrefix);
            line.drawTicks += 4;
        }

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


    template <typename T>
    void updateToValue(CompressedLine& line, uint8_t byte, const T& registers16, const Register8& a);
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
        l.isAlt = h.isAlt = isAlt;
        h.indexRegPrefix = l.indexRegPrefix;
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

    void loadFromReg16(CompressedLine& line, const Register16& reg) const
    {
        assert(!l.indexRegPrefix || !reg.l.indexRegPrefix);
        if (l.indexRegPrefix)
        {
            line.data.push_back(l.indexRegPrefix);
            line.drawTicks += 4;
        }
        else if (reg.l.indexRegPrefix)
        {
            line.data.push_back(reg.l.indexRegPrefix);
            line.drawTicks += 4;
        }

        if ((h.name == 'h' || h.name == 'i') && reg.h.name == 's')
            line.data.push_back(0x39); //< ADD HL, SP
        else if (h.name == 's' && (reg.h.name == 'h' || reg.h.name == 'i'))
            line.data.push_back(0xf9); //< LD SP, HL
        else
            assert(0);
        line.drawTicks += 6;
    }

    void exxHl(CompressedLine& line)
    {
        assert(h.name = 'd');
        line.data.push_back(0xeb);
        line.drawTicks += 4;
    }

    void loadXX(CompressedLine& line, uint16_t value)
    {
        h.value = value >> 8;
        l.value = (uint8_t) value;
        line.drawTicks += 10;

        if (h.name == 'i')
        {
            line.data.push_back(l.indexRegPrefix);
            line.drawTicks += 4;
        }

        line.data.push_back(uint8_t(0x01 + reg16Index() * 8));
        line.data.push_back(*l.value);
        line.data.push_back(*h.value);
    }

    void addSP(CompressedLine& line)
    {
        if (h.name != 'h' && h.name != 'i')
            assert(0);
        h.value.reset();
        l.value.reset();

        if (l.name == 'x')
        {
            line.data.push_back(IX_REG_PREFIX);
            line.drawTicks += 4;
        }
        else if (l.name == 'y')
        {
            line.data.push_back(IY_REG_PREFIX);
            line.drawTicks += 4;
        }

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
        if (h.name == 'i')
        {
            line.data.push_back(l.indexRegPrefix);
            line.drawTicks += 4;
        }
        line.data.push_back(0xc5 + reg16Index() * 8);
        line.drawTicks += 11;
    }

    template <class T>
    void updateToValue(CompressedLine& line, uint16_t value,
        const T& registers, const Register8& a = Register8('a'))
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
            dec(line);
        }
        else if (!isEmpty() && value16() == value - 1)
        {
            inc(line);
        }
        else if (h.name != 'i')
        {
            const Register8* regH = nullptr;
            const Register8* regL = nullptr;

            for (const auto& reg16: registers)
            {
                if (reg16.isAlt != isAlt)
                    continue;
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
                if (regL->name == h.name)
                {
                    l.loadFromReg(line, *regL);
                    h.loadFromReg(line, *regH);
                }
                else
                {
                    h.loadFromReg(line, *regH);
                    l.loadFromReg(line, *regL);
                }
            }
            else
            {
                loadXX(line, value);
            }
        }
        else
        {
            loadXX(line, value);
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
            if (l.indexRegPrefix)
            {
                line.data.push_back(l.indexRegPrefix);
                line.drawTicks +=4;
            }
            line.data.push_back(0x0b + reg16Index() * 8); //< dec
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
            if (l.indexRegPrefix)
            {
                line.data.push_back(l.indexRegPrefix);
                line.drawTicks += 4;
            }

            line.data.push_back(0x03 + reg16Index() * 8); //< inc
            line.drawTicks += 6;
        }
    }

    void poke(CompressedLine& line, uint16_t address)
    {
        assert(h.name == 'h');
        line.data.push_back(0x22); //< LD (**), HL
        line.data.push_back((uint8_t)address);
        line.data.push_back(address >> 8);
        line.drawTicks += 16;
    }
};

template <typename T>
void Register8::updateToValue(CompressedLine& line, uint8_t byte, const T& registers16, const Register8& a)
{
    for (const auto& reg16: registers16)
    {
        if (name != 'a' && name != 'i' && name != 'x' && name != 'y' && isAlt != reg16.isAlt)
            continue;
        if (reg16.h.name == 'i')
            continue;

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
        incValue(line);
    else if (*value == byte + 1)
        decValue(line);
    else
        loadX(line, byte);
}

struct CompressedData
{
    std::vector<CompressedLine> data;
    Register8 a{ 'a' };

public:
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

int sameVerticalBytes(int flags, uint8_t* buffer, int x, int y, int maxY)
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

int sameVerticalBytesOnRow(uint8_t* buffer, int y)
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
    CompressedLine& result,
    int flags,
    int maxY,
    uint8_t* buffer,
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success);

CompressedLine makeChoise(
    bool isAlt,
    int regIndex,
    uint16_t word,
    int flags,
    int maxY,
    uint8_t* buffer,
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success)
{
    CompressedLine result;
    result.isAltReg = isAlt;

    Register16& reg = registers[regIndex];
    if (reg.h.name != 'i' && reg.isAlt != result.isAltReg)
        result.exx();
    reg.updateToValue(result, word, registers, a);

    reg.push(result);
    compressLine(result, flags, maxY, buffer, registers, a, y, x + 2, success);
    return result;
}

uint16_t swapBytes(uint16_t word)
{
    uint8_t* ptr = (uint8_t*)&word;
    return ((uint16_t)ptr[0] << 8) + ptr[1];
}

void compressLine(
    CompressedLine&  result,
    int flags,
    int maxY,
    uint8_t* buffer,
    Registers& registers,
    const Register8& a, //< bestByte
    int y,
    int x,
    bool* success)
{
    static Register16 sp("sp");

    *success = true;

    while (x < 32)
    {
        int verticalRepCount =  sameVerticalBytes(flags, buffer, x, y, maxY);
        if (!(flags & oddVerticalCompression))
            verticalRepCount &= ~1;

        uint16_t* buffer16 = (uint16_t*) (buffer + y * 32 + x);
        uint16_t word = *buffer16;
        word = swapBytes(word);
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
        if (verticalRepCount > 4 && !result.isAltReg)
        {
            if (auto hl = findRegister(registers, "hl"))
            {
                hl->updateToValue(result, -verticalRepCount, registers, a);
                hl->addSP(result);
                sp.loadFromReg16(result, *hl);
                x += verticalRepCount;
                continue;
            }
        }
        if (verticalRepCount > 5 && !result.isAltReg)
        {
            if (auto de = findRegister(registers, "de"))
            {
                de->exxHl(result);
                Register16 hl("hl");
                hl.loadXX(result, -verticalRepCount);
                hl.addSP(result);
                sp.loadFromReg16(result, hl);
                de->exxHl(result);
                de->reset();
                x += verticalRepCount;
                continue;
            }
        }

        // push existing 16 bit value.
        bool isChoised = false;
        for (auto& reg : registers)
        {
            if (result.isAltReg == reg.isAlt && reg.hasValue16(word))
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

        for (auto& reg : registers)
        {
            if (result.isAltReg == reg.isAlt && reg.isEmpty() && reg.h.name == 'b')
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
            bool successChoise = false;
            auto newLine = makeChoise(result.isAltReg, regIndex, word, flags, maxY,  buffer, regCopy, a, y, x, &successChoise);
            if (successChoise && (choisedLine.data.empty() || newLine.drawTicks < choisedLine.drawTicks))
            {
                chosedRegisters = regCopy;
                choisedLine = newLine;
            }
        }

        if ((flags & oddVerticalCompression) && choisedLine.data.empty() && x % 2 == 0)
        {
            // try to reset flags for the rest of the line
            const auto flags1 = flags & ~oddVerticalCompression;
            for (int regIndex = 0; regIndex < registers.size(); ++regIndex)
            {
                Registers regCopy = registers;
                bool successChoise = false;
                auto newLine = makeChoise(result.isAltReg, regIndex, word, flags1, maxY, buffer, regCopy, a, y, x, &successChoise);
                if (successChoise && (choisedLine.data.empty() || newLine.drawTicks < choisedLine.drawTicks))
                {
                    chosedRegisters = regCopy;
                    choisedLine = newLine;
                }
            }
        }

        if (choisedLine.data.empty())
        {
            *success = false;
            return;
        }
        result += choisedLine;
        result.isAltReg = choisedLine.isAltReg;
        registers = chosedRegisters;
        return;
    }
    if (result.isAltReg)
        result.exx();
}

std::future<CompressedLine> compressLineAsync(int flags, uint8_t* buffer, const Register8& a, int line, int imageHeight)
{
    return std::async(
        [flags, buffer, &a, line, imageHeight]()
        {
            Registers registers1 = {Register16("bc"), Register16("de"), Register16("bc'") };
            Registers registers2 = registers1;

            bool success;
            CompressedLine line1, line2;
            compressLine(line1, flags, imageHeight,
                buffer, registers1, a, line, 0, &success);

            compressLine(line2, flags | oddVerticalCompression, imageHeight,
                buffer, registers2, a, line, 0, &success);

            if (success && line2.data.size() < line1.data.size())
                return line2;
            else
                return line1;
        }
    );
}

CompressedData compressLinesAsync(int flags, uint8_t* buffer, const Register8& a, int y, int count, int imageHeight)
{
    CompressedData compressedData;

    std::vector<std::future<CompressedLine>> compressors(count);
    for (int i = 0; i < count; ++i)
        compressors[i] = compressLineAsync(flags, buffer, a, y++, imageHeight);

    for (auto& compressor: compressors)
        compressedData.data.push_back(compressor.get());

    return compressedData;
}

CompressedData compress(int flags, uint8_t* buffer, int imageHeight)
{
    // Detect the most common byte in image
    std::vector<uint8_t> bytesCount(256);
    for (int i = 0; i < 32 * imageHeight; ++i)
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

    std::cout << "use reg A = " << (int) *a.value << std::endl;

    CompressedData result = compressLinesAsync(flags, buffer, a, 0, imageHeight, imageHeight);
    result.a = a;
    if (!(flags & inverseColors))
        return result;

    for (int y = 0; y < imageHeight / 8; ++y)
    {
        for (int x = 0; x < 32; x += 2)
        {
            std::cout << "Check block " << y << ":" << x  << std::endl;

            int lineNum = y * 8;
            int count = 8;
            if (y > 0)
            {
                --lineNum;
                ++count;
            }
            if (y < imageHeight - 8)
                ++count;

            inversBlock(buffer, x, y);
            auto candidateLeft = compressLinesAsync(flags, buffer, a, lineNum, count, imageHeight);

            inversBlock(buffer, x+1, y);
            auto candidateBoth = compressLinesAsync(flags, buffer, a, lineNum, count, imageHeight);

            inversBlock(buffer, x, y);
            auto candidateRight = compressLinesAsync(flags, buffer, a, lineNum, count, imageHeight);
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

    result.a = a;
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

CompressedData  compressColors(uint8_t* buffer, int imageHeight)
{
    CompressedData compressedData;

    Register8 a('a');

    for (int y = 0; y < imageHeight / 8; y ++)
    {
        Registers registers = { Register16("bc"), Register16("de"), Register16("bc'")};

        bool success;
        CompressedLine line;
        compressLine(line, verticalCompressionH | verticalCompressionL /*flags*/, imageHeight / 8 /*maxY*/,
            buffer, registers, a, y, 0, &success);
        compressedData.data.push_back(line);
    }

    return compressedData;
}

void moveLoadBcFirst(CompressedData& data)
{
    for (auto& line: data.data)
    {
        uint8_t* buffer = line.data.buffer();
        for (int i = 0; i < line.data.size(); ++i)
        {
            if (buffer[i] == DEC_SP_CODE)
                continue;
            else if (buffer[i] == LD_BC_CODE)
            {
                uint8_t tmpBuffer[3];
                memcpy(tmpBuffer, buffer + i, 3);
                memmove(buffer + 3, buffer, i);
                memcpy(buffer, tmpBuffer, 3);
                break;
            }
            else
            {
                assert(0);
            }
        }
    }
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
struct DescriptorsHeader
{
    uint8_t a = 0;
    uint8_t reserved0 = 0;
    uint16_t reserved1 = 0;
};
struct LineDescriptor
{
    uint16_t addressBegin = 0;
    uint16_t addressEnd = 0;
};
#pragma pack(pop)


int serializeMainData(const CompressedData& data, const std::string& inputFileName, uint16_t offset)
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
     * DescriptorsHeader,
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

    std::vector<LineDescriptor> descriptors;

    int bankSizeInLines = imageHeight / 8;

    for (int d = 0; d < imageHeight; ++d)
    {
        int lineBank = d % 8;
        int lineInBank = d / 8;

        int lineNum = bankSizeInLines * lineBank + lineInBank;
        if (lineNum + 8 > imageHeight)
            break;

        LineDescriptor descriptor;
        int line8Size = 0;
        for (int l = 0; l < 8; ++l)
        {
            const auto& line = data.data[lineNum + l];
            line8Size += line.data.size();
        }
        descriptor.addressBegin = data.size(0, lineNum) + offset;
        descriptor.addressEnd = descriptor.addressBegin + data.size(lineNum, 8);
        descriptors.push_back(descriptor);
    }

    DescriptorsHeader descriptorHeader;
    descriptorHeader.a = *data.a.value;
    lineDescriptorFile.write((const char*)&descriptorHeader, sizeof(descriptorHeader));
    for (const auto& descriptor : descriptors)
        lineDescriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    int size = 0;
    for (int y = 0; y < data.data.size(); ++y)
    {
        const auto& line = data.data[y];
        mainDataFile.write((const char*)line.data.data(), line.data.size());
        size += line.data.size();
    }
    uint16_t filler = 0; // reserved space for the BD_BC_CODE for the last line
    mainDataFile.write((const char*) &filler, sizeof(filler));
    size += sizeof(filler);
    return size;
}

int serializeColorData(const CompressedData& data, const std::string& inputFileName, uint16_t offset)
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

    
    std::vector<LineDescriptor> descriptors;
    
    for (int lineNum = 0; lineNum < data.data.size() - 7; ++lineNum)
    {
        LineDescriptor descriptor;
        descriptor.addressBegin = data.size(0, lineNum) + offset;
        descriptor.addressEnd = descriptor.addressBegin + data.size(lineNum, 8);
        descriptors.push_back(descriptor);
    }

    for (const auto& descriptor : descriptors)
        colorDescriptorFile.write((const char*)&descriptor, sizeof(descriptor));

    int size = 0;
    for (int y = 0; y < data.data.size(); ++y)
    {
        const auto& line = data.data[y];
        colorDataFile.write((const char*)line.data.data(), line.data.size());
        size += line.data.size();
    }

    uint16_t filler = 0; // reserved space for the BD_BC_CODE for the last line
    colorDataFile.write((const char*)&filler, sizeof(filler));
    size += sizeof(filler);
    return size;
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
    
    for (int line = 0; line <= imageHeight - 192; ++line)
    {
        int ticks = data.ticks(line, 192);
        int colorLine = line / 8;
        int colorTicks = color.ticks(colorLine, 24);
        uint16_t freeTicks = totalTicksPerFrame - (ticks + colorTicks);
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

    int flags = verticalCompressionH | verticalCompressionL;// | inverseColors; // | interlineRegisters

    const auto t1 = std::chrono::system_clock::now();
    CompressedData data = compress(flags, buffer.data(), imageHeight);
    const auto t2 = std::chrono::system_clock::now();

    std::cout << "compression time= " <<  std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000.0 << "sec" << std::endl;

    CompressedData colorData = compressColors(colorBuffer.data(), imageHeight);
    CompressedData realTimeColor = compressRealTimeColors(colorBuffer.data(), imageHeight/ 8);

    static const int uncompressedTicks = 21 * 16 * imageHeight;
    static const int uncompressedColorTicks = uncompressedTicks / 8;
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

    moveLoadBcFirst(data);
    moveLoadBcFirst(colorData);

    serializeTimingData(data, colorData, outputFileName);

    interleaveData(data);

    int mainDataSize = serializeMainData(data, outputFileName, codeOffset);
    serializeColorData(colorData, outputFileName, codeOffset + mainDataSize);
    
    return 0;
}
