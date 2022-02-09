#include <fstream>
#include <iostream>
#include <string>

// Usage: compiler --list-source-files <cookie>
//        compiler --output-dir <path> <cookie>
//
int main(int argc, char **argv) {
  if (argc < 2) {
    return -1;
  }

  std::string arg(argv[1]);
  if (arg == "--list-source-files" && argc == 3) {
    std::string c(argv[2]);
    std::cout << c << ".hpp" << std::endl
              << c << ".cpp" << std::endl;
    return 0;
  } else if (arg == "--output-dir" && argc == 4) {
    std::string d(argv[2]), c(argv[3]);
    std::ofstream of1(d + '/' + c + ".cpp");
    of1 << "int f1() {return 1;}" << std::endl;
    std::ofstream of2(d + '/' + c + ".hpp");
    of2 << "int f1();" << std::endl;
    return 0;
  }

  return -1;
}
