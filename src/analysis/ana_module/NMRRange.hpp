//
// Created by xiamr on 6/14/19.
//

#ifndef TINKER_NMRRANGE_HPP
#define TINKER_NMRRANGE_HPP

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "AbstractAnalysis.hpp"
#include "AminoTop.hpp"
#include "data_structure/atom.hpp"

class Frame;

class NMRRange : public AbstractAnalysis {
public:
    NMRRange();

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "NMRRange Analysis"; }

private:
    void recognize_amino_acid(std::shared_ptr<Frame> &frame);

    bool recognize_walk(std::shared_ptr<Atom> atom, std::shared_ptr<AminoTop::AminoItem> item, AminoTop &top,
                        std::shared_ptr<Frame> &frame, std::list<std::shared_ptr<Atom>> &atom_list,
                        std::list<std::shared_ptr<AminoTop::AminoItem>> &item_list);

    bool first_frame = true;

    void loadTop() {
        std::string path = std::getenv("ANALYSIS_TOP_PATH");
        for (auto &t : aminotype_str_bimap) {
            AminoTop top;
            top.readTop(path + "/" + t.right + ".top");
            top.type = t.left;
            amino_top_list.push_back(top);
        }
    }

    std::list<AminoTop> amino_top_list;

    std::list<std::shared_ptr<AminoAcid>> amino_acid_list;

    std::map<std::pair<int, int>, std::list<double>> dist_range_map;

    std::map<int, std::string> name_map;
};

#endif  // TINKER_NMRRANGE_HPP
