#include "registers.h"
#include "compressed_line.h"

void Register8::add(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.data.push_back(0x80 + reg.reg8Index); //< ADD a, reg8
    line.drawTicks += 4;
}

void Register8::loadX(CompressedLine& line, uint8_t byte)
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

void Register8::scf(CompressedLine& line)
{
    assert(name = 'f');
    line.data.push_back(0x37);
    line.drawTicks += 4;
}

void Register8::cpl(CompressedLine& line)
{
    assert(name = 'a');
    line.data.push_back(0x2f);
    line.drawTicks += 4;
    value = ~(*value);
}

void Register8::addReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0x80 + reg.reg8Index);
}

void Register8::xorReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0xa8 + reg.reg8Index);
}

void Register8::andReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0xa0 + reg.reg8Index);
}

void Register8::subReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0x90 + reg.reg8Index);
}

void Register8::loadFromReg(CompressedLine& line, const Register8& reg)
{
    value = reg.value;
    if (&reg == this)
        return;

    if (indexRegPrefix)
    {
        line.data.push_back(indexRegPrefix);
        line.drawTicks += 4;
    }

    line.drawTicks += 4;
    const uint8_t data = 0x40 + reg8Index * 8 + reg.reg8Index;
    line.data.push_back(data);
}

void Register8::incValue(CompressedLine& line)
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

void Register8::decValue(CompressedLine& line)
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

void Register8::setBit(CompressedLine& line, uint8_t bit)
{
    assert(bit < 8);
    line.data.push_back(0xcb);
    line.data.push_back(0xc0 + reg8Index + bit * 8);
    line.drawTicks += 8;
}

// --------------------- Register16 --------------------------------

void Register16::loadFromReg16(CompressedLine& line, const Register16& reg) const
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

void Register16::exxHl(CompressedLine& line)
{
    assert(h.name = 'd');
    line.data.push_back(0xeb);
    line.drawTicks += 4;
}

void Register16::loadXX(CompressedLine& line, uint16_t value)
{
    h.value = value >> 8;
    l.value = (uint8_t)value;
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

void Register16::addSP(CompressedLine& line, Register8* f)
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

void Register16::push(CompressedLine& line) const
{
    assert(isAlt == line.isAltReg);
    if (h.name == 'i')
    {
        line.data.push_back(l.indexRegPrefix);
        line.drawTicks += 4;
    }
    line.data.push_back(0xc5 + reg16Index() * 8);
    line.drawTicks += 11;
    if (!h.isEmpty())
        line.useReg(h);
    if (!l.isEmpty())
        line.useReg(l);
}

template <int N>
bool Register16::updateToValueForAF(CompressedLine& line, uint16_t value, std::array<Register16, N>& registers)
{
    auto& a = h;
    auto& f = l;

    switch (value)
    {
    case 0x0042:
        a.subReg(line, a);
        setValue(value);
        return true;
    case 0x0045:
        a.xorReg(line, a);
        setValue(value);
        return true;
    }

    const uint8_t hiByte = value >> 8;
    const uint8_t lowByte = (uint8_t)value;

    if (f.hasValue(lowByte))
    {
        a.loadX(line, hiByte);
        return true;
    }

    switch (value)
    {
    case 0x0040:
        a.xorReg(line, a);
        a.addReg(line, a);
        setValue(value);
        return true;
    case 0x0041:
        a.subReg(line, a);
        f.scf(line);
        setValue(value);
        return true;
    case 0x0044:
        a.xorReg(line, a);
        f.scf(line);
        setValue(value);
        return true;
    case 0x0054:
        a.xorReg(line, a);
        a.andReg(line, a);
        setValue(value);
        return true;
    case 0x0100:
        a.xorReg(line, a);
        a.incValue(line);
        setValue(value);
        return true;
    case 0xffba:
        a.xorReg(line, a);
        a.decValue(line);
        setValue(value);
        return true;
    case 0xff7e:
        a.xorReg(line, a);
        a.cpl(line);
        setValue(value);
        return true;
    case 0xff7a:
        a.subReg(line, a);
        a.cpl(line);
        setValue(value);
        return true;
    }

    if (lowByte == 0x44 || lowByte == 0x42)
    {
        const Register8* reg = nullptr;
        for (const auto& reg16 : registers)
        {
            if (reg16.l.name == 'f')
                continue;
            if (reg16.h.hasValue(hiByte))
                reg = &reg16.h;
            else if (reg16.l.hasValue(hiByte))
                reg = &reg16.l;
        }
        if (reg)
        {
            if (lowByte == 0x44)
                a.xorReg(line, h);
            else
                a.subReg(line, h);
            if (a.value != hiByte)
                h.loadFromReg(line, *reg);
            setValue(value);
            return true;
        }
    }

    return false;
}

template bool Register16::updateToValue(CompressedLine& line, uint16_t value, std::array<Register16, 3>& registers);


void Register16::dec(CompressedLine& line, int repeat)
{
    if (!isEmpty())
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
            line.drawTicks += 4;
        }
        line.data.push_back(0x0b + reg16Index() * 8); //< dec
        line.drawTicks += 6;
    }
}

void Register16::inc(CompressedLine& line, int repeat)
{
    if (!isEmpty())
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

void Register16::poke(CompressedLine& line, uint16_t address)
{
    assert(h.name == 'h');
    line.data.push_back(0x22); //< LD (**), HL
    line.data.push_back((uint8_t)address);
    line.data.push_back(address >> 8);
    line.drawTicks += 16;
}
