#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char** argv)
{
    using namespace std;

    std::cout << "Truncate scr file v.1.0" << std::endl;
    if (argc < 3)
    {
        std::cout << "Usage: file_truncate  input_file output_file" << std::endl;
        return -1;
    }
    
    std::string inputFileName = argv[1];
    std::string outputFileName = argv[2];
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
    if (fileSize != 6912)
    {
        std::cerr << "Source file should be full screen SCR file" << std::endl;
        return -1;
    }
    std::vector<uint8_t> buffer(6912);
    fileIn.read((char*)  buffer.data(), buffer.size());

    std::vector<uint8_t> dstBuffer(buffer.begin() + 2048, buffer.begin() + 2048*2);

    std::ofstream fileOut;
    fileOut.open(outputFileName, std::ios::binary);
    if (!fileOut.is_open())
    {
        std::cerr << "Can not write destination file " << outputFileName << std::endl;
        return -1;
    }
    fileOut.write((const char*) dstBuffer.data(), dstBuffer.size());
    return 0;
}
