#ifndef MOCK_CPR_H
#define MOCK_CPR_H

#include "trompeloeil.hpp"
#include "cpr/cpr.h"

class MockCprApi : public CprApi {
public:
    MAKE_MOCK4(Post, cpr::Response(const cpr::Url&, const cpr::Header&, const cpr::Body&, const cpr::SslOptions&));
    MAKE_MOCK3(Get, cpr::Response(const cpr::Url&, const cpr::Header&, const cpr::SslOptions&));
};

#endif // MOCK_CPR_H
