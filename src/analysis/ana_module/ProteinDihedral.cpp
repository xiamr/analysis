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
    for (std::size_t index = 0; index + 2 < atom_sequence.size(); ++index) {

        const auto &atom1 = atom_sequence[index];
        const auto &atom2 = atom_sequence[index + 1];
        const auto &atom3 = atom_sequence[index + 2];

        auto angle = atom_angle(atom1, atom2, atom3, frame);

        angles[index](angle);
    }

    for (std::size_t index = 0; index + 3 < atom_sequence.size(); ++index) {

        const auto &atom1 = atom_sequence[index];
        const auto &atom2 = atom_sequence[index + 1];
        const auto &atom3 = atom_sequence[index + 2];
        const auto &atom4 = atom_sequence[index + 3];

        auto angle = atom_dihedral(atom1, atom2, atom3, atom4, frame);

        dihedrals[index](angle);
    }
}

void ProteinDihedral::print(std::ostream &os) {

    std::vector<std::pair<std::array<std::string, 3>, double>> formated_items_angle;

    for (std::size_t index = 0; index + 3 < atom_sequence.size(); ++index) {
        std::array<std::string, 3> angle_id;
        for (std::size_t j = 0; j < 3; ++j) {
            const auto &atom = atom_sequence[index + j];
            angle_id[j] =
                (boost::format("%s%d@%s") % atom->residue_name.value() % atom->residue_num.value() % atom->atom_name)
                    .str();
        }
        formated_items_angle.emplace_back(std::move(angle_id), boost::accumulators::variance(angles[index]));
    }

    boost::sort(formated_items_angle, [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });

    std::vector<std::pair<std::array<std::string, 4>, double>> formated_items;

    for (std::size_t index = 0; index + 3 < atom_sequence.size(); ++index) {
        std::array<std::string, 4> dihedral_id;
        for (std::size_t j = 0; j < 4; ++j) {
            const auto &atom = atom_sequence[index + j];
            dihedral_id[j] =
                (boost::format("%s%d@%s") % atom->residue_name.value() % atom->residue_num.value() % atom->atom_name)
                    .str();
        }
        formated_items.emplace_back(std::move(dihedral_id), boost::accumulators::variance(dihedrals[index]));
    }

    boost::sort(formated_items, [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });

    os << std::string(50, '#') << '\n';
    os << '#' << title() << '\n';
    os << "#     atoms > " << mask << '\n';
    os << "# init atom > " << init_mask << '\n';
    os << std::string(50, '#') << '\n';
    os << "#  Angle Definition    STD\n";
    for (const auto &[names, value] : formated_items_angle) {
        os << boost::format("%s - %s - %s    %15.6f\n") % names[0] % names[1] % names[2] % std::sqrt(value);
    }

    os << std::string(50, '#') << '\n';
    os << "#  Dihedral Definition    STD\n";
    for (const auto &[names, value] : formated_items) {
        os << boost::format("%s - %s - %s - %s    %15.6f\n") % names[0] % names[1] % names[2] % names[3] %
                  std::sqrt(value);
    }
}

void ProteinDihedral::processFirstFrame(std::shared_ptr<Frame> &frame) {

    std::set<std::shared_ptr<Atom>> atoms;
    std::shared_ptr<Atom> init_atom;

    boost::for_each(frame->atom_list, [this, &atoms, &init_atom](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, mask)) {
            atoms.insert(atom);
            atom->mark = false;
        }
        if (is_match(atom, init_mask)) {
            init_atom = atom;
        }
    });

    for (;;) {
    cont:
        if (init_atom->atom_name == "CA")
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

    angles.resize(atom_sequence.size() - 2);
    dihedrals.resize(atom_sequence.size() - 3);
}

void ProteinDihedral::readInfo() {
    select1group(mask, "Enter Protien Backbone > ");
    select1group(init_mask, "Enter init atom > ");
}