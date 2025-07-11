#ifndef ENHANCED_ENDPOINTS_H
#define ENHANCED_ENDPOINTS_H

#include "httplib.h"
#include <string>

// Enhanced API endpoints with improved validation and features
void setup_enhanced_job_endpoints(httplib::Server &svr, const std::string& api_key);
void setup_enhanced_system_endpoints(httplib::Server &svr, const std::string& api_key);

#endif // ENHANCED_ENDPOINTS_H