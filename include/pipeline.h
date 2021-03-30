#ifndef PIPELINE_H
#define PIPELINE_H

#include "sink/base.h"
#include "source/base.h"

namespace MuonPi {

template <typename T>
/**
 * @brief The Pipeline class Combines a Sink and source
 */
class Pipeline : public Sink::Base<T>, public Source::Base<T> {
public:
    /**
     * @brief Pipeline
     * @param sink The sink to connect to
     */
    Pipeline(Sink::Base<T>& sink);
};

template <typename T>
Pipeline<T>::Pipeline(Sink::Base<T>& sink)
    : Source::Base<T>(sink)
{
}

}

#endif // PIPELINE_H
