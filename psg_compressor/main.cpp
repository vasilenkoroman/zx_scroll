#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>

#include "suffix_tree.h"

class PgsPacker
{
public:

    struct Stats
    {
        int emptyCnt = 0;
        int emptyFrames = 0;

        int refsCnt = 0;
        int refsFrames = 0;

        int ownCnt = 0;
        int ownBytes = 0;

        std::map<int, int> frameRegs;
    };

    using AYRegs = std::map<int, int>;

    std::map<AYRegs, uint16_t> regsToSymbol;
    std::map<uint16_t, AYRegs> symbolToRegs;
    std::vector<uint16_t> ayFrames;
    AYRegs ayRegs;
    AYRegs lastValues;
    Stats stats;
    uint8_t initReg13 = 0;

    std::vector<uint8_t> compressedData;
    std::vector<bool> isRef;

private:

    uint16_t toSymbol(const AYRegs& regs)
    {
        auto itr = regsToSymbol.find(regs);
        if (itr != regsToSymbol.end())
            return itr->second;

        uint16_t value = regsToSymbol.size();
        regsToSymbol.emplace(regs, value);
        symbolToRegs.emplace(value, regs);
        return value;
    }

    void writeRegs()
    {
        uint16_t symbol = toSymbol(ayRegs);
        ayFrames.push_back(symbol); //< Flush previous frame.
        ayRegs.clear();
    }

    int serializeEmptyFrames (int i)
    {
        int size = 0;
        while (i < ayFrames.size() && ayFrames[i] == 0 && size < 64)
        {
            ++i;
            ++size;
        }
        uint8_t header = 0x80; //< 00 xxxxxx   for pause command
        compressedData.push_back(header + size);
        return size;
    };

    void serializeRef(const std::vector<int>& frameOffsets, uint16_t pos, uint8_t size)
    {
        uint16_t offset = frameOffsets[pos];
        uint16_t delta = offset - frameOffsets.size();

        uint8_t* ptr = (uint8_t*)&delta;
        
        // Serialize in network byte order
        compressedData.push_back(ptr[1]);
        compressedData.push_back(ptr[0]);

        compressedData.push_back(size);
        for (int i = pos; i < pos + size; ++i)
            isRef[i] = true;
    };

    uint8_t makeRegMask(const AYRegs& regs, int from, int to)
    {
        uint8_t result = 0;
        uint8_t bit = 0x80;
        for (int i = from; i < to; ++i)
        {
            const auto reg = regs.find(i);
            if (reg == regs.end())
            {
                // Mask is inverted. 1 means skip reg.
                result += bit;
            }
            bit >>= 1;
        }
        return result;
    }

    auto serializeFrame (uint16_t pos)
    {
        int prevSize = compressedData.size();

        uint16_t symbol = ayFrames[pos];
        auto regs = symbolToRegs[symbol];
        
        bool usePsg2 = regs.size() > 1;
#if 0
        // TODO: support PSG1 for 2 regs (it give 100+ bytes extra compression for machined.psg)
        if (regs.size() == 2)
        {
            for (const auto& reg : regs)
            {
                if ((reg.first & ~3) == 4)
                    usePgs2 = false;
            }
        }
#endif
        uint8_t header1 = 0;
        if (usePsg2)
        {
            header1 = (makeRegMask(regs, 0, 6) >> 2) + 0x00;
            compressedData.push_back(header1);

            for (const auto& reg : regs)
            {
                if (reg.first < 6)
                    compressedData.push_back(reg.second); // reg value
            }

            uint8_t header2 = makeRegMask(regs, 6, 14);
            compressedData.push_back(header2);
            for (const auto& reg : regs)
            {
                if (reg.first >= 6)
                    compressedData.push_back(reg.second); // reg value
            }
        }
        else
        {
            header1 = 0x40;
            header1 += regs.begin()->first; //< Reg index in low 4 bit.
            compressedData.push_back(header1);
            for (const auto& reg : regs)
                compressedData.push_back(reg.second); // reg value
        }

        stats.ownBytes += compressedData.size() - prevSize;

    };

    auto findPrevChain(int pos)
    {
        int maxLength = std::min(64, (int)ayFrames.size() - pos);
        int maxChainLen = -1;
        int chainPos = -1;
        for (int i = 0; i < pos; ++i)
        {
            if (ayFrames[i] == ayFrames[pos] && !isRef[i])
            {
                int chainLen = 0;
                for (int j = 0; j < maxLength; ++j)
                {
                    if (ayFrames[i + j] != ayFrames[pos + j] && !isRef[i + j] && ayFrames[i + j] != 0)
                        break;
                    ++chainLen;
                }
                if (chainLen >= maxChainLen)
                {
                    maxChainLen = chainLen;
                    chainPos = i;
                }
            }
        }
        if (maxChainLen > 255)
            maxChainLen = 255;

        return std::tuple<int, int> { chainPos, maxChainLen };
    }

public:
    int parsePsg(const std::string& inputFileName, const std::string& outputFileName)
    {
        using namespace std;

        ifstream fileIn;
        fileIn.open(inputFileName, std::ios::binary);
        if (!fileIn.is_open())
        {
            std::cerr << "Can't open input file " << inputFileName << std::endl;
            return -1;
        }

        ofstream fileOut;
        fileOut.open(outputFileName, std::ios::binary | std::ios::trunc);
        if (!fileOut.is_open())
        {
            std::cerr << "Can't open output file " << outputFileName << std::endl;
            return -1;
        }

        fileIn.seekg(0, ios::end);
        int fileSize = fileIn.tellg();
        fileIn.seekg(0, ios::beg);

        std::vector<uint8_t> data(fileSize);
        fileIn.read((char*)data.data(), fileSize);

        const uint8_t* pos = data.data() + 16;
        const uint8_t* end = data.data() + data.size();

        bool firstTime = true;
        toSymbol(ayRegs); //< Write empty regs at symbol 0

        while (pos < end)
        {
            uint8_t value = *pos;
            if (value == 0xff)
            {
                ++pos;
                if (!firstTime)
                    writeRegs();
                firstTime = false;
            }
            else if (value == 0xfe)
            {
                if (!ayRegs.empty())
                    writeRegs();
                for (int i = 0; i < pos[1] * 4; ++i)
                    writeRegs();
                pos += 2;
            }
            else if (value == 0xfd)
            {
                break;
            }
            else
            {
                assert(value <= 13);
                if (value != 13 && pos[1] == lastValues[value])
                {
                    ; // skip
                }
                else
                {
                    ayRegs[value] = pos[1];
                    lastValues[value] = pos[1];
                }
                pos += 2;
            }
        }
        writeRegs();


#if 0
        float avaragePsgRegsPerFrame = frameRegs / (float)frameCount;
        std::cout << "avarage frame size = " << avaragePsgRegsPerFrame << std::endl;
        float avarageCompressedFrameSize = avaragePsgRegsPerFrame + 2;

        std::cout << "frames = " << frameCount << ". Uniq AY frames =" << ayFrames.size() << " . pauses=" << pauses << std::endl;
        std::cout << "PSG2 total frame size = " << avarageCompressedFrameSize * ayFrames.size() << std::endl;
        std::cout << "Expected compressed size = " << avarageCompressedFrameSize * ayFrames.size() + 1 * frameCount;
#endif

#if 0
        std::string str = "abcdfcdfab";
        std::vector<char> testStr(str.begin(), str.end());
        SuffixTree<char> tree(testStr);
        tree.build_tree();
#endif

        // compressData
        isRef.resize(ayFrames.size());

        std::vector<int> frameOffsets;

        for (int i = 0; i < ayFrames.size();)
        {
            while (frameOffsets.size() <= i)
                frameOffsets.push_back(compressedData.size());

            if (ayFrames[i] == 0)
            {
                int count = serializeEmptyFrames(i);
                i += count;
                ++stats.emptyCnt;
                stats.emptyFrames += count;
            }
            else
            {
                auto [pos, len] = findPrevChain(i);
                if (len > 1)
                {
                    serializeRef(frameOffsets, pos, len);
                    i += len;
                    stats.refsCnt++;
                    stats.refsFrames += len;
                }
                else
                {
                    serializeFrame(i);
                    ++i;
                    ++stats.ownCnt;
                }
            }
        }

        for (const auto& v : symbolToRegs)
            ++stats.frameRegs[v.second.size()];

        //SuffixTree<uint16_t> tree(ayFrames);
        //tree.build_tree();

        fileOut.write((const char*) compressedData.data(), compressedData.size());

        return 0;
    }

};


int main(int argc, char** argv)
{
    PgsPacker packer;

    if (argc != 3)
    {
        std::cout << "Usage: psg_pack <input_file> <output_file>" << std::endl;
        return -1;
    }

    return packer.parsePsg(argv[1], argv[2]);
}
