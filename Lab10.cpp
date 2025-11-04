#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <algorithm>

using std::string;

/* -------------------- Utilities -------------------- */

static inline bool is_all_digits(const string& s) {
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return !s.empty();
}

static inline void trim_leading_zeros(string& s) {
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    if (i > 0) s.erase(0, i);
}

static inline void trim_trailing_zeros(string& s) {
    // for fractional part only
    while (!s.empty() && s.back() == '0') s.pop_back();
}

/* -------------------- Validation -------------------- */
/*
   Valid double format (string only, no conversion):
   Optional sign [+|-], then digits, optionally '.' with at least 1 digit on BOTH sides.
   Allowed examples: "1", "1.0", "+1.0", "+0001.0", "-0001.005"
   Disallowed examples: "A", "+-1", "-5.", "-.5", "-5.-5"
*/
bool is_valid_double_literal(const string& x) {
    if (x.empty()) return false;

    size_t i = 0;
    if (x[i] == '+' || x[i] == '-') {
        ++i;
        if (i == x.size()) return false; // only sign is invalid
    }

    // must start with a digit now
    if (i >= x.size() || !std::isdigit(static_cast<unsigned char>(x[i]))) return false;

    // read integer digits
    size_t j = i;
    while (j < x.size() && std::isdigit(static_cast<unsigned char>(x[j]))) ++j;

    if (j == x.size()) {
        // pure integer
        return true;
    }

    // if next char is '.', there must be at least one digit after it
    if (x[j] == '.') {
        size_t k = j + 1;
        // at least one digit after '.'
        if (k >= x.size()) return false;
        if (!std::isdigit(static_cast<unsigned char>(x[k]))) return false;

        while (k < x.size() && std::isdigit(static_cast<unsigned char>(x[k]))) ++k;
        // nothing else allowed after fractional digits
        return (k == x.size());
    }

    // anything else after integer digits is invalid
    return false;
}

/* -------------------- BigDecimal (string-based) -------------------- */

struct BigDecimal {
    // sign: +1 or -1, zero uses +1 with "0" and empty frac.
    int sign = +1;
    string intPart = "0";   // no leading zeros except single "0"
    string fracPart = "";   // no trailing zeros; empty means no decimal

    bool isZero() const {
        return intPart == "0" && fracPart.empty();
    }
};

// Parse a validated literal into normalized BigDecimal.
// Assumes is_valid_double_literal(x) == true.
BigDecimal parse_normalize(const string& x) {
    BigDecimal r;
    size_t i = 0;

    if (x[i] == '+') { r.sign = +1; ++i; }
    else if (x[i] == '-') { r.sign = -1; ++i; }

    // split on dot if present
    size_t dot = x.find('.', i);
    if (dot == string::npos) {
        r.intPart = x.substr(i);
        r.fracPart.clear();
    } else {
        r.intPart = x.substr(i, dot - i);
        r.fracPart = x.substr(dot + 1);
    }

    // normalize integer part: remove leading zeros
    trim_leading_zeros(r.intPart);
    // normalize fractional part: remove trailing zeros
    trim_trailing_zeros(r.fracPart);

    // if all becomes zero -> sign should be + and represent canonical zero
    if (r.intPart == "0" && r.fracPart.empty()) {
        r.sign = +1;
    }

    return r;
}

// Align fractional lengths by padding right with zeros as needed
static inline void align_frac(BigDecimal& a, BigDecimal& b) {
    size_t L = std::max(a.fracPart.size(), b.fracPart.size());
    a.fracPart.append(L - a.fracPart.size(), '0');
    b.fracPart.append(L - b.fracPart.size(), '0');
}

// Compare |a| vs |b|. Return -1 if |a|<|b|, 0 if equal, +1 if |a|>|b|.
int cmp_abs(BigDecimal a, BigDecimal b) {
    align_frac(a, b);

    // compare integer length
    if (a.intPart.size() != b.intPart.size())
        return (a.intPart.size() < b.intPart.size()) ? -1 : +1;

    // compare integer lexicographically
    if (a.intPart != b.intPart)
        return (a.intPart < b.intPart) ? -1 : +1;

    // compare fraction lexicographically
    if (a.fracPart != b.fracPart)
        return (a.fracPart < b.fracPart) ? -1 : +1;

    return 0;
}

// Add absolute values: result is non-negative
BigDecimal add_abs(BigDecimal a, BigDecimal b) {
    align_frac(a, b);
    BigDecimal r;
    r.sign = +1;

    // add fractional part
    int carry = 0;
    r.fracPart.resize(a.fracPart.size());
    for (int i = static_cast<int>(a.fracPart.size()) - 1; i >= 0; --i) {
        int da = a.fracPart[i] - '0';
        int db = b.fracPart[i] - '0';
        int s = da + db + carry;
        r.fracPart[i] = char('0' + (s % 10));
        carry = s / 10;
    }
    trim_trailing_zeros(r.fracPart);

    // add integer part
    string A = a.intPart, B = b.intPart;
    // left-pad shorter integer with zeros
    if (A.size() < B.size()) A.insert(0, B.size() - A.size(), '0');
    if (B.size() < A.size()) B.insert(0, A.size() - B.size(), '0');

    string s(A.size(), '0');
    for (int i = static_cast<int>(A.size()) - 1; i >= 0; --i) {
        int da = A[i] - '0';
        int db = B[i] - '0';
        int sum = da + db + carry;
        s[i] = char('0' + (sum % 10));
        carry = sum / 10;
    }
    if (carry) s.insert(s.begin(), char('0' + carry));

    // normalize intPart
    r.intPart = s;
    trim_leading_zeros(r.intPart);

    return r;
}

// Subtract absolute values: assumes |a| >= |b|. Returns non-negative result = |a|-|b|.
BigDecimal sub_abs(BigDecimal a, BigDecimal b) {
    align_frac(a, b);
    BigDecimal r;
    r.sign = +1;

    // subtract fractional part
    int borrow = 0;
    r.fracPart.resize(a.fracPart.size());
    for (int i = static_cast<int>(a.fracPart.size()) - 1; i >= 0; --i) {
        int da = a.fracPart[i] - '0' - borrow;
        int db = b.fracPart[i] - '0';
        if (da < db) { da += 10; borrow = 1; } else borrow = 0;
        r.fracPart[i] = char('0' + (da - db));
    }
    trim_trailing_zeros(r.fracPart);

    // subtract integer part
    string A = a.intPart, B = b.intPart;
    if (A.size() < B.size()) A.insert(0, B.size() - A.size(), '0');
    if (B.size() < A.size()) B.insert(0, A.size() - B.size(), '0');

    string s(A.size(), '0');
    for (int i = static_cast<int>(A.size()) - 1; i >= 0; --i) {
        int da = A[i] - '0' - borrow;
        int db = B[i] - '0';
        if (da < db) { da += 10; borrow = 1; } else borrow = 0;
        s[i] = char('0' + (da - db));
    }

    // remove leading zeros
    trim_leading_zeros(s);
    if (s == "0" && r.fracPart.empty()) {
        r.sign = +1;
        r.intPart = "0";
    } else {
        r.intPart = s;
    }
    return r;
}

// a + b (with signs)
BigDecimal add_signed(BigDecimal a, BigDecimal b) {
    if (a.isZero()) return b;
    if (b.isZero()) return a;

    if (a.sign == b.sign) {
        BigDecimal r = add_abs(a, b);
        r.sign = a.sign;
        if (r.isZero()) r.sign = +1;
        return r;
    } else {
        // opposite signs => subtraction by larger magnitude
        int cmp = cmp_abs(a, b);
        if (cmp == 0) {
            return BigDecimal{+1, "0", ""};
        } else if (cmp > 0) {
            BigDecimal r = sub_abs(a, b);
            r.sign = a.sign;
            if (r.isZero()) r.sign = +1;
            return r;
        } else {
            BigDecimal r = sub_abs(b, a);
            r.sign = b.sign;
            if (r.isZero()) r.sign = +1;
            return r;
        }
    }
}

string to_string(const BigDecimal& x) {
    if (x.isZero()) return "0";
    string out;
    if (x.sign < 0) out.push_back('-');
    out += x.intPart;
    if (!x.fracPart.empty()) {
        out.push_back('.');
        out += x.fracPart;
    }
    return out;
}

/* -------------------- I/O & Driver -------------------- */

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "Enter input file name: ";
    std::string filename;
    if (!(std::cin >> filename)) {
        std::cerr << "Failed to read file name.\n";
        return 1;
    }

    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "Error: could not open file '" << filename << "'.\n";
        return 1;
    }

    std::cout << "Processing test cases from '" << filename << "'...\n\n";

    string a, b;
    int lineNo = 0;
    while (fin >> a >> b) {
        ++lineNo;
        bool okA = is_valid_double_literal(a);
        bool okB = is_valid_double_literal(b);

        std::cout << "Case " << lineNo << ": " << a << " + " << b << "\n";
        if (!okA) {
            std::cout << "  -> INVALID: '" << a << "' is not a valid double literal.\n\n";
            continue;
        }
        if (!okB) {
            std::cout << "  -> INVALID: '" << b << "' is not a valid double literal.\n\n";
            continue;
        }

        BigDecimal A = parse_normalize(a);
        BigDecimal B = parse_normalize(b);
        BigDecimal S = add_signed(A, B);

        std::cout << "  -> " << to_string(A) << " + " << to_string(B)
                  << " = " << to_string(S) << "\n\n";
    }

    return 0;
}
