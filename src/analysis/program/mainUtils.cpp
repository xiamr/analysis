
#include <chrono>
#include <cmath>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#if __has_include(<source_location>)
#include <source_location>
#else
#include <experimental/source_location>
using namespace std::experimental;
#endif

#include <boost/algorithm/cxx11/one_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/program_options.hpp>

#include <tbb/tbb.h>

#include "ana_module/AbstractAnalysis.hpp"
#include "ana_module/AngleDistributionBetweenTwoVectorWithCutoff.hpp"
#include "ana_module/Cluster.hpp"
#include "ana_module/ConditionalTimeCorrelationFunction.hpp"
#include "ana_module/CoordinateNumPerFrame.hpp"
#include "ana_module/CoplaneIndex.hpp"
#include "ana_module/DemixIndexOfTwoGroup.hpp"
#include "ana_module/Diffuse.hpp"
#include "ana_module/DiffuseCutoff.hpp"
#include "ana_module/DipoleVectorSelector.hpp"
#include "ana_module/Distance.hpp"
#include "ana_module/HBond.hpp"
#include "ana_module/HBondLifeTime.hpp"
#include "ana_module/HBondLifeTimeContinuous.hpp"
#include "ana_module/OrientationResolvedRadialDistributionFunction.hpp"
#include "ana_module/RMSDCal.hpp"
#include "ana_module/RMSFCal.hpp"
#include "ana_module/RadicalDistribtuionFunction.hpp"
#include "ana_module/RadiusOfGyration.hpp"
#include "ana_module/ResidenceTime.hpp"
#include "ana_module/RotAcf.hpp"
#include "ana_module/RotAcfCutoff.hpp"
#include "ana_module/ShellDensity.hpp"
#include "ana_module/Trajconv.hpp"
#include "ana_module/VelocityAutocorrelationFunction.hpp"
#include "data_structure/forcefield.hpp"
#include "data_structure/frame.hpp"
#include "dsl/Interpreter.hpp"
#include "mainUtils.hpp"
#include "others/PrintTopolgy.hpp"
#include "program/taskMenu.hpp"
#include "trajectory_reader/trajectoryreader.hpp"
#include "utils/IntertiaVector.hpp"
#include "utils/NormalVectorSelector.hpp"
#include "utils/ProgramConfiguration.hpp"
#include "utils/ThrowAssert.hpp"
#include "utils/TwoAtomVectorSelector.hpp"
#include "utils/TypeUtility.hpp"
#include "utils/common.hpp"

using namespace std;

void processOneFrame(shared_ptr<Frame> &frame, shared_ptr<list<shared_ptr<AbstractAnalysis>>> &task_list) {
    for (auto &task : *task_list) {
        task->process(frame);
    }
}

void processFirstFrame(shared_ptr<Frame> &frame, shared_ptr<list<shared_ptr<AbstractAnalysis>>> &task_list) {
    for (auto &task : *task_list) {
        task->processFirstFrame(frame);
    }
}

void fastTrajectoryConvert(const boost::program_options::variables_map &vm, const vector<std::string> &xyzfiles) {
    if (!vm.count("file")) {
        cerr << "ERROR !! trajctory file not set !\n";
        exit(EXIT_FAILURE);
    }

    auto reader = make_shared<TrajectoryReader>();
    if (boost::algorithm::one_of_equal<std::initializer_list<FileType>>(
            {FileType::NC, FileType::XTC, FileType::TRR, FileType::JSON, FileType::GRO}, getFileType(xyzfiles[0]))) {
        if (!vm.count("topology")) {
            cerr << "ERROR !! topology file not set !\n";
            exit(EXIT_FAILURE);
        }
        string topol = vm["topology"].as<string>();
        if (!boost::filesystem::exists(topol)) {
            cerr << "ERROR !! topology file not exist !\n";
            exit(EXIT_FAILURE);
        }
        reader->set_topology(topol);
    } else {
        if (vm.count("topology")) {
            cerr << "WRANING !!  do not use topolgy file !\n";
        }
    }

    for (auto &traj : xyzfiles) {
        string ext = ext_filename(traj);
        if (ext == "traj" && xyzfiles.size() != 1) {
            cerr << "traj file can not use multiple files\n";
            exit(EXIT_FAILURE);
        }
        reader->add_trajectoy_file(traj);
    }

    shared_ptr<Frame> frame;

    Trajconv writer;
    try {
        writer.fastConvertTo(vm["target"].as<string>());
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
    }

    auto time_before_process = std::chrono::steady_clock::now();
    cout << "Fast Trajectory Convert...\n";

    int current_frame_num = 0;
    while ((frame = reader->readOneFrame())) {
        current_frame_num++;
        if (current_frame_num == 1) {
            cout << "Processing Coordinate Frame  " << current_frame_num << "   " << flush;
            writer.processFirstFrame(frame);
        } else if (current_frame_num % 10 == 0) {
            cout << "\rProcessing Coordinate Frame  " << current_frame_num << "   " << flush;
        }
        writer.process(frame);
    }
    writer.CleanUp();
    auto total_time = chrono_cast(std::chrono::steady_clock::now() - time_before_process);
    cout << "\nMission Complete :) Time used  " << total_time << endl;
}

void printTopolgy(const boost::program_options::variables_map &vm) {
    if (vm.count("prm")) {
        auto ff = vm["prm"].as<std::string>();
        if (boost::filesystem::exists(ff)) {
            forcefield.read(ff);
        } else {
            std::cout << "force field file " << ff << " is bad !" << std::endl;
        }
    }
    if (vm.count("topology")) {
        string topol = vm["topology"].as<string>();
        if (boost::filesystem::exists(topol)) {
            PrintTopolgy::action(topol);
            exit(EXIT_SUCCESS);
        }
        cout << "topology file " << topol << " is not exist! please retype !" << endl;
    }
    PrintTopolgy::action(choose_file("input topology file : ").isExist(true));
}

namespace {

template <typename R, typename T, typename... ARGS>
auto constexpr get_args_number(R (T::*)(ARGS...)) {
    return sizeof...(ARGS);
}

template <typename T>
struct FunObject {
    FunObject(std::shared_ptr<list<shared_ptr<AbstractAnalysis>>> &task_list, std::string name,
              source_location location = source_location::current())
        : task_list(task_list), name(std::move(name)), location(std::move(location)) {}

    template <typename U>
    boost::any operator()(U &&args) {
        auto task = make_shared<T>();
        try {
            setParameters(task, std::forward<U>(args),
                          std::make_integer_sequence<std::size_t, get_args_number(&T::setParameters)>{});
        } catch (std::exception &e) {
            cerr << e.what() << " for function " << name << " (" << location.file_name() << ":" << location.line()
                 << ")\n";
            exit(EXIT_FAILURE);
        }
        print_desciption(task);
        task_list->emplace_back(task);
        return static_pointer_cast<AbstractAnalysis>(task);
    }

private:
    template <typename U, typename Args, std::size_t... Numbers>
    void setParameters(U &&task, Args &&args, std::integer_sequence<std::size_t, Numbers...>) {
        task->setParameters(AutoConvert(get<3>(args.at(Numbers)))...);
    }

    template <typename U>
    void print_desciption(U &) {}

    template <typename U, typename = decltype(U::description)>
    void print_desciption(shared_ptr<U> &task) {
        cout << task->description();
    }
    std::shared_ptr<list<shared_ptr<AbstractAnalysis>>> &task_list;
    std::string name;
    source_location location;
};

}  // namespace
void executeScript([[maybe_unused]] const boost::program_options::options_description &desc,
                   const boost::program_options::variables_map &vm, std::vector<std::string> &xyzfiles, int argc,
                   char *argv[]) {
    string scriptContent;

    boost::optional<string> script_file;
    if (vm.count("script")) {
        scriptContent = vm["script"].as<string>();

    } else if (vm.count("script-file")) {
        try {
            script_file = vm["script-file"].as<string>();
            ifstream f(script_file.value());
            string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            scriptContent = content;

        } catch (std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(EXIT_FAILURE);
        }
    } else {
        std::cerr << "program option error  \n";
        std::exit(EXIT_FAILURE);
    }

    boost::optional<string> topology;
    if (vm.count("topology")) topology = vm["topology"].as<string>();
    boost::optional<string> forcefield_file;
    if (vm.count("prm")) forcefield_file = vm["prm"].as<string>();
    boost::optional<string> output_file;
    if (vm.count("output")) output_file = vm["output"].as<string>();

    boost::any ast;
    auto it = scriptContent.begin();
    bool status = qi::phrase_parse(it, scriptContent.end(), InterpreterGrammarT(), SkipperT(), ast);

    if (!(status and it == scriptContent.end())) {
        std::cerr << "Syntax Parse Error\n";
        std::cerr << "error-pos : " << std::endl;
        std::cout << scriptContent << std::endl;
        for (auto iter = scriptContent.begin(); iter != it; ++iter) std::cerr << " ";
        std::cerr << "^" << std::endl;
        exit(EXIT_FAILURE);
    }

    Interpreter interpreter;

    auto task_list = make_shared<list<shared_ptr<AbstractAnalysis>>>();

#define REGISTER(name, type) interpreter.registerFunction(name, FunObject<type>{task_list, name})

    REGISTER("trajconv", Trajconv)
        .addArgument<string>("out")
        .addArgument<string>("pbc")
        .addArgument<AmberMask>("pbcmask", AmberMask{})
        .addArgument<AmberMask>("outmask", AmberMask{});

    REGISTER("distance", Distance).addArgument<AmberMask>("A").addArgument<AmberMask>("B").addArgument<string>("out");

    REGISTER("angle", Angle)
        .addArgument<IntertiaVector>("v1")
        .addArgument<IntertiaVector>("v2")
        .addArgument<string>("out");

    REGISTER("cn_per_frame", CoordinateNumPerFrame)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<double, int>("cutoff")
        .addArgument<string>("out");

    REGISTER("rdf", RadicalDistribtuionFunction)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<double, int>("max_dist", 10.0)
        .addArgument<double, int>("width", 0.01)
        .addArgument<bool>("intramol", false)
        .addArgument<string>("out");

    REGISTER("shelldensity", ShellDensity)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<double, int>("max_dist", 10.0)
        .addArgument<double, int>("width", 0.01)
        .addArgument<string>("out");

    REGISTER("cluster", Cluster)
        .addArgument<AmberMask>("mask")
        .addArgument<double, int>("cutoff")
        .addArgument<string>("out");

    REGISTER("rmsd", RMSDCal)
        .addArgument<AmberMask>("superpose")
        .addArgument<AmberMask>("rms")
        .addArgument<string>("out");

    REGISTER("rmsf", RMSFCal)
        .addArgument<AmberMask>("superpose")
        .addArgument<AmberMask>("rmsf")
        .addArgument<bool>("avg_residue", false)
        .addArgument<string>("out");

    REGISTER("vacf", VelocityAutocorrelationFunction)
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<double, int>("max_time_gap_ps", 100)
        .addArgument<string>("out");

    REGISTER("orrdf", OrientationResolvedRadialDistributionFunction)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<shared_ptr<VectorSelector>>("vector")
        .addArgument<double, int>("dist_width", 0.01)
        .addArgument<double, int>("ang_width", 0.5)
        .addArgument<double, int>("max_dist", 10.0)
        .addArgument<double, int>("temperature", 298)
        .addArgument<string>("out");

    REGISTER("rg", RadiusOfGyration)
        .addArgument<AmberMask>("mask")
        .addArgument<bool>("includeHydrogen", false)
        .addArgument<string>("out");

    REGISTER("ctcf", ConditionalTimeCorrelationFunction)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<shared_ptr<VectorSelector>>("vector")
        .addArgument<int>("P")
        .addArgument<double, int>("width", 0.01)
        .addArgument<double, int>("max_r", 10.0)
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<double, int>("max_time_gap_ps", 100)
        .addArgument<string>("out");

    REGISTER("rotacf", RotAcf)
        .addArgument<shared_ptr<VectorSelector>>("vector")
        .addArgument<int>("P")
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<double, int>("max_time_gap_ps")
        .addArgument<string>("out");

    REGISTER("rotacfCutoff", RotAcfCutoff)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<shared_ptr<VectorSelector>>("vector")
        .addArgument<int>("P")
        .addArgument<double, int>("cutoff")
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<double, int>("max_time_gap_ps")
        .addArgument<string>("out");

    REGISTER("demix", DemixIndexOfTwoGroup)
        .addArgument<AmberMask>("component1")
        .addArgument<AmberMask>("component2")
        .addArgument<Grid>("grid")
        .addArgument<string>("out");

    REGISTER("residenceTime", ResidenceTime)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<double, int>("cutoff")
        .addArgument<double, int>("time_star")
        .addArgument<string>("out");

    REGISTER("diffuse", Diffuse)
        .addArgument<AmberMask>("mask")
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<int>("total_frames")
        .addArgument<string>("out");

    REGISTER("diffuseCutoff", DiffuseCutoff)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<double, int>("cutoff")
        .addArgument<double, int>("time_increment_ps", 0.1)
        .addArgument<string>("out");

    REGISTER("crossAngleCutoff", AngleDistributionBetweenTwoVectorWithCutoff)
        .addArgument<AmberMask>("M")
        .addArgument<AmberMask>("L")
        .addArgument<shared_ptr<VectorSelector>>("vector1")
        .addArgument<shared_ptr<VectorSelector>>("vector2")
        .addArgument<double, int>("angle_max", 180)
        .addArgument<double, int>("angle_width", 0.5)
        .addArgument<double, int>("cutoff1", 0)
        .addArgument<double, int>("cutoff2")
        .addArgument<string>("out");

    REGISTER("cpi", CoplaneIndex).addArgument<string>("mask_list").addArgument<string>("out");

    REGISTER("hbond", HBond)
        .addArgument<AmberMask>("donor")
        .addArgument<AmberMask>("acceptor")
        .addArgument<double, int>("distance")
        .addArgument<double, int>("angle")
        .addArgument<string>("criteria")
        .addArgument<string>("out");

    REGISTER("hbondlifei", HBondLifeTime)
        .addArgument<AmberMask>("water_mask")
        .addArgument<double, int>("dist_R_cutoff")
        .addArgument<double, int>("angle_HOO_cutoff")
        .addArgument<double, int>("time_increment_ps")
        .addArgument<double, int>("max_time_gap_ps")
        .addArgument<string>("out");

    REGISTER("hbondlifec", HBondLifeTimeContinuous)
        .addArgument<AmberMask>("water_mask")
        .addArgument<double, int>("dist_R_cutoff")
        .addArgument<double, int>("angle_HOO_cutoff")
        .addArgument<double, int>("time_increment_ps")
        .addArgument<double, int>("max_time_gap_ps")
        .addArgument<string>("out");

    interpreter
        .registerFunction("DipoleVector",
                          [](auto &args) -> boost::any {
                              auto vector = make_shared<DipoleVectorSelector>();
                              try {
                                  vector->setParameters(boost::any_cast<AmberMask>(get<3>(args.at(0))));
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function DipoleVector (" << __FILE__ << ":" << __LINE__
                                       << ")\n";
                                  exit(EXIT_FAILURE);
                              }
                              return static_pointer_cast<VectorSelector>(vector);
                          })
        .addArgument<AmberMask>("mask");

    interpreter
        .registerFunction(
            "NormalVector",
            [](auto &args) -> boost::any {
                auto vector = make_shared<NormalVectorSelector>();
                try {
                    vector->setParameters(AutoConvert(get<3>(args.at(0))), AutoConvert(get<3>(args.at(1))),
                                          AutoConvert(get<3>(args.at(2))));
                } catch (std::exception &e) {
                    cerr << e.what() << " for function NormalVector (" << __FILE__ << ":" << __LINE__ << ")\n";
                    exit(EXIT_FAILURE);
                }
                return static_pointer_cast<VectorSelector>(vector);
            })
        .addArgument<AmberMask>("mask1")
        .addArgument<AmberMask>("mask2")
        .addArgument<AmberMask>("mask3");

    interpreter
        .registerFunction(
            "TwoAtomVector",
            [](auto &args) -> boost::any {
                auto vector = make_shared<TwoAtomVectorSelector>();
                try {
                    vector->setParameters(AutoConvert(get<3>(args.at(0))), AutoConvert(get<3>(args.at(1))));
                } catch (std::exception &e) {
                    cerr << e.what() << " for function TwoAtomVector (" << __FILE__ << ":" << __LINE__ << ")\n";
                    exit(EXIT_FAILURE);
                }
                return static_pointer_cast<VectorSelector>(vector);
            })
        .addArgument<AmberMask>("mask1")
        .addArgument<AmberMask>("mask2");

    interpreter
        .registerFunction("IntertiaVector",
                          [](auto &args) -> boost::any {
                              return IntertiaVector(AutoConvert(get<3>(args.at(0))), AutoConvert(get<3>(args.at(1))));
                          })
        .addArgument<AmberMask>("mask")
        .addArgument<string>("axis");

    interpreter
        .registerFunction("Grid",
                          [](auto &args) -> boost::any {
                              try {
                                  return Grid{AutoConvert(get<3>(args.at(0))), AutoConvert(get<3>(args.at(1))),
                                              AutoConvert(get<3>(args.at(2)))};
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function Grid (" << __FILE__ << ":" << __LINE__ << ")\n";
                                  exit(EXIT_FAILURE);
                              }
                          })
        .addArgument<int>("x")
        .addArgument<int>("y")
        .addArgument<int>("z");

    interpreter
        .registerFunction("readTop",
                          [&topology](auto &args) -> boost::any {
                              try {
                                  topology = AutoConvert(get<3>(args.at(0)));
                                  return topology.value();
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function readTop (" << __FILE__ << ":" << __LINE__ << ")\n";
                                  exit(EXIT_FAILURE);
                              }
                          })
        .addArgument<string>("file");

    interpreter
        .registerFunction("trajin",
                          [&xyzfiles](auto &args) -> boost::any {
                              try {
                                  string xyz = AutoConvert(get<3>(args.at(0)));
                                  xyzfiles.emplace_back(xyz);
                                  return xyz;
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function trajin (" << __FILE__ << ":" << __LINE__ << ")\n";
                                  exit(EXIT_FAILURE);
                              }
                          })
        .addArgument<string>("file");

    interpreter
        .registerFunction("readFF",
                          [&forcefield_file](auto &args) -> boost::any {
                              try {
                                  forcefield_file = AutoConvert(get<3>(args.at(0)));
                                  return forcefield_file.value();
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function readFF (" << __FILE__ << ":" << __LINE__ << ")\n";
                                  exit(EXIT_FAILURE);
                              }
                          })
        .addArgument<string>("file");

    interpreter
        .registerFunction("go",
                          [&](auto &args) -> boost::any {
                              int start, total_frames, step_size, nthreads;
                              try {
                                  start = AutoConvert(get<3>(args.at(0)));
                                  total_frames = AutoConvert(get<3>(args.at(1)));
                                  step_size = AutoConvert(get<3>(args.at(2)));
                                  nthreads = AutoConvert(get<3>(args.at(3)));
                              } catch (std::exception &e) {
                                  cerr << e.what() << " for function go (" << __FILE__ << ":" << __LINE__ << ")\n";
                                  exit(EXIT_FAILURE);
                              }

                              return executeAnalysis(xyzfiles, argc, argv, scriptContent, script_file, topology,
                                                     forcefield_file, output_file, task_list, start, total_frames,
                                                     step_size, nthreads);
                          })
        .addArgument<int>("start", 1)
        .addArgument<int>("end", 0)
        .addArgument<int>("step", 1)
        .addArgument<int>("nthreads", 0);

    interpreter.execute(ast);

    if (!task_list->empty()) {
        cout << "Program terminated with " << task_list->size() << " task(s) unperformed !\n";
        cout << "Don't forget to put go function in the end of script\n";
    }
    cout << " < Mission Complete >\n";
}

int executeAnalysis(const vector<string> &xyzfiles, int argc, char *const *argv, const string &scriptContent,
                    boost::optional<string> &script_file, boost::optional<string> &topology,
                    boost::optional<string> &forcefield_file, const boost::optional<string> &output_file,
                    shared_ptr<list<shared_ptr<AbstractAnalysis>>> &task_list, int start, int total_frames,
                    int step_size, int nthreads, const std::string &mask_string) {
    if (task_list->empty()) {
        cerr << "Empty task in the pending list, skip go function ...\n";
        return 0;
    }

    auto start_time = chrono::steady_clock::now();

    cout << boost::format("Start Process...  start = %d, end = %s, step = %d, nthreads = %s\n") % start %
                (total_frames == 0 ? "all" : to_string(total_frames)) % step_size %
                (nthreads == 0 ? "automatic" : to_string(nthreads));

    if (start <= 0) {
        cerr << "start frame cannot less than 1\n";
        exit(EXIT_FAILURE);
    }
    if (total_frames <= start and total_frames != 0) {
        cerr << format("end(%d) frame cannot less than start(%d) frame\n", total_frames, start);
        exit(EXIT_FAILURE);
    }
    if (step_size <= 0) {
        cerr << "frame step cannot less than 1\n";
        exit(EXIT_FAILURE);
    }
    if (nthreads < 0) {
        cerr << "thread number cannot less than zero\n";
        exit(EXIT_FAILURE);
    }
    if (enable_forcefield) {
        if (topology && getFileType(topology.value()) != FileType::ARC) {
        } else if (forcefield_file) {
            if (boost::filesystem::exists(forcefield_file.value())) {
                forcefield.read(forcefield_file.value());
            } else {
                cerr << "force field file " << forcefield_file.value() << " is bad  !\n";
                exit(EXIT_FAILURE);
            }
        } else {
            cerr << "force field file not given !\n";
            exit(EXIT_FAILURE);
        }
    }

    tbb::task_scheduler_init tbb_init(tbb::task_scheduler_init::deferred);
    if (nthreads > sysconf(_SC_NPROCESSORS_ONLN)) {
        cout << "WARNING !! nthreads larger than max core of system CPU, will use automatic mode\n";
        nthreads = 0;
    }

    tbb_init.initialize(nthreads == 0 ? tbb::task_scheduler_init::automatic : nthreads);

    auto reader = make_shared<TrajectoryReader>();
    bool b_added_topology = true;
    if (boost::algorithm::one_of_equal<initializer_list<FileType>>(
            {FileType::NC, FileType::XTC, FileType::TRR, FileType::JSON, FileType::GRO}, getFileType(xyzfiles[0]))) {
        b_added_topology = false;
    } else {
        if (topology) {
            cerr << "WRANING !!  do not use topolgy file !\n";
        }
    }

    if (xyzfiles.empty()) {
        cerr << "trajectory file not set\n";
        exit(EXIT_FAILURE);
    }
    for (auto &xyzfile : xyzfiles) {
        reader->add_trajectoy_file(xyzfile);
        string ext = ext_filename(xyzfile);
        if (ext == "traj" && xyzfiles.size() != 1) {
            cout << "traj file can not use multiple files" << endl;
            exit(EXIT_FAILURE);
        }
        if (!b_added_topology) {
            if (topology) {
                if (boost::filesystem::exists(topology.value())) {
                    reader->set_topology(topology.value());
                    b_added_topology = true;
                    continue;
                }
                cerr << "topology file " << topology.value() << " is bad ! please retype !\n";
                exit(EXIT_FAILURE);
            }
            cerr << "topology file  not given !\n";
            exit(EXIT_FAILURE);
        }
    }

    if (enable_outfile) {
        if (output_file) {
            cerr << "Output file option do not need\n";
            exit(EXIT_FAILURE);
        }
    }
    reader->set_mask(mask_string);

    shared_ptr<Frame> frame;
    int Clear = 0;
    int current_frame_num = 0;
    while ((frame = reader->readOneFrame())) {
        current_frame_num++;
        if (total_frames != 0 and current_frame_num > total_frames) break;
        if (current_frame_num % 10 == 0) {
            if (Clear) {
                cout << "\r";
            }
            cout << "Processing Coordinate Frame  " << current_frame_num << "   " << flush;
            Clear = 1;
        }
        if (current_frame_num >= start && (current_frame_num - start) % step_size == 0) {
            if (current_frame_num == start) {
                if (forcefield.isValid()) {
                    forcefield.assign_forcefield(frame);
                }
                processFirstFrame(frame, task_list);
            }
            processOneFrame(frame, task_list);
        }
    }
    cout << '\n';

    tbb::parallel_for_each(*task_list, [&](auto &task) {
        ofstream ofs(task->getOutfileName());
        ofs << "#  workdir > " << boost::filesystem::current_path() << '\n';
        ofs << "#  cmdline > " << print_cmdline(argc, argv) << '\n';
        if (script_file) {
            ofs << "#  script-file > " << script_file.value() << '\n';
        }
        ofs << "#  script BEGIN>\n" << scriptContent << "\n#  script END>\n";
        task->print(ofs);
    });

    int totol_task_count = task_list->size();

    cout << "Complete " << totol_task_count << " task(s)         Run Time "
         << chrono_cast(chrono::steady_clock::now() - start_time) << '\n';

    task_list->clear();
    return totol_task_count;
}

void processTrajectory(const boost::program_options::options_description &desc,
                       const boost::program_options::variables_map &vm, const std::vector<std::string> &xyzfiles,
                       int argc, char *argv[]) {
    if (!vm.count("file")) {
        std::cerr << "input trajectory file is not set !" << std::endl;
        std::cerr << desc;
        exit(EXIT_FAILURE);
    }

    auto task_list = getTasks();

    for (;;) {
        if (vm.count("topology") && getFileType(vm["topology"].as<std::string>()) != FileType::ARC) {
            break;
        }
        if (vm.count("prm")) {
            auto ff = vm["prm"].as<std::string>();
            if (boost::filesystem::exists(ff)) {
                forcefield.read(ff);
                break;
            } else if (enable_forcefield) {
                std::cout << "force field file " << ff << " is bad ! please retype !" << std::endl;
            } else {
                break;
            }
        }
        forcefield.read(choose_file("force field filename:").isExist(true));
        break;
    }
    int start = choose(1, INT32_MAX, "Enter the start frame[1]:", Default(1));
    int step_size = choose(1, INT32_MAX, "Enter the step size[1]:", Default(1));
    int total_frames = choose(0, INT32_MAX, "How many frames to read [all]:", Default(0));

    tbb::task_scheduler_init tbb_init(tbb::task_scheduler_init::deferred);
    if (enable_tbb) {
        int threads = choose<int>(
            0, sysconf(_SC_NPROCESSORS_ONLN),
            (boost::format("How many cores to used in parallel[%s]:") % program_configuration->get_nthreads()).str(),
            Default(program_configuration->get_nthreads()));
        tbb_init.initialize(threads == 0 ? tbb::task_scheduler_init::automatic : threads);
    }

    auto reader = std::make_shared<TrajectoryReader>();
    bool b_added_topology = true;
    if (boost::algorithm::one_of_equal<std::initializer_list<FileType>>(
            {FileType::NC, FileType::XTC, FileType::TRR, FileType::JSON, FileType::GRO}, getFileType(xyzfiles[0]))) {
        b_added_topology = false;
    } else {
        if (vm.count("topology")) {
            std::cerr << "WRANING !!  do not use topology file !\n";
        }
    }
    for (auto &xyzfile : xyzfiles) {
        reader->add_trajectoy_file(xyzfile);
        if (getFileType(xyzfile) == FileType::TRAJ && xyzfiles.size() != 1) {
            std::cout << "traj file can not use multiple files" << std::endl;
            exit(EXIT_FAILURE);
        }
        if (!b_added_topology) {
            if (vm.count("topology")) {
                std::string topol = vm["topology"].as<std::string>();
                if (boost::filesystem::exists(topol)) {
                    reader->set_topology(topol);
                    b_added_topology = true;
                    continue;
                }
                std::cout << "topology file " << topol << " is bad ! please retype !" << std::endl;
            }
            reader->set_topology(choose_file("input topology file : ").isExist(true));
            b_added_topology = true;
        }
    }

    if (choose_bool("Do you want to use multiple files [N]:", Default(false))) {
        while (true) {
            std::string input_line = choose_file("next file [Enter for End]:").isExist(true).can_empty(true);
            if (input_line.empty()) break;
            if (getFileType(input_line) == FileType::TRAJ) {
                std::cout << "traj file can not use multiple files [retype]" << std::endl;
                continue;
            }
            reader->add_trajectoy_file(input_line);
        }
    }

    std::ofstream outfile;
    if (enable_outfile) {
        outfile.open(getOutputFilename(vm));
    }

    if (vm.count("mask")) reader->set_mask(vm["mask"].as<std::string>());

    std::shared_ptr<AbstractAnalysis> parallel_while_task;
    for (auto &task : *task_list) {
        if (task->enable_parralle_while()) {
            if (parallel_while_task) {
                std::cerr << "Cannot select more than one task do parallel_while parallelism\n";
                exit(EXIT_FAILURE);
            } else {
                parallel_while_task = task;
            }
        }
    }

    auto time_before_process = std::chrono::steady_clock::now();
    int current_frame_num = 0;
    if (parallel_while_task) {
        task_list->remove(parallel_while_task);
        parallel_while_task->do_parallel_while(
            [&] { return getFrame(task_list, start, step_size, total_frames, current_frame_num, reader); });
        task_list->push_back(parallel_while_task);
    } else {
        while (getFrame(task_list, start, step_size, total_frames, current_frame_num, reader))
            ;
    }
    auto time_after_process = std::chrono::steady_clock::now();

    std::cout << '\n';

    std::stringstream ss;
    for (auto &task : *task_list) {
        task->print(ss);
    }

    auto time_after_print = std::chrono::steady_clock::now();
    auto finish_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto finish_time_format = std::put_time(std::localtime(&finish_time), "%F %T");
    auto frame_process_time = chrono_cast(time_after_process - time_before_process);
    auto post_process_time = chrono_cast(time_after_print - time_after_process);
    auto total_time = chrono_cast(time_after_print - time_before_process);

    if (outfile.is_open()) {
        outfile << "#  Finish Time  > " << finish_time_format << '\n';
        outfile << "#  Frame process time = " << frame_process_time << '\n';
        outfile << "#    Postprocess time = " << post_process_time << '\n';
        outfile << "#          Total time = " << total_time << '\n';
        outfile << "#  workdir > " << boost::filesystem::current_path() << '\n';
        outfile << "#  cmdline > " << print_cmdline(argc, argv) << '\n';
        outfile << "#  start frame  > " << start << '\n';
        outfile << "#  step (frame) > " << step_size << '\n';
        outfile << "#  end   frame  > "
                << (total_frames == 0 ? ("all(" + std::to_string(current_frame_num) + ')')
                                      : std::to_string(total_frames))
                << '\n';
        boost::iostreams::copy(ss, outfile);
    }

    std::cout << "(:  Mission Complete  :)   Finish Time  >>> " << finish_time_format << " <<<\n";
    std::cout << "Frame process time = " << frame_process_time << '\n';
    std::cout << "  Postprocess time = " << post_process_time << '\n';
    std::cout << "        Total time = " << total_time << '\n';
}

std::shared_ptr<Frame> getFrame(std::shared_ptr<std::list<std::shared_ptr<AbstractAnalysis>>> &task_list,
                                const int start, const int step_size, const int total_frames, int &current_frame_num,
                                std::shared_ptr<TrajectoryReader> &reader) {
    std::shared_ptr<Frame> frame;
    while ((frame = reader->readOneFrame())) {
        current_frame_num++;
        if (total_frames != 0 and current_frame_num > total_frames) break;
        std::cout << "\rProcessing Coordinate Frame  " << current_frame_num << "   " << std::flush;
        if (current_frame_num >= start && (current_frame_num - start) % step_size == 0) {
            if (current_frame_num == start) {
                if (forcefield.isValid()) {
                    forcefield.assign_forcefield(frame);
                }
                processFirstFrame(frame, task_list);
            }
            processOneFrame(frame, task_list);
            return frame;
        }
    }
    return {};
}
