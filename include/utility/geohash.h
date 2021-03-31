#ifndef GEOHASH_H
#define GEOHASH_H

#include "defaults.h"

#include <string>

namespace muonpi {
/**
 * @brief The GeoHash class
 * a class with a static function to generate a geo hash string from given geographic ccordinates
 */
class GeoHash {
public:
    GeoHash() = delete;
    static std::string hashFromCoordinates(double lon, double lat, std::size_t precision);
};

} // namespace muonpi

#endif // GEOHASH_H
