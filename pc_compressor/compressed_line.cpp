#include <iostream>

#include "compressed_line.h"
#include "registers.h"
#include "code_parser.h"

void RegUsageInfo::useReg(const Register8& reg)
{
    regUseMask |= 1 << reg.reg8Index;
}

void RegUsageInfo::selfReg(const Register8& reg)
{
    uint8_t mask = 1 << reg.reg8Index;
    if ((regUseMask & mask) == 0)
        selfRegMask |= mask;
}

void RegUsageInfo::useReg(const Register8& reg1, const Register8& reg2)
{
    useReg(reg1);
    useReg(reg2);
}

void RegUsageInfo::selfReg(const Register8& reg1, const Register8& reg2)
{
    selfReg(reg1);
    selfReg(reg2);
}

std::vector<Register16> RegUsageInfo::getUsedRegisters(const std::vector<Register16>& inputRegisters) const
{
    std::vector<Register16> result;

    for (const auto& reg16 : inputRegisters)
    {
        uint8_t hMask = 1 << reg16.h.reg8Index;
        uint8_t lMask = 1 << reg16.l.reg8Index;

        Register16 usedReg(reg16.name());

        if (!(selfRegMask & hMask) && (regUseMask & hMask))
            usedReg.h.value = reg16.h.value;

        if (!(selfRegMask & lMask) && (regUseMask & lMask) && reg16.l.name != 'f')
            usedReg.l.value = reg16.l.value;

        if (!usedReg.h.isEmpty() || !usedReg.l.isEmpty())
            result.push_back(usedReg);
    }
    return result;
}

CompressedLine RegUsageInfo::getSerializedUsedRegisters(const std::vector<Register16>& inputRegisters, const Register16& _af) const
{
    return getSerializedRegisters(getUsedRegisters(inputRegisters), _af);
}

CompressedLine getSerializedRegisters(const std::vector<Register16>& data, const Register16& af)
{
    CompressedLine line;

#if 1
    std::array<Register16, 3> registers = { Register16("bc"), Register16("de"), Register16("hl") };
    for (auto& reg16: data)
    {
        if (!reg16.h.isEmpty() && !reg16.l.isEmpty())
        {
            if (auto reg = findRegister(registers, reg16.name()))
                reg->updateToValue(line, reg16.value16(), registers, af);
            else
            {
                std::cerr << "Register not found!" << std::endl;
                abort();
            }
        }
        else if (!reg16.h.isEmpty())
        {
            if (auto reg = findRegister8(registers, reg16.h.name))
                reg->updateToValue(line, *reg16.h.value, registers, af);
            else
            {
                std::cerr << "Register not found!" << std::endl;
                abort();
            }

        }
        else if (!reg16.l.isEmpty())
        {
            if (auto reg = findRegister8(registers, reg16.l.name))
                reg->updateToValue(line, *reg16.l.value, registers, af);
            else
            {
                std::cerr << "Register not found!" << std::endl;
                abort();
            }
        }
    }
#else
    for (auto& reg16 : getUsedRegisters(inputRegisters))
    {
        if (!reg16.h.isEmpty() && !reg16.l.isEmpty())
            reg16.loadXX(line, reg16.value16());
        else if (!reg16.h.isEmpty())
            reg16.h.loadX(line, *reg16.h.value);
        else if (!reg16.l.isEmpty())
            reg16.l.loadX(line, *reg16.l.value);
    }
#endif

    return line;
}


// ----------------------- CompressedLine ------------------------------

void CompressedLine::serialize(std::vector<uint8_t>& vector) const
{
    const auto dataSize = vector.size();
    vector.resize(dataSize + data.size());
    memcpy(vector.data() + dataSize, data.data(), data.size());
}

void CompressedLine::append(const uint8_t* buffer, int size)
{
    for (int i  = 0; i < size; ++i)
        data.push_back(*buffer++);
}

void CompressedLine::append(const std::vector<uint8_t>& data)
{
    append(data.data(), data.size());
}

void CompressedLine::appendCommand(const z80Command& command)
{
    drawTicks += command.ticks;
    data.push_back(command.opCode);
    if (command.size > 1)
        data.push_back((uint8_t) command.data);
    if (command.size > 2)
        data.push_back(command.data >> 8);
}

void CompressedLine::push_front(const std::vector<uint8_t>& v)
{
    for (auto itr = v.rbegin(); itr != v.rend(); ++itr)
        data.push_front(*itr);
}

void CompressedLine::push_front(const ZxData& buffer)
{
    for (int i = buffer.size() - 1; i >= 0; --i)
        data.push_front(buffer.data()[i]);
}

std::vector<uint8_t> CompressedLine::getFirstCommands(int size) const
{
    return Z80Parser::getCode(data.buffer(), size);
}

void CompressedLine::splitPreLoadAndPush(CompressedLine* preloadLine, CompressedLine* pushLine)
{
    std::vector<uint8_t> result;
    const uint8_t* ptr = data.buffer();
    const uint8_t* end = data.buffer() + data.size();
    bool canUsePreload = true;
    uint8_t regUseMask = 0;

    static const uint8_t bcMask = 0x03;
    static const uint8_t deMask = 0xc0;
    static const uint8_t hlMask = 0x30;

    while (ptr < end)
    {
        int commandSize = Z80Parser::parseCommand(ptr).size;
        bool toPreload = false;

        switch (*ptr)
        {
            case 0x01: // LD BC, XX
                if (!(regUseMask & bcMask))
                {
                    regUseMask |= bcMask;
                    toPreload = true;
                }
                break;
            case 0x11: // LD DE, XX
                if (!(regUseMask & deMask))
                {
                    regUseMask |= deMask;
                    toPreload = true;
                }
                break;
            case 0x21: // LD HL, XX
                if (!(regUseMask & hlMask))
                {
                    regUseMask |= hlMask;
                    toPreload = true;
                }
                break;
        }


        if (toPreload)
        {
            preloadLine->append(ptr, commandSize);
            preloadLine->drawTicks += 10;
        }
        else
        {
            pushLine->append(ptr, commandSize);
        }

        ptr += commandSize;
    }
    pushLine->drawTicks = drawTicks - preloadLine->drawTicks;
}

std::vector<Register16> CompressedLine::getUsedRegisters() const
{
    return regUsage.getUsedRegisters(*inputRegisters);
}

CompressedLine CompressedLine::getSerializedUsedRegisters(const Register16& af) const
{
    return regUsage.getSerializedUsedRegisters(*inputRegisters, af);
}
