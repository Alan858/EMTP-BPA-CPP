// author: Alan W. Zhang - w.zhang@ieee.org
// Copyright (c) 2015-2020

#include "stdafx.h"
#include "Tester.h"
//#include <mutex> 

namespace test
{
  std::vector<std::string_view> split(const std::string_view strv, const std::string_view delim) {
    std::vector<std::string_view> result;
    size_t pos = 0;
    size_t len = 0;
    while ((len = strv.substr(pos).find(delim)) != std::string::npos) {
      if (0 < len)
        result.push_back(strv.substr(pos, len));
      pos += len + delim.length();
    }
    if (len = strv.size() - pos; 0 < len)
      result.push_back(strv.substr(pos, len));
    return result;
  }

  std::string_view trim_left(const std::string_view strv) {
    return strv.substr(std::min(strv.find_first_not_of(' '), strv.size()));
  }
  std::string_view trim_right(const std::string_view strv) {
    return strv.substr(0, strv.find_last_not_of(' ') + 1);
  }
  std::string_view trim(const std::string_view strv) {
    return trim_left(trim_right(strv));
  }

  bool runProgram(const std::string& cmd, bool wait = true) {
    return -1 != system(('\"' + cmd + '\"').c_str());
  }


  ///////////////////////////////////////////////////////////
  // test functions
  bool check_test(bool exp) {
    ++number_of_total_tests;
    if (!exp) ++number_of_failed_tests;
    return exp;
  }

  class ChangeCurrentPath {
    std::filesystem::path curPath_;
  public:
    ChangeCurrentPath(std::string curPath) : curPath_{ std::filesystem::current_path() }
    {
      std::filesystem::current_path(curPath);
    }
    ~ChangeCurrentPath() {
      std::filesystem::current_path(curPath_);
    }
  };

  // parse lineNum to get all line numbers for comparison
  std::vector<int> Line_Num_Parser(std::string lineNum)
  {
    std::vector<int> lineNumbs;
    std::vector<std::string> vectStr;
    for (auto s : split(lineNum, ","))
      vectStr.push_back(std::string(s));
    for (auto i = vectStr.begin(); i != vectStr.end(); ++i)
    {
      auto pos = (*i).find('-');
      if (pos != std::string::npos)
      {
        int b = stoi((*i).substr(0, pos++));
        int e = stoi((*i).substr(pos, (*i).size() - pos));
        for (int i = b; i <= e; ++i)
          lineNumbs.push_back(i);
      }
      else
        lineNumbs.push_back(stoi((*i)));
    }
    sort(lineNumbs.begin(), lineNumbs.end());
    lineNumbs.erase(unique(lineNumbs.begin(), lineNumbs.end()), lineNumbs.end());
    return lineNumbs;
  }

  // find all testing cases (folder name in format of case0001, case0002, ect) under test_folder
  auto getTestCases(std::string_view batFile)
  {
    namespace fs = std::filesystem;
    std::vector<std::string> testCases;
    //std::stringstream ss;
    //for (int i = 0; i < 500; ++i) {
    //  ss.str("");
    //  ss << "case" << std::setw(4) << std::setfill('0') << i + 1;
    //  if (!fs::exists(std::string(program_folder) + ss.str()))
    //    break;
    //  testCases.emplace_back(ss.str());
    //}
    std::ifstream in(batFile);
    if (!in.is_open()) return testCases;
    std::string sLine;
    const std::string key = "num_cases=";
    int numCases = 0;
    while (std::getline(in, sLine)) {
      if (auto pos = sLine.find(key); pos != std::string::npos) {
        numCases = std::stoi(sLine.substr(pos + key.size()));
        break;
      }
    }

    std::stringstream ss;
    for (int i = 0; i < numCases; ++i) {
      ss.str("");
      ss << "case" << std::setw(4) << std::setfill('0') << i + 1;
      testCases.push_back(ss.str());
    }
    return testCases;
  }

  /////////////////////////////////////////////////////////
  // class ProgramRegessionTest implementation

  void ProgramRegessionTest::readJson(const std::string_view jsFile)
  {
    std::ifstream in(jsFile);
    json j;
    in >> j;

    test_folder_ = j["test_folder"];
    if (test_folder_.empty()) {
      test_folder_ = std::filesystem::path(jsFile).parent_path().string();
    }
    if (test_folder_.back() != '/' || test_folder_.back() != '\\')
      test_folder_ += '\\';

    for (const auto& proj : j["proj_folder"]) {
      projects_.push_back({
        proj["name"],
        Line_Num_Parser(proj["lines"]),
        getTestCases(test_folder_ + std::string(proj["name"]) + '\\' + batName_),
        proj["max_sec"],
        });
    }
  }

  //bool 
  auto getResult(std::string_view resultFile, const std::vector<int>& lines)
  {
    std::ifstream ifile(resultFile);  // result text file 
    if (!check_test(ifile.is_open())) {
      throw std::runtime_error("  Failed to open: " + std::string(resultFile));
    }
    std::vector<std::pair<int, std::string>> strLines;
    std::string sLine;
    int lineIdx = 0;
    int cnt = 0;
    while (std::getline(ifile, sLine) && lineIdx < lines.size() && strLines.size() < 50000)
    {
      if (auto num = lines[lineIdx]; ++cnt == num) {
        if (sLine = trim(sLine); !sLine.empty())
          strLines.push_back({ num, sLine });
        ++lineIdx;
      }
    }
    return strLines;
  }

  void compare(std::string_view resultFile, std::string_view baseFile, const std::vector<int>& lines)
  {
    using namespace std;
    namespace fs = std::filesystem;
    if (!check_test(fs::exists(resultFile))) {
      throw std::range_error(string("  Result file was not found: ") + string(resultFile));
    }
    if (!check_test(fs::exists(baseFile))) {
      std::filesystem::copy_file(resultFile, baseFile);
      throw std::range_error("  Baseline file was not found! We set it with the current result file!");
    }

    // read result and baseline files
    auto curLines = getResult(resultFile, lines);
    auto basLines = getResult(baseFile, lines);

    // compare
    if (!check_test(curLines.size() == basLines.size())) {
      throw std::range_error("  Result file size does not match!");
    }
    for (size_t i = 0; i < curLines.size(); ++i) {
      const auto& curLine = curLines[i];
      const auto& basLine = basLines[i];
      if (!check_test(curLine == basLine)) {
        auto curStrs = split(curLine.second, " ");
        auto basStrs = split(basLine.second, " ");
        if (check_test(curLine.first == basLine.first && curStrs.size() == basStrs.size())) {
          for (size_t i = 0, e = curStrs.size(); i != e; ++i) {
            if (auto curStr = curStrs[i], basStr = basStrs[i];  !check_test(curStr == basStr)) {
              string msg = " line " + std::to_string(curLine.first) + ": \""
                + curLine.second.substr(0, std::min(80, int(curLine.second.size()))) + "...\"";
              throw std::range_error(msg + "\n  \"" + string(curStr) + "\" vs \"" + string(basStr) + "\".");
            }
          }
        }
        else {
          string msg = " line " + std::to_string(curLine.first) + ": \""
            + curLine.second.substr(0, std::min(80, int(curLine.second.size()))) + "...\"";
          throw std::range_error(msg);
        }
      }
    }
  }

  void ProgramRegessionTest::doTest(const ProgramFolder& proj)
  {
    using namespace std;
    namespace fs = std::filesystem;
    if (test_folder_.empty())
      return;
    number_of_failed_tests = 0;
    number_of_total_tests = 0;

    // run projects and compare results
    std::cout << "Program: " << proj.name << ". Number of testing cases: " << proj.cases.size() << '\n';
    auto program_folder = test_folder_ + proj.name + '\\';
    if (proj.lines.empty()) throw std::range_error("  json configuration error !");

    ChangeCurrentPath curPath(program_folder);
    // for each case, remove possible existing result file
    for (auto& caseName : proj.cases) {
      fs::path folder = program_folder + caseName + '\\';
      for (fs::directory_iterator it(folder), end; it != end; ++it) {
        if (auto f = it->path().filename(); f.extension().string() == ".out"
          || f.string().find("_result.txt") != std::string::npos) {
          fs::remove(it->path());
        }
      }
    }

    // run all testing cases
    const auto startTime = std::chrono::high_resolution_clock::now();
    string cmdLine = program_folder + batName_;
    if (!check_test(runProgram(cmdLine))) {
      throw std::runtime_error("fialed to run :" + cmdLine);
    }
    auto diffex = chrono::high_resolution_clock::now() - startTime;
    auto elapsed = (size_t)chrono::duration<double, milli>(diffex).count();
    if (auto maxSec = proj.max_sec * proj.cases.size(); maxSec < elapsed * 0.001) {
      stringstream ss;
      ss << "  calculation time exceeds the limit : " << maxSec << " sec.";
      cout << ss.str() << '\n';
      throw std::range_error(ss.str());
    }

    // compare case by case
    for (auto& caseName : proj.cases) {
      std::cout << "Testing " << caseName << '\n';
      string resultFile = program_folder + caseName + '\\' + caseName + "_result.txt";
      string saved_resultFile = program_folder + baselineFolder_ + caseName + "_result.txt";
      try {
        compare(resultFile, saved_resultFile, proj.lines);
      }
      catch (const std::exception& e) {
        cout << std::string("  ") + e.what() << '\n';
        continue;
      }
    }

    diffex = chrono::high_resolution_clock::now() - startTime;
    elapsed = (size_t)chrono::duration<double, milli>(diffex).count();
    cout << "Total time elapsed: " + std::to_string(elapsed) + " ms" << '\n';
  }





}