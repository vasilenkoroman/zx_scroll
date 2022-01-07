#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <array>
#include <sstream>
#include <iomanip>

using FontData = std::array<uint8_t, 8>;

// trim from start (in place)
static inline void ltrim(std::string& s) 
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), 
        [](unsigned char ch) 
        {
            return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) 
{
    s.erase(std::find_if(s.rbegin(), s.rend(), 
        [](unsigned char ch) 
        {
            return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) 
{
    ltrim(s);
    rtrim(s);
}

int writeFile(const std::string& outputFileName, const std::vector<uint8_t>& buffer,
    bool eventLines, bool oddLines)
{
    const int height = buffer.size() / 32;

    std::ofstream fileOut;
    fileOut.open(outputFileName, std::ios::binary);
    if (!fileOut.is_open())
    {
        std::cerr << "Can not write destination file " << outputFileName << std::endl;
        return -1;
    }

    for (int i = 0; i < height; ++i)
    {
        if ((i % 2 == 0) && !eventLines)
            continue;
        if ((i % 2 == 1) && !oddLines)
            continue;
        fileOut.write((const char*)buffer.data() + i * 32, 32);
    }
    fileOut.close();
    return 0;
}

std::vector<std::string> splitLine(const std::string& line, std::string delimiter = ";")
{
    size_t prevPos = 0;
    size_t pos = 0;
    std::vector<std::string> result;
    while ((pos = line.find(delimiter, prevPos)) != std::string::npos)
    {
        std::string token = line.substr(prevPos, pos - prevPos);
        result.push_back(token);
        prevPos = pos + delimiter.length();
    }
    if (prevPos < line.size())
        result.push_back(line.substr(prevPos));
    return result;
}


FontData parseFont(const std::string& string)
{
    FontData result{};
    auto params = splitLine(string, ",");
    int index = 0;
    for (auto param: params)
    {
        int pos = param.find('#');
        if (pos == std::string::npos)
            pos = param.find('&');
        if (pos == std::string::npos)
            continue;
        
        param.erase(param.begin(), param.begin() + pos + 1);
        trim(param);
        result[index] = std::stoi(param, nullptr, 16);

        ++index;
        if (index >= 8)
            break;
    }
    return result;
}

int main(int argc, char** argv)
{
    using namespace std;

    std::cout << "Convert UTF8 string to the scroller internal encoding format v.1.0" << std::endl;
    if (argc < 5)
    {
        std::cout << "Usage: encoding_convertor  <full_font.asm> <reduced_font.asm> <text.txt> <encoded_text.dat>" << std::endl;
        return -1;
    }

    std::string fullFontFileName = argv[1];
    std::string reducedFontFileName = argv[2];
    std::string textFileName = argv[3];
    std::string encodedTextFileName = argv[4];

    std::ifstream fullFontFileIn;
    fullFontFileIn.open(fullFontFileName);
    if (!fullFontFileIn.is_open())
    {
        std::cerr << "Can not read source file " << fullFontFileName << std::endl;
        return -1;
    }

    std::ofstream reducedFontFileOut;
    reducedFontFileOut.open(reducedFontFileName);
    if (!fullFontFileIn.is_open())
    {
        std::cerr << "Can not write file " << reducedFontFileName << std::endl;
        return -1;
    }

    std::ifstream textFielIn;
    textFielIn.open(textFileName);
    if (!textFielIn.is_open())
    {
        std::cerr << "Can not read file " << textFileName << std::endl;
        return -1;
    }

    std::ofstream encodedTextFileOut;
    encodedTextFileOut.open(encodedTextFileName, ios::binary);
    if (!encodedTextFileOut.is_open())
    {
        std::cerr << "Can not write file " << encodedTextFileName << std::endl;
        return -1;
    }

    using namespace std;

    // Parse source full font
    string line;
    std::map<int, FontData> srcFont;
    std::vector<FontData> dstFont;
    std::map<int, int> mapping;

    dstFont.push_back(FontData{ 0x00, 0x47, 0x78, 0x7f, 0x00, 0x00, 0x00, 0x00 }); // Helper data at code 0.
    mapping.emplace(0, 0);

    while (getline(fullFontFileIn, line))
    {
        auto params = splitLine(line);
        if (params.size() != 2)
            continue;
        trim(params[1]);
        if (params[1].size() > 1)
            continue;
        int utf8Code = 32;
        if (!params[1].empty())
            utf8Code = params[1][0];
        srcFont.emplace(utf8Code, parseFont(params[0]));
    }

    // Generate reduced dst font by source text line

    getline(textFielIn, line);
    std::vector<uint8_t> encodedLine;
    for (const auto& ch : line)
    {
        int charCode = int(ch);
        auto itr = srcFont.find(charCode);
        if (itr == srcFont.end())
        {
            std::cerr << "Can't find char '" << ch << "' in the source font file" << std::endl;
            return -1;
        }
        auto mapItr = mapping.find(charCode);
        int encoded = 0;
        if (mapItr == mapping.end())
        {
            // Add new char to the dst font
            encoded = mapping.size();
            mapping.emplace(charCode, mapping.size());
            dstFont.push_back(itr->second);
        }
        else
        {
            encoded = mapItr->second;
        }
        encodedLine.push_back(encoded);
    }

    // Write encoded data
    encodedLine.push_back(0);
    encodedTextFileOut.write((const char*) encodedLine.data(), encodedLine.size());

    // Write reduced font file
    for (int symbol = 0; symbol < dstFont.size(); ++symbol)
    {
        const auto& charData = dstFont[symbol];
        reducedFontFileOut << "\tdefb ";
        for (int i = 0; i < charData.size(); ++i)
        {
            uint8_t shiftedValue = charData[i];
            // Do 'rrc'
            if (symbol > 0)
            {
                if (shiftedValue % 1)
                    shiftedValue = shiftedValue / 2 + 128;
                else
                    shiftedValue = shiftedValue / 2;
            }

            reducedFontFileOut << "#" << std::hex << std::setw(2) << std::setfill('0') << (int) shiftedValue;
            if (i < 7)
                reducedFontFileOut << ", ";
        }
        reducedFontFileOut << "; ";
        for (const auto& value : mapping)
        {
            if (value.second == symbol && symbol > 0)
                reducedFontFileOut << (char) value.first;
        }
        reducedFontFileOut << std::endl;
    }
    std::cout << "Done" << std::endl;
    return 0;
}
