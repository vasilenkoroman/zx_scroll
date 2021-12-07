#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>

#include "suffix_tree.h"

static const uint8_t kEndTrackMarker = 0x3f;
static const int kMaxDelay = 33;

class PgsPacker
{
public:

    struct Stats
    {
        int emptyCnt = 0;
        int emptyFrames = 0;

        int singleRepeat = 0;
        int allRepeat = 0;
        int allRepeatFrames = 0;

        int ownCnt = 0;
        int ownBytes = 0;

        std::map<int, int> frameRegs;
        std::map<int, int> regsChange;
    };

    using AYRegs = std::map<int, int>;

    std::map<AYRegs, uint16_t> regsToSymbol;
    std::map<uint16_t, AYRegs> symbolToRegs;
    std::vector<uint16_t> ayFrames;
    AYRegs ayRegs;
    AYRegs lastValues;
    Stats stats;

    std::vector<uint8_t> compressedData;
    std::vector<int> refCount;

private:

    bool isPsg2(const AYRegs& regs) const
    {
        bool usePsg2 = regs.size() > 2;
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
        return usePsg2;
    }

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

    void writeRegs(int delay)
    {
        if (!ayRegs.empty())
        {
            uint16_t symbol = toSymbol(ayRegs);
            ayFrames.push_back(symbol); //< Flush previous frame.
            ayRegs.clear();
        }
        while (delay > 0)
        {
            int d = std::min(kMaxDelay, delay);
            ayFrames.push_back(d); //< Special code for delay
            delay -= d;
        }
    }

    void serializeEmptyFrames (int count)
    {
        while (count > 0)
        {
            uint8_t value = std::min(33, count);
            uint8_t header = 30;
            compressedData.push_back(header + value - 1);
            count -= value;
        }
    };

    void serializeRef(const std::vector<int>& frameOffsets, uint16_t pos, uint8_t size)
    {
        int offset = frameOffsets[pos];
        int recordSize = size == 1 ? 2 : 3;
        int16_t delta = offset - compressedData.size() - (recordSize - 1);
        assert(delta < 0);

        uint8_t* ptr = (uint8_t*)&delta;
        
        if (size == 1)
            ptr[1] &= ~0x40; // reset 6-th bit

        // Serialize in network byte order
        compressedData.push_back(ptr[1]);
        compressedData.push_back(ptr[0]);

        if (size > 1)
            compressedData.push_back(size);
    };

    uint8_t makeRegMask(const AYRegs& regs, int from, int to)
    {
        uint8_t result = 0;
        uint8_t bit = 0x80;
        for (int i = from; i < to; ++i)
        {
            const auto reg = regs.find(i);
            if (reg != regs.end())
            {
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
        
        bool usePsg2 = isPsg2(regs);

        uint8_t header1 = 0;
        if (usePsg2)
        {
            header1 = 0x40 + (makeRegMask(regs, 0, 6) >> 2);
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
            header1 = regs.size() == 1 ? 0x10 : 0;
            for (const auto& reg : regs)
            {
                compressedData.push_back(reg.first + header1);
                compressedData.push_back(reg.second); // reg value
                header1 = 0;
            }
        }

        stats.ownBytes += compressedData.size() - prevSize;

    };

    auto findPrevChain(int pos)
    {
        const int maxLength = std::min(255, (int) ayFrames.size() - pos);

        int maxChainLen = -1;
        int chainPos = -1;
        for (int i = 0; i < pos; ++i)
        {
            if (ayFrames[i] == ayFrames[pos] && refCount[i] ==  0)
            {
                int chainLen = 0;
                for (int j = 0; j < maxLength && i + j < pos; ++j)
                {
                    if (ayFrames[i + j] != ayFrames[pos + j] || refCount[i + j] > 0)
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

        for (int i = 0; i <= kMaxDelay; ++i)
        {
            AYRegs fakeRegs;
            fakeRegs[-1] = i;
            regsToSymbol.emplace(fakeRegs, i);
            symbolToRegs.emplace(i, fakeRegs);
        }

        int delayCounter = 0;

        while (pos < end)
        {
            uint8_t value = *pos;
            if (value == 0xff)
            {
                ++pos;
                ++delayCounter;
            }
            else if (value == 0xfe)
            {
                delayCounter += pos[1] * 4;
                pos += 2;
            }
            else if (value == 0xfd)
            {
                break;
            }
            else
            {
                if (delayCounter > 0)
                    writeRegs(delayCounter - 1);
                delayCounter = 0;

                assert(value <= 13);
                ayRegs[value] = pos[1];
                lastValues[value] = pos[1];
                ++stats.regsChange[value];
                pos += 2;
            }
        }
        writeRegs(delayCounter);

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
        refCount.resize(ayFrames.size());

        std::vector<int> frameOffsets;

        for (int i = 0; i < ayFrames.size();)
        {
            while (frameOffsets.size() <= i)
                frameOffsets.push_back(compressedData.size());

            if (ayFrames[i] <= kMaxDelay)
            {
                serializeEmptyFrames(ayFrames[i]);
                stats.emptyFrames += ayFrames[i];
                ++stats.emptyCnt;
                ++i;
            }
            else
            {
                auto symbol = ayFrames[i];
                auto currentRegs = symbolToRegs[symbol];

                auto [pos, len] = findPrevChain(i);
                if (len > 1 || len == 1 && currentRegs.size() > 1)
                {
                    serializeRef(frameOffsets, pos, len);

                    for (int j = i; j < i + len; ++j)
                        refCount[j] = len;

                    i += len;
                    if (len == 1)
                        stats.singleRepeat++;
                    stats.allRepeat++;
                    stats.allRepeatFrames += len;
                }
                else
                {
                    serializeFrame(i);
                    ++i;
                    ++stats.ownCnt;
                }
            }
        }

        compressedData.push_back(kEndTrackMarker);

        for (const auto& v : symbolToRegs)
            ++stats.frameRegs[v.second.size()];


        fileOut.write((const char*) compressedData.data(), compressedData.size());
        fileOut.close();

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
