

#include "FindMinBetweenTwoGroups.hpp"

#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/common.hpp"

using namespace std;

FindMinBetweenTwoGroups::FindMinBetweenTwoGroups() { enable_outfile = true; }

void FindMinBetweenTwoGroups::process(std::shared_ptr<Frame> &frame) {
    std::size_t length = mol_list.size();

    std::vector<double> line_rest;
    for (std::size_t i = 0; i < length - 1; i++) {
        for (std::size_t j = i + 1; j < length; j++) {
            auto &mol1 = mol_list[i];
            auto &mol2 = mol_list[j];
            line_rest.push_back(min_distance(mol1, mol2, frame).first);
        }
    }
    results.push_back(line_rest);
}

void FindMinBetweenTwoGroups::print(std::ostream &os) {
    os << "************************************************\n";
    os << "*****" << FindMinBetweenTwoGroups::title() << " ****\n";
    os << "Group  " << amberMask << '\n';
    os << "************************************************\n";

    os << boost::format("%6s") % "Frame";
    std::size_t length = mol_list.size();
    for (std::size_t i = 0; i < length - 1; i++) {
        for (std::size_t j = i + 1; j < length; j++) {
            os << boost::format("  %4d-%-4d  ") % i % j;
        }
    }

    os << '\n';

    std::size_t nframe = 0;

    for (auto &v : results) {
        nframe++;

        os << boost::format("%6d") % nframe;
        for (auto value : v) os << boost::format("  %9.2f  ") % value;
        os << '\n';
    }

    os << "************************************************\n";
}

void FindMinBetweenTwoGroups::readInfo() { select1group(amberMask, "Input Residue Name Mask: "); }

void FindMinBetweenTwoGroups::processFirstFrame(std::shared_ptr<Frame> &frame) {
    for (auto &mol : frame->molecule_list) {
        for (auto &atom : mol->atom_list) {
            if (is_match(atom, this->amberMask)) {
                mol_list.push_back(mol);
                break;
            }
        }
    }
}
