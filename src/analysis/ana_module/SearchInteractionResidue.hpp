//
// Created by xiamr on 6/14/19.
//

#ifndef TINKER_SEARCHINTERACTIONRESIDUE_HPP
#define TINKER_SEARCHINTERACTIONRESIDUE_HPP

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"

class Frame;

class SearchInteractionResidue : public AbstractAnalysis {
public:
    SearchInteractionResidue();

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Search Interaction Residue between two groups"; }

private:
    AmberMask ids1;
    AmberMask ids2;

    std::unordered_set<std::shared_ptr<Atom>> group1;
    std::unordered_set<std::shared_ptr<Atom>> group2;

    double cutoff;

    std::list<std::unordered_set<std::string>> interaction_residues;
    int total_frames = 0;

    enum class OutputStyle { BOOL = 0, NUMBER = 1 };
    OutputStyle style;
};

#endif  // TINKER_SEARCHINTERACTIONRESIDUE_HPP
