#ifndef TINKER_MULTIWFNAIMDRIVER_HPP
#define TINKER_MULTIWFNAIMDRIVER_HPP

#include <tuple>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/container_hash/hash.hpp>


class MultiwfnAIMDriver {
public:

    [[nodiscard]] static std::string_view title() { return "QTAIM analysis of BCP"; }

    static void process(const std::string &options);

    static void process_interactive();

private:

    struct BCP_property {
        double Density_of_all_electrons;
        double Hb;
        double Laplacian_of_electron_density;
        double Ellipticity;
    };

    template<typename X> friend
    struct boost::hash;

    static boost::bimap<boost::bimaps::unordered_set_of<double BCP_property::*>,
            boost::bimaps::unordered_set_of<std::string>> property_names, property_options;


    struct BCP {
        BCP(int i, int j) : atom_pair_belonging{i, j} {}

        int index{};
        std::tuple<double, double, double> coord;
        std::pair<int, int> atom_pair_belonging;

        BCP_property property{};
    };

    static void readBCP(std::istream &is, std::vector<BCP> &bcp_vector);

    static void print(const std::vector<BCP> &bcp_vector,
                      const std::vector<double BCP_property::*> &output_field);

    [[nodiscard]] static std::tuple<std::string, int, std::vector<int>> inputParameter();

    static void printProperty(const boost::format &fmt, const std::vector<BCP> &bcp_vector,
                              std::string_view name, double BCP_property::* field);

    static void process(const std::string &file, const std::vector<std::pair<int, int>> &bonds,
                        const std::vector<double BCP_property::*> &output_field);

    [[nodiscard]] static std::vector<double BCP_property::*> parse_options(const std::string &option_string);

};


#endif //TINKER_MULTIWFNAIMDRIVER_HPP
