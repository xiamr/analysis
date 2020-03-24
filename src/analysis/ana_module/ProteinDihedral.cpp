#include "ProteinDihedral.hpp"
#include "data_structure/frame.hpp"
#include "utils/common.hpp"
#include <boost/algorithm/algorithm.hpp>
#include <boost/range/algorithm.hpp>

ProteinDihedral::ProteinDihedral() {
    enable_outfile = true;
    enable_forcefield = true;
}

void ProteinDihedral::process(std::shared_ptr<Frame> &frame) {
    for (std::size_t index = 0; index + 3 < atom_sequence.size(); ++index) {

        const auto &atom1 = atom_sequence[index];
        const auto &atom2 = atom_sequence[index + 1];
        const auto &atom3 = atom_sequence[index + 2];
        const auto &atom4 = atom_sequence[index + 3];

        auto v1_2 = atom2->getCoordinate() - atom1->getCoordinate();
        auto v2_3 = atom3->getCoordinate() - atom2->getCoordinate();
        auto v3_4 = atom4->getCoordinate() - atom3->getCoordinate();

        frame->image(v1_2);
        frame->image(v2_3);
        frame->image(v3_4);

        auto cv1 = cross_multiplication(v1_2, v2_3);
        auto cv2 = cross_multiplication(v2_3, v3_4);

        auto angle =
            radian * std::acos(dot_multiplication(cv1, cv2) / std::sqrt(vector_norm2(cv1) * vector_norm2(cv2)));

        dihedrals[index](angle);
    }
}

void ProteinDihedral::print(std::ostream &os) {

    std::vector<std::pair<std::array<std::string, 4>, double>> formated_items;

    for (std::size_t index = 0; index + 3 < atom_sequence.size(); ++index) {
        std::array<std::string, 4> dihedral_id;
        for (std::size_t j = 0; j < 4; ++j) {
            const auto &atom = atom_sequence[index + j];
            dihedral_id[j] =
                (boost::format("%s%d@%s") % atom->residue_name % atom->residue_num % atom->atom_name).str();
        }
        formated_items.emplace_back(std::move(dihedral_id), boost::accumulators::variance(dihedrals[index]));
    }

    boost::sort(formated_items, [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });

    for (const auto &[names, value] : formated_items) {
        os << boost::format("%s - %s - %s - %s    %15.6f\n") % names[0] % names[1] % names[2] % names[3] %
                  std::sqrt(value);
    }
}

void ProteinDihedral::processFirstFrame(std::shared_ptr<Frame> &frame) {

    std::set<std::shared_ptr<Atom>> atoms;
    std::shared_ptr<Atom> init_atom;

    boost::for_each(frame->atom_list, [this, &atoms, &init_atom](std::shared_ptr<Atom> &atom) {
        if (Atom::is_match(atom, mask)) {
            atoms.insert(atom);
            atom->mark = false;
        }
        if (Atom::is_match(atom, init_mask)) {
            init_atom = atom;
        }
    });

    for (;;) {
    cont:
        atom_sequence.push_back(init_atom);
        init_atom->mark = true;

        for (auto i : init_atom->con_list) {
            const auto &atom = frame->atom_map[i];
            if (atoms.contains(atom) and atom->mark == false) {
                init_atom = atom;
                goto cont;
            }
        }
        break;
    }
    dihedrals.resize(atom_sequence.size() - 3);
}

void ProteinDihedral::readInfo() {
    Atom::select1group(mask, "Enter Protien MainChain > ");
    Atom::select1group(init_mask, "Enter init atom > ");
}