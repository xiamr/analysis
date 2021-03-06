//
// Created by xiamr on 8/29/19.
//

#ifndef TINKER_GMXTOPOLOGYPRINTER_HPP
#define TINKER_GMXTOPOLOGYPRINTER_HPP

#include "utils/std.hpp"

class Frame;

class GmxTopologyPrinter {
public:
    [[nodiscard]] static std::string_view title() { return "Convert Tinker xyz to gromacs topolgy file"; }

    static void print(const std::string &topolgy, const std::string &prm, const std::string &out);

    static void printFrame(const std::shared_ptr<Frame> &frame, std::ofstream &os);
};

#endif  // TINKER_GMXTOPOLOGYPRINTER_HPP
