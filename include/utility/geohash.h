#ifndef GEOHASH_H
#define GEOHASH_H

#include "defaults.h"

#include <string>

namespace muonpi::geohash {

static constexpr const char* base32 { "0123456789bcdefghjkmnpqrstuvwxyz" }; // (geohash-specific) base32 map

[[nodiscard]] static inline auto from_coordinates(double lon, double lat, std::size_t precision) -> std::string
{
    if (precision > 12) {
        precision = 12;
    }

    uint8_t idx = 0; // index into base32 map
    uint8_t bit = 0; // each char holds 5 bits
    bool evenBit = true;
    std::string geohash;

    double latMin = -90.;
    double latMax = 90.;
    double lonMin = -180.;
    double lonMax = 180.;

    if (lon < lonMin || lon > lonMax) {
        return geohash;
    }
    if (lat < latMin || lat > latMax) {
        return geohash;
    }

    while (geohash.size() < precision) {
        if (evenBit) {
            // bisect E-W longitude
            const double lonMid = (lonMin + lonMax) / 2.;
            if (lon >= lonMid) {
                idx = idx * 2 + 1;
                lonMin = lonMid;
            } else {
                idx = idx * 2;
                lonMax = lonMid;
            }
        } else {
            // bisect N-S latitude
            const double latMid = (latMin + latMax) / 2.;
            if (lat >= latMid) {
                idx = idx * 2 + 1;
                latMin = latMid;
            } else {
                idx = idx * 2;
                latMax = latMid;
            }
        }
        evenBit = !evenBit;

        if (++bit == 5) {
            // 5 bits gives us a character: append it and start over
            geohash += base32[idx];
            bit = 0;
            idx = 0;
        }
    }
    return geohash;
}

} // namespace muonpi

#endif // GEOHASH_H
