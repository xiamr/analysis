//
// Created by xiamr on 6/14/19.
//

#include "DipoleAngle.hpp"

#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/ThrowAssert.hpp"
#include "utils/common.hpp"

using namespace std;

DipoleAngle::DipoleAngle() {
    enable_forcefield = true;
    enable_outfile = true;
}

void DipoleAngle::process(std::shared_ptr<Frame> &frame) {
    for (auto &ref : group1) {
        for (auto &atom : group2) {
            auto mol = atom->molecule.lock();

            double xr = atom->x - ref->x;
            double yr = atom->y - ref->y;
            double zr = atom->z - ref->z;

            frame->image(xr, yr, zr);

            double distance = sqrt(xr * xr + yr * yr + zr * zr);

            auto [dipole_x, dipole_y, dipole_z] = mol->calc_dipole(frame);

            double dipole_scalar = std::sqrt(dipole_x * dipole_x + dipole_y * dipole_y + dipole_z * dipole_z);

            double _cos = (xr * dipole_x + yr * dipole_y + zr * dipole_z) / (distance * dipole_scalar);

            double angle = acos(_cos) * radian;

            int i_distance_bin = int(distance / distance_width) + 1;
            int i_angle_bin = int(angle / angle_width) + 1;

            if (i_distance_bin <= distance_bins and i_angle_bin <= angle_bins) {
                hist[make_pair(i_distance_bin, i_angle_bin)] += 1;
            }
        }
    }
}

void DipoleAngle::print(std::ostream &os) {
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        size_t total = 0;
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            total += hist[make_pair(i_distance, i_angle)];
        }
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            os << boost::format("%5.3f%10.3f%10.4f\n") % ((i_distance - 0.5) * distance_width) %
                      ((i_angle - 0.5) * angle_width) %
                      (total == 0 ? 0.0 : double(hist[make_pair(i_distance, i_angle)]) / total);
        }
    }
}

void DipoleAngle::readInfo() {
    select2group(mask1, ids2);
    double rmax = choose(0.0, std::numeric_limits<double>::max(),
                         "Enter Maximum Distance to Accumulate[10.0 Ang]:", Default(10.0));
    distance_width =
        choose(0.0, std::numeric_limits<double>::max(), "Enter Width of Distance Bins [0.01 Ang]:", Default(0.01));
    double angle_max = choose(0.0, 180.0, "Enter Maximum Angle to Accumulate[180.0 degree]:", Default(180.0));
    angle_width = choose(0.0, 180.0, "Enter Width of Angle Bins [0.5 degree]:", Default(0.5));

    distance_bins = int(rmax / distance_width);
    angle_bins = int(angle_max / angle_width);

    for (int i_distance = 1; i_distance <= distance_bins; i_distance++) {
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            hist[make_pair(i_distance, i_angle)] = 0;
        }
    }
}

void DipoleAngle::processFirstFrame(std::shared_ptr<Frame> &frame) {
    std::for_each(frame->atom_list.begin(), frame->atom_list.end(), [this](shared_ptr<Atom> &atom) {
        if (is_match(atom, this->mask1)) this->group1.insert(atom);
        if (is_match(atom, this->ids2)) this->group2.insert(atom);
    });
}
