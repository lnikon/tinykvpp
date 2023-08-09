#include <structures/lsmtree/lsmtree.h>

#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>

namespace po = boost::program_options;

po::options_description constructOptionsDescription();
po::variables_map parseCommandLine(po::options_description descriptions,
                                   int argc, char **argv);
structures::lsmtree::lsmtree_config_t constructLSMTreeConfig(po::variables_map vm);

int main(int argc, char **argv) {
  spdlog::set_level(spdlog::level::debug);

  auto descriptions = constructOptionsDescription();
  auto vm = parseCommandLine(descriptions, argc, argv);
  if (vm.count("help")) {
    std::stringstream ss;
    descriptions.print(ss);
    spdlog::info(ss.str());
    return 0;
  }

  auto lsmTreeConfig = constructLSMTreeConfig(vm);

  structures::lsmtree::lsmtree_t lsmTree(lsmTreeConfig);
  // TODO: Run REPL loop for testing. Configure REPLing via config;

  return 0;
}

po::options_description constructOptionsDescription() {
  po::options_description descriptions;
  descriptions.add_options()("help", "show help message")(
      "diskFlushThresholdSize", po::value<uint64_t>(),
      "specify memtable size to write into sstable");

  return descriptions;
}

po::variables_map parseCommandLine(po::options_description descriptions,
                                   int argc, char **argv) {

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, descriptions), vm);
  po::notify(vm);

  return vm;
}

structures::lsmtree::lsmtree_config_t
constructLSMTreeConfig(po::variables_map vm) {
  structures::lsmtree::lsmtree_config_t lsmTreeConfig;
  if (vm.count("diskFlushThresholdSize")) {
    lsmTreeConfig.DiskFlushThresholdSize =
        vm["diskFlushThresholdSize"].as<uint64_t>();
  }

  return lsmTreeConfig;
}
