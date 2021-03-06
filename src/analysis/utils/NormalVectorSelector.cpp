//
// Created by xiamr on 7/4/19.
//

#include "NormalVectorSelector.hpp"

#include <iostream>

#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/ThrowAssert.hpp"
#include "utils/common.hpp"

using namespace std;

std::vector<std::tuple<double, double, double>> NormalVectorSelector::calculateVectors(
    const std::shared_ptr<Frame> &frame) {
    std::vector<std::tuple<double, double, double>> vectors;
    for (auto &pair : pairs) {
        vectors.push_back(calVector(pair, frame));
    }
    return vectors;
}

std::tuple<double, double, double> NormalVectorSelector::calVector(
    const std::tuple<std::shared_ptr<Atom>, std::shared_ptr<Atom>, std::shared_ptr<Atom>> &atoms,
    const std::shared_ptr<Frame> &frame) {
    auto &[atom_i, atom_j, atom_k] = atoms;

    auto u = atom_i->getCoordinate() - atom_j->getCoordinate();
    frame->image(u);

    auto v = atom_k->getCoordinate() - atom_j->getCoordinate();
    frame->image(v);

    auto n = cross_multiplication(u, v);

    n /= vector_norm(n);

    return n;
}

void NormalVectorSelector::readInfo() {
    std::cout << "# " << title() << " #\n";
    select1group(ids1, "Please Enter for Atom1 > ");
    select1group(ids2, "Please Enter for Atom2 > ");
    select1group(ids3, "Please Enter for Atom3 > ");
}

void NormalVectorSelector::print(std::ostream &os) {
    os << "# " << title() << "#\n";
    os << "# Group1 > " << ids1 << "\n Group2 > " << ids2 << "\n Group3 > " << ids3 << '\n';
}

int NormalVectorSelector::initialize(const std::shared_ptr<Frame> &frame) {
    if (!pairs.empty()) return pairs.size();
    for (auto &mol : frame->molecule_list) {
        shared_ptr<Atom> atom1, atom2, atom3;
        for (auto &atom : mol->atom_list) {
            if (is_match(atom, ids1)) {
                atom1 = atom;
            } else if (is_match(atom, ids2)) {
                atom2 = atom;
            } else if (is_match(atom, ids3)) {
                atom3 = atom;
            }
        }

        throw_assert((atom1 && atom2 && atom3) or (!atom1 && !atom2 && !atom3), "Atom selection semantic error");
        if (atom1 && atom2 && atom3) {
            pairs.emplace_back(atom1, atom2, atom3);
        }
    }

    throw_assert(!pairs.empty(), "Can not empty");
    return pairs.size();
}

tuple<double, double, double> NormalVectorSelector::calculateVector(const std::shared_ptr<Molecule> &mol,
                                                                    const std::shared_ptr<Frame> &frame) {
    shared_ptr<Atom> atom1, atom2, atom3;
    for (auto &atom : mol->atom_list) {
        if (is_match(atom, ids1)) {
            atom1 = atom;
        } else if (is_match(atom, ids2)) {
            atom2 = atom;
        } else if (is_match(atom, ids3)) {
            atom3 = atom;
        }
    }
    throw_assert(atom1 && atom2 && atom3, "Atom selection semantic error");
    return calVector({atom1, atom2, atom3}, frame);
}

void NormalVectorSelector::setParameters(const AmberMask &id1, const AmberMask &id2, const AmberMask &id3) {
    ids1 = id1;
    ids2 = id2;
    ids3 = id3;
}

string NormalVectorSelector::description() {
    stringstream ss;
    ss << "NormalVector ( [" << ids1 << "] , [" << ids2 << "] , [" << ids3 << "] )";
    return ss.str();
}
