#include "utils.h"

#define POINT 48// Fixed decimal point position

decodeDouble::decodeDouble() : num() {}

decodeDouble::decodeDouble(double x) : num(x) {}

Fixed::Fixed() : l() {}

Fixed::Fixed(int x) : l((long long) x << POINT) {}

Fixed::Fixed(long long x) : l(x) {}

Fixed::Fixed(double x) : l() {
    decodeDouble xd(x);
    if (xd.field.mantissa == 0 && xd.field.exponent == 0) {
        l = 0;
        return;
    }
    unsigned long long xv = (1uLL << 52) | xd.field.mantissa;
    int shift = (int) (xd.field.exponent) - (1023 + 52 - POINT);
    if (shift <= -64 || shift >= 64) {
        l = 0;
        return;
    }
    l = (long long) (shift > 0 ? xv << shift : xv >> -shift);
    if (xd.field.sign) l = -l;
}

double Fixed::to_double() const {
    decodeDouble xd;
    long long lv = l;
    if (lv == 0) return 0.0;
    if (lv < 0) xd.field.sign = 1, lv = -lv;
    else
        xd.field.sign = 0;
    int shift = 11 - countLeadingZero(lv);
    if (shift <= -64 || shift >= 64) xd.field.mantissa = 0;
    else
        xd.field.mantissa = shift > 0 ? lv >> shift : lv << -shift;
    xd.field.exponent = shift + (1023 + 52 - POINT);
    return xd.num;
}

Fixed Fixed::operator+(Fixed x) const { return Fixed(l + x.l); }

Fixed Fixed::operator-(Fixed x) const { return Fixed(l - x.l); }

Fixed Fixed::operator*(Fixed x) const {
    long long al = l < 0 ? -l : l;
    long long bl = x.l < 0 ? -x.l : x.l;
    long long ah = al >> 32;
    long long bh = bl >> 32;
    al &= 0xffffffffLL;
    bl &= 0xffffffffLL;
    long long r = ((al * bl) >> POINT) + ((al * bh + ah * bl) >> (POINT - 32)) + ((ah * bh) << (64 - POINT));
    return Fixed((l < 0) == (x.l < 0) ? r : -r);
}

Fixed Fixed::operator/(int x) const { return Fixed(l / x); }

Fixed Fixed::operator-() const { return Fixed(-l); }

bool Fixed::operator>(Fixed x) const { return operator-(x).l > 0; }

int countLeadingZero(unsigned long long x) {
    int r = 0;
    if (!(x & 0xFFFFFFFF00000000)) r += 32, x <<= 32;
    if (!(x & 0xFFFF000000000000)) r += 16, x <<= 16;
    if (!(x & 0xFF00000000000000)) r += 8, x <<= 8;
    if (!(x & 0xF000000000000000)) r += 4, x <<= 4;
    if (!(x & 0xC000000000000000)) r += 2, x <<= 2;
    if (!(x & 0x8000000000000000)) r += 1;
    return r;
}

unsigned int crc8(const std::vector<bool> &source) {
    static unsigned char sourceString[20];
    int stringLength = ((int) source.size() - 1) / 8 + 1;
    for (int i = 0; i < stringLength; ++i) {
        unsigned q = 0;
        for (int j = 0; j < 8 && i * 8 + j < source.size(); ++j) q |= (int) source[i * 8 + j] << j;
        sourceString[i] = (unsigned char) q;
    }
    boost::crc_basic<8> crc{0XA7, 0X00, 0, false, false};
    crc.process_bytes(sourceString, stringLength);
    return crc.checksum();
}

std::vector<Fixed> smooth(const std::vector<Fixed> &y, size_t span) {
    if (span == 0) { return y; }
    auto size = y.size();
    auto smoothSize = ((span - 1) | 1);
    std::vector<Fixed> result;
    result.reserve(size);
    for (size_t pos = 0; pos < size; ++pos) {
        int maxDistToEdge = (int) std::min({smoothSize / 2, pos, size - pos - 1});
        result.push_back(y[pos]);
        for (int i = 1; i <= maxDistToEdge; ++i) result[pos] = result[pos] + y[pos - i] + y[pos + i];
        result[pos] = result[pos] * Fixed(1.0 / (maxDistToEdge * 2 + 1));
    }
    return result;
}

std::string getPath(std::string path, int depth) {
    for (int i = 0; i < depth; ++i) {
        FILE *file = fopen(path.c_str(), "r");
        if (file) {
            fclose(file);
            return path;
        }
        path = "../" + path;
    }
    exit(1);
}
