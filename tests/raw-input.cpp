#include "dotline/dotline.hpp"

int main()
{
    std::string s = "abc";
    s = dotl::prompt("> ").read_string();
    std::cout << s << std::endl;
}
