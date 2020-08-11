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
    for (int i = 0; i < projects.size(); ++i) {
      cout << i + 1 << " - " + projects[i].name << '\n';
    }
    cin >> selected;
    selected = std::clamp(selected, 0, int(projects.size() - 1));
  }

  const auto& selProj = projects[selected];

  try
  {
    programTest.doTest(selProj);
  }
  catch (std::exception& e)
  {
    cout << "\n\n" 
          << "//////////////////// TEST FAILED: " << selProj.name
          << "////////////////////\n";
    cout << "Failure message: " << e.what() << endl;

  }

  if (number_of_failed_tests == 0)
  {
    cout << "\n\nTesting Finished\n";
    cout << "Total number of tests: "<< number_of_total_tests << endl;
    cout << "All tests completed successfully\n\n";
  }
  else
  {
    cout << "\n\nTesting Finished\n";
    cout << "Total number of tests: "<< number_of_total_tests << endl;
    cout << "Number of failed tests: " << number_of_failed_tests << "\n";
  }

  system("pause");
  return 0;
}

