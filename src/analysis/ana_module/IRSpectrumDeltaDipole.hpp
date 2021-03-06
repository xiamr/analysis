//
// Created by xiamr on 8/17/19.
//

#ifndef TINKER_IRSPECTRUMDELTADIPOLE_HPP
#define TINKER_IRSPECTRUMDELTADIPOLE_HPP

#include "IRSpectrum.hpp"

class IRSpectrumDeltaDipole : public IRSpectrum {
public:
    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    [[nodiscard]] static std::string_view title() { return "Infrared radiation (IR) Spectrum from delta Dipole"; }

    static void calculateSpectrum(const std::string &out);
};

#endif  // TINKER_IRSPECTRUMDELTADIPOLE_HPP
