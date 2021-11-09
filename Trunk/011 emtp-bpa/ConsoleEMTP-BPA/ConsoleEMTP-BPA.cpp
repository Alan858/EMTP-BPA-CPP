// ConsoleEMTP-BPA.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "emtp_cmn.h"


int main(int argc, char const* argv[])
{
  namespace fs = std::filesystem;
  std::string input;
  if (argc < 2) {
    std::cout << "Enter input File name: ";
    std::cin >> input;
  }
  else input = argv[1];
  input = fs::absolute(input).string();
  if (!fs::exists(input)) {
    std::cout << "input file : \"" << input << "\" was not found !\n";
    return 1;
  }
  try {
    emtp::program_main(input, input + ".log", input + ".out");
  }
  catch (const std::exception& e) {
    std::string get_exception_msg(const std::exception & e, int const level = 0);
    std::cout << get_exception_msg(e) << '\n';
  }

  //std::cout << "Hello World!\n";
  return 0;
}

// prints the explanatory string of an exception. If the exception is nested,
// recurses to print the explanatory of the exception it holds
std::string get_exception_msg(const std::exception& e, int const level = 0)
{
  std::string msg;
  msg += std::string(level, ' ') + "exception: " + e.what() + '\n';
  try {
    std::rethrow_if_nested(e);
  }
  catch (const std::exception& e) {
    msg += get_exception_msg(e, level + 1);
  }
  catch (...) {}
  return msg;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
 