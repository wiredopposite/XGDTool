#include <fstream>
#include <vector>

#include "XGD.h"
#include "Formats/Xiso.h"
#include "Tests/Tests.h"

namespace Tests {

void compare_iso_sectors(std::filesystem::path in_iso_one, std::filesystem::path in_iso_two) {
    std::vector<uint32_t> different_sectors;
    std::vector<uint32_t> different_sectors_empty_2;

    std::ifstream in_one_file(in_iso_one, std::ios::binary);
    std::ifstream in_two_file(in_iso_two, std::ios::binary);
    if (!in_one_file.is_open() || !in_two_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), in_iso_one.string() + " and " + in_iso_two.string());
    }

    in_one_file.seekg(0, std::ios::end);
    in_two_file.seekg(0, std::ios::end);
    uint32_t total_sectors_1 = static_cast<uint32_t>(in_one_file.tellg() / Xiso::SECTOR_SIZE);
    uint32_t total_sectors_2 = static_cast<uint32_t>(in_two_file.tellg() / Xiso::SECTOR_SIZE);
    if (total_sectors_1 != total_sectors_2) {
        throw XGDException(ErrCode::MISC, HERE(), "Different number of sectors in files");
    }

    in_one_file.seekg(0, std::ios::beg);
    in_two_file.seekg(0, std::ios::beg);

    for (uint32_t i = 0; i < total_sectors_1; i++) {
        std::vector<char> sector_one(Xiso::SECTOR_SIZE, 0);
        std::vector<char> sector_two(Xiso::SECTOR_SIZE, 0);

        in_one_file.read(sector_one.data(), Xiso::SECTOR_SIZE);
        in_two_file.read(sector_two.data(), Xiso::SECTOR_SIZE);

        if (in_one_file.fail() || in_two_file.fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        // if (sector_one != sector_two) {
            for (auto j = 0; j < Xiso::SECTOR_SIZE; j++) {
                if (sector_one[j] != sector_two[j]) {
                    // if (std::find(different_sectors_empty_2.begin(), different_sectors_empty_2.end(), i) == different_sectors_empty_2.end()) {
                    different_sectors_empty_2.push_back(i);
                    // }
                    break;
                }
            }
            different_sectors.push_back(i);
        // }
        XGDLog().print_progress(i, total_sectors_1);
    }

    in_one_file.close();
    in_two_file.close();

    // for (auto sector : different_sectors) {
    //     std::cerr << "Different sector: " << sector << std::endl;
    // }
    std::cerr << "Total different sectors: " << different_sectors.size() << std::endl;

    // for (auto sector : different_sectors_empty_2) {
    //     std::cerr << "Different sector empty in 2: " << sector << std::endl;
    // }
    std::cerr << "Total different sectors empty in 2: " << different_sectors_empty_2.size() << std::endl;
}

};