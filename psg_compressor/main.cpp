#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>

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
    //toSymbol(ayRegs); //< Write empty regs at symbol 0

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

    SuffixTree<uint16_t> tree(ayFrames);
    tree.build_tree();

    return 0;
}

int main(int argc, char** argv)
{

    parsePsg("c:/zx/tbk-psg/machined.psg");

    return 0;
}
