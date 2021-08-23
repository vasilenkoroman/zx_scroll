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
        { 1, 11}, // 1b: dec de
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

    return commands[*ptr];
}

Z80CodeInfo Z80Parser::parseCodeToTick(
    const std::vector<Register16>& inputRegisters,
    const std::vector<uint8_t>& serializedData,
    int startOffset,
    int endOffset,
    uint16_t codeOffset,
    int maxTicks)
{
    Z80CodeInfo result;
    RegUsageInfo info;

    auto registers = inputRegisters;
    result.inputRegisters = inputRegisters;
    result.startOffset = startOffset;

    Register16* bc = findRegister(registers, "bc");
    Register16* de = findRegister(registers, "de");
    Register16* hl = findRegister(registers, "hl");
    Register16* af = findRegister(registers, "af");

    Register8& b = bc->h;
    Register8& c = bc->l;
    Register8& d = de->h;
    Register8& e = de->l;
    Register8& h = hl->h;
    Register8& l = hl->l;
    Register8& a = af->h;


    const uint8_t* ptr = serializedData.data() + startOffset;
    const uint8_t* end = serializedData.data() + endOffset;

    auto push =
        [&](const Register16* reg)
        {
            reg->push(info);
            result.spDelta += 2;
    };

    while (result.ticks < maxTicks && ptr != end)
    {
        auto command = parseCommand(ptr);
        if (result.ticks + command.ticks > maxTicks)
            break;
        result.ticks += command.ticks;

        switch (*ptr)
        {
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
            case 0x1b: de->incValue(info);
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
            case 0x2b: hl->decValue(info);
                break;
            case 0x2c: l.incValue(info);
                break;
            case 0x2d: l.decValue(info);
                break;
            case 0x2e: l.loadX(info, ptr[1]);
                break;
            case 0x33: result.spDelta--;    // incSP
                break;
            case 0x34: hl->incValue(info);
                break;
            case 0x39:
            {
                // ADD HL, SP. The current value is not known after this
                result.spDelta -= (int16_t)hl->value16();
                info.useReg(h, l);
                hl->reset();
                break;
            }
            case 0x3b: result.spDelta++;
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
                ptr = serializedData.data() + jumpTo;
                result.ticks += command.ticks;
                continue;
            }
            case 0xc5: // push BC
                push(bc);
                break;
            case 0xd5: // push de
                push(de);
                break;
            case 0xe5: // push hl
                push(hl);
                break;
            case 0xf5: // push af
                push(af);
                break;
            case 0xc6: a.addValue(info, ptr[1]);
                break;
            case 0xd6: a.subValue(info, ptr[1]);
                break;
            case 0xe6: a.andValue(info, ptr[1]);
                break;
            case 0xee: a.xorValue(info, ptr[1]);
                break;
            case 0xf6: a.orValue(info, ptr[1]);
                break;
            case 0xf9: // LD SP, HL
                break;
            default:
                // Unsopported command
                assert(0);
        }
        ptr += command.size;
    }

    result.outputRegisters = registers;
    result.regUsage = info;
    result.endOffset = ptr - serializedData.data();

    return result;
}


std::vector<uint8_t> Z80Parser::getCode(const uint8_t* buffer, int requestedOpCodeSize)
{
    std::vector<uint8_t> result;
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
