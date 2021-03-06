//
// Created by xiamr on 11/1/19.
//

#ifndef TINKER_NBOSPIN_HPP
#define TINKER_NBOSPIN_HPP

#include "utils/std.hpp"

class NBOSpin {
public:
    [[nodiscard]] static std::string_view title() { return "NBO Spin"; }

    static void process();

    static void do_process(const std::string &filename);

    [[nodiscard]] static double total_spin(std::string_view line);

    [[nodiscard]] static std::map<int, std::pair<std::string, std::array<double, 3>>> getElectronSpin(
        std::istream &ifs);
};

#endif  // TINKER_NBOSPIN_HPP
