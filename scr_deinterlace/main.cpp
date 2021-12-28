#include <iostream>
#include <fstream>
#include <vector>

std::vector<uint8_t> deinterlaceBuffer(const std::vector<uint8_t>& buffer)
{
    std::vector<uint8_t> result(buffer.size());
    int imageHeight = buffer.size() / 32;

    for (int i = 0; i < imageHeight; ++i)
    {
        const uint8_t* src = buffer.data();

        int third = i / 64;
        int block = (i / 8) % 8;
        int inside = i % 8;

        src += third * 2048;
        src += inside * 256;
        src += block * 32;

        uint8_t* dst = result.data() + i * 32;
        for (int i = 0; i < 32; ++i)
            *dst++ = *src++;
    }
    return result;
}

int main(int argc, char** argv)
{
    using namespace std;

    std::cout << "SCR deinterlacer v.1.0" << std::endl;
    if (argc < 2)
    {
        std::cout << "Usage: SCR deinterlacer  input_file" << std::endl;
        return -1;
    }
    
    std::string inputFileName = argv[1];
    std::ifstream fileIn;
    fileIn.open(inputFileName, std::ios::binary);
    if (!fileIn.is_open())
    {
        std::cerr << "Can not read source file " << inputFileName << std::endl;
        return -1;
    }

    fileIn.seekg(0, ios::end);
    int fileSize = fileIn.tellg();
    fileIn.seekg(0, ios::beg);

    const int minBlockSize = 2048 + 2048 / 8;
    if (fileSize % minBlockSize || fileSize <= 0)
    {
        std::cerr << "Unexpected file size " << fileSize << ". Each block should be aligned at 64 lines" << std::endl;
    }

    int blocks = fileSize / minBlockSize;

    std::vector<uint8_t> buffer;
    buffer.resize(blocks * 2048);
    fileIn.read((char*)buffer.data(), blocks * 2048);
    fileIn.close();

    buffer = deinterlaceBuffer(buffer);

    std::string outputFileName = inputFileName + ".deinterlaced";
    std::ofstream fileOut;
    fileOut.open(outputFileName, std::ios::binary);
    if (!fileOut.is_open())
    {
        std::cerr << "Can not write destination file " << outputFileName << std::endl;
        return -1;
    }
    fileOut.write((const char*)buffer.data(), buffer.size());
    fileOut.close();

    return 0;
}
