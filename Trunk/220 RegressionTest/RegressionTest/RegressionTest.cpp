// RegressionTest.cpp : Defines the entry point for the console application.
// author: Alan W. Zhang - w.zhang@ieee.org
// Copyright (c) 2015-2020

#include "stdafx.h"
#include "Tester.h"


///////////////////////////////////////////////////////////
//
int main(int argc, const char* argv[])
{
  using namespace std;
  using namespace test;

  auto jsFileName = std::string(argv[0]) + ".json";
  ProgramRegessionTest programTest;
  programTest.readJson(jsFileName);
  const auto& projects = programTest.getProjects();
  if (projects.empty()) return 1;
  
  int selected{};
  if (1 < projects.size()) {
    cout << "Please select\n";
    cout << 0 << " - All" << '\n';
    for (int i = 0; i < projects.size(); ++i) {
      cout << i + 1 << " - " + projects[i].name << '\n';
    }
    selected = -1;
    while (selected < 0 || projects.size() < selected) {
      cin >> selected;
    }
  }

  for (size_t i = 0; i < projects.size(); ++i) {
    if (selected != 0 && selected != i + 1) // if 0 run all programs
      continue;

    const auto& selProj = projects[i];
    try
    {
      cout << "================================\n";
      programTest.doTest(selProj);
    }
    catch (std::exception& e)
    {
      cout << "\n"
        << "//////////////////// TEST FAILED: " << selProj.name
        << "////////////////////\n";
      cout << "Failure message: " << e.what() << endl;

    }

    if (number_of_failed_tests == 0)
    {
      cout << "\n"
        << "Testing Finished : " << selProj.name << '\n'
        << "Total number of tests: " << number_of_total_tests << '\n'
        << "All tests completed successfully\n\n";
    }
    else
    {
      cout << "\n"
        << "Testing Finished : " << selProj.name << '\n'
        << "Total number of tests: " << number_of_total_tests << '\n'
        << "Number of failed tests: " << number_of_failed_tests << "\n\n";
    }
  }

  system("pause");
  return 0;
}

