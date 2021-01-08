#ifndef PIPELINE_H
#define PIPELINE_H

#include "sink/base.h"
#include "source/base.h"

namespace MuonPi {

template <typename T>
class Pipeline : public Sink::Base<T>, public Source::Base<T> {
public:
    Pipeline(Sink::Base<T>& sink);
};

template <typename T>
Pipeline<T>::Pipeline(Sink::Base<T>& sink)
    : Source::Base<T>(sink)
{
}

}

#endif // PIPELINE_H
