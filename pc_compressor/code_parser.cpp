#include "code_parser.h"

#include <iostream>


z80Command Z80Parser::parseCommand(const uint8_t* ptr)
{
    static std::vector<z80Command> commands =
    {
        { 1, 4, }, // 00: nop
        { 3, 10}, // 01: ld bc, **
        { 1, 7}, // 02: ld (bc),a
        { 1, 6}, // 03: inc bc
        { 1, 4}, // 04: inc b
        { 1, 4}, // 05: dec b
        { 2, 7}, // 06: ld b, *
        { 1, 4}, // 07: rlca
        { 1, 4}, // 08: ex af,af'
        { 1, 11}, // 09: add hl,bc
        { 1, 7}, // 0a: ld a,(bc)
        { 1, 6}, // 0b: dec bc
        { 1, 4}, // 0c: inc c
        { 1, 4}, // 0d: dec c
        { 2, 7}, // 0e: ld c,*
        { 1, 4}, // 0f: rrca
        { 2, -1}, // 10: djnz * (13/8). -1 means unknown ticks
        { 3, 10}, // 11: ld de,**
        { 1, 7}, // 12: ld (de),a
        { 1, 6}, // 13: inc de
        { 1, 4}, // 14: inc d
        { 1, 4}, // 15: dec d
        { 2, 7}, // 16: ld d,*
        { 1, 4}, // 17: rla
        { 2, 12}, // 18: jr *
        { 1, 11}, // 19: add hl,de
        { 1, 7}, // 1a: ld a,(de)
        { 1, 6}, // 1b: dec de
        { 1, 4}, // 1c: inc e
        { 1, 4}, // 1d: dec e
        { 2, 7}, // 1e: ld e,*
        { 1, 4}, // 1f: rra
        { 2, -1}, // 20: jr nz,* (12/7). -1 means unknown ticks
        { 3, 10}, // 21: ld hl,**
        { 3, 16}, // 22: ld (**),hl
        { 1, 6}, // 23: inc hl
        { 1, 4}, // 24: inc h
        { 1, 4}, // 25: dec h
        { 2, 7}, // 26: ld h,*
        { 1, 4}, // 27: daa
        { 2, -1}, // 28: jr z,* (12/7). -1 means unknown ticks
        { 1, 11}, // 29: add hl,hl
        { 3, 16}, // 2a: ld hl,(**)
        { 1, 6}, // 2b: dec hl
        { 1, 4}, // 2c: inc l
        { 1, 4}, // 2d: dec l
        { 2, 7}, // 2e: ld l,*
        { 1, 4}, // 2f: cpl
        { 2, -1}, // 30: jr nc,* (12/7). -1 means unknown ticks
        { 3, 10}, // 31: ld sp,**
        { 3, 13}, // 32: ld (**),a
        { 1, 6}, // 33: inc sp
        { 1, 11}, // 34: inc (hl)
        { 1, 11}, // 35: dec (hl)
        { 2, 10}, // 36: ld (hl),*
        { 1, 4}, // 37: scf
        { 2, -1}, // 38: jr c,* (12/7). -1 means unknown ticks
        { 1, 11}, // 39: add hl,sp
        { 3, 13}, // 3a: ld a,(**)
        { 1, 6}, // 3b: dec sp
        { 1, 4}, // 3c: inc a
        { 1, 4}, // 3d: dec a
        { 2, 7}, // 3e: ld a,*
        { 1, 4}, // 3f: ccf
        { 1, 4}, // 40: ld b,b
        { 1, 4}, // 41: ld b,c
        { 1, 4}, // 42: ld b,d
        { 1, 4}, // 43: ld b,e
        { 1, 4}, // 44: ld b,h
        { 1, 4}, // 45: ld b,l
        { 1, 7}, // 46: ld b,(hl)
        { 1, 4}, // 47: ld b,a
        { 1, 4}, // 48: ld c,b
        { 1, 4}, // 49: ld c,c
        { 1, 4}, // 4a: ld c,d
        { 1, 4}, // 4b: ld c,e
        { 1, 4}, // 4c: ld c,h
        { 1, 4}, // 4d: ld c,l
        { 1, 7}, // 4e: ld c,(hl)
        { 1, 4}, // 4f: ld c,a
        { 1, 4}, // 50: ld d,b
        { 1, 4}, // 51: ld d,c
        { 1, 4}, // 52: ld d,d
        { 1, 4}, // 53: ld d,e
        { 1, 4}, // 54: ld d,h
        { 1, 4}, // 55: ld d,l
        { 1, 7}, // 56: ld d,(hl)
        { 1, 4}, // 57: ld d,a
        { 1, 4}, // 58: ld e,b
        { 1, 4}, // 59: ld e,c
        { 1, 4}, // 5a: ld e,d
        { 1, 4}, // 5b: ld e,e
        { 1, 4}, // 5c: ld e,h
        { 1, 4}, // 5d: ld e,l
        { 1, 7}, // 5e: ld e,(hl)
        { 1, 4}, // 5f: ld e,a
        { 1, 4}, // 60: ld h,b
        { 1, 4}, // 61: ld h,c
        { 1, 4}, // 62: ld h,d
        { 1, 4}, // 63: ld h,e
        { 1, 4}, // 64: ld h,h
        { 1, 4}, // 65: ld h,l
        { 1, 7}, // 66: ld h,(hl)
        { 1, 4}, // 67: ld h,a
        { 1, 4}, // 68: ld l,b
        { 1, 4}, // 69: ld l,c
        { 1, 4}, // 6a: ld l,d
        { 1, 4}, // 6b: ld l,e
        { 1, 4}, // 6c: ld l,h
        { 1, 4}, // 6d: ld l,l
        { 1, 7}, // 6e: ld l,(hl)
        { 1, 4}, // 6f: ld l,a
        { 1, 7}, // 70: ld (hl),b
        { 1, 7}, // 71: ld (hl),c
        { 1, 7}, // 72: ld (hl),d
        { 1, 7}, // 73: ld (hl),e
        { 1, 7}, // 74: ld (hl),h
        { 1, 7}, // 75: ld (hl),l
        { 1, 4}, // 76: halt
        { 1, 7}, // 77: ld (hl),a
        { 1, 4}, // 78: ld a,b
        { 1, 4}, // 79: ld a,c
        { 1, 4}, // 7a: ld a,d
        { 1, 4}, // 7b: ld a,e
        { 1, 4}, // 7c: ld a,h
        { 1, 4}, // 7d: ld a,l
        { 1, 7}, // 7e: ld a,(hl)
        { 1, 4}, // 7f: ld a,a
        { 1, 4}, // 80: add a,b
        { 1, 4}, // 81: add a,c
        { 1, 4}, // 82: add a,d
        { 1, 4}, // 83: add a,e
        { 1, 4}, // 84: add a,h
        { 1, 4}, // 85: add a,l
        { 1, 7}, // 86: add a,(hl)
        { 1, 4}, // 87: add a,a
        { 1, 4}, // 88: add a,b
        { 1, 4}, // 89: add a,c
        { 1, 4}, // 8a: add a,d
        { 1, 4}, // 8b: add a,e
        { 1, 4}, // 8c: adc a,h
        { 1, 4}, // 8d: adc a,l
        { 1, 7}, // 8e: adc a,(hl)
        { 1, 4}, // 8f: adc a,a
        { 1, 4}, // 90: sub b
        { 1, 4}, // 91: sub c
        { 1, 4}, // 92: sub d
        { 1, 4}, // 93: sub e
        { 1, 4}, // 94: sub h
        { 1, 4}, // 95: sub l
        { 1, 7}, // 96: sub (hl)
        { 1, 4}, // 97: sub a
        { 1, 4}, // 98: sbc a,b
        { 1, 4}, // 99: sbc a,c
        { 1, 4}, // 9a: sbc a,d
        { 1, 4}, // 9b: sbc a,e
        { 1, 4}, // 9c: sbc a,h
        { 1, 4}, // 9d: sbc a,l
        { 1, 7}, // 9e: sbc a,(hl)
        { 1, 4}, // 9f: sbc a,a
        { 1, 4}, // a0: and b
        { 1, 4}, // a1: and c
        { 1, 4}, // a2: and d
        { 1, 4}, // a3: and e
        { 1, 4}, // a4: and h
        { 1, 4}, // a5: and l
        { 1, 7}, // a6: and (hl)
        { 1, 4}, // a7: and a
        { 1, 4}, // a8: xor b
        { 1, 4}, // a9: xor c
        { 1, 4}, // aa: xor d
        { 1, 4}, // ab: xor e
        { 1, 4}, // ac: xor h
        { 1, 4}, // ad: xor l
        { 1, 7}, // ae: xor (hl)
        { 1, 4}, // af: xor a
        { 1, 4}, // b0: or b
        { 1, 4}, // b1: or c
        { 1, 4}, // b2: or d
        { 1, 4}, // b3: or e
        { 1, 4}, // b4: or h
        { 1, 4}, // b5: or l
        { 1, 7}, // b6: or (hl)
        { 1, 4}, // b7: or a
        { 1, 4}, // b8: cp b
        { 1, 4}, // b9: cp c
        { 1, 4}, // ba: cp d
        { 1, 4}, // bb: cp e
        { 1, 4}, // bc: cp h
        { 1, 4}, // bd: cp l
        { 1, 7}, // be: cp (hl)
        { 1, 4}, // bf: cp a
        { 1, -1}, // c0: ret nz   (11/5)
        { 1, 10}, // c1: pop bc
        { 3, 10}, // c2: jp nz,**
        { 3, 10}, // c3: jp **
        { 3, -1}, // c4: call nz,** (17/10)
        { 1, 11}, // c5: push bc
        { 2, 7}, // c6: add a,*
        { 1, 11}, // c7: rst 00h
        { 1, -1}, // c8: ret z (11/5)
        { 1, 10}, // c9: ret
        { 3, 10}, // ca: jp z,**
        { -1, -1}, // cb: BITS
        { 3, -1}, // cc: call z,** (17/10)
        { 3, 17}, // cd: call **
        { 2, 7}, // ce: adc a,*
        { 1, 11}, // cf: rst 08h
        { 1, -1}, // d0: ret nc (11/5)
        { 1, 10}, // d1: pop de
        { 3, 10}, // d2: jp nc,**
        { 2, 11}, // d3: out (*),a
        { 3, -1}, // d4: call nc,** (17/10)
        { 1, 11}, // d5: push de
        { 2, 7}, // d6: sub *
        { 1, 11}, // d7: rst 10h
        { 1, -1}, // d8: ret c (11/5)
        { 1, 4}, // d9: exx
        { 3, 10}, // da: jp c,**
        { 2, 11}, // db: in a,(*)
        { 3, -1}, // dc: call c,** (17/10)
        { 1, 4}, // dd: IX
        { 2, 7}, // de: sbc a,*
        { 1, 11}, // df: rst 18h
        { 1, -1}, // e0: ret po (11/5)
        { 1, 10}, // e1: pop hl
        { 3, 10}, // e2: jp po,**
        { 1, 19}, // e3: ex (sp),hl
        { 3, -1}, // e4: call po,** (17/10)
        { 1, 11}, // e5: push hl
        { 2, 7}, // e6: and *
        { 1, 11}, // e7: rst 20h
        { 1, -1}, // e8: ret pe (11/5)
        { 1, 4}, // e9: jp (hl)
        { 3, 10}, // ea: jp pe,**
        { 1, 4}, // eb: ex de,hl
        { 3, -1}, // ec: call pe,** (17/10)
        { -1, -1}, // ed: EXTD (unsupported to parse now)
        { 2, 7}, // ee: xor *
        { 1, 11}, // ef: rst 28h
        { 1, -1}, // f0: ret p (11/5)
        { 1, 10}, // f1: pop af
        { 3, 10}, // f2: jp p,**
        { 1, 4}, // f3: di
        { 3, -1}, // f4: call p,** (17/10)
        { 1, 11}, // f5: push af
        { 2, 7}, // f6: or *
        { 1, 11}, // f7: rst 30h
        { 1, -1}, // f8: ret m (11/5)
        { 1, 6}, // f9: ld sp,hl
        { 3, 10}, // fa: jp m,**
        { 1, 4}, // fb: ei
        { 3, -1}, // fc: call m,** (17/10)
        { 1, 4}, // fd: IY
        { 2, 7}, // fe: cp *
        { 1, 11} // ff: rst 38h
    };

    auto result = commands[*ptr];
    result.opCode = *ptr;
    return result;
}

Z80CodeInfo Z80Parser::parseCode(const Register16& af, const std::vector<uint8_t>& serializedData)
{
    return parseCode(af, serializedData.data(), serializedData.size());
}

Z80CodeInfo Z80Parser::parseCode(const Register16& af, const uint8_t* buffer, int size)
{
    std::vector<Register16> registers = { Register16("bc"), Register16("de"), Register16("hl") };
    for (auto& reg : registers)
        reg.setValue(0); //< Make registers non-empty to correct update regUseMask
    return parseCode(
        af,
        registers,
        buffer,
        size,
        /*parse start*/ 0,
        /*parse end*/ size,
        /*code offset for JP command*/ 0);
}

Z80CodeInfo Z80Parser::parseCode(
    const Register16& af,
    const std::vector<Register16>& inputRegisters,
    const std::vector<uint8_t>& serializedData,
    int startOffset,
    int endOffset,
    uint16_t codeOffset,
    BreakCondition breakCondition)
{
    return parseCode(af, inputRegisters,
        serializedData.data(), serializedData.size(),
        startOffset, endOffset, codeOffset, breakCondition);
}


Z80CodeInfo Z80Parser::parseCode(
    const Register16& af,
    const std::vector<Register16>& inputRegisters,
    const uint8_t* serializedData,
    const int serializedDataSize,
    int startOffset,
    int endOffset,
    uint16_t codeOffset,
    BreakCondition breakCondition)
{
    Z80CodeInfo result;
    RegUsageInfo info;

    result.inputRegisters = inputRegisters;
    result.outputRegisters = result.inputRegisters;
    result.startOffset = startOffset;

    Register16 ix{ "ix" };
    Register16 iy{ "iy" };

    Register16* bc = findRegister(result.outputRegisters, "bc");
    Register16* de = findRegister(result.outputRegisters, "de");

    Register16* hl = findRegister(result.outputRegisters, "hl");
    Register8& h = hl->h;
    Register8& l = hl->l;

    Register8& b = bc->h;
    Register8& c = bc->l;
    Register8& d = de->h;
    Register8& e = de->l;


    Register8 a = af.h;

    result.bufferBegin = serializedData;
    result.bufferEnd = serializedData + serializedDataSize;
    const uint8_t* ptr = serializedData + startOffset;
    const uint8_t* end = serializedData + endOffset;
    result.iySpOffset = 16; //< delta IY offset to initial offset

    auto push =
        [&](const Register16* reg)
        {
            reg->push(info);
            result.spOffset -= 2;
    };

    bool isIndexReg = false;

    while (ptr != end)
    {
        if (ptr < result.bufferBegin || ptr >= result.bufferEnd)
            break;

        auto command = parseCommand(ptr);
        command.ptr = ptr - result.bufferBegin;
        //if (result.ticks + command.ticks > maxTicks)
        if (breakCondition && breakCondition(result, command))
            break;
        result.ticks += command.ticks;
        result.commands.push_back(command);

        switch (*ptr)
        {
            case 0x00:
                break;  // NOP. Nothing to do
            case 0x01: bc->loadXX(info, ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x03: bc->incValue(info);
                break;
            case 0x04: b.incValue(info);
                break;
            case 0x05: b.decValue(info);
                break;
            case 0x06: b.loadX(info, ptr[1]);
                break;
            case 0x08: // ex af, af'
                break;
            case 0x10: // djnz
                break;
            case 0x0b: bc->decValue(info);
                break;
            case 0x0c: c.incValue(info);
                break;
            case 0x0d: c.decValue(info);
                break;
            case 0x0e: c.loadX(info, ptr[1]);
                break;
            case 0x11: de->loadXX(info, ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x13: de->incValue(info);
                break;
            case 0x14: d.incValue(info);
                break;
            case 0x15: d.decValue(info);
                break;
            case 0x16: d.loadX(info, ptr[1]);
                break;
            case 0x1b: de->decValue(info);
                break;
            case 0x1c: e.incValue(info);
                break;
            case 0x1d: e.decValue(info);
                break;
            case 0x1e: e.loadX(info, ptr[1]);
                break;
            case 0x21: hl->loadXX(info, ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x23: hl->incValue(info);
                break;
            case 0x24: h.incValue(info);
                break;
            case 0x25: h.decValue(info);
                break;
            case 0x26: h.loadX(info, ptr[1]);
                break;
            case 0x2b:
                if (isIndexReg)
                    result.iySpOffset--;
                else
                    hl->decValue(info);
                break;
            case 0x2c: l.incValue(info);
                break;
            case 0x2d: l.decValue(info);
                break;
            case 0x2e: l.loadX(info, ptr[1]);
                break;
            case 0x29: // ADD HL, HL
                break;
            case 0x33: result.spOffset++;    // incSP
                break;
            case 0x34: hl->incValue(info);
                break;
            case 0x37: // scf
                break;
            case 0x39:
            {
                // ADD HL, SP. The current value is not known after this
                int16_t delta = (int16_t) hl->value16();
                result.spOffset += delta;
                info.useReg(h, l);
                hl->reset();
                break;
            }
            case 0x3b:
                    result.spOffset--;  // DEC SP
                break;
            case 0x3c: a.incValue(info);
                break;
            case 0x3d: a.decValue(info);
                break;
            case 0x3e: a.loadX(info, ptr[1]);
                break;
            case 0x40: b.loadFromReg(info, b);
                break;
            case 0x41: b.loadFromReg(info, c);
                break;
            case 0x42: b.loadFromReg(info, d);
                break;
            case 0x43: b.loadFromReg(info, e);
                break;
            case 0x44: b.loadFromReg(info, h);
                break;
            case 0x45: b.loadFromReg(info, l);
                break;
            case 0x47: b.loadFromReg(info, a);
                break;
            case 0x48: c.loadFromReg(info, b);
                break;
            case 0x49: c.loadFromReg(info, c);
                break;
            case 0x4a: c.loadFromReg(info, d);
                break;
            case 0x4b: c.loadFromReg(info, e);
                break;
            case 0x4c: c.loadFromReg(info, h);
                break;
            case 0x4d: c.loadFromReg(info, l);
                break;
            case 0x4f: c.loadFromReg(info, a);
                break;
            case 0x50: d.loadFromReg(info, b);
                break;
            case 0x51: d.loadFromReg(info, c);
                break;
            case 0x52: d.loadFromReg(info, d);
                break;
            case 0x53: d.loadFromReg(info, e);
                break;
            case 0x54: d.loadFromReg(info, h);
                break;
            case 0x55: d.loadFromReg(info, l);
                break;
            case 0x57: d.loadFromReg(info, a);
                break;
            case 0x58: e.loadFromReg(info, b);
                break;
            case 0x59: e.loadFromReg(info, c);
                break;
            case 0x5a: e.loadFromReg(info, d);
                break;
            case 0x5b: e.loadFromReg(info, e);
                break;
            case 0x5c: e.loadFromReg(info, h);
                break;
            case 0x5d: e.loadFromReg(info, l);
                break;
            case 0x5f: e.loadFromReg(info, a);
                break;
            case 0x60: h.loadFromReg(info, b);
                break;
            case 0x61: h.loadFromReg(info, c);
                break;
            case 0x62: h.loadFromReg(info, d);
                break;
            case 0x63: h.loadFromReg(info, e);
                break;
            case 0x64: h.loadFromReg(info, h);
                break;
            case 0x65: h.loadFromReg(info, l);
                break;
            case 0x67: h.loadFromReg(info, a);
                break;
            case 0x68: l.loadFromReg(info, b);
                break;
            case 0x69: l.loadFromReg(info, c);
                break;
            case 0x6a: l.loadFromReg(info, d);
                break;
            case 0x6b: l.loadFromReg(info, e);
                break;
            case 0x6c: l.loadFromReg(info, h);
                break;
            case 0x6d: l.loadFromReg(info, l);
                break;
            case 0x6e: // LD L, (HL)
                break;
            case 0x6f: l.loadFromReg(info, a);
                break;
            case 0x78: a.loadFromReg(info, b);
                break;
            case 0x79: a.loadFromReg(info, c);
                break;
            case 0x7a: a.loadFromReg(info, d);
                break;
            case 0x7b: a.loadFromReg(info, e);
                break;
            case 0x7c: a.loadFromReg(info, h);
                break;
            case 0x7d: a.loadFromReg(info, l);
                break;
            case 0x7f: a.loadFromReg(info, a);
                break;
            case 0x80: a.addReg(info, b);
                break;
            case 0x81: a.addReg(info, c);
                break;
            case 0x82: a.addReg(info, d);
                break;
            case 0x83: a.addReg(info, e);
                break;
            case 0x84: a.addReg(info, h);
                break;
            case 0x85: a.addReg(info, l);
                break;
            case 0x87: a.addReg(info, a);
                break;
            case 0x88: a.addReg(info, b);
                break;
            case 0x89: a.addReg(info, c);
                break;
            case 0x8a: a.addReg(info, d);
                break;
            case 0x8b: a.addReg(info, e);
                break;
            case 0x90: a.subReg(info, b);
                break;
            case 0x91: a.subReg(info, c);
                break;
            case 0x92: a.subReg(info, d);
                break;
            case 0x93: a.subReg(info, e);
                break;
            case 0x94: a.subReg(info, h);
                break;
            case 0x95: a.subReg(info, l);
                break;
            case 0x97: a.subReg(info, a);
                break;
            case 0xa0: a.andReg(info, b);
                break;
            case 0xa1: a.andReg(info, c);
                break;
            case 0xa2: a.andReg(info, d);
                break;
            case 0xa3: a.andReg(info, e);
                break;
            case 0xa4: a.andReg(info, h);
                break;
            case 0xa5: a.andReg(info, l);
                break;
            case 0xa7: a.andReg(info, a);
                break;
            case 0xa8: a.xorReg(info, b);
                break;
            case 0xa9: a.xorReg(info, c);
                break;
            case 0xaa: a.xorReg(info, d);
                break;
            case 0xab: a.xorReg(info, e);
                break;
            case 0xac: a.xorReg(info, h);
                break;
            case 0xad: a.xorReg(info, l);
                break;
            case 0xaf: a.xorReg(info, a);
                break;
            case 0xb0: a.orReg(info, b);
                break;
            case 0xb1: a.orReg(info, c);
                break;
            case 0xb2: a.orReg(info, d);
                break;
            case 0xb3: a.orReg(info, e);
                break;
            case 0xb4: a.orReg(info, h);
                break;
            case 0xb5: a.orReg(info, l);
                break;
            case 0xb7: a.orReg(info, a);
                break;
            case 0xc3:
            {
                // jp **
                uint16_t jumpTo = ptr[1] + ((uint16_t)ptr[2] << 8);
                jumpTo -= codeOffset;
                ptr = serializedData + jumpTo;
                result.hasJump = true;
                continue;
            }
            case 0xc5: // push BC
                push(bc);
                break;
            case 0xd0: // ret nc
                break;
            case 0xd5: // push de
                push(de);
                break;
            case 0xe5: // push hl
                push(hl);
                break;
            case 0xf5: // push af
                push(&af);
                break;
            case 0xc6: a.addValue(info, ptr[1]);
                break;
            case 0xd6: a.subValue(info, ptr[1]);
                break;
            case 0xd9: // exx
                break;
            case 0xe6: a.andValue(info, ptr[1]);
                break;
            case 0xee: a.xorValue(info, ptr[1]);
                break;
            case 0xf6: a.orValue(info, ptr[1]);
                break;
            case 0xf9: // LD SP, HL
                if (isIndexReg)
                    result.spOffset = result.iySpOffset;
                else
                    info.useReg(h, l);
                break;
            case 0xdd: // IX
            case 0xfd: // IY
                isIndexReg = true;
                ptr += command.size;
                continue;
            default:
                // Unsopported command
                assert(0);
                std::cerr << "Unsupported op code " << std::hex << (int) *ptr << " to parse" << std::endl;
                abort();
        }
        isIndexReg = false;
        ptr += command.size;
    }

    result.regUsage = info;
    result.endOffset = ptr - serializedData;

    return result;
}


RegUsageInfo Z80Parser::regUsageByCommand(const z80Command& command)
{
    static Register16 bc("bc");
    static Register16 de("de");
    static Register16 hl("hl");

    RegUsageInfo regUsage;

    switch (command.opCode)
    {
        case 0x01:  // LD BC, xx
            regUsage.selfReg(bc.h, bc.l);
            break;
        case 0x11:  // LD DE, xx
            regUsage.selfReg(de.h, de.l);
            break;
        case 0x21:  // LD HL, xx
            regUsage.selfReg(hl.h, hl.l);
            break;
        case 0x06:  // LD B, x
            regUsage.selfReg(bc.h);
            break;
        case 0x0e:  // LD C, x
            regUsage.selfReg(bc.l);
            break;
        case 0x16:  // LD D, x
            regUsage.selfReg(de.h);
            break;
        case 0x1e:  // LD E, x
            regUsage.selfReg(de.l);
            break;
        case 0x26:  // LD H, x
            regUsage.selfReg(hl.h);
            break;
        case 0x2e:  // LD L, x
            regUsage.selfReg(hl.l);
            break;
        default:
            break;
    }
    return regUsage;
}

std::vector<uint8_t> Z80Parser::getCode(const uint8_t* buffer, int requestedOpCodeSize)
{
    std::vector<uint8_t> result;
    if (requestedOpCodeSize <= 0)
        return result;

    const uint8_t* ptr = buffer;
    while (result.size() < requestedOpCodeSize)
    {
        int commandSize = Z80Parser::parseCommand(ptr).size;
        if (commandSize < 1)
        {
            std::cerr << "Invalid command size " << commandSize << ". opCode= " << (int)*ptr << std::endl;
            abort();
        }
        for (int i = 0; i < commandSize; ++i)
            result.push_back(*ptr++);
    }
    return result;
}

std::vector<uint8_t> Z80Parser::genDelay(int ticks)
{
    std::vector<uint8_t> result;

    if (ticks <= 0)
        return result;

    if (ticks >= 32)
    {
        int count = (ticks - 6) / 13;
        result.push_back(0x06); // LD B,*
        result.push_back(count);
        result.push_back(0x10); // DJNZ
        result.push_back((int8_t) -2); // DJNZ
        ticks -= 7 + (count-1) * 13 + 8;
    }

    bool hasAddHl = false;
    while (ticks > 18)
    {
        ticks -= 11;
        result.push_back(0x29); // ADD HL, HL
        hasAddHl = true;
    }
    if (hasAddHl)
    {
        ticks -= 4;
        result.push_back(0x37); // SCF
    }

    while (ticks > 10)
    {
        ticks -= 7;
        result.push_back(0x6e); // LD L, (HL)

    }

    if (ticks > 7)
    {
        ticks -= 4;
        result.push_back(0x00); // NOP

    }
    switch (ticks)
    {
        case 7:
            result.push_back(0x6e); // LD L, (HL)
            break;
        case 6:
            result.push_back(0x23); // INC HL
            break;
        case 5:
            result.push_back(0xd0); // RET NC
            break;
        case 4:
            result.push_back(0x00); // NOP
            break;
        case 0:
            break;
        default:
            std::cerr << "Invalid ticks amount to align:" << ticks << std::endl;
            assert(0);
            abort();
    }

    return result;
}

void swapCommands(uint8_t* buffer, z80Command command1, z80Command command2)
{
    std::vector<uint8_t> tmp(command1.size);
    memcpy(tmp.data(), buffer, command1.size);
    memmove(buffer, buffer + command1.size, command2.size);
    memcpy(buffer + command2.size, tmp.data(), command1.size);
}

bool isBetween(int start, int value, int end)
{
    return value >= start && value < end;
}

bool isIntersect(const std::pair<int, int>& block1,
    const std::pair<int, int>& block2)
{
    return isBetween(block1.first, block2.first, block1.first + block1.second)
        || isBetween(block1.first, block2.first + block2.second, block1.first + block1.second);
}

int Z80Parser::swap2CommandIfNeed(
    std::vector<uint8_t>& serializedData,
    int startOffset,
    std::vector<std::pair<int, int>>& lockedBlocks)
{
    uint8_t* ptr = serializedData.data() + startOffset;
    uint8_t* end = serializedData.data() + serializedData.size();
    if (ptr == end)
        return 0;
    z80Command command1 = parseCommand(ptr);
    if (command1.size + ptr == end)
        return 0;

    z80Command command2 = parseCommand(ptr + command1.size);
    bool isLocked = false;
    const std::pair<int, int> newBlock = { startOffset, command1.size + command2.size };
    for (const auto& lock: lockedBlocks)
    {
        if (isIntersect(lock, newBlock))
            isLocked = true;
    }

    lockedBlocks.push_back(newBlock);
    if (isLocked)
        return 0;


    if (command1.size > 1 || command2.size == 1)
        return 0; //< Nothing to optimize.

    /**
     * Do not swap such commands.If it is first command for the second block (offscreen rastr)
     * then SP can't get correct value from HL. Fortunately, this command is omitted by descriptor
     * preambula. So, do not touch it.
     */
    if (command1.opCode == kLdSpHlCode)
        return 0;

    const Register16 af("af");
    auto info1 = parseCode(af, ptr, command1.size);
    auto info2 = parseCode(af, ptr + command1.size, command2.size);

    if (info2.hasJump)
        return 0;

    auto mask1 = info1.regUsage.regUseMask | info1.regUsage.selfRegMask;
    auto mask2 = info2.regUsage.regUseMask | info2.regUsage.selfRegMask;
    if ((mask1 & mask2) == 0)
        swapCommands(ptr, command1, command2);


    return command1.size + command2.size;
}

const Register16* Z80Parser::findRegByItsPushOpCode(
    const std::vector<Register16>& registers, uint8_t pushOpCode)
{
    if (pushOpCode == 0xc5)
        return findRegister(registers, "bc");
    else if (pushOpCode == 0xd5)
        return findRegister(registers, "de");
    else if (pushOpCode == 0xe5)
        return findRegister(registers, "hl");
    else if (pushOpCode == 0xf5)
        return findRegister(registers, "af");
    return nullptr;
}

int Z80Parser::removeTrailingStackMoving(Z80CodeInfo& codeInfo, int maxCommandToRemove)
{
    int removedBytes = 0;
    if (maxCommandToRemove == 0)
        return removedBytes;

    while (!codeInfo.commands.empty() && codeInfo.commands.rbegin()->opCode == kDecSpCode)
    {
        ++codeInfo.spOffset;
        codeInfo.ticks -= 6;
        codeInfo.commands.pop_back();
        ++removedBytes;
        codeInfo.endOffset--;
        if (--maxCommandToRemove == 0)
            return removedBytes;
    }

    if (int index = codeInfo.commands.size() - 2;
        codeInfo.commands.size() >= 2 && codeInfo.commands[index].opCode == kAddHlSpCode && codeInfo.commands[index + 1].opCode == kLdSpHlCode)
    {
        auto hl = findRegister(codeInfo.outputRegisters, "hl");
        int16_t spDelta = -(int16_t)hl->value16();
        hl->reset();

        codeInfo.spOffset += spDelta;
        codeInfo.ticks -= 17;
        codeInfo.endOffset -= 2;
        codeInfo.commands.pop_back();
        codeInfo.commands.pop_back();
        removedBytes += 2;
        if (--maxCommandToRemove == 0)
            return removedBytes;
    }

    return removedBytes;
}

uint16_t Z80Parser::removeTrailingCommands(Z80CodeInfo& codeInfo, int commandToRemove)
{
    uint16_t result = 0;

    while (!codeInfo.commands.empty() && commandToRemove > 0)
    {
        auto command = codeInfo.commands.rbegin();
        codeInfo.ticks -= command->ticks;
        codeInfo.endOffset -= command->size;
        result = command->ptr;
        codeInfo.commands.pop_back();
        --commandToRemove;
    }

    return result;
}

int Z80Parser::removeStartStackMoving(Z80CodeInfo& codeInfo)
{
    int removedBytes = 0;
    while (!codeInfo.commands.empty() && codeInfo.commands[0].opCode == kDecSpCode)
    {
        ++codeInfo.spOffset;
        codeInfo.ticks -= 6;
        codeInfo.commands.erase(codeInfo.commands.begin());
        ++removedBytes;
        codeInfo.startOffset++;
    }

    if (codeInfo.commands.size() >= 2 && codeInfo.commands[0].opCode == kAddHlSpCode && codeInfo.commands[1].opCode == kLdSpHlCode)
    {
        auto hl = findRegister(codeInfo.inputRegisters, "hl");
        int16_t spDelta = -(int16_t)hl->value16();
        codeInfo.spOffset += spDelta;
        hl->reset();

        codeInfo.ticks -= 17;
        codeInfo.startOffset += 2;
        codeInfo.commands.erase(codeInfo.commands.begin(), codeInfo.commands.begin() + 1);
        removedBytes += 2;
    }
    else if (codeInfo.commands.size() >= 1 && codeInfo.commands[0].opCode == kLdSpHlCode)
    {
        ++codeInfo.spOffset;
        codeInfo.startOffset++;
        codeInfo.ticks -= 6;
        codeInfo.commands.erase(codeInfo.commands.begin());
        removedBytes++;
    }
    return removedBytes;
}

CompressedLine serializeAdHlSp(int value)
{
    CompressedLine line;
    static Register16 sp("sp");
    static Register16 hl("hl");
    if (value >= 5)
    {
        hl.loadXX(line, -value);
        hl.addSP(line);
        sp.loadFromReg16(line, hl);
        if (value < 0)
            line.scf(); //< Keep flag 'c' on during multicolors
    }
    else
    {
        sp.decValue(line, value);
    }
    return line;
}

void Z80Parser::serializeAddSpToFront(CompressedLine& line, int value)
{
    auto temp = serializeAdHlSp(value);
    line.push_front(temp.data);
    line.drawTicks += temp.drawTicks;
};

void Z80Parser::serializeAddSpToBack(CompressedLine& line, int value)
{
    auto temp = serializeAdHlSp(value);
    line += temp;
};
