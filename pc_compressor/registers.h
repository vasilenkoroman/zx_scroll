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
    uint8_t reg8Mask;
    bool isAlt = false;
    uint8_t indexRegPrefix = 0;

    Register8(const char name) : name(name)
    {
        reg8Index = calculateIndex();
        reg8Mask = 1 << reg8Index;
        if (name == 'x')
            indexRegPrefix = IX_REG_PREFIX;
        else if (name == 'y')
            indexRegPrefix = IY_REG_PREFIX;
    }

    __forceinline bool hasValue(uint8_t byte) const
    {
        return value && *value == byte;
    }

    __forceinline bool isEmpty() const
    {
        return !value.has_value();
    }

    void pushViaHL(CompressedLine& line) const;
    void pushViaHL(RegUsageInfo& info) const;

    void loadX(CompressedLine& line, uint8_t byte);
    void loadX(RegUsageInfo& info, uint8_t byte);
    void scf(CompressedLine& line);
    void cpl(CompressedLine& line);

    void addValue(RegUsageInfo& info, uint8_t);

    void addReg(CompressedLine& line, const Register8& reg);
    void addReg(RegUsageInfo& info, const Register8& reg);

    void xorReg(CompressedLine& line, const Register8& reg);
    void xorReg(RegUsageInfo& info, const Register8& reg);
    void xorValue(RegUsageInfo& info, uint8_t v);

    void orReg(CompressedLine& line, const Register8& reg);
    void orReg(RegUsageInfo& info, const Register8& reg);
    void orValue(RegUsageInfo& info, uint8_t v);

    void andReg(CompressedLine& line, const Register8& reg);
    void andReg(RegUsageInfo& info, const Register8& reg);
    void andValue(RegUsageInfo& info, uint8_t v);

    void subReg(CompressedLine& line, const Register8& reg);
    void subReg(RegUsageInfo& info, const Register8& reg);
    void subValue(RegUsageInfo& info, uint8_t);

    void loadFromReg(CompressedLine& line, const Register8& reg);
    void loadFromReg(RegUsageInfo& info, const Register8& reg);

    void incValue(CompressedLine& line);
    void incValue(RegUsageInfo& info);

    void decValue(CompressedLine& line);
    void decValue(RegUsageInfo& info);

    void setBit(CompressedLine& line, uint8_t bit);

    template <int N>
    __forceinline bool updateToValue(CompressedLine& line, uint8_t byte, std::array<Register16, N>& registers16, const Register16& af);
};

class Register16
{
public:

    Register8 h;
    Register8 l;
    bool isAlt = false;
    bool isAltAf = false;

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

    __forceinline int reg16Index() const
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
    void addValue(uint16_t value);

    void loadXX(CompressedLine& line, uint16_t value)
    {
        loadXX(line.regUsage, value);
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

    void loadXX(RegUsageInfo& info, uint16_t value)
    {
        h.value = value >> 8;
        l.value = (uint8_t)value;
        info.selfReg(h.reg8Mask, l.reg8Mask);
    }

    void addSP(CompressedLine& line, Register8* f = nullptr);

    __forceinline bool isEmpty() const
    {
        return h.isEmpty() || l.isEmpty();
    }

    __forceinline bool hasValue16(uint16_t data) const
    {
        if (!h.value || !l.value)
            return false;
        uint16_t value = (*h.value << 8) + *l.value;
        return value == data;
    }

    __forceinline bool hasValue16(uint16_t word, bool canAvoidFirst, bool canAvoidSecond) const
    {
        if (canAvoidFirst && canAvoidSecond)
            return true;
        else if (canAvoidFirst)
            return l.hasValue((uint8_t)word);
        else if (canAvoidSecond)
            return h.hasValue(word >> 8);
        else
            return hasValue16(word);
    }

    __forceinline uint16_t value16() const
    {
        return *h.value * 256 + *l.value;
    }

    __forceinline bool hasValue8(uint8_t value) const
    {
        return h.hasValue(value) || l.hasValue(value);
    }

    void push(RegUsageInfo& line) const;
    void push(CompressedLine& line) const;
    void push(CompressedLine& line, bool canAvoidFirst, bool canAvoidSecond) const;

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
                return true;
            case 0x0044:
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
            case 0x0045:
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
                {
                    h.loadFromReg(line, *reg);
                }
                setValue(value);
                return true;
            }
        }

        return false;
    }

    template <typename T>
    bool updateToValue(CompressedLine& line, uint16_t value, T& registers, const Register16& af)
    {
        if (hasValue16(value))
        {
            line.regUsage.useReg(h.reg8Mask, l.reg8Mask);
            return true;
        }

        if (h.name == 'a')
            return updateToValueForAF(line, value, registers);

        const uint8_t hiByte = value >> 8;
        const uint8_t lowByte = (uint8_t)value;

        if (h.hasValue(hiByte))
        {
            line.regUsage.useReg(h.reg8Mask);
            l.updateToValue(line, lowByte, registers, af);
        }
        else if (l.hasValue(lowByte))
        {
            line.regUsage.useReg(l.reg8Mask);
            h.updateToValue(line, hiByte, registers, af);
        }
        else if (hasValue16(value + 1))
        {
            decValue(line);
        }
        else if (hasValue16(value - 1))
        {
            incValue(line);
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

            if (af.h.hasValue(hiByte) && af.isAltAf == line.isAltAf)
                regH = &af.h;
            if (af.h.hasValue(lowByte) && af.isAltAf == line.isAltAf)
                regL = &af.h;

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

            else if (h.hasValue(hiByte - 1) && lowByte == hiByte)
            {
                h.incValue(line);
                l.loadFromReg(line, h);
            }
            else if (h.hasValue(hiByte + 1) && lowByte == hiByte)
            {
                h.decValue(line);
                l.loadFromReg(line, h);
            }
            else if (l.hasValue(lowByte + 1) && lowByte == hiByte)
            {
                l.decValue(line);
                h.loadFromReg(line, l);
            }
            else if (l.hasValue(lowByte - 1) && lowByte == hiByte)
            {
                l.incValue(line);
                h.loadFromReg(line, l);
            }

            else if (h.hasValue(hiByte - 1) && l.hasValue(lowByte - 1))
            {
                h.incValue(line);
                l.incValue(line);
            }
            else if (h.hasValue(hiByte - 1) && l.hasValue(lowByte + 1))
            {
                h.incValue(line);
                l.decValue(line);
            }
            else if (h.hasValue(hiByte + 1) && l.hasValue(lowByte - 1))
            {
                h.decValue(line);
                l.incValue(line);
            }
            else if (h.hasValue(hiByte + 1) && l.hasValue(lowByte + 1))
            {
                h.decValue(line);
                l.decValue(line);
            }

            else if (regL && h.hasValue(hiByte - 1))
            {
                l.loadFromReg(line, *regL);
                h.incValue(line);
            }
            else if (regL && h.hasValue(hiByte + 1))
            {
                l.loadFromReg(line, *regL);
                h.decValue(line);
            }
            else if (regH && l.hasValue(lowByte - 1))
            {
                h.loadFromReg(line, *regH);
                l.incValue(line);
            }
            else if (regH && l.hasValue(lowByte + 1))
            {
                h.loadFromReg(line, *regH);
                l.decValue(line);
            }
            else
            {
                loadXX(line, value);
            }
        }
        return true;
    }

    void decValue(CompressedLine& line, int repeat = 1);
    void incValue(CompressedLine& line, int repeat = 1);
    void decValue(RegUsageInfo& info);
    void incValue(RegUsageInfo& info);
    void poke(CompressedLine& line, uint16_t address);
};

template <int N>
__forceinline bool Register8::updateToValue(CompressedLine& line, uint8_t byte, std::array<Register16, N>& registers16, const Register16& af)
{
    if (name == 'f')
        return false;

    if (af.h.hasValue(byte) && af.isAltAf == line.isAltAf)
    {
        loadFromReg(line, af.h);
        return true;
    }

    for (const auto& reg16 : registers16)
    {
        if (name != 'a' && name != 'i' && name != 'x' && name != 'y' && isAlt != reg16.isAlt)
            continue;
        if (reg16.h.name == 'i')
            continue;

        if (reg16.h.hasValue(byte))
        {
            loadFromReg(line, reg16.h);
            return true;
        }
        else if (reg16.l.name != 'f' && reg16.l.hasValue(byte))
        {
            loadFromReg(line, reg16.l);
            return true;
        }
    }

    if (isEmpty())
    {
        loadX(line, byte);
    }
    else if (!line.isAltAf && *value == uint8_t(byte - 1))
    {
        incValue(line);
        if (auto f = findRegister8(registers16, 'f'))
            f->value.reset();
    }
    else if (!line.isAltAf && *value == uint8_t(byte + 1))
    {
        decValue(line);
        if (auto f = findRegister8(registers16, 'f'))
            f->value.reset();
    }
    else
    {
        loadX(line, byte);
    }
    return true;
}

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

template <typename T>
const Register8* findRegisterByValue(T& registers, uint8_t value, const Register8* a = nullptr)
{
    if (a && a->hasValue(value))
        return a;
    for (auto& reg: registers)
    {
        if (reg.h.hasValue(value))
            return &reg.h;
        else if (reg.l.hasValue(value))
            return &reg.l;
    }
    return nullptr;
}

template <typename T>
Register8* findRegisterByIndex(T& registers, int index, Register8* a = nullptr)
{
    if (a && a->reg8Index == index)
        return a;
    for (auto& reg: registers)
    {
        if (reg.h.reg8Index == index)
            return &reg.h;
        else if (reg.l.reg8Index == index)
            return &reg.l;
    }
    return nullptr;
}

template <typename T>
Register16* findRegister(T& registers, const std::string& name, std::optional<bool> isAlt = std::nullopt)
{
    for (auto& reg: registers)
    {
        if (reg.name() == name && (!isAlt || reg.isAlt == *isAlt))
            return &reg;
    }
    return nullptr;
}

template <typename T>
const Register16* findRegister(const T& registers, const std::string& name, std::optional<bool> isAlt = std::nullopt)
{
    for (auto& reg : registers)
    {
        if (reg.name() == name && (!isAlt || reg.isAlt == *isAlt))
            return &reg;
    }
    return nullptr;
}
