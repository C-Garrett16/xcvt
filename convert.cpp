#include <cctype>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

constexpr double PROGRAM_VERSION{0.5};

std::unordered_map<std::string, double> length_factors{
    {"m", 1.0},     {"cm", 0.01}, {"mm", 0.001},  {"ft", 0.3048},
    {"yd", 0.9144}, {"km", 1000}, {"mi", 1609.34}};

std::unordered_map<std::string, double> mass_factors{
    {"kg", 1.0}, {"g", 0.001}, {"lb", 0.453592}, {"oz", 0.0283495}};

std::unordered_map<std::string, double> volume_factors{
    {"L", 1.0},       // liter
    {"l", 1.0},       // lowercase alias
    {"mL", 0.001},    // milliliter
    {"ml", 0.001},    // lowercase alias
    {"uL", 0.000001}, // microliter
    {"ul", 0.000001}, // lowercase alias

    {"gal", 3.78541},    // US gallon
    {"qt", 0.946353},    // US quart
    {"pt", 0.473176},    // US pint
    {"cup", 0.24},       // metric cup
    {"floz", 0.0295735}, // US fluid ounce

    {"tbsp", 0.0147868}, // tablespoon
    {"tsp", 0.00492892}, // teaspoon

    {"m3", 1000.0},     // cubic meter
    {"cm3", 0.001},     // cubic centimeter = milliliter
    {"cc", 0.001},      // cc (same as mL)
    {"in3", 0.0163871}, // cubic inch
    {"ft3", 28.3168},   // cubic foot
};

struct TempUnit {
    std::function<double(double)> to_celsius;
    std::function<double(double)> from_celsius;
};

std::unordered_map<std::string, TempUnit> temp_units{
    // Celsius
    {"C", TempUnit{[](double c) { return c; }, [](double c) { return c; }}},
    // Fahrenheit
    {"F", TempUnit{[](double f) { return (f - 32.0) * 5.0 / 9.0; },
                   [](double c) { return c * 9.0 / 5.0 + 32.0; }}},
    // Kelvin
    {"K", TempUnit{[](double k) { return k - 273.15; },
                   [](double c) { return c + 273.15; }}},
};

// std::unordered_map<std::string, double> currency_factors {   <-- This is a
// WIP
//     { "USD", 1.0 },
//     { "EUR", 1.1 },
//     { "GBP", 1.3 },
//     { "AUD", 1.4 },
//     { "CNY", 0.3 },
//     { "JPY", 0.8 }
// };

// Maps "weird user input" -> canonical unit key used in your factor maps
std::unordered_map<std::string, std::string> unit_aliases = {
    // length
    {"meter", "m"},
    {"meters", "m"},
    {"metre", "m"},
    {"metres", "m"},
    {"kilometer", "km"},
    {"kilometers", "km"},
    {"kilometre", "km"},
    {"kilometres", "km"},
    {"foot", "ft"},
    {"feet", "ft"},
    {"yard", "yd"},
    {"yards", "yd"},
    {"mile", "mi"},
    {"miles", "mi"},

    // mass
    {"kilogram", "kg"},
    {"kilograms", "kg"},
    {"gram", "g"},
    {"grams", "g"},
    {"pound", "lb"},
    {"pounds", "lb"},
    {"lbs", "lb"}, // common typo / plural
    {"ounce", "oz"},
    {"ounces", "oz"},

    // volume
    {"liter", "L"},
    {"liters", "L"},
    {"litre", "L"},
    {"litres", "L"},
    {"milliliter", "mL"},
    {"milliliters", "mL"},
    {"millilitre", "mL"},
    {"millilitres", "mL"},
    {"cup", "cup"},
    {"cups", "cup"},
    {"tablespoon", "tbsp"},
    {"tablespoons", "tbsp"},
    {"teaspoon", "tsp"},
    {"teaspoons", "tsp"},

    // temperature
    {"c", "C"},
    {"celsius", "C"},
    {"centigrade", "C"},
    {"f", "F"},
    {"fahrenheit", "F"},
    {"k", "K"},
    {"kelvin", "K"},
};

void print_usage() {
    std::cout << "Usage: convert -f <from_unit> -t <to_unit> <value>\n"
                 "Options:\n"
                 "  -h, --help        Show this help message\n"
                 "  -l, --list        List supported units\n"
              << std::endl;
}

enum class UnitCategory { Length, Mass, Volume, Tempurature, Unknown };

struct ConversionRule {
    std::string from_unit;
    std::string to_unit;
    std::function<double(double)> convert;
};

struct Args {
    std::string from_unit;
    std::string to_unit;
    double value;
    bool show_help{false};
    bool list_units{false};
    bool show_version{false};
};

UnitCategory get_unit_category(std::string unit) {

    if (length_factors.count(unit) > 0) {
        return UnitCategory::Length;
    }
    else if (mass_factors.count(unit) > 0) {
        return UnitCategory::Mass;
    }
    else if (volume_factors.count(unit) > 0) {
        return UnitCategory::Volume;
    }
    else if (temp_units.count(unit) > 0) {
        return UnitCategory::Tempurature;
    }
    else {
        return UnitCategory::Unknown;
    }
}

std::string to_lower(std::string s) {
    for (char &ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
};

std::string normalize_unit(std::string u) {

    std::string lower = to_lower(u);

    auto it = unit_aliases.find(lower);
    if (it != unit_aliases.end()) {
        return it->second;
    }

    return u;
}

Args parse_args(int argc, char *argv[]) {
    // Argument handling
    Args result;
    bool have_from{false};
    bool have_to{false};
    bool have_value{false};

    for (int i = 1; i < argc; ++i) {

        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            result.show_help = true;
        } else if (arg == "-l" || arg == "--list" || arg == "--units") {
            result.list_units = true;
        } else if (arg == "-v" || arg == "--version") {
            result.show_version = true;
        } else if (arg == "-f" || arg == "--from") {
            if (i + 1 < argc) {
                if (i + 1 >= argc) {
                    throw std::runtime_error(
                        "'-f/--from' flag requires a unit.");
                }
                result.from_unit = normalize_unit(argv[++i]);
                have_from = true;
            }
        } else if (arg == "-t" || arg == "--to") {
            if (i + 1 < argc) {
                if (i + 1 >= argc) {
                    throw std::runtime_error("'-t/--to' flag requires a unit.");
                }
                result.to_unit = normalize_unit(argv[++i]);
                have_to = true;
            }
        } else {
            try {
                result.value = std::stod(arg);
                have_value = true;

            } catch (const std::exception &) {
                throw std::invalid_argument("Value must be a valid number.");
            }
        }
    }

    if (!result.list_units && !result.show_help && !result.show_version) {
        if (!have_from || !have_to || !have_value) {
            throw std::runtime_error("Missing required arguments");
        }
    }

    return result;
}

double convert_via_factors(
    const std::unordered_map<std::string, double>& factors,
    const std::string& from_unit,
    const std::string& to_unit,
    double value
) {
    double from_factor = factors.at(from_unit);
    double to_factor   = factors.at(to_unit);

    double value_in_base = value * from_factor; // meters, kg, liters, etc.
    return value_in_base / to_factor;
}


double convert(const std::string& from_unit,
               const std::string& to_unit,
               double value) {

    UnitCategory cat_from = get_unit_category(from_unit);
    UnitCategory cat_to   = get_unit_category(to_unit);

    if (cat_from == UnitCategory::Unknown || cat_to == UnitCategory::Unknown) {
        throw std::runtime_error("Category unknown");
    }

    if (cat_from != cat_to) {
        throw std::runtime_error("Incompatible categories");
    }

    if (cat_from == UnitCategory::Length) {
        return convert_via_factors(length_factors, from_unit, to_unit, value);
    }

    if (cat_from == UnitCategory::Mass) {
        return convert_via_factors(mass_factors, from_unit, to_unit, value);
    }

    if (cat_from == UnitCategory::Volume) {
        return convert_via_factors(volume_factors, from_unit, to_unit, value);
    }

    if (cat_from == UnitCategory::Tempurature) {
        double in_c = temp_units.at(from_unit).to_celsius(value);
        return temp_units.at(to_unit).from_celsius(in_c);
    }

    // Should never get here, but just in case:
    throw std::runtime_error("Unhandled category");
}


void print_units() {
    std::cout << "Supported units:\n\n";

    std::cout << "Length:\n  ";
    for (const auto &[unit, factor] : length_factors) {
        std::cout << unit << "  ";
    }
    std::cout << "\n\n";

    std::cout << "Mass:\n  ";
    for (const auto &[unit, factor] : mass_factors) {
        std::cout << unit << "  ";
    }
    std::cout << "\n\n";

    std::cout << "Volume:\n  ";
    for (const auto &[unit, factor] : volume_factors) {
        std::cout << unit << "  ";
    }
    std::cout << "\n\n";

    std::cout << "Temperature:\n  ";
    for (const auto &[unit, funcs] : temp_units) {
        std::cout << unit << "  ";
    }
    std::cout << "\n";
}

int main(int argc, char *argv[]) {
    try {
        Args args = parse_args(argc, argv);

        if (args.show_help) {
            print_usage();
            std::cout << "For a list of units, use the -l or --list flag.\n";
            return 0;
        }

        if (args.list_units) {
            print_units();
            return 0;
        }

        if (args.show_version) {
            std::cout << "Current Version:\t\033[1;32m" << PROGRAM_VERSION << "\033[0m\n";
            return 0;
        }

        if (args.from_unit == "" || args.to_unit == "") {
            print_usage();
            return 1;
        }

        double result = convert(args.from_unit, args.to_unit, args.value);

        // Print out values.
        std::cout << "From: " << args.from_unit << "\n"
                  << "To: " << args.to_unit << "\n"
                  << "Value: \033[1;32m" << result << args.to_unit
                  << "\033[0m\n";

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "\033[1;31mError: \033[31m" << e.what() << "\033[0m\n";
        print_usage();
        return 1;
    }
}
