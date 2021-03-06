

#ifndef TINKER_CONVERTVELOCITYTOVELOCITYCHARGE_HPP
#define TINKER_CONVERTVELOCITYTOVELOCITYCHARGE_HPP

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"
#include "utils/std.hpp"
#include "utils/trr_writer.hpp"

class Frame;

class ConvertVelocityToVelocityCharge : public AbstractAnalysis {
public:
    ConvertVelocityToVelocityCharge(std::unique_ptr<TRRWriter> writer = std::make_unique<TRRWriter>());

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() {
        return "Convert velocities to velocities * charge in order to use velacc of gmx";
    }

protected:
    virtual void do_select_mol(std::shared_ptr<Frame> &frame);

    std::unique_ptr<TRRWriter> writer;

    std::string trr_vq_outfilename;

    AmberMask selected_mols_mask;

    std::unordered_set<std::shared_ptr<Molecule>> selected_mols;
};

#endif  // TINKER_CONVERTVELOCITYTOVELOCITYCHARGE_HPP
