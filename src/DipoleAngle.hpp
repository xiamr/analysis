//
// Created by xiamr on 6/14/19.
//

#ifndef TINKER_DIPOLEANGLE_HPP
#define TINKER_DIPOLEANGLE_HPP

#include <memory>
#include <unordered_set>
#include <string>
#include <map>
#include <list>
#include <utility>


#include "common.hpp"
#include "BasicAnalysis.hpp"
#include "atom.hpp"

class Frame;

class DipoleAngle : public BasicAnalysis {
public:
    DipoleAngle() {
        enable_forcefield = true;
        enable_outfile = true;
    }

    virtual ~DipoleAngle() = default;

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void readInfo() override;

    static const std::string title() {
        return "Dipole Angle";
    }

protected:

    Atom::AtomIndenter ids1;
    Atom::AtomIndenter ids2;

    std::unordered_set<std::shared_ptr<Atom>> group1;
    std::unordered_set<std::shared_ptr<Atom>> group2;


    double distance_width;
    double angle_width;

    int distance_bins;
    int angle_bins;

    std::map<std::pair<int, int>, size_t> hist;
};


#endif //TINKER_DIPOLEANGLE_HPP
