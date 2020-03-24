//
// Created by xiamr on 6/14/19.
//

#include "PrintTopolgy.hpp"

#include "data_structure/atom.hpp"
#include "data_structure/forcefield.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "dsl/center_selection_grammar.hpp"
#include "trajectory_reader/trajectoryreader.hpp"
#include "utils/EnergyCalculator.hpp"
#include "utils/PBCUtils.hpp"
#include <boost/range/algorithm.hpp>

namespace qi = boost::spirit::qi;

void PrintTopolgy::action(const std::string &topology_filename) {
    TrajectoryReader reader;

    auto t1 = std::chrono::high_resolution_clock::now();

    reader.set_topology(topology_filename);
    auto frame = reader.readTopology();
    if (forcefield.isValid()) {
        forcefield.assign_forcefield(frame);
    }

    enum class Mode {
        Mass,
        Geom,
        Noop,
    } mode;

    std::cout
        << std::left << "Total atom numbers : " << std::setw(10) << frame->atom_list.size()
        << "Total molecule numbers : " << std::setw(10) << frame->molecule_list.size() << "Load time : "
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t1).count()
        << " ms\n";

    for (;;) {
    beg:
        CenterRuleNode r;
        selectCentergroup(r, "> ");
        Atom::Node ast;

        if (auto ret = boost::get<std::shared_ptr<MassCenterRuleNode>>(&r)) {
            ast = (*ret)->SelectionMask;
            mode = Mode::Mass;
        } else if (auto ret = boost::get<std::shared_ptr<GeomCenterRuleNode>>(&r)) {
            ast = (*ret)->SelectionMask;
            mode = Mode::Geom;
        } else if (auto ret = boost::get<std::shared_ptr<NoopRuleNode>>(&r)) {
            ast = (*ret)->SelectionMask;
            mode = Mode::Noop;
        } else if (auto ret = boost::get<std::shared_ptr<EDARuleNode>>(&r)) {
            EnergyCalculator calculator;
            calculator.setMask((*ret)->mask1, (*ret)->mask2, frame);
            auto [ele, lj] = calculator.calculate_energy(frame);
            std::cout << "E(ele) = " << ele << " (kcal/mole)\n"
                      << "E(vdW) = " << lj << " (kcal/mole)\n";
            continue;
        } else if (auto ret = boost::get<std::shared_ptr<BondRuleNode>>(&r)) {
            auto atom1 = PBCUtils::find_atom((*ret)->mask1, frame);
            auto atom2 = PBCUtils::find_atom((*ret)->mask2, frame);

            auto distance = atom_distance(atom1, atom2, frame);

            std::array<int, 2> key{atom1->seq, atom2->seq};
            boost::sort(key);

            if (auto it = frame->f_bond_params.find(key); it != std::end(frame->f_bond_params)) {
                std::cout << boost::format(" K = %g kJ/mol    b0 = %g  Ang    b = %g  Ang\n") % it->second.krA %
                                 it->second.rA % distance;
            } else {
                std::cout << "No bond parameter found !!!\n";
            }
            continue;
        } else if (auto ret = boost::get<std::shared_ptr<AngleRuleNode>>(&r)) {
            std::array atoms{PBCUtils::find_atom((*ret)->mask1, frame), PBCUtils::find_atom((*ret)->mask2, frame),
                             PBCUtils::find_atom((*ret)->mask3, frame)};

            boost::sort(atoms, [](const auto &lhs, const auto &rhs) { return lhs->seq < rhs->seq; });
            auto angle = atom_angle(atoms, frame);
            if (auto it = frame->f_angle_params.find(std::array<int, 3>{atoms[0]->seq, atoms[1]->seq, atoms[2]->seq});
                it != std::end(frame->f_angle_params)) {
                std::cout << boost::format(" K = %g kJ/mol/rad    theta0 = %g  degree    theta = %g  degree\n") %
                                 it->second.krA % it->second.rA % angle;
            } else {
                std::cout << "No angle parameter found !!!\n";
            }
            continue;

        } else if (auto ret = boost::get<std::shared_ptr<DihedralRuleNode>>(&r)) {
            std::array atoms{PBCUtils::find_atom((*ret)->mask1, frame), PBCUtils::find_atom((*ret)->mask2, frame),
                             PBCUtils::find_atom((*ret)->mask3, frame), PBCUtils::find_atom((*ret)->mask4, frame)};

            boost::sort(atoms, [](const auto &lhs, const auto &rhs) { return lhs->seq < rhs->seq; });
            auto angle = atom_dihedral(atoms, frame);
            std::array<int, 4> key{atoms[0]->seq, atoms[1]->seq, atoms[2]->seq, atoms[3]->seq};

            if (auto [lower_bound, upper_bound] = frame->f_dihedral_params.equal_range(key);
                lower_bound != std::end(frame->f_dihedral_params)) {
                for (auto it = lower_bound; it != upper_bound; ++it)
                    std::cout << boost::format(" K = %g kJ/mol    phi0 = %g degree    multi = %d  phi = %g degree\n") %
                                     it->second.cpA % it->second.phiA % it->second.mult % angle;
            } else {
                std::cout << "No dihedral parameter found !!!\n";
            }
            continue;
        } else if (boost::get<std::shared_ptr<QuitRuleNode>>(&r)) {
            break;
        } else if (boost::get<std::shared_ptr<HelpRuleNode>>(&r)) {
            std::cout << "Help : \n"
                      << " 1. com mask\n"
                      << " 2. geom mask\n"
                      << " 3. mask\n"
                      << " 4. eda mask1 mask2\n"
                      << " 5. bond mask1 mask2\n"
                      << " 6. angle mask1 mask2 mask3\n"
                      << " 7. dihedral mask1 mask2 mask3 mask4\n"
                      << " 8. help\n"
                      << " 9. quit\n";
            continue;
        }

        std::cout << boost::format("%-6s %-7s %4s %-7s %4s %-6s  %8s %8s  %8s%8s%8s") % "#Atom" % "Name" % "#Res" %
                         "Name" % "#Mol" % "Type" % "Charge" % "Mass" % "X(Ang)" % "Y(Ang)" % "Z(Ang)";
        if (frame->atom_list.front()->lj_param.has_value())
            std::cout << boost::format("%14s%14s%14s%14s") % "LJ c6" % "LJ c12" % "sigma(nm)" % "eps(kJ/mol)";
        std::cout << '\n';

        double weight = 0;
        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_z = 0.0;
        const boost::format fmt{"%6d %-7s %4s %-7s %4s %-6s  %8s %8s  %8.3f%8.3f%8.3f"};

        for (auto &atom : frame->atom_list) {
            if (Atom::is_match(atom, ast)) {
                std::cout << boost::format(fmt) % atom->seq % atom->atom_name %
                                 (atom->residue_num ? std::to_string(atom->residue_num.get()) : "-") %
                                 (atom->residue_name ? atom->residue_name.get() : "-") %
                                 atom->molecule.lock()->sequence % atom->type_name %
                                 (atom->charge ? (boost::format("%8.4f") % atom->charge.get()).str() : "-") %
                                 (atom->mass ? (boost::format("%8.4f") % atom->mass.get()).str() : "-") % atom->x %
                                 atom->y % atom->z;
                if (atom->lj_param.has_value()) {
                    const auto c6 = (*atom->lj_param).c6;
                    const auto c12 = (*atom->lj_param).c12;
                    const auto &[sigma, epsilon] = atom->get_lj_p();
                    std::cout << boost::format("%14e%14e%14e%14e") % c6 % c12 % sigma % epsilon;
                }
                std::cout << '\n';
                switch (mode) {
                case Mode::Mass:
                    if (!atom->mass) {
                        std::cerr << "atom mass not available !\n";
                        goto beg;
                    }
                    sum_x += atom->x * atom->mass.get();
                    sum_y += atom->y * atom->mass.get();
                    sum_z += atom->z * atom->mass.get();
                    weight += atom->mass.get();
                    break;
                case Mode::Geom:
                    sum_x += atom->x;
                    sum_y += atom->y;
                    sum_z += atom->z;
                    weight++;
                    break;
                case Mode::Noop:
                    break;
                }
            }
        }
        if (weight != 0.0) {
            switch (mode) {
            case Mode::Mass:

                std::cout << boost::format("Mass Center %8s%8s%8s\n") % "X(Ang)" % "Y(Ang)" % "Z(Ang)";
                std::cout << boost::format("            %8.3f%8.3f%8.3f\n") % (sum_x / weight) % (sum_y / weight) %
                                 (sum_z / weight);

                break;
            case Mode::Geom:
                std::cout << boost::format("Geom Center %8s%8s%8s\n") % "X(Ang)" % "Y(Ang)" % "Z(Ang)";
                std::cout << boost::format("            %8.3f%8.3f%8.3f\n") % (sum_x / weight) % (sum_y / weight) %
                                 (sum_z / weight);
                break;
            case Mode::Noop:
                break;
            }
        }
    }
}
