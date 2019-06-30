//
// Created by xiamr on 6/30/19.
//

#ifndef TINKER_ANGLEWAT_HPP
#define TINKER_ANGLEWAT_HPP

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "BasicAnalysis.hpp"
#include "common.hpp"
#include "atom.hpp"


class Frame;

class AngleWat : public BasicAnalysis {
public:

    AngleWat() {
        enable_outfile = true;
    }

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    static const std::string title() { return "Angle (Ow-Hw) Distribution with cutoff"; }

protected:
    Atom::AtomIndenter ids1, ids2, ids3;

    std::unordered_set<std::shared_ptr<Atom>> group1, group2, group3;

    std::list<std::tuple<std::shared_ptr<Atom>, std::shared_ptr<Atom>>> pairs;

    double angle_width;

    int angle_bins;

    std::unordered_map<int, std::size_t> hist;

    double cutoff1, cutoff2;

    void printData(std::ostream &os) const;
};


#endif //TINKER_ANGLEWAT_HPP