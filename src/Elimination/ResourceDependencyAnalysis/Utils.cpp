#include "Utils.h"

bool compare_by_depth(RangeCodeBlock *a, RangeCodeBlock *b) {
    return a->getDepth() == b->getDepth() ?
                a->getStartLine() < b->getStartLine() :
                a->getDepth() > b->getDepth();
}

bool compare_by_line(DiffCodeBlock *a, DiffCodeBlock *b) {
    return a->getStartLine() < b->getStartLine();
}

bool compare_by_rangetype(DiffCodeBlock *a, DiffCodeBlock *b) {
    return a->getParent()->getRangeType() < a->getParent()->getRangeType();
}

void replace(std::string& str, const std::string& from, const std::string& to) {
	if(from.empty())
		return;
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

bool endsWith(std::string const &str, std::string const &suffix) {
  if (str.length() < suffix.length()) {
      return false;
  }
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}