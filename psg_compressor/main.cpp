#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>

#include "suffix_tree.h"


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

    std::vector<int> ayRegs;

    std::set<std::vector<int>> ayFrames;

    auto writeRegs = [&]()
    {
        if (ayRegs.empty())
        {
            ayRegs.resize(14);
        }
        else
        {
            ayFrames.insert(ayRegs);
            ayRegs.clear();
            ayRegs.resize(14);
        }
    };

    int frameCount = 0;
    int frameRegs = 0;
    int pauses = 0;

    while (pos < end)
    {
        uint8_t value = *pos;
        if (value == 0xff)
        {
            ++frameCount;

            if (pos == end - 1 || pos[1] == 0xff)
                ++pauses;

            ++pos;
            writeRegs();
        }
        else if (value == 0xfe)
        {
            writeRegs();
            frameCount += pos[1] * 4;
            pauses += pos[1] * 4;
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
            ++frameRegs;
        }
    }

    float avaragePsgRegsPerFrame = frameRegs / (float)frameCount;
    std::cout << "avarage frame size = " << avaragePsgRegsPerFrame << std::endl;
    float avarageCompressedFrameSize = avaragePsgRegsPerFrame + 2;

    std::cout << "frames = " << frameCount << ". Uniq AY frames =" << ayFrames.size() << " . pauses=" << pauses << std::endl;
    std::cout << "PSG2 total frame size = " << avarageCompressedFrameSize * ayFrames.size() << std::endl;
    std::cout << "Expected compressed size = " << avarageCompressedFrameSize * ayFrames.size() + 1 * frameCount;

    //SuffixTree tree("abcadfbc");
    SuffixTree tree("abcdfcdfab");
    tree.build_tree();

    return 0;
}

int main(int argc, char** argv)
{

    parsePsg("c:/zx/tbk-psg/machined.psg");

    return 0;
}
