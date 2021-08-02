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

void CompressedLine::appendItselfToVector(std::vector<uint8_t>& vector) const
{
    const int dataSize = vector.size();
    vector.resize(dataSize + data.size());
    memcpy(vector.data() + dataSize, data.data(), data.size());
}
