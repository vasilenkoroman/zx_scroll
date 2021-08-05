#pragma once

#include <array>
#include <cassert>
#include <vector>
#include <memory>

#include "registers.h"

class Register8;
class Register16;
using Registers = std::array<Register16, 3>;

class ZxData
{
    uint8_t m_buffer[64 + 32];
    int m_size = 0;

public:

    uint8_t* buffer() { return m_buffer; }

    inline bool empty() const { return m_size == 0; }

    uint8_t last() const { return m_buffer[m_size - 1]; }

    inline void push_back(uint8_t value)
    {
        assert(m_size < sizeof(m_buffer));
        m_buffer[m_size++] = value;
    }

    inline void pop_back()
    {
        --m_size;
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
        regUseMask |= other.regUseMask;
        selfRegMask |= other.selfRegMask;
    }

    ZxData data;
    std::shared_ptr<Registers> inputRegisters;
    std::shared_ptr<Registers> outputRegisters;

    int drawTicks = 0;
    bool isAltReg = false;
    bool isAltAf = false;
    int prelineTicks = 0;

    uint8_t regUseMask = 0;
    uint8_t selfRegMask = 0;

public:
    void useReg(const Register8& reg);
    void selfReg(const Register8& reg);
    void useReg(const Register8& reg1, const Register8& reg2);
    void selfReg(const Register8& reg1, const Register8& reg2);

    std::vector<Register16> getUsedRegisters() const;
    CompressedLine getSerializedUsedRegisters() const;

    void serialize(std::vector<uint8_t>& vector) const;

};