#ifndef UNITS_H
#define UNITS_H

namespace muonpi {

namespace units {
    static constexpr double pi = 3.14159265358979323846;

    static constexpr double giga = 1.0e9;
    static constexpr double mega = 1.0e6;
    static constexpr double kilo = 1.0e3;

    static constexpr double centi = 1.0e-2;
    static constexpr double milli = 1.0e-3;
    static constexpr double micro = 1.0e-6;
    static constexpr double nano = 1.0e-9;


    static constexpr double radian = 1.0;
    static constexpr double degree = (pi / 180.0) * radian;


    static constexpr double kilometer = 1.0;
    static constexpr double meter = milli * kilometer;
}

}

#endif // UNITS_H
