#ifndef MOCK_CPR_H
#define MOCK_CPR_H

#include "trompeloeil.hpp"
#include "cpr/cpr.h"

class MockCpr {
public:
    MAKE_MOCK5(Post, cpr::Response(const cpr::Url&, const cpr::Header&, const cpr::Header&, const cpr::Body&, const cpr::SslOptions&));
};

#endif // MOCK_CPR_H
