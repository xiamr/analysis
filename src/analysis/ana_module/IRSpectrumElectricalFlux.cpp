//
// Created by xiamr on 8/13/19.
//

#include "IRSpectrumElectricalFlux.hpp"

#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

#include "IRSpectrum.hpp"
#include "VelocityAutocorrelationFunction.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/common.hpp"

IRSpectrumElectricalFlux::IRSpectrumElectricalFlux() {
    enable_outfile = true;
    enable_read_velocity = true;
    enable_tbb = true;
}

void IRSpectrumElectricalFlux::process([[maybe_unused]] std::shared_ptr<Frame> &frame) {
    if (!frame->has_velocity) {
        std::cerr << "Trajectory does not have velocity\n";
        std::exit(EXIT_FAILURE);
    }

    std::tuple<double, double, double> flux{};

    for (auto &mol : selected_mols) {
        for (auto &atom : mol->atom_list) {
            flux += std::make_tuple(atom->vx, atom->vy, atom->vz) * atom->charge.value();
        }
    }

    electricalFlux.emplace_back(flux);
}

void IRSpectrumElectricalFlux::print(std::ostream &os) {
    auto acf = VelocityAutocorrelationFunction::calculateAcf({electricalFlux}, std::numeric_limits<int>::max());
    os << std::string(50, '#') << '\n';
    os << "# " << title() << '\n';
    os << "# time_increment_ps > " << time_increment_ps << '\n';
    os << "# AmberMask > " << selected_mols_mask << '\n';
    os << std::string(50, '#') << '\n';

    os << boost::format("%15s %15s\n") % "Time(ps)" % "ACF";

    for (std::size_t i = 0; i < acf.size(); ++i) {
        os << boost::format("%15.4f %15.5f\n") % (time_increment_ps * i) % acf[i];
    }

    os << std::string(50, '#') << '\n';

    acf.resize(acf.size() / 3);

    auto intense = IRSpectrum::calculateIntense(acf, time_increment_ps);

    os << boost::format("%15s %15s\n") % "Frequency (cm-1)" % "Intensity";

    for (std::size_t i = 0; i < intense.size(); ++i) {
        os << boost::format("%15.3f %15.5f\n") % (i + 1) % intense[i];
    }

    os << std::string(50, '#') << '\n';
}

void IRSpectrumElectricalFlux::readInfo() {
    time_increment_ps = choose(0.0, 100.0, "time_increment_ps [0.1 ps] :", Default(0.1));
    select1group(selected_mols_mask, " Enter molecule mask for dipole calculation > ");
}

void IRSpectrumElectricalFlux::processFirstFrame(std::shared_ptr<Frame> &frame) {
    boost::for_each(frame->atom_list, [this](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, selected_mols_mask)) {
            selected_mols.insert(atom->molecule.lock());
        }
    });
}
