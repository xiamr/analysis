

#include <boost/range/algorithm.hpp>

#include "ConvertVelocityToVelocityCharge.hpp"

#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/common.hpp"
#include "utils/trr_writer.hpp"

ConvertVelocityToVelocityCharge::ConvertVelocityToVelocityCharge(std::unique_ptr<TRRWriter> writer)
    : writer(std::move(writer)) {
    enable_read_velocity = true;
}

void ConvertVelocityToVelocityCharge::processFirstFrame(std::shared_ptr<Frame> &frame) {
    writer->open(trr_vq_outfilename);
    writer->setWriteVelocities(true);

    do_select_mol(frame);
}

void ConvertVelocityToVelocityCharge::do_select_mol(std::shared_ptr<Frame> &frame) {
    boost::for_each(frame->atom_list, [this](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, selected_mols_mask)) {
            selected_mols.insert(atom->molecule.lock());
        }
    });
}

void ConvertVelocityToVelocityCharge::process(std::shared_ptr<Frame> &frame) {
    std::tuple<double, double, double> vq;
    for (auto &mol : selected_mols) {
        for (auto &atom : mol->atom_list) {
            vq += atom->charge.value() * atom->getVelocities();
        }
    }
    for (auto &atom : frame->atom_list) {
        atom->vx = atom->vy = atom->vz = 0.0;
    }
    auto &last_atom = frame->atom_list.back();
    last_atom->vx = std::get<0>(vq);
    last_atom->vy = std::get<1>(vq);
    last_atom->vz = std::get<2>(vq);

    writer->setCurrentTime(frame->getCurrentTime().value());
    writer->write(frame);
}

void ConvertVelocityToVelocityCharge::print(std::ostream &os) {
    boost::ignore_unused(os);
    writer->close();
}

void ConvertVelocityToVelocityCharge::readInfo() {
    trr_vq_outfilename = choose_file("Enter trr output filename > ").isExist(false).extension("trr");
    select1group(selected_mols_mask, " Enter molecule mask for dipole calculation > ");
}
