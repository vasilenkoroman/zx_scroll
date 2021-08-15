#pragma once

#include <optional>
#include <cassert>
#include <string>

#include "compressed_line.h"

static const uint8_t  IX_REG_PREFIX = 0xdd;
static const uint8_t  IY_REG_PREFIX = 0xfd;

class Register16;

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

    void add(CompressedLine& line, const Register8& reg);
    void loadX(CompressedLine& line, uint8_t byte);
    void scf(CompressedLine& line);
    void cpl(CompressedLine& line);
    void addReg(CompressedLine& line, const Register8& reg);
    void xorReg(CompressedLine& line, const Register8& reg);
    void orReg(CompressedLine& line, const Register8& reg);
    void andReg(CompressedLine& line, const Register8& reg);
    void subReg(CompressedLine& line, const Register8& reg);
    void loadFromReg(CompressedLine& line, const Register8& reg);
    void incValue(CompressedLine& line);
    void decValue(CompressedLine& line);
    void setBit(CompressedLine& line, uint8_t bit);
    
    template <int N>
    inline bool updateToValue(CompressedLine& line, uint8_t byte, std::array<Register16, N>& registers16)
    {
        if (name == 'f')
            return false;

        for (const auto& reg16 : registers16)
        {
            if (name != 'a' && name != 'i' && name != 'x' && name != 'y' && isAlt != reg16.isAlt)
                continue;
            if (reg16.h.name == 'i')
                continue;

            if (reg16.h.hasValue(byte))
            {
                line.useReg(reg16.h);
                line.selfReg(*this);
                loadFromReg(line, reg16.h);
                return true;
            }
            else if (reg16.l.name != 'f' && reg16.l.hasValue(byte))
            {
                line.useReg(reg16.l);
                line.selfReg(*this);
                loadFromReg(line, reg16.l);
                return true;
            }
        }

        if (isEmpty())
        {
            line.selfReg(*this);
            loadX(line, byte);
        }
        else if (*value == uint8_t(byte - 1))
        {
            line.useReg(*this);
            incValue(line);
            if (auto f = findRegister8(registers16, 'f'))
                f->value.reset();
        }
        else if (*value == uint8_t(byte + 1))
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
        return true;
    }


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

    void loadFromReg16(CompressedLine& line, const Register16& reg) const;
    void exxHl(CompressedLine& line);
    void setValue(uint16_t value)
    {
        h.value = value >> 8;
        l.value = (uint8_t)value;
    }
    void loadXX(CompressedLine& line, uint16_t value);
    void addSP(CompressedLine& line, Register8* f = nullptr);

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

    void push(CompressedLine& line) const;

    template <int N>
    bool updateToValueForAF(CompressedLine& line, uint16_t value, std::array<Register16, N>& registers)
    {
        auto& a = h;
        auto& f = l;

        switch (value)
        {
        case 0x0042:
            a.subReg(line, a);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0x0045:
            a.xorReg(line, a);
            setValue(value);
            line.selfReg(a);
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
            line.selfReg(a);
            return true;
        case 0x0041:
            a.subReg(line, a);
            f.scf(line);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0x0044:
            a.xorReg(line, a);
            f.scf(line);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0x0054:
            a.xorReg(line, a);
            a.andReg(line, a);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0x0100:
            a.xorReg(line, a);
            a.incValue(line);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0xffba:
            a.xorReg(line, a);
            a.decValue(line);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0xff7e:
            a.xorReg(line, a);
            a.cpl(line);
            setValue(value);
            line.selfReg(a);
            return true;
        case 0xff7a:
            a.subReg(line, a);
            a.cpl(line);
            setValue(value);
            line.selfReg(a);
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
                {
                    h.loadFromReg(line, *reg);
                    line.useReg(*reg);
                }
                setValue(value);
                line.selfReg(a);
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
        else if (hasValue16(value + 1))
        {
            line.useReg(h, l);
            dec(line);
        }
        else if (hasValue16(value - 1))
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

    void dec(CompressedLine& line, int repeat = 1);
    void inc(CompressedLine& line, int repeat = 1);
    void poke(CompressedLine& line, uint16_t address);
};

template <typename T>
Register8* findRegister8(T& registers, const char& name)
{
    for (auto& reg : registers)
    {
        if (reg.h.name == name)
            return &reg.h;
        else if (reg.l.name == name)
            return &reg.l;
    }
    return nullptr;
}
