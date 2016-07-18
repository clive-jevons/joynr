/*
 * #%L
 * %%
 * Copyright (C) 2011 - 2016 BMW Car IT GmbH
 * %%
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * #L%
 */
#ifndef DLTSINK_H
#define DLTSINK_H

#include <spdlog/sinks/sink.h>
#include <spdlog/details/log_msg.h>
#include <dlt/dlt.h>

namespace joynr
{
class DltSink : public spdlog::sinks::sink
{
private:
    struct DltContextHolder
    {
        DltContext dltContext;

        DltContextHolder() : dltContext()
        {
            dlt_register_context(&dltContext, "JOYN", "joynr DLT context");
        }

        ~DltContextHolder()
        {
            dlt_unregister_context(&dltContext);
        }
    };

    static DltContextHolder& getInstance()
    {
        static DltContextHolder instance;
        return instance;
    }

public:
    DltSink() = default;

    ~DltSink() override = default;

protected:
    void flush() override
    {
    }

    void log(const spdlog::details::log_msg& msg) override
    {
        DltLogLevelType dltLogLevel = DltLogLevelType::DLT_LOG_DEFAULT;
        switch (msg.level) {
        case spdlog::level::trace:
            dltLogLevel = DLT_LOG_VERBOSE;
            break;
        case spdlog::level::debug:
            dltLogLevel = DLT_LOG_DEBUG;
            break;
        case spdlog::level::info:
            dltLogLevel = DLT_LOG_INFO;
            break;
        case spdlog::level::warn:
            dltLogLevel = DLT_LOG_WARN;
            break;
        case spdlog::level::err:
            dltLogLevel = DLT_LOG_ERROR;
            break;
        case spdlog::level::emerg:
            dltLogLevel = DLT_LOG_FATAL;
            break;
        default:
            break;

        // The following cases are never used.
        case spdlog::level::notice:
        case spdlog::level::critical:
        case spdlog::level::alert:
        case spdlog::level::off:
            dltLogLevel = DLT_LOG_VERBOSE;
            break;
        }
        DltContext& dltContext = getInstance().dltContext;
        DLT_LOG_STRING(dltContext, dltLogLevel, msg.formatted.c_str());
    }
};
} // namespace joynr
#endif // DLTSINK_H
