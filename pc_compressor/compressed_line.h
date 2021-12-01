#pragma once

#include <array>
#include <cassert>
#include <vector>
#include <memory>
#include <optional>

class Register8;
class Register16;
class CompressedLine;
class z80Command;

struct RegUsageInfo
{
    void useReg(const Register8& reg);
    void selfReg(const Register8& reg);
    void useReg(const Register8& reg1, const Register8& reg2);
    void selfReg(const Register8& reg1, const Register8& reg2);


    std::vector<Register16> getUsedRegisters(const std::vector<Register16>& inputRegisters) const;
    CompressedLine getSerializedUsedRegisters(const std::vector<Register16>& inputRegisters, const Register16& af) const;

    uint8_t regUseMask = 0;
    uint8_t selfRegMask = 0;
};

class ZxData
{
    uint8_t m_buffer[64 + 32];
    int m_size = 0;

public:

    uint8_t* buffer() { return m_buffer; }
    const uint8_t* buffer() const { return m_buffer; }

    inline bool empty() const { return m_size == 0; }

    uint8_t last() const { return m_buffer[m_size - 1]; }

    inline void push_back(uint8_t value)
    {
        assert(m_size < sizeof(m_buffer));
        m_buffer[m_size++] = value;
    }

    void erase(int index, int count)
    {
        memmove(m_buffer + index, m_buffer + index + count, m_size - index - count);
        m_size -= count;
    }

    inline void push_front(uint8_t value)
    {
        assert(m_size < sizeof(m_buffer));
        memmove(m_buffer+1, m_buffer, m_size);
        m_buffer[0] = value;
        m_size++;
    }

    inline void pop_back()
    {
        --m_size;
    }

    inline void pop_front()
    {
        assert(m_size < sizeof(m_buffer));
        memmove(m_buffer, m_buffer + 1, m_size - 1);
        m_size--;
    }

    inline int size() const { return m_size; }
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

    void scf()
    {
        data.push_back(0x37);
        drawTicks += 4;
    }

    void xorA()
    {
        data.push_back(0xaf);
        drawTicks += 4;
    }

    void jp(uint16_t address)
    {
        data.push_back(0xc3);
        data.push_back((uint8_t)address);
        data.push_back(address >> 8);
        drawTicks += 10;
    }

    void jpIx()
    {
        data.push_back(0xdd);
        data.push_back(0xe9);
        drawTicks += 8;
    }

    void exAf()
    {
        data.push_back(0x08);
        drawTicks += 4;
        isAltAf = !isAltAf;
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
        regUsage.regUseMask |= other.regUsage.regUseMask;
        regUsage.selfRegMask |= other.regUsage.selfRegMask;
    }

    //void useReg(const Register8& reg);
    //void selfReg(const Register8& reg);
    //void useReg(const Register8& reg1, const Register8& reg2);
    //void selfReg(const Register8& reg1, const Register8& reg2);

    // Return first 'size' bytes of the line.
    // It round size up to not break Z80 instruction.
    std::vector<uint8_t> getFirstCommands(int size) const;

    void serialize(std::vector<uint8_t>& vector) const;
    void append(const uint8_t* buffer, int size);
    void append(const std::vector<uint8_t>& data);
    void appendCommand(const z80Command& data);
    void push_front(const std::vector<uint8_t>& data);
    void push_front(uint8_t v);
    void push_front(const ZxData& buffer);
    void erase(int index, int count);

    std::vector<Register16> getUsedRegisters() const;
    CompressedLine getSerializedUsedRegisters(const Register16& af) const;

    void splitPreLoadAndPush(CompressedLine* preloadLine, CompressedLine* pushLine);

public:

    struct Range
    {
        int min = 0;
        int pos = 0;
        int max = 0;
        int virtualTicks = 0;
        std::optional<uint16_t> lendIx;
        std::optional<uint16_t> lendIy;
    };

    ZxData data;
    std::shared_ptr<std::vector<Register16>> inputRegisters;
    std::shared_ptr<Register16> inputAf;

    int drawTicks = 0;
    bool isAltReg = false;
    bool isAltAf = false;

    //int maxDrawDelayTicks = 0;
    //int maxMcDrawShift = 0;
    Range mcStats;

    RegUsageInfo regUsage;

    int flags = 0;
    int lastOddRepPosition = 0;
    std::array<int, 2> spPosHints{-1,-1};

    int16_t minX = 0;
    int16_t maxX = 0;
    int16_t stackMovingAtStart = 0;

    std::optional<uint16_t> updatedHlValue() const
    {
        if (stackMovingAtStart >= 5 && minX > 0)
            return -minX;
        return std::nullopt;
    }
};

CompressedLine getSerializedRegisters(const std::vector<Register16>& registers, const Register16& af);


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

