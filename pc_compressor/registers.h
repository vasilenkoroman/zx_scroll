#pragma once

#include <optional>

static const uint8_t  IX_REG_PREFIX = 0xdd;
static const uint8_t  IY_REG_PREFIX = 0xfd;

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
        else if (name == 'f')
            return -1; //< AF

        assert(0);
        return 0;
    }
public:

    char name;
    std::optional<uint8_t> value;
    int reg8Index;
    bool isAlt = false;
    uint8_t indexRegPrefix = 0;

    Register8(const char name) : name(name)
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

    void scf(CompressedLine& line)
    {
        assert(name = 'f');
        line.data.push_back(0x37);
        line.drawTicks += 4;
    }

    void cpl(CompressedLine& line)
    {
        assert(name = 'a');
        line.data.push_back(0x2f);
        line.drawTicks += 4;
        value = ~(*value);
    }

    void addReg(CompressedLine& line, const Register8& reg)
    {
        assert(name == 'a');
        line.drawTicks += 4;
        line.data.push_back(0x80 + reg.reg8Index);
    }

    void xorReg(CompressedLine& line, const Register8& reg)
    {
        assert(name == 'a');
        line.drawTicks += 4;
        line.data.push_back(0xa8 + reg.reg8Index);
    }

    void andReg(CompressedLine& line, const Register8& reg)
    {
        assert(name == 'a');
        line.drawTicks += 4;
        line.data.push_back(0xa0 + reg.reg8Index);
    }

    void subReg(CompressedLine& line, const Register8& reg)
    {
        assert(name == 'a');
        line.drawTicks += 4;
        line.data.push_back(0x90 + reg.reg8Index);
    }

    void loadFromReg(CompressedLine& line, const Register8& reg)
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
    void updateToValue(CompressedLine& line, uint8_t byte, T& registers16);
};

class Register16
{
public:

    Register8 h;
    Register8 l;
    bool isAlt = false;

    bool hasReg8(const Register8* reg) const
    {
        return &h == reg || &l == reg;
    }

    Register16(const std::string& name, std::optional<uint16_t> value = std::nullopt) :
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

    void setValue(uint16_t value)
    {
        h.value = value >> 8;
        l.value = (uint8_t)value;
    }

    void loadXX(CompressedLine& line, uint16_t value)
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

    void addSP(CompressedLine& line, Register8* f = nullptr)
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

        if (f)
        {

        }

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
    bool updateToValueForAF(CompressedLine& line, uint16_t value, T& registers)
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

    template <typename T>
    bool updateToValue(CompressedLine& line, uint16_t value, T& registers)
    {
        if (hasValue16(value))
        {
            line.useReg(h, l);
            return true;
        }

        if (h.name == 'a')
            return updateToValueForAF(line, value, registers);

        const uint8_t hiByte = value >> 8;
        const uint8_t lowByte = (uint8_t)value;

        if (h.hasValue(hiByte))
        {
            line.useReg(h);
            l.updateToValue(line, lowByte, registers);
        }
        else if (l.hasValue(lowByte))
        {
            line.useReg(l);
            h.updateToValue(line, hiByte, registers);
        }
        else if (!isEmpty() && value16() == value + 1)
        {
            line.useReg(h, l);
            dec(line);
        }
        else if (!isEmpty() && value16() == value - 1)
        {
            line.useReg(h, l);
            inc(line);
        }
        else
        {
            const Register8* regH = nullptr;
            const Register8* regL = nullptr;

            for (const auto& reg16 : registers)
            {
                if (reg16.isAlt != isAlt)
                    continue;
                if (reg16.h.hasValue(hiByte) && !reg16.hasReg8(regL))
                    regH = &reg16.h;
                else if (reg16.h.hasValue(lowByte) && !reg16.hasReg8(regH))
                    regL = &reg16.h;
                else if (reg16.l.name != 'f' && reg16.l.hasValue(hiByte) && !reg16.hasReg8(regL))
                    regH = &reg16.l;
                else if (reg16.l.name != 'f' && reg16.l.hasValue(lowByte) && !reg16.hasReg8(regH))
                    regL = &reg16.l;
            }

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
                line.useReg(h, *regH);
                line.useReg(h, *regL);
            }
            else
            {
                loadXX(line, value);
                line.selfReg(h, l);
            }
        }
        return true;
    }

    void dec(CompressedLine& line, int repeat = 1)
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

    void inc(CompressedLine& line, int repeat = 1)
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
void Register8::updateToValue(CompressedLine& line, uint8_t byte, T& registers16)
{
    for (const auto& reg16 : registers16)
    {
        if (name != 'a' && name != 'i' && name != 'x' && name != 'y' && isAlt != reg16.isAlt)
            continue;
        if (reg16.h.name == 'i')
            continue;

        if (reg16.h.hasValue(byte))
        {
            line.useReg(reg16.h);
            loadFromReg(line, reg16.h);
            return;
        }
        else if (reg16.l.name != 'f' && reg16.l.hasValue(byte))
        {
            line.useReg(reg16.l);
            loadFromReg(line, reg16.l);
            return;
        }
    }

    if (isEmpty())
    {
        line.selfReg(*this);
        loadX(line, byte);
    }
    else if (*value == byte - 1)
    {
        line.useReg(*this);
        incValue(line);
        if (auto f = findRegister8(registers16, 'f'))
            f->value.reset();
    }
    else if (*value == byte + 1)
    {
        line.useReg(*this);
        decValue(line);
        if (auto f = findRegister8(registers16, 'f'))
            f->value.reset();
    }
    else
    {
        line.selfReg(*this);
        loadX(line, byte);
    }
}
