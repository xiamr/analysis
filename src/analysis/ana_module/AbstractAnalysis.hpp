

//  This Pure Virtrial Class is the interface for All trajectory analysis method

#ifndef TINKER_ABSTRACTANALYSIS_HPP
#define TINKER_ABSTRACTANALYSIS_HPP

#include "utils/std.hpp"

class Frame;

class AbstractAnalysis {
public:
    void process_topology(std::shared_ptr<Frame> &frame) { process_topology_impl(frame); }

    virtual void processFirstFrame([[maybe_unused]] std::shared_ptr<Frame> &frame){};

    virtual void process(std::shared_ptr<Frame> &frame) = 0;

    virtual void print(std::ostream &os) = 0;

    virtual void readInfo() = 0;

    [[nodiscard]] virtual std::string getOutfileName() { return outfilename; }

    [[nodiscard]] static std::string title() { return "Abstract Class"; }

    void do_parallel_while(std::function<std::shared_ptr<Frame>()> func) { do_parallel_while_impl(std::move(func)); }

    bool enable_parralle_while() { return enable_paralel_while_impl(); }

    virtual ~AbstractAnalysis() = default;

protected:
    virtual void do_parallel_while_impl([[maybe_unused]] std::function<std::shared_ptr<Frame>()> func){};

    virtual bool enable_paralel_while_impl() { return false; }

    virtual void process_topology_impl([[maybe_unused]] std::shared_ptr<Frame> &frame) {}

    void setOutFilename(std::string outfilename);

    std::string outfilename;
};

#endif  // TINKER_ABSTRACTANALYSIS_HPP
