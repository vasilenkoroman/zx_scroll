#include "registers.h"
#include "compressed_line.h"

#include <iostream>

enum class Operation
{
    none,
    add,
    sub
};

struct SourceArguments
{
    uint8_t a = 0;
    uint8_t b = 0;
    Operation op = Operation::none;
};


static const int kUnusedAfValue = 0x0042;
std::vector<SourceArguments> afValueForAdd(65536);
std::vector<SourceArguments> afValueForSub(65536);

void createAfValueTable()
{
    // calculate AF for operation: LD A, a: ADD B
    for (int a = 0; a < 256; ++a)
    {
        for (int b = 0; b < 256; ++b)
        {
            {
                uint8_t result = (uint8_t)a + (uint8_t)b;
                uint8_t flags = a & 0x38; // F5, F3 from A

                flags |= result & 0x80;   // S flag

                if (result == 0)
                    flags |= 0x40;   // Z flag

                if ((result & 0x0f) < (a & 0x0f))
                    flags |= 0x10;   // H flag (half carry)

                if ((result & 0x80) != (a & 0x80))
                    flags |= 0x40;   // V flag (overflow)

                flags &= ~0x20;   // N flag is reset (not a substraction)

                if (result < a)
                    flags |= 0x1;   // C flag

                uint16_t value = (result << 8) + flags;
                afValueForAdd[value] = { (uint8_t)a, (uint8_t)b, Operation::add };
            }

            // calculate flags for sub
            {
                uint8_t result = (uint8_t)a - (uint8_t)b;
                uint8_t flags = a & 0x38; // F5, F3 from A

                flags |= result & 0x80;   // S flag

                if (result == 0)
                    flags |= 0x40;   // Z flag

                if ((result & 0x0f) > (a & 0x0f))
                    flags |= 0x10;   // H flag (half carry)

                if ((result & 0x80) != (a & 0x80))
                    flags |= 0x40;   // V flag (overflow)

                flags |= 0x20;   // N flag is set (substraction)

                if (result > a)
                    flags |= 0x1;   // C flag

                uint16_t value = (result << 8) + flags;
                afValueForSub[value] = { (uint8_t)a, (uint8_t)b, Operation::sub };
            }

        }
    }

    int hasValues = 0;
    for (int i = 0; i < 65535; ++i)
    {
        if (afValueForAdd[i].op != Operation::none || afValueForSub[i].op != Operation::none)
            ++hasValues;
    }
    std::cout << "available AFt values for add/sub: " << hasValues << std::endl;
}


void Register8::addReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.data.push_back(0x80 + reg.reg8Index); //< ADD a, reg8
    line.drawTicks += 4;
    addReg(line.regUsage, reg);
}

void Register8::addReg(RegUsageInfo& info, const Register8& reg)
{
    assert(name == 'a');
    assert(!isEmpty() && !reg.isEmpty());
    value = *value + *reg.value;
    info.useReg(reg.reg8Mask);
    info.useReg(reg8Mask);
}

void Register8::addValue(RegUsageInfo& info, uint8_t v)
{
    assert(name == 'a');
    assert(isEmpty());
    value = *value + v;
    info.useReg(reg8Mask);
}

void Register8::subValue(RegUsageInfo& info, uint8_t v)
{
    assert(name == 'a');
    assert(isEmpty());
    value = *value - v;
    info.useReg(reg8Mask);
}


void Register8::pushViaHL(CompressedLine& line) const
{
    pushViaHL(line.regUsage);
    line.drawTicks += 7;
    line.data.push_back(uint8_t(0x70 + reg8Index));
}

void Register8::pushViaHL(RegUsageInfo& info) const
{
    info.useReg(reg8Mask);
}

void Register8::loadX(RegUsageInfo& info, uint8_t byte)
{
    value = byte;
    info.selfReg(reg8Mask);
}

void Register8::loadX(CompressedLine& line, uint8_t byte)
{
    loadX(line.regUsage, byte);

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

void Register8::xorReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0xa8 + reg.reg8Index);
    xorReg(line.regUsage, reg);
}

void Register8::xorReg(RegUsageInfo& info, const Register8& reg)
{
    assert(name == 'a');
    if (reg.name != 'a')
    {
        assert(!isEmpty());
        value = *value ^ *reg.value;
    }
    else
    {
        value = 0;
    }
    info.useReg(reg8Mask);
    info.useReg(reg.reg8Mask);
}

void Register8::xorValue(RegUsageInfo& info, uint8_t v)
{
    assert(name == 'a');
    assert(isEmpty());
    value = *value ^ v;
    info.useReg(reg8Mask);
}

void Register8::orReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0xb0 + reg.reg8Index);
    orReg(line.regUsage, reg);
}

void Register8::orReg(RegUsageInfo& info, const Register8& reg)
{
    assert(name == 'a');
    assert(!isEmpty() && !reg.isEmpty());
    value = *value | *reg.value;
    info.useReg(reg8Mask);
    info.useReg(reg.reg8Mask);
}

void Register8::orValue(RegUsageInfo& info, uint8_t v)
{
    assert(name == 'a');
    assert(isEmpty());
    value = *value | v;
    info.useReg(reg8Mask);
}

void Register8::andReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0xa0 + reg.reg8Index);
    andReg(line.regUsage, reg);
}

void Register8::andReg(RegUsageInfo& info, const Register8& reg)
{
    assert(name == 'a');
    if (reg.name != 'a')
    {
        assert(!isEmpty());
        value = *value & *reg.value;
    }
    else
    {
        value = 0;
    }
    info.useReg(reg8Mask);
    info.useReg(reg.reg8Mask);
}

void Register8::andValue(RegUsageInfo& info, uint8_t v)
{
    assert(name == 'a');
    assert(isEmpty());
    value = *value & v;
    info.useReg(reg8Mask);
}

void Register8::subReg(CompressedLine& line, const Register8& reg)
{
    assert(name == 'a');
    line.drawTicks += 4;
    line.data.push_back(0x90 + reg.reg8Index);
    subReg(line.regUsage, reg);
}

void Register8::subReg(RegUsageInfo& info, const Register8& reg)
{
    if (reg.name != 'a')
    {
        assert(!isEmpty());
        value = *value - *reg.value;
    }
    else
    {
        value = 0;
    }
    info.useReg(reg8Mask);
    info.useReg(reg.reg8Mask);
}

void Register8::loadFromReg(CompressedLine& line, const Register8& reg)
{
    assert(!reg.isEmpty());
    loadFromReg(line.regUsage, reg);
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

void Register8::loadFromReg(RegUsageInfo& info, const Register8& reg)
{
    assert(!reg.isEmpty());
    value = reg.value;

    info.useReg(reg.reg8Mask);
    info.selfReg(reg8Mask);
}

void Register8::incValue(CompressedLine& line)
{
    incValue(line.regUsage);

    if (indexRegPrefix)
    {
        line.data.push_back(indexRegPrefix);
        line.drawTicks += 4;
    }

    line.drawTicks += 4;
    const uint8_t data = 0x04 + 8 * reg8Index;
    line.data.push_back(data);
}

void Register8::incValue(RegUsageInfo& info)
{
    value = *value + 1;
    info.useReg(reg8Mask);
}

void Register8::decValue(CompressedLine& line)
{
    decValue(line.regUsage);

    if (indexRegPrefix)
    {
        line.data.push_back(indexRegPrefix);
        line.drawTicks += 4;
    }

    line.drawTicks += 4;
    const uint8_t data = 0x05 + 8 * reg8Index;
    line.data.push_back(data);
}

void Register8::decValue(RegUsageInfo& info)
{
    value = *value - 1;
    info.useReg(reg8Mask);
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

    if (h.name == 's' && (reg.h.name == 'h' || reg.h.name == 'i'))
    {
        line.data.push_back(0xf9); //< LD SP, HL
        line.regUsage.useReg(reg.h.reg8Mask, reg.l.reg8Mask);
        line.drawTicks += 6;
    }
    else
    {
        assert(0);
    }
}

void Register16::exxHl(CompressedLine& line)
{
    assert(h.name = 'd');
    line.data.push_back(0xeb);
    line.drawTicks += 4;
}

void Register16::addSP(CompressedLine& line, Register8* f)
{
    if (h.name != 'h' && h.name != 'i')
        assert(0);
    line.regUsage.useReg(h.reg8Mask, l.reg8Mask);
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

void Register16::push(RegUsageInfo& info) const
{
    if (!h.isEmpty())
        info.useReg(h.reg8Mask);
    if (!l.isEmpty())
        info.useReg(l.reg8Mask);
}

void Register16::push(CompressedLine& line) const
{
    assert(isAlt == line.isAltReg);
    push(line.regUsage);

    if (h.name == 'i')
    {
        line.data.push_back(l.indexRegPrefix);
        line.drawTicks += 4;
    }
    const int index = h.name == 'a' ? 6 : reg16Index();
    line.data.push_back(0xc5 + index * 8);
    line.drawTicks += 11;
}

void Register16::push(CompressedLine& line, bool canAvoidFirst, bool canAvoidSecond) const
{
    assert(isAlt == line.isAltReg);

    if (!h.isEmpty() && !canAvoidFirst)
        line.regUsage.useReg(h.reg8Mask);
    if (!l.isEmpty() && !canAvoidSecond)
        line.regUsage.useReg(l.reg8Mask);

    if (h.name == 'i')
    {
        line.data.push_back(l.indexRegPrefix);
        line.drawTicks += 4;
    }
    const int index = h.name == 'a' ? 6 : reg16Index();
    line.data.push_back(0xc5 + index * 8);
    line.drawTicks += 11;
}

void Register16::decValue(CompressedLine& line, int repeat)
{
    for (int i = 0; i < repeat; ++i)
    {
        decValue(line.regUsage);

        if (l.indexRegPrefix)
        {
            line.data.push_back(l.indexRegPrefix);
            line.drawTicks += 4;
        }
        line.data.push_back(0x0b + reg16Index() * 8); //< dec
        line.drawTicks += 6;
    }
}

void Register16::decValue(RegUsageInfo& info)
{
    if (h.name == 's')
        return;

    assert(!isEmpty());
    setValue(value16() - 1);
    info.useReg(h.reg8Mask, l.reg8Mask);
}

void Register16::addValue(uint16_t value)
{
    assert(!isEmpty());
    setValue(value16() + value);
}


void Register16::incValue(RegUsageInfo& info)
{
    assert(!isEmpty());
    setValue(value16() + 1);
    info.useReg(h.reg8Mask, l.reg8Mask);
}

void Register16::incValue(CompressedLine& line, int repeat)
{
    incValue(line.regUsage);

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
