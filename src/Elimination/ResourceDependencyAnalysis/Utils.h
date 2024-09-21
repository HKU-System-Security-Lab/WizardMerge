#ifndef UTILS_H
#define UTILS_H

#include "ProjectRepresentation.h"

bool compare_by_depth(RangeCodeBlock *a, RangeCodeBlock *b);
bool compare_by_line(DiffCodeBlock *a, DiffCodeBlock *b);
bool compare_by_rangetype(DiffCodeBlock *a, DiffCodeBlock *b);

void replace(std::string& str, const std::string& from, const std::string& to);
bool endsWith(std::string const &str, std::string const &suffix);

#endif