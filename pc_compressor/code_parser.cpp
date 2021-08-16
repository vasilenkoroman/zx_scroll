#include "code_parser.h"

z80Command Z80Parser::parseCommand(const uint8_t* ptr)
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

ParseResult Z80Parser::parseCodeToTick(
    const std::vector<Register16>& inputRegisters,
    const std::vector<uint8_t>& serializedData,
    int offsetToParse,
    uint16_t codeOffset,
    int ticks)
{
    ParseResult result;
    result.registers = inputRegisters;

    Register16* bc = findRegister(result.registers, "bc");
    Register16* de = findRegister(result.registers, "de");
    Register16* hl = findRegister(result.registers, "hl");
    Register16* af = findRegister(result.registers, "af");

    Register8& b = bc->h;
    Register8& c = bc->l;
    Register8& d = de->h;
    Register8& e = de->l;
    Register8& h = hl->h;
    Register8& l = hl->l;
    Register8& a = af->h;


    const uint8_t* ptr = serializedData.data() + offsetToParse;
    while (result.ticks < ticks)
    {
        auto command = parseCommand(ptr);
        result.ticks += command.ticks;

        switch (*ptr)
        {
            case 0x01: bc->setValue(ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x03: bc->incValue();
                break;
            case 0x04: b.incValue();
                break;
            case 0x05: b.decValue();
                break;
            case 0x06: b.setValue(ptr[1]);
                break;
            case 0x0b: bc->decValue();
                break;
            case 0x0c: c.incValue();
                break;
            case 0x0d: c.decValue();
                break;
            case 0x0e: c.setValue(ptr[1]);
                break;
            case 0x11: de->setValue(ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x13: de->incValue();
                break;
            case 0x14: d.incValue();
                break;
            case 0x15: d.decValue();
                break;
            case 0x16: d.setValue(ptr[1]);
                break;
            case 0x19: hl->addValue(de->value16());
                break;
            case 0x1b: de->incValue();
                break;
            case 0x1c: e.incValue();
                break;
            case 0x1d: e.decValue();
                break;
            case 0x1e: e.setValue(ptr[1]);
                break;
            case 0x21: hl->setValue(ptr[1] + ((uint16_t)ptr[2] << 8));
                break;
            case 0x23: hl->incValue();
                break;
            case 0x24: h.incValue();
                break;
            case 0x25: h.decValue();
                break;
            case 0x26: h.setValue(ptr[1]);
                break;
            case 0x29: hl->addValue(hl->value16());
                break;
            case 0x2b: hl->decValue();
                break;
            case 0x2c: l.incValue();
                break;
            case 0x2d: l.decValue();
                break;
            case 0x2e: l.setValue(ptr[1]);
                break;
            case 0x33: result.spDelta--;
                break;
            case 0x34: hl->incValue();
                break;
            case 0x39:
            {
                // ADD HL, SP. The current value is not known after this
                result.spDelta -= (int16_t)hl->value16();
                hl->reset();
                break;
            }
            case 0x3b: result.spDelta++;
                break;
            case 0x3c: a.incValue();
                break;
            case 0x3d: a.decValue();
                break;
            case 0x3e: a.setValue(ptr[1]);
                break;
            case 0x40: b.loadFromReg(b);
                break;
            case 0x41: b.loadFromReg(c);
                break;
            case 0x42: b.loadFromReg(d);
                break;
            case 0x43: b.loadFromReg(e);
                break;
            case 0x44: b.loadFromReg(h);
                break;
            case 0x45: b.loadFromReg(l);
                break;
            case 0x47: b.loadFromReg(a);
                break;
            case 0x48: c.loadFromReg(b);
                break;
            case 0x49: c.loadFromReg(c);
                break;
            case 0x4a: c.loadFromReg(d);
                break;
            case 0x4b: c.loadFromReg(e);
                break;
            case 0x4c: c.loadFromReg(h);
                break;
            case 0x4d: c.loadFromReg(l);
                break;
            case 0x4f: c.loadFromReg(a);
                break;
            case 0x50: d.loadFromReg(b);
                break;
            case 0x51: d.loadFromReg(c);
                break;
            case 0x52: d.loadFromReg(d);
                break;
            case 0x53: d.loadFromReg(e);
                break;
            case 0x54: d.loadFromReg(h);
                break;
            case 0x55: d.loadFromReg(l);
                break;
            case 0x57: d.loadFromReg(a);
                break;
            case 0x58: e.loadFromReg(b);
                break;
            case 0x59: e.loadFromReg(c);
                break;
            case 0x5a: e.loadFromReg(d);
                break;
            case 0x5b: e.loadFromReg(e);
                break;
            case 0x5c: e.loadFromReg(h);
                break;
            case 0x5d: e.loadFromReg(l);
                break;
            case 0x5f: e.loadFromReg(a);
                break;
            case 0x60: h.loadFromReg(b);
                break;
            case 0x61: h.loadFromReg(c);
                break;
            case 0x62: h.loadFromReg(d);
                break;
            case 0x63: h.loadFromReg(e);
                break;
            case 0x64: h.loadFromReg(h);
                break;
            case 0x65: h.loadFromReg(l);
                break;
            case 0x67: h.loadFromReg(a);
                break;
            case 0x68: l.loadFromReg(b);
                break;
            case 0x69: l.loadFromReg(c);
                break;
            case 0x6a: l.loadFromReg(d);
                break;
            case 0x6b: l.loadFromReg(e);
                break;
            case 0x6c: l.loadFromReg(h);
                break;
            case 0x6d: l.loadFromReg(l);
                break;
            case 0x6f: l.loadFromReg(a);
                break;
            case 0x78: a.loadFromReg(b);
                break;
            case 0x79: a.loadFromReg(c);
                break;
            case 0x7a: a.loadFromReg(d);
                break;
            case 0x7b: a.loadFromReg(e);
                break;
            case 0x7c: a.loadFromReg(h);
                break;
            case 0x7d: a.loadFromReg(l);
                break;
            case 0x7f: a.loadFromReg(a);
                break;
            case 0x80: a.addReg(b);
                break;
            case 0x81: a.addReg(c);
                break;
            case 0x82: a.addReg(d);
                break;
            case 0x83: a.addReg(e);
                break;
            case 0x84: a.addReg(h);
                break;
            case 0x85: a.addReg(l);
                break;
            case 0x87: a.addReg(a);
                break;
            case 0x88: a.addReg(b);
                break;
            case 0x89: a.addReg(c);
                break;
            case 0x8a: a.addReg(d);
                break;
            case 0x8b: a.addReg(e);
                break;
            case 0x90: a.subReg(b);
                break;
            case 0x91: a.subReg(c);
                break;
            case 0x92: a.subReg(d);
                break;
            case 0x93: a.subReg(e);
                break;
            case 0x94: a.subReg(h);
                break;
            case 0x95: a.subReg(l);
                break;
            case 0x97: a.subReg(a);
                break;
            case 0xa0: a.andReg(b);
                break;
            case 0xa1: a.andReg(c);
                break;
            case 0xa2: a.andReg(d);
                break;
            case 0xa3: a.andReg(e);
                break;
            case 0xa4: a.andReg(h);
                break;
            case 0xa5: a.andReg(l);
                break;
            case 0xa7: a.andReg(a);
                break;
            case 0xa8: a.xorReg(b);
                break;
            case 0xa9: a.xorReg(c);
                break;
            case 0xaa: a.xorReg(d);
                break;
            case 0xab: a.xorReg(e);
                break;
            case 0xac: a.xorReg(h);
                break;
            case 0xad: a.xorReg(l);
                break;
            case 0xaf: a.xorReg(a);
                break;
            case 0xb0: a.orReg(b);
                break;
            case 0xb1: a.orReg(c);
                break;
            case 0xb2: a.orReg(d);
                break;
            case 0xb3: a.orReg(e);
                break;
            case 0xb4: a.orReg(h);
                break;
            case 0xb5: a.orReg(l);
                break;
            case 0xb7: a.orReg(a);
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
                break;
            case 0xd5: // push de
                break;
            case 0xe5: // push hl
                break;
            case 0xf5: // push af
                result.lastPushAddress = ptr - serializedData.data();;
                result.lastPushTicks = result.ticks;
                break;
            case 0xc6: a.addValue(ptr[1]);
                break;
            case 0xd6: a.subValue(ptr[1]);
                break;
            case 0xe6: a.andValue(ptr[1]);
                break;
            case 0xee: a.xorValue(ptr[1]);
                break;
            case 0xf6: a.orValue(ptr[1]);
                break;
            default:
                // Unsopported command
                assert(0);
        }
        ptr += command.size;
    }

    return result;
}