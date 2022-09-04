#include "utils.h"

std::vector<double> linspace(double min, double max, int n) {
	std::vector<double> result;
	result.reserve(n);
	int it = 0;
	for (double interval = (max - min) / (n - 1); it < n - 1; it++) {
		result.push_back(min + it * interval);
	}
	result.push_back(max);
	return result;
}

std::vector<double> cumtrapz(std::vector<double> t, std::vector<double> f) {
//	assert(t.size() == f.size());
	size_t size = t.size();
	std::vector<double> result;
	result.reserve(size);
	double total = 0.0;
	double last = *t.begin();
	result.push_back(0);
	for (int k = 0; k < size; k++) {
		double d;
		double s_tmp;
		d = t[k + 1] - t[k];
		t[k] = d;
		s_tmp = f[k + 1];
		total += d * ((last + s_tmp) / 2.0);
		last = s_tmp;
		result.push_back(total);
	}
	return result;
}