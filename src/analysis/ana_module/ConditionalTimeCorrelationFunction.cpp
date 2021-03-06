

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/irange.hpp>

#include "ConditionalTimeCorrelationFunction.hpp"

#include "data_structure/frame.hpp"
#include "utils/LegendrePolynomial.hpp"
#include "utils/VectorSelectorFactory.hpp"
#include "utils/common.hpp"

ConditionalTimeCorrelationFunction::ConditionalTimeCorrelationFunction()
    : func_mapping{{1, [this] { calculateFrame(LegendrePolynomialLevel1()); }},
                   {2, [this] { calculateFrame(LegendrePolynomialLevel2()); }},
                   {3, [this] { calculateFrame(LegendrePolynomialLevel3()); }},
                   {4, [this] { calculateFrame(LegendrePolynomialLevel4()); }}} {
    enable_outfile = true;
}

void ConditionalTimeCorrelationFunction::processFirstFrame(std::shared_ptr<Frame> &frame) {
    water_Ow_atoms.reserve(frame->atom_list.size() / 3);
    boost::for_each(frame->atom_list, [this](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, water_Ow_atoms_mask))
            water_Ow_atoms.push_back(atom);
        else if (is_match(atom, reference_atom_mask)) {
            if (reference_atom) {
                std::cerr << "ERROR !! Only one reference atom allowed for module <" << title() << ">\n";
                exit(1);
            } else {
                reference_atom = atom;
            }
        }
    });

    water_Ow_atoms.shrink_to_fit();

    cb_vector.resize(water_Ow_atoms.size(),
                     boost::circular_buffer<std::tuple<double, double, double>>(max_time_gap_frame));
    assert(!cb_vector.empty());

    ctcf.resize(boost::extents[distance_bins][max_time_gap_frame]);
    std::fill_n(ctcf.data(), ctcf.num_elements(), 0);

    cache_x.set_capacity(max_time_gap_frame);
}

void ConditionalTimeCorrelationFunction::process(std::shared_ptr<Frame> &frame) {
    auto it1 = water_Ow_atoms.begin();
    auto it2 = cb_vector.begin();
    cache_x.push_back(std::vector<double>());
    auto &d_ref_vector = cache_x.back();
    d_ref_vector.reserve(water_Ow_atoms.size());
    for (; it1 != water_Ow_atoms.end(); ++it1, ++it2) {
        auto unit_vector = vectorSelector->calculateVector((*it1)->molecule.lock(), frame);
        unit_vector /= vector_norm(unit_vector);
        it2->push_back(unit_vector);
        d_ref_vector.push_back(atom_distance(reference_atom, *it1, frame));
    }
    func_mapping.at(LegendrePolynomial)();
}

template <typename Function>
void ConditionalTimeCorrelationFunction::calculateFrame(Function f) {
    if (cb_vector.at(0).full()) {
        for (std::size_t i = 0; i < water_Ow_atoms.size(); ++i) {
            auto d_ref = cache_x[0][i];
            auto rbin = static_cast<int>(d_ref / distance_width);
            if (rbin < distance_bins) {
                auto &cb = cb_vector[i];
                for (int t = 0; t < max_time_gap_frame; ++t) {
                    ctcf[rbin][t] += f(dot_multiplication(cb[0], cb[t]));
                }
            }
        }
    }
}

void ConditionalTimeCorrelationFunction::print(std::ostream &os) {
    normalize();

    os << std::string(50, '#') << '\n';
    os << "# " << title() << '\n';
    os << "# reference atom > " << reference_atom_mask << '\n';
    os << "# water Ow atoms > " << water_Ow_atoms_mask << '\n';
    os << vectorSelector;
    os << "# LegendrePolynomial = " << LegendrePolynomial << "  [ " << LegendreStr.at(LegendrePolynomial) << " ] \n";
    os << "# distance_width(Ang) > " << distance_width << '\n';
    os << "# max_distance(Ang) > " << max_distance << '\n';
    os << "# time_increment_ps(ps) > " << time_increment_ps << "\n";
    os << "# max_time_gap_ps(ps) > " << max_time_gap_ps << "\n";
    os << std::string(50, '#') << '\n';

    os << format("#%15s %15s %15s\n", "Time(ps)", "Distance(Ang)", "C(r,t)");
    const boost::format fmt("%15.5f %15.5f %15.5f\n");
    for (int t = 0; t < max_time_gap_frame; ++t) {
        for (int rbin = 0; rbin < distance_bins; ++rbin) {
            os << boost::format(fmt) % (t * time_increment_ps) % ((rbin + 0.5) * distance_width) % ctcf[rbin][t];
        }
    }
}

void ConditionalTimeCorrelationFunction::normalize() {
    for (int rbin = 0; rbin < distance_bins; ++rbin) {
        double n_factor = ctcf[rbin][0] == 0.0 ? 0.0 : 1 / ctcf[rbin][0];
        for (int t = 0; t < max_time_gap_frame; ++t) {
            ctcf[rbin][t] *= n_factor;
        }
    }
}

void ConditionalTimeCorrelationFunction::readInfo() {
    select1group(reference_atom_mask, "Enter mask for reference atom > ");
    select1group(water_Ow_atoms_mask, "Enter mask for OW atoms > ");

    vectorSelector = VectorSelectorFactory::getVectorSelector();
    vectorSelector->readInfo();

    std::cout << "Legendre Polynomial\n";
    boost::range::for_each(boost::irange<int>(1, LegendreStr.size() + 1),
                           [](auto i) { std::cout << i << ". " << LegendreStr.at(i) << '\n'; });
    LegendrePolynomial = choose<int>(1, LegendreStr.size(), "select > ");

    distance_width = choose<double>(0, 1000, "Binwidth in r (Ang) [ 0.01 ] > ", Default(0.01));
    max_distance = choose<double>(0, 1000, "Maximum value of r (Ang) [ 10.0 ] >", Default(10.0));

    distance_bins = static_cast<int>(max_distance / distance_width);

    time_increment_ps =
        choose(0.0, std::numeric_limits<double>::max(), "Enter the Time Increment in Picoseconds [0.1]:", Default(0.1));

    max_time_gap_ps = choose(0.0, std::numeric_limits<double>::max(), "Enter the Max Time Gap in Picoseconds :");

    max_time_gap_frame = std::ceil(max_time_gap_ps / time_increment_ps);
}

void ConditionalTimeCorrelationFunction::setParameters(const AmberMask &M, const AmberMask &L,
                                                       const std::shared_ptr<VectorSelector> &vector, int P,
                                                       double width, double max_r, double time_increment_ps,
                                                       double max_time_gap_ps, const std::string &out) {
    reference_atom_mask = M;
    water_Ow_atoms_mask = L;

    vectorSelector = vector;

    LegendrePolynomial = P;

    distance_width = width;
    max_distance = max_r;

    distance_bins = static_cast<int>(max_distance / distance_width);

    this->time_increment_ps = time_increment_ps;
    this->max_time_gap_ps = max_time_gap_ps;
    max_time_gap_frame = std::ceil(max_time_gap_ps / time_increment_ps);

    this->outfilename = out;
    boost::trim(this->outfilename);
    if (this->outfilename.empty()) {
        throw std::runtime_error("outfilename cannot empty");
    }
}
