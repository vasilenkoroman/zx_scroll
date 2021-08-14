#include <iostream>

#include "compressed_line.h"
#include "registers.h"

void CompressedLine::useReg(const Register8& reg)
{
    regUseMask |= 1 << reg.reg8Index;
}

void CompressedLine::selfReg(const Register8& reg)
{
    uint8_t mask = 1 << reg.reg8Index;
    if ((regUseMask & mask) == 0)
        selfRegMask |= mask;
}

void CompressedLine::useReg(const Register8& reg1, const Register8& reg2)
{
    useReg(reg1);
    useReg(reg2);
}

void CompressedLine::selfReg(const Register8& reg1, const Register8& reg2)
{
    selfReg(reg1);
    selfReg(reg2);
}

std::vector<Register16> CompressedLine::getUsedRegisters() const
{
    std::vector<Register16> result;

    for (const auto& reg16: *inputRegisters)
    {
        if (reg16.h.name == 'a')
            continue;

        uint8_t hMask = 1 << reg16.h.reg8Index;
        uint8_t lMask = 1 << reg16.l.reg8Index;

        Register16 usedReg(reg16.name());

        if (!(selfRegMask & hMask) && (regUseMask & hMask))
            usedReg.h.value = reg16.h.value;

        if (!(selfRegMask & lMask) && (regUseMask & lMask))
            usedReg.l.value = reg16.l.value;

        if (!usedReg.h.isEmpty() || !usedReg.l.isEmpty())
            result.push_back(usedReg);
    }
    return result;
}

CompressedLine CompressedLine::getSerializedUsedRegisters() const
{
    CompressedLine line;
    for (auto& reg16 : getUsedRegisters())
    {
        if (!reg16.h.isEmpty() && !reg16.l.isEmpty())
            reg16.loadXX(line, reg16.value16());
        else if (!reg16.h.isEmpty())
            reg16.h.loadX(line, *reg16.h.value);
        else if (!reg16.l.isEmpty())
            reg16.l.loadX(line, *reg16.l.value);
    }

    return line;
}

void CompressedLine::serialize(std::vector<uint8_t>& vector) const
{
    const auto dataSize = vector.size();
    vector.resize(dataSize + data.size());
    memcpy(vector.data() + dataSize, data.data(), data.size());
}

std::vector<uint8_t> CompressedLine::getFirstCommands(int size) const 
{
    std::vector<uint8_t> result;
    const uint8_t* ptr = data.buffer();
    while (result.size() < size)
    {
        int commandSize = 0;

        switch (*ptr)
        {
            case 0x06:
            case 0x0e:
            case 0x10:
            case 0x16:
            case 0x18:
            case 0x1e:
            case 0x20:
            case 0x26:
            case 0x28:
            case 0x2e:
            case 0x30:
            case 0x36:
            case 0x38:
            case 0x3e:
            case 0xc6:
            case 0xce:
            case 0xd3:
            case 0xd6:
            case 0xdb:
            case 0xde:
            case 0xe6:
            case 0xee:
            case 0xf6:
            case 0xfe:
                commandSize = 2;
                break;
            case 0x01:
            case 0x11:
            case 0x21:
            case 0x22:
            case 0x2a:
            case 0x31:
            case 0x32:
            case 0x3a:
            case 0xc2:
            case 0xc3:
            case 0xc4:
            case 0xca:
            case 0xcc:
            case 0xcd:
            case 0xd2:
            case 0xd4:
            case 0xda:
            case 0xdc:
            case 0xe2:
            case 0xe4:
            case 0xea:
            case 0xec:
            case 0xf2:
            case 0xf4:
            case 0xfa:
            case 0xfc:
                commandSize = 3;
                break;
            case 0xcb:
            case 0xdd:
            case 0xed:
            case 0xfd:
                std::cout << "Unsupported command " << (int)*ptr << ". It never can be. Some bug!";
                assert(0);
            default:
            commandSize = 1;
        }
        for (int i = 0; i < commandSize; ++i)
        {
            result.push_back(*ptr++);
        }
    }
    return result;
}
