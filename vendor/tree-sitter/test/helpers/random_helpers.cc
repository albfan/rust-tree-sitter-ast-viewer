#include "helpers/random_helpers.h"
#include <string>
#include <vector>
#include <random>
#include <time.h>

using std::string;
using std::vector;

unsigned get_time_as_seed() {
  return time(nullptr);
}

void Generator::reseed(unsigned seed) {
  engine.seed(seed);
}

unsigned Generator::operator()() {
  return distribution(engine);
}

unsigned Generator::operator()(unsigned max) {
  return distribution(engine) % max;
}

string Generator::str(char min, char max) {
  string result;
  size_t length = operator()(12);
  for (size_t i = 0; i < length; i++) {
    result += (min + operator()(max - min));
  }
  return result;
}

static string operator_characters = "!(){}[]<>+-=";

string Generator::words(size_t count) {
  string result;
  bool just_inserted_word = false;
  for (size_t i = 0; i < count; i++) {
    if (operator()(10) < 6) {
      result += operator_characters[operator()(operator_characters.size())];
    } else {
      if (just_inserted_word)
        result += " ";
      result += str('a', 'z');
      just_inserted_word = true;
    }
  }
  return result;
}

string Generator::select(const vector<string> &list) {
  return list[operator()(list.size())];
}

#ifdef _WIN32

#include <windows.h>

void Generator::sleep_some() {
  Sleep(operator()(5));
}

#else

#include <unistd.h>

void Generator::sleep_some() {
  usleep(operator()(5 * 1000));
}

#endif
