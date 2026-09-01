#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

struct Column { std::string name; std::vector<double> values; std::size_t invalid = 0; };
struct Stats { std::size_t count, invalid; double min, max, mean, median, mode, stdev, q1, q3; };

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

static std::vector<std::string> parseCsvRow(const std::string& line) {
    std::vector<std::string> fields; std::string field; bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') { field += '"'; ++i; }
            else quoted = !quoted;
        } else if (c == ',' && !quoted) { fields.push_back(trim(field)); field.clear(); }
        else field += c;
    }
    fields.push_back(trim(field));
    return fields;
}

static bool toDouble(const std::string& text, double& value) {
    if (text.empty()) return false;
    try {
        std::size_t used = 0; value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value);
    } catch (...) { return false; }
}

static double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.size() == 1) return sorted.front();
    const double position = p * (sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    return sorted[lower] + (position - lower) * (sorted[upper] - sorted[lower]);
}

static Stats calculate(const Column& column) {
    std::vector<double> v = column.values; std::sort(v.begin(), v.end());
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    double sumSquares = 0.0; for (double x : v) sumSquares += (x - mean) * (x - mean);
    std::map<double, std::size_t> frequency; for (double x : v) ++frequency[x];
    double mode = v.front(); std::size_t best = 0;
    for (const auto& [value, count] : frequency) if (count > best) { best = count; mode = value; }
    return {v.size(), column.invalid, v.front(), v.back(), mean, percentile(v, .5), mode,
            v.size() > 1 ? std::sqrt(sumSquares / (v.size() - 1)) : 0.0,
            percentile(v, .25), percentile(v, .75)};
}

static void printTable(std::ostream& out, const std::vector<Column>& columns) {
    out << std::left << std::setw(18) << "Column" << std::right << std::setw(8) << "Count"
        << std::setw(9) << "Invalid" << std::setw(10) << "Min" << std::setw(10) << "Max"
        << std::setw(10) << "Mean" << std::setw(10) << "Median" << std::setw(10) << "Mode"
        << std::setw(12) << "Sample SD" << std::setw(10) << "Q1 (25%)" << std::setw(10) << "Q3 (75%)" << '\n'
        << std::string(117, '-') << '\n' << std::fixed << std::setprecision(3);
    for (const auto& c : columns) {
        if (c.values.empty()) continue;
        const Stats s = calculate(c);
        out << std::left << std::setw(18) << c.name << std::right << std::setw(8) << s.count
            << std::setw(9) << s.invalid << std::setw(10) << s.min << std::setw(10) << s.max
            << std::setw(10) << s.mean << std::setw(10) << s.median << std::setw(10) << s.mode
            << std::setw(12) << s.stdev << std::setw(10) << s.q1 << std::setw(10) << s.q3 << '\n';
    }
}

static void writeCsv(const std::string& path, const std::vector<Column>& columns) {
    std::ofstream out(path); if (!out) throw std::runtime_error("Cannot create report: " + path);
    out << "column,count,invalid,min,max,mean,median,mode,sample_std_dev,q1_25_percent,q3_75_percent\n" << std::fixed << std::setprecision(6);
    for (const auto& c : columns) if (!c.values.empty()) {
        const Stats s = calculate(c);
        out << '"' << c.name << "\"," << s.count << ',' << s.invalid << ',' << s.min << ',' << s.max << ','
            << s.mean << ',' << s.median << ',' << s.mode << ',' << s.stdev << ',' << s.q1 << ',' << s.q3 << '\n';
    }
}

int main(int argc, char* argv[]) {
    const std::string input = argc > 1 ? argv[1] : "data/iris.csv";
    const std::string report = argc > 2 ? argv[2] : "output/summary.csv";
    std::ifstream file(input); if (!file) { std::cerr << "Error: cannot open " << input << '\n'; return 1; }
    std::string line; if (!std::getline(file, line)) { std::cerr << "Error: CSV is empty.\n"; return 1; }
    auto headers = parseCsvRow(line); if (headers.empty()) { std::cerr << "Error: missing header.\n"; return 1; }
    std::vector<Column> columns; for (const auto& h : headers) columns.push_back({h, {}, 0});
    std::size_t rows = 0;
    while (std::getline(file, line)) {
        if (trim(line).empty()) continue;
        ++rows;
        auto fields = parseCsvRow(line);
        fields.resize(headers.size());
        for (std::size_t i = 0; i < headers.size(); ++i) { double value; if (toDouble(fields[i], value)) columns[i].values.push_back(value); else ++columns[i].invalid; }
    }
    std::vector<Column> numeric; for (auto& c : columns) if (!c.values.empty()) numeric.push_back(std::move(c));
    if (numeric.empty()) { std::cerr << "Error: no numeric columns found.\n"; return 1; }
    std::cout << "CSV Statistics Analyzer\nInput: " << input << " | Data rows: " << rows << " | Numeric columns: " << numeric.size() << "\n\n";
    printTable(std::cout, numeric);
    try { writeCsv(report, numeric); } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
    std::cout << "\nReport saved to: " << report << '\n'; return 0;
}
