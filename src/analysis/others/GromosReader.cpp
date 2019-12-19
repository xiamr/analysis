#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/function/adapt_function.hpp>
#include <boost/xpressive/xpressive_static.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#include <pugixml.hpp>
#include "GromosReader.hpp"
#include "utils/common.hpp"

namespace bp = boost::fusion;

namespace {
    BOOST_PHOENIX_ADAPT_FUNCTION(void, trim, boost::trim, 1)

    struct Validator : boost::static_visitor<bool> {
        Validator(std::size_t maxTotalEnergyGroupNum, std::size_t maxEnergyGroupNum)
                : max_total_energy_group_num(maxTotalEnergyGroupNum), max_energy_group_num(maxEnergyGroupNum) {}

        bool operator()(const bp::vector<char, std::pair<uint, uint>> &i) const {
            auto g1 = bp::at_c<1>(i).first;
            auto g2 = bp::at_c<1>(i).second;

            return g1 >= 1 and g1 <= max_energy_group_num and g2 >= 1 and g2 <= max_energy_group_num;
        }

        bool operator()(const uint &i) const {
            return i >= 1 and i < max_total_energy_group_num;
        }

    private:
        std::size_t max_total_energy_group_num;
        std::size_t max_energy_group_num;
    };

    struct TitlePrinter : boost::static_visitor<std::string> {

        TitlePrinter(std::vector<std::string> &menuStrings,
                     std::array<std::string, 3> &energyNames,
                     std::vector<std::string> &groupNames) :
                menuStrings(menuStrings),
                energy_names(energyNames),
                group_names(groupNames) {}

        std::string operator()(const bp::vector<char, std::pair<uint, uint>> &i) const {
            auto g1 = bp::at_c<1>(i).first;
            auto g2 = bp::at_c<1>(i).second;

            return energy_names[bp::at_c<0>(i) - 'a'] + "(" + group_names[g1 - 1] + "," + group_names[g2 - 1] + ")";
        }

        std::string operator()(const uint &i) const {
            return menuStrings[i - 1];
        }

    private:
        std::vector<std::string> &menuStrings;
        std::array<std::string, 3> &energy_names;
        std::vector<std::string> &group_names;
    };

    struct EnergyPrinter : boost::static_visitor<double> {

        EnergyPrinter(GromosReader::Energy &e, size_t groupNums) : e(e), group_nums(groupNums) {}

        double operator()(const bp::vector<char, std::pair<uint, uint>> &i) const {
            auto g1 = bp::at_c<1>(i).first;
            auto g2 = bp::at_c<1>(i).second;

            auto index = ((group_nums + 1) * group_nums - (group_nums + 2 - g1) * (group_nums + 1 - g1)) / 2 + g2 - 1;
            return e.intergroups[bp::at_c<0>(i) - 'a'][index];
        }

        double operator()(const uint &i) const {
            return e.energies[i - 1];
        }

    private:
        GromosReader::Energy &e;
        std::size_t group_nums;
    };
}

void GromosReader::process() {
    std::string omd_filename = choose_file("GROMOS omd file > ").isExist(true).extension("omd");

    // mmap all content from file
    boost::iostreams::mapped_file_source file;
    file.open(omd_filename);
    if (!file) {
        std::cerr << "ERROR! GROMOS omd file cannot open \n";
        std::exit(EXIT_FAILURE);
    }

    std::vector<Energy> energies;

    using namespace boost::spirit::qi;
    using namespace boost::phoenix;

    auto timestep_parser = "TIMESTEP" >> eol >>
                                      uint_ >> double_ >> eol
                                      >> "END" >> eol;

    auto energies_parser = "ENERGIES" >> eol
                                      >> +(as_string[lexeme[+(char_ - ':')]][trim(_1)]
                                              >> ':' >> double_ >> eol)
                                      >> eol;

    auto parser = timestep_parser >> energies_parser;

    auto energy_group_parser =
            repeat(3)[as_string[lexeme[+(char_ - char_("0-9"))]][trim(_1)]
                    >> +as_string[lexeme[+char_("0-9") >> char_('-') >> +char_("0-9")]]
                    >> eol >> +(omit[lexeme[+char_("0-9") >> char_('-') >> +char_("0-9")]] >> +double_ >> eol)
                    >> eol];

    using namespace boost::xpressive;
    auto rex = (s1 = "TIMESTEP" >> -*_ >> _n >> _n)
            >> (s2 = -*_ >> _n >> _n)
            >> (s3 = boost::xpressive::repeat<3, 3>(-*_ >> _n >> _n));

    bool bFilled = false;

    std::vector<std::string> menuStrings;
    std::size_t max_length = 0;

    std::array<std::string, 3> energy_names;
    std::vector<std::string> group_names;

    for (cregex_iterator pos(file.begin(), file.end(), rex), end; pos != end; ++pos) {

        bp::vector<uint, double, std::vector<bp::vector<std::string, double>>> attribute;
        if (auto it = (*pos)[1].first;
                !(phrase_parse(it, (*pos)[1].second, parser, ascii::space - boost::spirit::eol, attribute) &&
                  it == (*pos)[1].second)) {
            std::cerr << "omd file is ill-formed !\n";
            std::exit(EXIT_FAILURE);
        };

        std::vector<bp::vector<std::string, std::vector<std::string>, std::vector<double>>> energy_group_attribute;
        if (auto it = (*pos)[3].first;
                !(phrase_parse(it, (*pos)[3].second, energy_group_parser, ascii::space - boost::spirit::eol,
                               energy_group_attribute) && it == (*pos)[3].second)) {
            std::cerr << "omd file is ill-formed !\n";
            std::exit(EXIT_FAILURE);
        };

        Energy e;
        e.step = bp::at_c<0>(attribute);
        e.time = bp::at_c<1>(attribute);

        for (auto &bpv : bp::at_c<2>(attribute)) {
            e.energies.push_back(bp::at_c<1>(bpv));
            if (!bFilled) {
                menuStrings.push_back(bp::at_c<0>(bpv));
                max_length = std::max(max_length, bp::at_c<0>(bpv).size());
            }
        }
        for (const auto &element : energy_group_attribute | boost::adaptors::indexed()) {
            if (!bFilled) {
                energy_names[element.index()] = bp::at_c<0>(element.value());
                group_names = bp::at_c<1>(element.value());
            }
            e.intergroups[element.index()] = bp::at_c<2>(element.value());
        }

        bFilled = true;
        energies.push_back(std::move(e));
    }

    printMenu(menuStrings, max_length);

    std::cout << " >>>--- Energy Group ---<<<< \n";
    std::cout << "Component : (a) " << energy_names[0] << "  (b) " << energy_names[1] << "  (c) " << energy_names[2]
              << '\n';
    std::cout << "Group : " << std::left;
    for (const auto &element : group_names | boost::adaptors::indexed(1)) {
        std::cout << '[' << element.index() << "] " << element.value();
        if (static_cast<std::size_t>(element.index()) != group_names.size()) std::cout << "  ";
    }
    std::cout << '\n';

    boost::spirit::qi::rule<std::string::iterator, std::pair<uint, uint>(), ascii::space_type>
            pair_parser_ = (uint_ >> ',' >> uint_)[_val = construct<std::pair<uint, uint>>(_1, _2)];

    boost::spirit::qi::rule<std::string::iterator,
            bp::vector<char, std::pair<uint, uint>>(), ascii::space_type>
            sel_paser_ = char_("a-c") >> '(' >> pair_parser_ >> ')';

    boost::spirit::qi::rule<std::string::iterator,
            boost::variant<uint, bp::vector<char, std::pair<uint, uint>>>(), ascii::space_type>
            item_parser_ = uint_ | sel_paser_;

    boost::spirit::qi::rule<std::string::iterator,
            std::vector<boost::variant<uint, bp::vector<char, std::pair<uint, uint>>>>(), ascii::space_type>
            input_parser = +item_parser_;

    for (;;) {

        std::cout << "Seleect Items > ";
        std::string line;
        std::getline(std::cin, line);

        std::vector<boost::variant<uint, bp::vector<char, std::pair<uint, uint>>>> attribute;

        if (auto it = begin(line);
                !(phrase_parse(it, end(line), input_parser, ascii::space, attribute) && it == end(line))) {
            std::cerr << "Paser Error !\n" << line << '\n';
            for (auto iter = line.begin(); iter != it; ++iter) std::cout << " ";
            std::cout << "^\n";
            continue;
        }

        Validator validator(menuStrings.size(), group_names.size());

        if (!boost::accumulate(attribute, true, [&validator](bool init, auto &v) {
            return init && boost::apply_visitor(validator, v);
        })) {
            std::cerr << "number out of range\n";
            continue;
        }

        std::ofstream ofs;
        for (;;) {
            std::string outfile = choose_file("Output xvg file > ").isExist(false);
            ofs.open(outfile);
            if (ofs) break;
            std::cerr << "Cannot open file <" << outfile << ">\n";
        }

        ofs << '#' << std::setw(9) << "Time";
        TitlePrinter titlePrinter(menuStrings, energy_names, group_names);
        boost::for_each(attribute, [&](auto &i) { ofs << std::setw(15) << boost::apply_visitor(titlePrinter, i); });
        ofs << '\n';

        for (auto &e : energies) {
            ofs << std::setw(10) << e.time;
            EnergyPrinter energyPrinter(e, group_names.size());
            boost::for_each(attribute,
                            [&](auto &i) { ofs << std::setw(15) << boost::apply_visitor(energyPrinter, i); });
            ofs << '\n';
        }
        break;
    }
}

void GromosReader::printMenu(std::vector<std::string> &menuStrings, std::size_t width) {
    std::cout << ">>>  MENU  <<<<\n" << std::left;
    for (const auto &element : menuStrings | boost::adaptors::indexed(1)) {
        std::cout << '(' << element.index() << ") " << std::setw(width + 2) << element.value();
        if (element.index() % 4 == 0) std::cout << '\n';
    }
    if (menuStrings.size() % 4 != 0) std::cout << '\n';
}

void GromosReader::save2Xml(const std::vector<Energy> &energies,
                            const std::vector<std::string> &menuStrings,
                            const std::array<std::string, 3> &energy_names,
                            const std::vector<std::string> &group_names) {


}
