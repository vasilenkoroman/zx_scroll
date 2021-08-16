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

void CompressedLine::append(const uint8_t* buffer, int size)
{
    for (int i  = 0; i < size; ++i)
        data.push_back(*buffer++);
}

z80Command CompressedLine::parseZ80Command(const uint8_t* ptr) const
{
    static std::vector<z80Command> commands =
    {
        { 1, 4, 0, 0 }, // 00: nop
        { 3, 10, 0, 0 }, // 01: ld bc, **
        { 1, 7, 0, 0 }, // 02: ld (bc),a
        { 1, 6, 0, 0 }, // 03: inc bc
        { 1, 4, 0, 0 }, // 04: inc b
        { 1, 4, 0, 0 }, // 05: dec b
        { 2, 7, 0, 0 }, // 06: ld b, *
        { 1, 4, 0, 0 }, // 07: rlca
        { 1, 4, 0, 0 }, // 08: ex af,af'
        { 1, 11, 0, 0 }, // 09: add hl,bc
        { 1, 7, 0, 0 }, // 0a: ld a,(bc)
        { 1, 6, 0, 0 }, // 0b: dec bc
        { 1, 4, 0, 0 }, // 0c: inc c
        { 1, 4, 0, 0 }, // 0d: dec c
        { 2, 7, 0, 0 }, // 0e: ld c,*
        { 1, 4, 0, 0 }, // 0f: rrca
        { 2, -1, 0, 0 }, // 10: djnz * (13/8). -1 means unknown ticks
        { 3, 10, 0, 0 }, // 11: ld de,**
        { 1, 7, 0, 0 }, // 12: ld (de),a
        { 3, 6, 0, 0 }, // 13: inc de
        { 1, 4, 0, 0 }, // 14: inc d
        { 1, 4, 0, 0 }, // 15: dec d
        { 2, 7, 0, 0 }, // 16: ld d,*
        { 1, 4, 0, 0 }, // 17: rla
        { 2, 12, 0, 0 }, // 18: jr *
        { 1, 11, 0, 0 }, // 19: add hl,de
        { 1, 7, 0, 0 }, // 1a: ld a,(de)
        { 1, 11, 0, 0 }, // 1b: dec de
        { 1, 4, 0, 0 }, // 1c: inc e
        { 1, 4, 0, 0 }, // 1d: dec e
        { 2, 7, 0, 0 }, // 1e: ld e,*
        { 1, 4, 0, 0 }, // 1f: rra
        { 2, -1, 0, 0 }, // 20: jr nz,* (12/7). -1 means unknown ticks
        { 3, 10, 0, 0 }, // 21: ld hl,**
        { 3, 16, 0, 0 }, // 22: ld (**),hl
        { 1, 6, 0, 0 }, // 23: inc hl
        { 1, 4, 0, 0 }, // 24: inc h
        { 1, 4, 0, 0 }, // 25: dec h
        { 2, 7, 0, 0 }, // 26: ld h,*
        { 1, 4, 0, 0 }, // 27: daa
        { 1, -1, 0, 0 }, // 28: jr z,* (12/7). -1 means unknown ticks
        { 1, 11, 0, 0 }, // 29: add hl,hl
        { 1, 16, 0, 0 }, // 2a: ld hl,(**)
        { 1, 6, 0, 0 }, // 2b: dec hl
        { 1, 4, 0, 0 }, // 2c: inc l
        { 1, 4, 0, 0 }, // 2d: dec l
        { 2, 7, 0, 0 }, // 2e: ld l,*
        { 1, 4, 0, 0 }, // 2f: cpl
        { 2, -1, 0, 0 }, // 30: jr nc,* (12/7). -1 means unknown ticks
        { 3, 10, 0, 0 }, // 31: ld sp,**
        { 3, 13, 0, 0 }, // 32: ld (**),a
        { 1, 6, 0, 0 }, // 33: inc sp
        { 1, 11, 0, 0 }, // 34: inc (hl)
        { 1, 11, 0, 0 }, // 35: dec (hl)
        { 2, 10, 0, 0 }, // 36: ld (hl),*
        { 1, 4, 0, 0 }, // 37: scf
        { 2, -1, 0, 0 }, // 38: jr c,* (12/7). -1 means unknown ticks
        { 1, 11, 0, 0 }, // 39: add hl,sp
        { 3, 13, 0, 0 }, // 3a: ld a,(**)
        { 1, 6, 0, 0 }, // 3b: dec sp
        { 1, 4, 0, 0 }, // 3c: inc a
        { 1, 4, 0, 0 }, // 3d: dec a
        { 2, 7, 0, 0 }, // 3e: ld a,*
        { 1, 4, 0, 0 }, // 3f: ccf
        { 1, 4, 0, 0 }, // 40: ld b,b
        { 1, 4, 0, 0 }, // 41: ld b,c
        { 1, 4, 0, 0 }, // 42: ld b,d
        { 1, 4, 0, 0 }, // 43: ld b,e
        { 1, 4, 0, 0 }, // 44: ld b,h
        { 1, 4, 0, 0 }, // 45: ld b,l
        { 1, 7, 0, 0 }, // 46: ld b,(hl)
        { 1, 4, 0, 0 }, // 47: ld b,a
        { 1, 4, 0, 0 }, // 48: ld c,b
        { 1, 4, 0, 0 }, // 49: ld c,c
        { 1, 4, 0, 0 }, // 4a: ld c,d
        { 1, 4, 0, 0 }, // 4b: ld c,e
        { 1, 4, 0, 0 }, // 4c: ld c,h
        { 1, 4, 0, 0 }, // 4d: ld c,l
        { 1, 7, 0, 0 }, // 4e: ld c,(hl)
        { 1, 4, 0, 0 }, // 4f: ld c,a
        { 1, 4, 0, 0 }, // 50: ld d,b
        { 1, 4, 0, 0 }, // 51: ld d,c
        { 1, 4, 0, 0 }, // 52: ld d,d
        { 1, 4, 0, 0 }, // 53: ld d,e
        { 1, 4, 0, 0 }, // 54: ld d,h
        { 1, 4, 0, 0 }, // 55: ld d,l
        { 1, 7, 0, 0 }, // 56: ld d,(hl)
        { 1, 4, 0, 0 }, // 57: ld d,a
        { 1, 4, 0, 0 }, // 58: ld e,b
        { 1, 4, 0, 0 }, // 59: ld e,c
        { 1, 4, 0, 0 }, // 5a: ld e,d
        { 1, 4, 0, 0 }, // 5b: ld e,e
        { 1, 4, 0, 0 }, // 5c: ld e,h
        { 1, 4, 0, 0 }, // 5d: ld e,l
        { 1, 7, 0, 0 }, // 5e: ld e,(hl)
        { 1, 4, 0, 0 }, // 5f: ld e,a
        { 1, 4, 0, 0 }, // 60: ld h,b
        { 1, 4, 0, 0 }, // 61: ld h,c
        { 1, 4, 0, 0 }, // 62: ld h,d
        { 1, 4, 0, 0 }, // 63: ld h,e
        { 1, 4, 0, 0 }, // 64: ld h,h
        { 1, 4, 0, 0 }, // 65: ld h,l
        { 1, 7, 0, 0 }, // 66: ld h,(hl)
        { 1, 4, 0, 0 }, // 67: ld h,a
        { 1, 4, 0, 0 }, // 68: ld l,b
        { 1, 4, 0, 0 }, // 69: ld l,c
        { 1, 4, 0, 0 }, // 6a: ld l,d
        { 1, 4, 0, 0 }, // 6b: ld l,e
        { 1, 4, 0, 0 }, // 6c: ld l,h
        { 1, 4, 0, 0 }, // 6d: 	ld l,l
        { 1, 7, 0, 0 }, // 6e: ld l,(hl)
        { 1, 4, 0, 0 }, // 6f: ld l,a
        { 1, 7, 0, 0 }, // 70: ld (hl),b
        { 1, 7, 0, 0 }, // 71: ld (hl),c
        { 1, 7, 0, 0 }, // 72: ld (hl),d
        { 1, 7, 0, 0 }, // 73: ld (hl),e
        { 1, 7, 0, 0 }, // 74: ld (hl),h
        { 1, 7, 0, 0 }, // 75: ld (hl),l
        { 1, 4, 0, 0 }, // 76: halt
        { 1, 7, 0, 0 }, // 77: ld (hl),a
        { 1, 4, 0, 0 }, // 78: ld a,b
        { 1, 4, 0, 0 }, // 79: ld a,c
        { 1, 4, 0, 0 }, // 7a: ld a,d
        { 1, 4, 0, 0 }, // 7b: ld a,e
        { 1, 4, 0, 0 }, // 7c: ld a,h
        { 1, 4, 0, 0 }, // 7d: ld a,l
        { 1, 7, 0, 0 }, // 7e: ld a,(hl)
        { 1, 4, 0, 0 }, // 7f: ld a,a
        { 1, 4, 0, 0 }, // 80: add a,b
        { 1, 4, 0, 0 }, // 81: add a,c
        { 1, 4, 0, 0 }, // 82: add a,d
        { 1, 4, 0, 0 }, // 83: add a,e
        { 1, 4, 0, 0 }, // 84: add a,h
        { 1, 4, 0, 0 }, // 85: add a,l
        { 1, 7, 0, 0 }, // 86: add a,(hl)
        { 1, 4, 0, 0 }, // 87: add a,a
        { 1, 4, 0, 0 }, // 88: add a,b
        { 1, 4, 0, 0 }, // 89: add a,c
        { 1, 4, 0, 0 }, // 8a: add a,d
        { 1, 4, 0, 0 }, // 8b: add a,e
        { 1, 4, 0, 0 }, // 8c: adc a,h
        { 1, 4, 0, 0 }, // 8d: adc a,l
        { 1, 7, 0, 0 }, // 8e: adc a,(hl)
        { 1, 4, 0, 0 }, // 8f: adc a,a
        { 1, 4, 0, 0 }, // 90: sub b
        { 1, 4, 0, 0 }, // 91: sub c
        { 1, 4, 0, 0 }, // 92: sub d
        { 1, 4, 0, 0 }, // 93: sub e
        { 1, 4, 0, 0 }, // 94: sub h
        { 1, 4, 0, 0 }, // 95: sub l
        { 1, 7, 0, 0 }, // 96: sub (hl)
        { 1, 4, 0, 0 }, // 97: sub a
        { 1, 4, 0, 0 }, // 98: sbc a,b
        { 1, 4, 0, 0 }, // 99: sbc a,c
        { 1, 4, 0, 0 }, // 9a: sbc a,d
        { 1, 4, 0, 0 }, // 9b: sbc a,e
        { 1, 4, 0, 0 }, // 9c: sbc a,h
        { 1, 4, 0, 0 }, // 9d: sbc a,l
        { 1, 7, 0, 0 }, // 9e: sbc a,(hl)
        { 1, 4, 0, 0 }, // 9f: sbc a,a
        { 1, 4, 0, 0 }, // a0: and b
        { 1, 4, 0, 0 }, // a1: and c
        { 1, 4, 0, 0 }, // a2: and d
        { 1, 4, 0, 0 }, // a3: and e
        { 1, 4, 0, 0 }, // a4: and h
        { 1, 4, 0, 0 }, // a5: and l
        { 1, 7, 0, 0 }, // a6: and (hl)
        { 1, 4, 0, 0 }, // a7: and a
        { 1, 4, 0, 0 }, // a8: xor b
        { 1, 4, 0, 0 }, // a9: xor c
        { 1, 4, 0, 0 }, // aa: xor d
        { 1, 4, 0, 0 }, // ab: xor e
        { 1, 4, 0, 0 }, // ac: xor h
        { 1, 4, 0, 0 }, // ad: xor l
        { 1, 7, 0, 0 }, // ae: xor (hl)
        { 1, 4, 0, 0 }, // af: xor a
        { 1, 4, 0, 0 }, // b0: or b
        { 1, 4, 0, 0 }, // b1: or c
        { 1, 4, 0, 0 }, // b2: or d
        { 1, 4, 0, 0 }, // b3: or e
        { 1, 4, 0, 0 }, // b4: or h
        { 1, 4, 0, 0 }, // b5: or l
        { 1, 7, 0, 0 }, // b6: or (hl)
        { 1, 4, 0, 0 }, // b7: or a
        { 1, 4, 0, 0 }, // b8: cp b
        { 1, 4, 0, 0 }, // b9: cp c
        { 1, 4, 0, 0 }, // ba: cp d
        { 1, 4, 0, 0 }, // bb: cp e
        { 1, 4, 0, 0 }, // bc: cp h
        { 1, 4, 0, 0 }, // bd: cp l
        { 1, 7, 0, 0 }, // be: cp (hl)
        { 1, 4, 0, 0 }, // bf: cp a
        { 1, -1, 0, 0 }, // c0: ret nz   (11/5)
        { 1, 10, 0, 0 }, // c1: pop bc
        { 3, 10, 0, 0 }, // c2: jp nz,**
        { 3, 10, 0, 0 }, // c3: jp **
        { 3, -1, 0, 0 }, // c4: call nz,** (17/10)
        { 1, 11, 0, 0 }, // c5: push bc
        { 2, 7, 0, 0 }, // c6: add a,*
        { 1, 11, 0, 0 }, // c7: rst 00h
        { 1, -1, 0, 0 }, // c8: ret z (11/5)
        { 1, 10, 0, 0 }, // c9: ret
        { 3, 10, 0, 0 }, // ca: jp z,**
        { -1, -1, 0, 0 }, // cb: BITS
        { 3, -1, 0, 0 }, // cc: call z,** (17/10)
        { 3, 17, 0, 0 }, // cd: call **
        { 2, 7, 0, 0 }, // ce: adc a,*
        { 1, 11, 0, 0 }, // cf: rst 08h
        { 1, -1, 0, 0 }, // d0: ret nc (11/5)
        { 1, 10, 0, 0 }, // d1: pop de
        { 3, 10, 0, 0 }, // d2: jp nc,**
        { 2, 11, 0, 0 }, // d3: out (*),a
        { 3, -1, 0, 0 }, // d4: call nc,** (17/10)
        { 1, 11, 0, 0 }, // d5: push de
        { 2, 7, 0, 0 }, // d6: sub *
        { 1, 11, 0, 0 }, // d7: rst 10h
        { 1, -1, 0, 0 }, // d8: ret c (11/5)
        { 1, 4, 0, 0 }, // d9: exx
        { 3, 10, 0, 0 }, // da: jp c,**
        { 2, 11, 0, 0 }, // db: in a,(*)
        { 3, -1, 0, 0 }, // dc: call c,** (17/10)
        { 1, 4, 0, 0 }, // dd: IX
        { 2, 7, 0, 0 }, // de: sbc a,*
        { 1, 11, 0, 0 }, // df: rst 18h
        { 1, -1, 0, 0 }, // e0: ret po (11/5)
        { 1, 10, 0, 0 }, // e1: pop hl
        { 3, 10, 0, 0 }, // e2: jp po,**
        { 1, 19, 0, 0 }, // e3: ex (sp),hl
        { 3, -1, 0, 0 }, // e4: call po,** (17/10)
        { 1, 11, 0, 0 }, // e5: push hl
        { 2, 7, 0, 0 }, // e6: and *
        { 1, 11, 0, 0 }, // e7: rst 20h
        { 1, -1, 0, 0 }, // e8: ret pe (11/5)
        { 1, 4, 0, 0 }, // e9: jp (hl)
        { 3, 10, 0, 0 }, // ea: jp pe,**
        { 1, 4, 0, 0 }, // eb: ex de,hl
        { 3, -1, 0, 0 }, // ec: call pe,** (17/10)
        { -1, -1, 0, 0 }, // ed: EXTD (unsupported to parse now)
        { 2, 7, 0, 0 }, // ee: xor *
        { 1, 11, 0, 0 }, // ef: rst 28h
        { 1, -1, 0, 0 }, // f0: ret p (11/5)
        { 1, 10, 0, 0 }, // f1: pop af
        { 3, 10, 0, 0 }, // f2: jp p,**
        { 1, 4, 0, 0 }, // f3: di
        { 3, -1, 0, 0 }, // f4: call p,** (17/10)
        { 1, 11, 0, 0 }, // f5: push af
        { 2, 7, 0, 0 }, // f6: or *
        { 1, 11, 0, 0 }, // f7: rst 30h
        { 1, -1, 0, 0 }, // f8: ret m (11/5)
        { 1, 6, 0, 0 }, // f9: ld sp,hl
        { 3, 10, 0, 0 }, // fa: jp m,**
        { 1, 4, 0, 0 }, // fb: ei
        { 3, -1, 0, 0 }, // fc: call m,** (17/10)
        { 1, 4, 0, 0 }, // fd: IY
        { 2, 7, 0, 0 }, // fe: cp *
        { 1, 11, 0, 0 } // ff: rst 38h
    };

    return commands[*ptr];
}

std::vector<uint8_t> CompressedLine::getFirstCommands(int size) const 
{
    std::vector<uint8_t> result;
    const uint8_t* ptr = data.buffer();
    while (result.size() < size)
    {
        int commandSize = parseZ80Command(ptr).size;
        for (int i = 0; i < commandSize; ++i)
            result.push_back(*ptr++);
    }
    return result;
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
        int commandSize = parseZ80Command(ptr).size;
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
