#ifndef DETECTORSUMMARY_H
#define DETECTORSUMMARY_H

#include "userinfo.h"

#include <chrono>

namespace muonpi {

/**
 * @brief The detetor_summary_t class
 * Holds information about accumulated statistics and gathered info about a detector
 */
class detetor_summary_t {
public:
    struct Data {
        double deadtime { 0.0 };
        bool active { false };
        double mean_eventrate { 0.0 };
        double stddev_eventrate { 0.0 };
        double mean_pulselength { 0.0 };
        std::int64_t ublox_counter_progress { 0 };
        std::uint64_t incoming { 0UL };
        std::uint8_t change { 0 };
        double mean_time_acc { 0.0 };
    };

    /**
     * @brief detector_log_t
     * @param hash The hash of the detector identifier
     * @param data The data struct to be provided
     */
    detetor_summary_t(std::size_t hash, userinfo_t user_info, Data data);

    detetor_summary_t() noexcept;

    /**
     * @brief hash
     * @return The hash of the detector for this event
     */
    [[nodiscard]] auto hash() const noexcept -> std::size_t;

    /**
     * @brief time The time this log message was created
     * @return The creation time
     */
    [[nodiscard]] auto time() const -> std::chrono::system_clock::time_point;

    /**
     * @brief valid Indicates whether this message is valid
     * @return message is valid
     */
    [[nodiscard]] auto valid() const -> bool;

    /**
     * @brief data Accesses the data from the object
     * @return data struct
     */
    [[nodiscard]] auto data() const -> Data;

    /**
     * @brief data Accesses the user info from the object
     * @return the userinfo_t struct
     */
    [[nodiscard]] auto user_info() const -> userinfo_t;

    void set_change_flag();

private:
    std::size_t m_hash { 0 };

    std::chrono::system_clock::time_point m_time { std::chrono::system_clock::now() };
    Data m_data {};
    userinfo_t m_userinfo {};
    bool m_valid { true };
};
}

#endif // DETECTORSUMMARY_H
