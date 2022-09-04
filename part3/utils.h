#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>

#define PI acos(-1)

std::vector<double> linspace(double min, double max, int n);

std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f);