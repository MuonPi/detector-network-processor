#ifndef PIPELINE_H
#define PIPELINE_H

#include "sink/base.h"
#include "source/base.h"

namespace muonpi {

template <typename T>
class pipeline : public sink::base<T>, public source::base<T> {
public:
    pipeline(sink::base<T>& sink);
};

template <typename T>
pipeline<T>::pipeline(sink::base<T>& sink)
    : source::base<T>(sink)
{
}

}

#endif // PIPELINE_H
