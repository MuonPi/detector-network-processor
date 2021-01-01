#ifndef ASCIIEVENTSINK_H
#define ASCIIEVENTSINK_H

#include "abstractsink.h"
#include "event.h"

#include <memory>
#include <iostream>

namespace MuonPi::Sinks {

template <typename T>
/**
 * @brief The AsciiEventSink class
 */
class Ascii : public Sink<Event>
{
public:
    /**
     * @brief AsciiEventSink
     * @param a_ostream The stream to which the output should be written
     */
    Ascii(std::ostream& a_ostream);

    ~Ascii() override;

    void get(T message);

private:
    [[nodiscard]] auto to_string(const Event& evt) const -> std::string;

    std::ostream& m_ostream;
};

template <typename T>
Ascii::Ascii(std::ostream& ostream)
    : Sink<T> {}
    , m_ostream { ostream }
{
}

template <typename T>
Ascii::~Ascii() = default;



}

#endif // ASCIIEVENTSINK_H
