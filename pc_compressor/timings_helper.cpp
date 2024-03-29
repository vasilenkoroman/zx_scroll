#include "timings_helper.h"

#include "compressed_line.h"

std::string toString (Role role)
{
    switch (role)
    {
    case Role::regularBottom:
        return "regularBottom";
    case Role::regularTop:
        return "regularTop";
    case Role::nextBottom:
        return "nextBottom";
    case Role::nextTop:
        return "nextTop";
    }
    return "";
};



McToRastrInfo calculateTimingsTable(int imageHeight, bool showLog)
{
    const int colorHeight = imageHeight / 8;

    McToRastrInfo banks;
    for (auto& bankData : banks)
        bankData.resize(colorHeight);

    int step = 0;
    int lastB0 = 0;
    do
    {
        if (showLog)
            std::cout << "Step: " << step << "\t";
        if (step % 8 == 0)
        {
            if (showLog)
                std::cout << "Non MC drawing step " << std::endl;
            lastB0 = step / 8;
        }
        else
        {
            // 1. Regular lines
            int mc = step / 8;
            mc += 8;
            if (mc < 0)
                mc += colorHeight;
            if (mc >= colorHeight)
                mc -= colorHeight;


            int topUpdatedBanks = 8 - (step % 8);

            for (int y = 0; y < 8; ++y)
            {
                if (showLog)
                    std::cout << "MC=" << mc << ": R=";

                int bankNum = y;

                int lowLines = 8 - topUpdatedBanks;
                if (y < lowLines)
                {
                    banks[bankNum][lastB0].insert({ mc, Role::regularBottom });
                    if (showLog)
                        std::cout << bankNum << "-" << lastB0;
                }
                else
                {
                    int prevBankIndex = lastB0 - 1;
                    if (prevBankIndex < 0)
                        prevBankIndex = imageHeight / 8 - 1;
                    banks[bankNum][prevBankIndex].insert({ mc, Role::regularTop });
                    if (showLog)
                        std::cout << bankNum << "-" << prevBankIndex;
                }
                if (showLog)
                    std::cout << "\t";

                ++mc;
                if (mc >= colorHeight)
                    mc -= colorHeight;
            }
            if (showLog)
                std::cout << std::endl;

            // 2. Next frame lines
            if (showLog)
                std::cout << "-->Next frame\t\t\t";

            mc = step / 8;
            mc -= 16;
            mc++;

            if (mc < 0)
                mc += colorHeight;
            if (mc >= colorHeight)
                mc -= colorHeight;

            topUpdatedBanks = 8 - (step % 8);

            for (int y = 1; y < 8; ++y)
            {
                if (showLog)
                    std::cout << "MC=" << mc << ": R=";

                int lowLines = 8 - topUpdatedBanks;
                if (y < lowLines)
                {
                    int bankNum = y - 1;

                    banks[bankNum][lastB0].insert({ mc, Role::nextBottom });
                    if (showLog)
                        std::cout << bankNum << "-" << lastB0;
                }
                else
                {
                    int bankNum = y;
                    int prevBankIndex = lastB0 - 1;

                    if (prevBankIndex < 0)
                        prevBankIndex = imageHeight / 8 - 1;
                    banks[bankNum][prevBankIndex].insert({ mc, Role::nextTop });
                    if (showLog)
                        std::cout << bankNum << "-" << prevBankIndex;
                }
                if (showLog)
                    std::cout << "\t";

                ++mc;
                if (mc >= colorHeight)
                    mc -= colorHeight;
            }
            if (showLog)
                std::cout << std::endl;
        }

        --step;
        if (step < 0)
            step = imageHeight - 1;
    } while (step != 0);

    if (showLog)
        std::cout << std::endl << std::endl;

    for (int d = 0; d < imageHeight; ++d)
    {
        int bank = d % 8;
        int lineInBank = d / 8;
        if (showLog)
            std::cout << "D=" << d << "\t";

        std::set<int> mcList;
        std::string detailsStr;
        for (const auto& data : banks[bank][lineInBank])
        {
            mcList.insert(data.mc);
            detailsStr += toString(data.role);
            detailsStr += "=";
            detailsStr += std::to_string(data.mc);
            detailsStr += " ";
        }

        for (const auto& mc : mcList)
        {
            if (showLog)
                std::cout << "MC=" << mc << ", ";
        }
        if (showLog)
            std::cout << "\tdetails=" << detailsStr << std::endl;
    }

    return banks;

}

int getMcTicks(const McToRastrInfo& info, const CompressedData& multicolor, int lineNum, Role role)
{
    int bankNum = lineNum % 8;
    int bankOffset = lineNum / 8;
    const auto& data = info[bankNum][bankOffset];
    for (const auto& v : data)
    {
        if (v.role == role)
            return multicolor.data[v.mc].mcStats.virtualTicks;
    }

    if (role == Role::regularBottom)
        role = Role::regularTop;
    else if (role == Role::regularTop)
        role = Role::regularBottom;
    else if (role == Role::nextBottom)
        role = Role::nextTop;
    else if (role == Role::nextTop)
        role = Role::nextBottom;
    for (const auto& v : data)
    {
        if (v.role == role)
            return multicolor.data[v.mc].mcStats.virtualTicks;
    }

    std::cerr << "Something wrong! MC ticks not found!" << std::endl;
    abort();
    return 0;
}
