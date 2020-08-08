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

  std::string_view left_trim(const std::string_view strv) {
    return strv.substr(strv.find_first_not_of(' '));
  }

  bool runProgram(const std::string& cmd) {
    auto res = ShellExecuteA(nullptr, "open", cmd.c_str(), nullptr, nullptr, SW_HIDE);
    return 32 < int(res);
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
    while (std::getline(in, sLine)) {
      if (sLine.size() < 10 || sLine.substr(0, 4) != "call") continue;
      if (auto pos = sLine.find("\"case") + 1; pos != std::string::npos)
        testCases.push_back(sLine.substr(pos, 8));
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
      test_folder_ += '/';

    for (const auto& proj : j["proj_folder"]) {
      projects_.push_back({
        proj["name"],
        Line_Num_Parser(proj["lines"]),
        getTestCases(test_folder_ + std::string(proj["name"]) + '/' + batName_),
        proj["max_sec"],
        });
    }
  }

  //bool 
  auto getResult(std::string_view resultFile, const std::vector<int>& lines)
  {
    using namespace std;
    ifstream ifile(resultFile);  // result text file 
    if (!check_test(ifile.good())) {
      throw std::runtime_error("  Result file was not created: " + string(resultFile));
    }
    std::vector<std::pair<int, std::string>> vectResFileLines;
    int linesInd = 0;
    int askedLineNum = lines[linesInd];
    int cnt = 0;
    string sLine;
    while (getline(ifile, sLine) && vectResFileLines.size() < 10000)
    {
      if (++cnt == askedLineNum) {
        if (!left_trim(sLine).empty())
          vectResFileLines.push_back({ askedLineNum,sLine });
        if (++linesInd == lines.size())
          break;
        askedLineNum = lines[linesInd];
      }
    }
    return vectResFileLines;
  }

  void compare(std::string_view resultFile, std::string_view baseFile, const std::vector<int>& lines)
  {
    using namespace std;
    auto vectResFileLines = getResult(resultFile, lines);
    if (!check_test(!vectResFileLines.empty())) {
      throw std::range_error("  No result found !");
    }

    // read saved result text file and compare to vectResFileLines
    ifstream ifile(baseFile);  // result text file 
    if (!check_test(ifile.good())) {
      // the file doesn't exit, just copy the new one
      std::filesystem::copy_file(resultFile, baseFile);
      throw std::range_error(" No baseline found and we set it now"); // no need to compare this time
    }

    int cnt = 0;
    int linesInd = 0;
    int askedLineNum = vectResFileLines[linesInd].first;
    string sLine;
    while (getline(ifile, sLine))
    {
      if (++cnt != askedLineNum) continue;
      if (!left_trim(sLine).empty())
      {
        // compare
        if (!check_test(sLine == vectResFileLines[linesInd].second)) {
          auto vectStrSaved = split(sLine, " ");
          auto vectStrCurr = split(vectResFileLines[linesInd].second, " ");
          if (check_test(vectStrCurr.size() == vectStrSaved.size())) {
            for (size_t i = 0, e = vectStrCurr.size(); i != e; ++i) {
              if (!check_test(vectStrCurr[i] == vectStrSaved[i])) {
                string fileLineNum = " line:" + std::to_string(askedLineNum)
                  + "\n\"" + sLine.substr(0, std::min(40, int(sLine.size()))) + "...\"";
                throw std::range_error(fileLineNum + ": \"" + string(vectStrCurr[i]) + "\" vs \"" + string(vectStrSaved[i]) + "\".");
              }
            }
          }
          else {
            string fileLineNum = " line:" + std::to_string(askedLineNum)
              + "\n\"" + sLine.substr(0, std::min(40, int(sLine.size()))) + "...\"";
            throw std::range_error(fileLineNum);
          }
        }
      }
      if (vectResFileLines.size() <= ++linesInd)
        break;
      askedLineNum = vectResFileLines[linesInd].first;
    }
    if (!check_test(linesInd == vectResFileLines.size())) {
      throw std::range_error("  Result has less lines of " + std::to_string(vectResFileLines.size()));
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
    auto program_folder = test_folder_ + proj.name + '/';
    if (proj.lines.empty()) throw std::range_error("  json configuration error !");

    // for each case, remove possible existing result file
    for (auto& caseName : proj.cases) {
      string resultFile = program_folder + caseName + '\\' + caseName + "_result.txt";
      fs::remove(resultFile);
    }

    // run all testing cases
    const auto startTime = std::chrono::high_resolution_clock::now();
    string cmdLine = program_folder + batName_;
    ChangeCurrentPath curPath(program_folder);
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
        cout << e.what() << '\n';
        continue;
      }
    }

    diffex = chrono::high_resolution_clock::now() - startTime;
    elapsed = (size_t)chrono::duration<double, milli>(diffex).count();
    cout << "Total time elapsed: " + std::to_string(elapsed) + " ms" << '\n';
  }





}