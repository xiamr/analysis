//
// Created by xiamr on 7/5/19.
//

#ifndef TINKER_DIPOLEVECTORSELECTOR_HPP
#define TINKER_DIPOLEVECTORSELECTOR_HPP

#include <set>

#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"
#include "data_structure/molecule.hpp"
#include "utils/VectorSelector.hpp"

class DipoleVectorSelector : public VectorSelector {
public:
    DipoleVectorSelector();

    int initialize(const std::shared_ptr<Frame> &frame) override;

    [[nodiscard]] std::vector<std::tuple<double, double, double>> calculateVectors(
        const std::shared_ptr<Frame> &frame) override;

    void readInfo() override;

    [[nodiscard]] std::string description() override;

    void setParameters(const AmberMask &id);

    [[nodiscard]] std::tuple<double, double, double> calculateVector(const std::shared_ptr<Molecule> &mol,
                                                                     const std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    [[nodiscard]] static std::string_view title() {
        return "Dipole vector (define by molecule that has selected atom) selector";
    }

protected:
    AmberMask amberMask;

    std::set<std::shared_ptr<Molecule>> selected_mols;
};

#endif  // TINKER_DIPOLEVECTORSELECTOR_HPP
