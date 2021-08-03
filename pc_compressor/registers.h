#pragma once

#include <optional>
#include <cassert>
#include <string>

static const uint8_t  IX_REG_PREFIX = 0xdd;
static const uint8_t  IY_REG_PREFIX = 0xfd;

struct CompressedLine;

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
    void andReg(CompressedLine& line, const Register8& reg);
    void subReg(CompressedLine& line, const Register8& reg);
    void loadFromReg(CompressedLine& line, const Register8& reg);
    void incValue(CompressedLine& line);
    void decValue(CompressedLine& line);
    void setBit(CompressedLine& line, uint8_t bit);
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

    template <class T>
    bool updateToValueForAF(CompressedLine& line, uint16_t value, T& registers);

    template <typename T>
    bool updateToValue(CompressedLine& line, uint16_t value, T& registers);

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
