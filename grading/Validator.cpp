/* FILENAME: Validator.cpp
 * YEAR: 2014
 * AUTHORS:
 *   Members of Rensselaer Center for Open Source (rcos.rpi.edu):
 *   Chris Berger
 *   Jesse Freitas
 *   Severin Ibarluzea
 *   Kiana McNellis
 *   Kienan Knight-Boehm
 *   Sam Seng
 * LICENSE: Please refer to 'LICENSE.md' for the conditions of using this code
 *
 * RELEVANT DOCUMENTATION:
 *
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <iterator>
#include <typeinfo>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>
#include <unistd.h>
#include <algorithm>

#include "modules/modules.h"
#include "grading/TestCase.h"

#include "grading/TestCase.cpp"  /* Should not #include a .cpp file */

int validateTestCases(const std::string &hw_id, const std::string &rcsid, int subnum, const std::string &subtime);

int main(int argc, char *argv[]) {

  /* Check argument usage */
  if (argc != 5) {
    std::cerr << "VALIDATOR USAGE: validator <hw_id> <rcsid> <submission#> <time-of-submission>" << std::endl;
    return 1;
  }

  std::string hw_id = argv[1];
  std::string rcsid = argv[2];
  int subnum = atoi(argv[3]);
  std::string time_of_submission = argv[4];


  // TODO: add more error checking of arguments


  int rc = validateTestCases(hw_id,rcsid,subnum,time_of_submission);

  if (rc > 0) {
    std::cerr << "Validator terminated" << std::endl;
    return 1;
  }

  return 0;
}


/* Runs through each test case, pulls in the correct files, validates, and outputs the results */
int validateTestCases(const std::string &hw_id, const std::string &rcsid, int subnum, const std::string &subtime) {

  std::string grade_path = ".submit.grade";
  std::ofstream gradefile(grade_path.c_str());

  gradefile << "Grade for: " << rcsid << std::endl;
  gradefile << "  submission#: " << subnum << std::endl;
  int penalty = -std::min(submission_penalty,int(std::ceil(std::max(0,subnum-max_submissions)/10.0)));
  assert (penalty >= -submission_penalty && penalty <= 0);
  if (penalty != 0) {
    gradefile << "  penalty for excessive submissions: " << penalty << " points" << std::endl;
  }

  int nonhidden_auto_pts = penalty;
  int hidden_auto_pts = penalty;

  int nonhidden_extra_credit = 0;
  int hidden_extra_credit = 0;

  int nonhidden_possible_pts = 0;
  int hidden_possible_pts = 0;

  int possible_ta_pts = ta_pts;

  std::stringstream testcase_json;


  // LOOP OVER ALL TEST CASES
  for (int i = 0; i < num_testcases; ++i) {
    std::cout << "------------------------------------------\n" << testcases[i].title() << " - points: " << testcases[i].points() << std::endl;
    
    // START JSON FOR TEST CASE
    testcase_json << "\t\t{\n"
                  << "\t\t\t\"test_name\": \"" << testcases[i].title() << "\",\n";
    int testcase_pts = 0;
    std::string message = "";

    // FILE EXISTS & COMPILATION TESTS DON'T HAVE FILE COMPARISONS
    if (testcases[i].isFileExistsTest()) {
      std::cerr << "THIS IS A FILE EXISTS TEST! " << std::endl;
      if ( access( (std::string("STUDENT_FILES/")+testcases[i].details()).c_str(), F_OK|R_OK|W_OK ) != -1 ) { /* file exists */
	std::cerr << "file does exist: " << testcases[i].details() << std::endl;
	testcase_pts = testcases[i].points();
      } else {
	std::cerr << "ERROR file DOES NOT exist: " << testcases[i].details() << std::endl;
	message += "ERROR: " + testcases[i].details() + " was NOT FOUND!";
      }
    } else if (testcases[i].isCompilationTest()) {
      std::cerr << "THIS IS A COMPILATION! " << std::endl;
      if ( access( testcases[i].details().c_str(), F_OK|R_OK|W_OK ) != -1 ) { /* file exists */
	std::cerr << "file does exist: " << testcases[i].details() << std::endl;
	testcase_pts = testcases[i].points();
      } else {
	std::cerr << "ERROR file DOES NOT exist: " << testcases[i].details() << std::endl;
	message += "ERROR: " + testcases[i].details() + " was NOT FOUND!";
      }
      if (testcases[i].isCompilationTest()) {
	testcase_json << "\t\t\t\"compilation_output\": \".submit_compilation_output.txt\",\n";
      }
    } else {
      // ALL OTHER TESTS HAVE 1 OR MORE FILE COMPARISONS
      testcase_json << "\t\t\t\"diffs\": [\n";
      float pts_helper = 1.0;
      //      std::cerr << "-----------------------\ntest case " << i+1 << std::endl;
      for (int j = 0; j < testcases[i].numFileComparisons(); j++) {
	std::cerr << "comparison #" << j << std::endl;
	std::string helper_message = "";

	bool ok_to_compare = true;

	// GET THE FILES READY
	std::ifstream student_instr(testcases[i].filename(j).c_str());
	if (!student_instr) {
	  std::stringstream tmp;
	  tmp << "ERROR: comparison #" << j << ": Student's " << testcases[i].filename(j) << " does not exist";
	  std::cerr << tmp.str() << std::endl;
	  helper_message += tmp.str();
	  ok_to_compare = false;
	} 
	std::ifstream expected_instr(testcases[i].expected(j).c_str());
	if (!expected_instr && testcases[i].expected(j) != "") {
	  std::stringstream tmp;
	  tmp << "ERROR: comparison #" << j << ": Instructor's " + testcases[i].expected(j) + " does not exist!";
	  std::cerr << tmp.str() << std::endl;
	  if (helper_message != "") helper_message += "<br>";
	  helper_message += tmp.str();
	  ok_to_compare = false;
	}

	// DO THE COMPARISON
	TestResults *result = NULL;
	if (ok_to_compare) {
	  result = testcases[i].compare(j);
	} 

	// PREPARE THE JSON DIFF FILE
	std::stringstream diff_path;
	diff_path << testcases[i].prefix() << "_" << j << "_diff.json";
	std::ofstream diff_stream(diff_path.str().c_str());

	if (result != NULL) {
	  // THE GRADE (will be compiled across all comparisons)
	  std::cout << "result->grade() " << result->grade() << std::endl;
	  pts_helper *= result->grade();
	  result->printJSON(diff_stream);
	  
	  helper_message += " " + result->get_message();
	
	  // CLEANUP THIS COMPARISON
	  delete result;
	} else {
	  pts_helper = 0;
	}

	// JSON FOR THIS COMPARISON
	std::stringstream expected_path;
	expected_path << expected_out_dir << testcases[i].expected(j);
	testcase_json
	  << "\t\t\t\t{\n"
	  << "\t\t\t\t\t\"diff_id\":\"" << testcases[i].prefix() << "_" << j << "_diff\",\n"
	  << "\t\t\t\t\t\"student_file\":\"" << testcases[i].filename(j) << "\",\n";
	if (testcases[i].expected(j) != "") {
	  testcase_json << "\t\t\t\t\t\"instructor_file\":\"" << expected_path.str() << "\",\n";
	  if (ok_to_compare) {
	    testcase_json << "\t\t\t\t\t\"difference\":\"" << testcases[i].prefix() << "_" << j << "_diff.json\",\n";
	  }
	}
	testcase_json << "\t\t\t\t\t\"description\": \"" << testcases[i].description(j) << "\",\n";
	if (helper_message != "") {
	  testcase_json << "\t\t\t\t\t\"message\": \"" << helper_message << "\",\n";
	}
	testcase_json << "\t\t\t\t},\n";
      } // END COMPARISON LOOP

      testcase_json << "\t\t\t],\n";
      testcase_pts = (int)floor(pts_helper * testcases[i].points());

    } // end if/else of test case type



    // output grade & message

    std::cout << "Grade: " << testcase_pts << std::endl;



    // TODO: LOGIC NEEDS TO BE TESTED WITH MORE COMPLEX HOMEWORK!

    if (!testcases[i].hidden()) {
      nonhidden_auto_pts += testcase_pts;
      if (testcases[i].extracredit()) {
	nonhidden_extra_credit += testcases[i].points();
      } else {
	nonhidden_possible_pts += testcases[i].points();
      }
    } 
    hidden_auto_pts += testcase_pts;
    if (testcases[i].extracredit()) {
      hidden_extra_credit += testcases[i].points();
    } else {
      hidden_possible_pts += testcases[i].points();
    }
 
    testcase_json << "\t\t\t\"points_awarded\": " << testcase_pts << ",\n";

    if (message != "") {
      testcase_json << "\t\t\t\"message\": \"" << message << "\",\n";
    }

    const char *last_line = (i == num_testcases - 1) ? "\t\t}\n" : "\t\t},\n";
    testcase_json << last_line;


    gradefile << "  Test " << std::setw(2) << std::right << i+1 << ":" 
	      << std::setw(30) << std::left << testcases[i].just_title() << " " 
	      << std::setw(2) << std::right << testcase_pts << " / " 
	      << std::setw(2) << std::right << testcases[i].points() << std::endl;

  } // end test case loop



  int total_possible_pts = possible_ta_pts + hidden_possible_pts;



  std::cout << "penalty                 " <<  penalty << std::endl;
  std::cout << "nonhidden auto pts      " <<  nonhidden_auto_pts << std::endl;
  std::cout << "hidden auto pts         " <<  hidden_auto_pts << std::endl;
  std::cout << "nonhidden extra credit  " <<  nonhidden_extra_credit << std::endl;
  std::cout << "hidden extra credit     " <<  hidden_extra_credit << std::endl;
  std::cout << "nonhidden possible pts  " <<  nonhidden_possible_pts << std::endl;
  std::cout << "hidden possible pts     " <<  hidden_possible_pts << std::endl;
  std::cout << "possible ta pts         " <<  possible_ta_pts << std::endl;
  std::cout << "total possible pts      " <<  total_possible_pts << std::endl;


  assert (total_possible_pts == total_pts);


  /* Generate submission.json */
  std::ofstream json_file("submission.json");
  json_file << "{\n"
            << "\t\"submission_number\": " << subnum << ",\n"
            << "\t\"points_awarded\": " << hidden_auto_pts << ",\n"
            << "\t\"nonhidden_points_awarded\": " << nonhidden_auto_pts << ",\n"
            << "\t\"submission_time\": \"" << subtime << "\",\n"
            << "\t\"testcases\": [\n";
  json_file << testcase_json.str() << "\t]\n"
	    << "}";
  json_file.close();


  gradefile << "Automatic extra credit (w/o hidden):               " << "+ " << nonhidden_extra_credit << " points" << std::endl;
  gradefile << "Automatic grading total (w/o hidden):              " << nonhidden_auto_pts << " / " << nonhidden_possible_pts << std::endl;
  gradefile << "Max possible hidden automatic grading points:      " << hidden_possible_pts - nonhidden_possible_pts << std::endl;
  gradefile << "Automatic extra credit:                            " << "+ " << hidden_extra_credit << " points" << std::endl;
  gradefile << "Automatic grading total:                           " << hidden_auto_pts << " / " << hidden_possible_pts << std::endl;
  gradefile << "Remaining points to be graded by TA:               " << possible_ta_pts << std::endl;
  gradefile << "Max points for assignment (excluding extra credit):" << total_possible_pts << std::endl;


  return 0;
}
