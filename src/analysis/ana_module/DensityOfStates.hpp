//
// Created by xiamr on 8/13/19.
//

#ifndef TINKER_DENSITYOFSTATES_HPP
#define TINKER_DENSITYOFSTATES_HPP

#include "VelocityAutocorrelationFunction.hpp"

class DensityOfStates : public VelocityAutocorrelationFunction {
public:
    void print(std::ostream &os) override;

    [[nodiscard]] static std::string_view title() { return "Density of States (DOS)"; }
};

#endif  // TINKER_DENSITYOFSTATES_HPP
