#include "cli.hpp"

int main(int argc, char** argv)
{
    CmdLine cmd_line(argc, argv);

    cmd_line.setup();
    cmd_line.run();

    for(auto ft:unimplemented) std::cerr<< "[UNIMPLEMENTED!] " << ft << std::endl;
    return EXIT_SUCCESS;
}
