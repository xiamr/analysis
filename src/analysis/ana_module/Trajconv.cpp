//
// Created by xiamr on 6/14/19.
//

#include "Trajconv.hpp"

#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/PBCUtils.hpp"
#include "utils/common.hpp"
#include <algorithm>
#include <boost/range/algorithm.hpp>

Trajconv::Trajconv(std::shared_ptr<TrajectoryWriterFactoryInterface> factory)
    : factory(std::move(factory)), pbc_utils(std::make_shared<PBCUtils>()) {}

void Trajconv::process(std::shared_ptr<Frame> &frame) {
    pbc_utils->doPBC(pbc_type, mask, frame);

    for (auto &[name, w] : writers) {
        w->write(frame, atoms_for_writetraj);
    }
    step++;
}

void Trajconv::print(std::ostream &) { CleanUp(); }

void Trajconv::CleanUp() {
    for (auto &[name, w] : writers) {
        w->close();
    }
}

void Trajconv::readInfo() {
    inputOutputFiles();
    selectPBCMode();
}

void Trajconv::inputOutputFiles(std::istream &in, std::ostream &out) {
    for (;;) {
        std::string filename = choose_file("output file [empty for next]: ", in, out).isExist(false).can_empty(true);
        if (filename.empty()) {
            if (!writers.empty()) {
                break;
            }
        }
        try {
            writers.emplace_back(filename, factory->make_instance(getFileType(filename)));
        } catch (std::exception &) {
            out << "ERROR !!  wrong type of target trajectory file (" << filename << ")\n";
        }
    }
}

void Trajconv::selectPBCMode() {
    std::cout << "PBC transform option\n";
    std::cout << "(0) Do nothing\n";
    std::cout << "(1) Make atom i as center\n";
    std::cout << "(2) Make molecule i as center\n";
    std::cout << "(3) Make atom group as center\n";
    std::cout << "(4) all atoms into box\n";

    while (true) {
        int option = choose(0, 4, "Choose : ");
        switch (option) {
        case 0:
            pbc_type = PBCType::None;
            break;
        case 1:
            pbc_type = PBCType::OneAtom;
            select1group(mask, "Please enter atom mask : ");
            break;
        case 2:
            pbc_type = PBCType::OneMol;
            select1group(mask, "Please enter one atom mask that the molecule include: ");
            break;
        case 3:
            pbc_type = PBCType::AtomGroup;
            select1group(mask, "Please enter atom group: ");
            break;
        case 4:
            pbc_type = PBCType::AllIntoBox;
            break;
        default:
            std::cerr << "option not found !\n";
            continue;
        }
        break;
    }
    select1group(mask_for_writetraj, "Enter mask for output > ", true);
}

void Trajconv::fastConvertTo(std::string target) noexcept(false) {
    boost::trim(target);
    if (target.empty()) {
        throw std::runtime_error("ERROR !! empty target trajectory file");
    }
    pbc_type = PBCType::None;

    try {
        writers.emplace_back(target, factory->make_instance(getFileType(target)));
    } catch (std::exception &) {
        throw std::runtime_error("ERROR !!  wrong type of target trajectory file (" + target + ")");
    }
}

void Trajconv::processFirstFrame(std::shared_ptr<Frame> &frame) {
    for (auto &[name, w] : writers) {
        w->open(name);
    }

    atoms_for_writetraj =
        isBlank(mask_for_writetraj) ? frame->atom_list : PBCUtils::find_atoms(mask_for_writetraj, frame);
}

void Trajconv::setParameters(const std::string &out, const std::string &pbc, const AmberMask &pbcmask,
                             const AmberMask &outmask) {
    writers.emplace_back(out, factory->make_instance(getFileType(out)));

    static std::map<std::string, PBCType> mapping{{"", PBCType::None},
                                                  {"oneAtom", PBCType::OneAtom},
                                                  {"oneMol", PBCType::OneMol},
                                                  {"atomGroup", PBCType::AtomGroup},
                                                  {"allIntoBox", PBCType::AllIntoBox}};

    if (auto it = mapping.find(pbc); it != mapping.end()) {
        pbc_type = it->second;
        mask = pbcmask;
        mask_for_writetraj = outmask;
    } else {
        std::cerr << "pbc parameter must be one of oneAtom, oneMol, atomGroup, allIntoBox or empty\n";
        exit(EXIT_FAILURE);
    }
}
