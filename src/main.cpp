#include <structures/lsmtree/lsmtree.h>

#include <iostream>

// namespace po = boost::program_options;

// po::options_description constructOptionsDescription();
// po::variables_map parseCommandLine(po::options_description descriptions, int argc, char **argv);
// structures::lsmtree::lsmtree_config_t constructLSMTreeConfig(po::variables_map vm);

void sayHello()
{
    std::cout << "hello\n";
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // spdlog::set_level(spdlog::level::debug);
    //
    // auto descriptions = constructOptionsDescription();
    // auto vm = parseCommandLine(descriptions, argc, argv);
    // if (vm.count("help")) {
    //   std::stringstream ss;
    //   descriptions.print(ss);
    //   spdlog::info(ss.str());
    //   return 0;
    // }
    //
    // auto lsmTreeConfig = constructLSMTreeConfig(vm);
    //
    // structures::lsmtree::lsmtree_t lsmTree(lsmTreeConfig);
    // // TODO: Run REPL loop for testing. Configure REPLing via config;

    return 0;
}
