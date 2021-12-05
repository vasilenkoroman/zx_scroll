#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "suffix_tree.h"

using AYRegs = std::map<int, int>;

std::map<AYRegs, uint16_t> regsToSymbol;
std::map<uint16_t, AYRegs> symbolToRegs;

uint16_t toSymbol(AYRegs regs)
{
    auto itr = regsToSymbol.find(regs);
    if (itr != regsToSymbol.end())
        return itr->second;

    uint16_t value = regsToSymbol.size();
    regsToSymbol.emplace(regs, value);
    symbolToRegs.emplace(value, regs);

    return value;
}

int parsePsg(const std::string& fileName)
{
    using namespace std;

    ifstream fileIn;
    fileIn.open(fileName, std::ios::binary);
    if (!fileIn.is_open())
        return -1;

    fileIn.seekg(0, ios::end);
    int fileSize = fileIn.tellg();
    fileIn.seekg(0, ios::beg);

    std::vector<uint8_t> data(fileSize);
    fileIn.read((char*)data.data(), fileSize);

    const uint8_t* pos = data.data() + 16;
    const uint8_t* end = data.data() + data.size();

    AYRegs ayRegs;

    std::vector<uint16_t> ayFrames;

    auto writeRegs =
        [&]()
    {
        uint16_t symbol = toSymbol(std::move(ayRegs));
        ayFrames.push_back(symbol); //< Flush previous frame.
    };

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
            ayRegs[value] = pos[1];
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
    std::vector<uint8_t> compressedData;
    std::vector<bool> isRef;
    isRef.resize(ayFrames.size());

    struct Stats
    {
        int emptyCnt = 0;
        int emptyFrames = 0;

        int refsCnt = 0;
        int refsFrames = 0;

        int ownCnt = 0;
        int ownBytes = 0;
    };
    Stats stats;

    auto serializeEmptyFrames = 
        [&](int i)
        {
            int size = 0;
            while (i < ayFrames.size() && ayFrames[i] == 0 && size < 64)
            {
                ++i;
                ++size;
            }
            compressedData.push_back(size);
            return size;
        };

    auto serializeRef = 
        [&](uint16_t pos, uint8_t size)
        {
            compressedData.push_back(pos % 256);
            compressedData.push_back(pos >> 8);
            compressedData.push_back(size);
            for (int i = pos; i < pos + size; ++i)
                isRef[i] = true;
        };

    auto serializeFrame =
        [&](uint16_t pos)
    {
        uint16_t symbol = ayFrames[pos];
        auto regs = symbolToRegs[symbol];
        compressedData.push_back(0); // mask1
        compressedData.push_back(0); // mask2
        stats.ownBytes += 2;
        for (const auto& reg : regs)
            compressedData.push_back(reg.second); // reg value
        stats.ownBytes += regs.size();
    };

    auto findPrevChain = 
        [&](int pos) 
        {
            int maxLength = std::min(64, (int) ayFrames.size() - pos);
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
            return std::tuple<int,int> { chainPos, maxChainLen };
        };

    for (int i = 0; i < ayFrames.size();)
    {
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
                serializeRef(pos, len);
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

    //SuffixTree<uint16_t> tree(ayFrames);
    //tree.build_tree();

    return 0;
}

int main(int argc, char** argv)
{

    parsePsg("c:/zx/tbk-psg/machined.psg");

    return 0;
}
