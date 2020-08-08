// author: Alan W. Zhang - w.zhang@ieee.org
// Copyright (c) 2015-2020

#pragma once

#include "json.hpp"
using json = nlohmann::json;


// common functions
namespace test
{
  inline auto number_of_failed_tests = 0u;
  inline auto number_of_total_tests = 0u;

  struct ProgramFolder
  {
    std::string name;
    std::vector<int> lines;
    std::vector<std::string> cases;
    float max_sec;  
  };

  extern std::vector<int> Line_Num_Parser(std::string lineNum);

  class ProgramRegessionTest
  {
    const std::string batName_{ "_Run all.bat" };
    const std::string baselineFolder_{ "BaselineResults\\" };

    std::string test_folder_;                // the folder holds all program testings
    std::vector<ProgramFolder> projects_;    // individual analysis program testing
    
  public:
    ProgramRegessionTest() = default;

    void readJson(const std::string_view jsFile);
    const auto& getProjects() const {
      return projects_;
    }

    void doTest(const ProgramFolder& project);
  };
}
