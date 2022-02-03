#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    return -1;
  }

  std::string arg(argv[1]);
  if (arg == "list_source_files" && argc == 3) {
    std::string arg2(argv[2]);
    std::cout << "./" << arg2 << ".cpp" << std::endl
              << "./" << arg2 << ".hpp" << std::endl;
    return 0;
  } else if (arg == "generate_files") {
    std::ofstream of1("./generated_file.cpp");
    of1 << "int f1() {return 1;}" << std::endl;
    std::ofstream of2("./generated_file.hpp");
    of2 << "int f1();" << std::endl;
    return 0;
  }

  return -2;
}
