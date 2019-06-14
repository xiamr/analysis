//
// Created by xiamr on 6/14/19.
//

#include "FindMinBetweenTwoGroups.hpp"

#include "frame.hpp"

#include "atom.hpp"

using namespace std;

void FindMinBetweenTwoGroups::process(std::shared_ptr<Frame> &frame) {

    std::size_t length = mol_list.size();

    std::vector<double> line_rest;
    for (std::size_t i = 0; i < length - 1; i++) {
        for (std::size_t j = i + 1; j < length; j++) {
            auto &mol1 = mol_list[i];
            auto &mol2 = mol_list[j];
            line_rest.push_back(min_distance(mol1, mol2, frame));
        }
    }
    results.push_back(line_rest);
}

void FindMinBetweenTwoGroups::print() {
    outfile << "************************************************\n";
    outfile << "*****" << FindMinBetweenTwoGroups::title() << " ****\n";
    outfile << "Group  " << ids << '\n';
    outfile << "************************************************\n";


    outfile << boost::format("%6s") % "Frame";
    std::size_t length = mol_list.size();
    for (std::size_t i = 0; i < length - 1; i++) {
        for (std::size_t j = i + 1; j < length; j++) {
            outfile << boost::format("  %4d-%-4d  ") % i % j;
        }
    }

    outfile << '\n';

    std::size_t nframe = 0;

    for (auto &v : results) {
        nframe++;

        outfile << boost::format("%6d") % nframe;
        for (auto value: v) outfile << boost::format("  %9.2f  ") % value;
        outfile << '\n';
    }


    outfile << "************************************************\n";
}


void FindMinBetweenTwoGroups::readInfo() {
    Atom::select1group(ids, "Input Residue Name Mask: ");
}

void FindMinBetweenTwoGroups::processFirstFrame(std::shared_ptr<Frame> &frame) {
    for (auto &mol : frame->molecule_list) {
        for (auto &atom : mol->atom_list) {
            if (Atom::is_match(atom, this->ids)) {
                mol_list.push_back(mol);
                break;
            }
        }
    }
}
