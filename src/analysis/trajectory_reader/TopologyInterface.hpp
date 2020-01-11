#ifndef TINKER_TOPOLOGYINTERFACE_HPP
#define TINKER_TOPOLOGYINTERFACE_HPP

#include <string>
#include <memory>

class Frame;

class TopologyInterface {
public:
    virtual std::shared_ptr<Frame> read(const std::string &filename) = 0;

    virtual ~TopologyInterface() = default;
};


#endif //TINKER_TOPOLOGYINTERFACE_HPP
