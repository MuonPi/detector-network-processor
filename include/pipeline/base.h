#ifndef PIPELINE_H
#define PIPELINE_H

#include "sink/base.h"
#include "source/base.h"

namespace muonpi::pipeline {

template <typename T>
class base : public sink::base<T>, public source::base<T> {
public:
    base(sink::base<T>& sink);
};

template <typename T>
base<T>::base(sink::base<T>& sink)
    : source::base<T>(sink)
{
}

}

#endif // PIPELINE_H
