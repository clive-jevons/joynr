/*
 * #%L
 * %%
 * Copyright (C) 2011 - 2017 BMW Car IT GmbH
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
#include "joynr/Arbitrator.h"

#include <cassert>
#include <chrono>
#include <vector>

#include <boost/algorithm/string/join.hpp>

#include "joynr/Future.h"
#include "joynr/Logger.h"
#include "joynr/Semaphore.h"
#include "joynr/exceptions/JoynrException.h"
#include "joynr/exceptions/NoCompatibleProviderFoundException.h"
#include "joynr/system/IDiscovery.h"
#include "joynr/types/DiscoveryScope.h"

namespace joynr
{

INIT_LOGGER(Arbitrator);

Arbitrator::Arbitrator(
        const std::string& domain,
        const std::string& interfaceName,
        const joynr::types::Version& interfaceVersion,
        std::weak_ptr<joynr::system::IDiscoveryAsync> discoveryProxy,
        const DiscoveryQos& discoveryQos,
        std::unique_ptr<const ArbitrationStrategyFunction> arbitrationStrategyFunction)
        : discoveryProxy(discoveryProxy),
          discoveryQos(discoveryQos),
          systemDiscoveryQos(discoveryQos.getCacheMaxAgeMs(),
                             discoveryQos.getDiscoveryTimeoutMs(),
                             discoveryQos.getDiscoveryScope(),
                             discoveryQos.getProviderMustSupportOnChange()),
          domains({domain}),
          interfaceName(interfaceName),
          interfaceVersion(interfaceVersion),
          discoveredIncompatibleVersions(),
          arbitrationError("Arbitration could not be finished in time."),
          arbitrationStrategyFunction(std::move(arbitrationStrategyFunction)),
          arbitrationFinished(false),
          arbitrationRunning(false),
          keepArbitrationRunning(false),
          arbitrationThread()
{
}

Arbitrator::~Arbitrator()
{
    keepArbitrationRunning = false;

    if (arbitrationThread.joinable()) {
        // do not cause an abort waiting for ourselves
        if (std::this_thread::get_id() == arbitrationThread.get_id()) {
            arbitrationThread.detach();
        } else {
            arbitrationThread.join();
        }
    }
}

void Arbitrator::startArbitration(
        std::function<void(const types::DiscoveryEntryWithMetaInfo& discoveryEntry)> onSuccess,
        std::function<void(const exceptions::DiscoveryException& exception)> onError)
{
    if (arbitrationRunning) {
        return;
    }

    arbitrationRunning = true;
    keepArbitrationRunning = true;

    onSuccessCallback = onSuccess;
    onErrorCallback = onError;

    arbitrationThread = std::thread([this]() {
        Semaphore semaphore;
        arbitrationFinished = false;

        std::string serializedDomainsList = boost::algorithm::join(domains, ", ");
        JOYNR_LOG_DEBUG(logger,
                        "DISCOVERY lookup for domain: [{}], interface: {}",
                        serializedDomainsList,
                        interfaceName);

        // Arbitrate until successful or timed out
        auto start = std::chrono::system_clock::now();

        while (keepArbitrationRunning) {
            attemptArbitration();

            if (arbitrationFinished) {
                return;
            }

            // If there are no suitable providers, retry the arbitration after the retry interval
            // elapsed
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

            if (discoveryQos.getDiscoveryTimeoutMs() <= duration.count()) {
                // discovery timeout reached
                break;
            } else if (discoveryQos.getDiscoveryTimeoutMs() - duration.count() <=
                       discoveryQos.getRetryIntervalMs()) {
                /*
                 * no retry possible -> wait until discoveryTimeout is reached and inform caller
                 * about
                 * cancelled arbitration
                 */
                semaphore.waitFor(std::chrono::milliseconds(discoveryQos.getDiscoveryTimeoutMs() -
                                                            duration.count()));
                break;
            } else {
                // wait for retry interval and attempt a new arbitration
                semaphore.waitFor(std::chrono::milliseconds(discoveryQos.getRetryIntervalMs()));
            }
        }

        // If this point is reached the arbitration timed out
        if (!discoveredIncompatibleVersions.empty()) {
            onErrorCallback(
                    exceptions::NoCompatibleProviderFoundException(discoveredIncompatibleVersions));
        } else {
            onErrorCallback(arbitrationError);
        }

        arbitrationRunning = false;
    });
}

void Arbitrator::attemptArbitration()
{
    std::vector<joynr::types::DiscoveryEntryWithMetaInfo> result;
    try {
        if (auto discoveryProxySharedPtr = discoveryProxy.lock()) {
            if (discoveryQos.getArbitrationStrategy() ==
                DiscoveryQos::ArbitrationStrategy::FIXED_PARTICIPANT) {
                types::DiscoveryEntryWithMetaInfo fixedParticipantResult;
                std::string fixedParticipantId =
                        discoveryQos.getCustomParameter("fixedParticipantId").getValue();
                auto future = discoveryProxySharedPtr->lookupAsync(fixedParticipantId);
                future->get(fixedParticipantResult);
                result.push_back(fixedParticipantResult);
            } else {
                auto future = discoveryProxySharedPtr->lookupAsync(
                        domains, interfaceName, systemDiscoveryQos);
                future->get(result);
            }
        } else {
            throw exceptions::JoynrRuntimeException("discoveryProxy not available");
        }
        receiveCapabilitiesLookupResults(result);
    } catch (const exceptions::JoynrException& e) {
        std::string errorMsg = "Unable to lookup provider (domain: " +
                               (domains.empty() ? std::string("EMPTY") : domains.at(0)) +
                               ", interface: " + interfaceName + ") from discovery. Error: " +
                               e.getMessage();
        JOYNR_LOG_ERROR(logger, errorMsg);
        arbitrationError.setMessage(errorMsg);
    }
}

void Arbitrator::receiveCapabilitiesLookupResults(
        const std::vector<joynr::types::DiscoveryEntryWithMetaInfo>& discoveryEntries)
{
    discoveredIncompatibleVersions.clear();

    // Check for empty results
    if (discoveryEntries.empty()) {
        arbitrationError.setMessage("No entries found for domain: " +
                                    (domains.empty() ? std::string("EMPTY") : domains.at(0)) +
                                    ", interface: " + interfaceName);
        return;
    }

    std::vector<joynr::types::DiscoveryEntryWithMetaInfo> preFilteredDiscoveryEntries;
    joynr::types::Version providerVersion;
    std::size_t providersWithoutSupportOnChange = 0;
    std::size_t providersWithIncompatibleVersion = 0;
    for (const joynr::types::DiscoveryEntryWithMetaInfo& discoveryEntry : discoveryEntries) {
        const types::ProviderQos& providerQos = discoveryEntry.getQos();
        JOYNR_LOG_TRACE(logger, "Looping over capabilitiesEntry: {}", discoveryEntry.toString());
        providerVersion = discoveryEntry.getProviderVersion();

        if (discoveryQos.getProviderMustSupportOnChange() &&
            !providerQos.getSupportsOnChangeSubscriptions()) {
            ++providersWithoutSupportOnChange;
            continue;
        }

        if (providerVersion.getMajorVersion() != interfaceVersion.getMajorVersion() ||
            providerVersion.getMinorVersion() < interfaceVersion.getMinorVersion()) {
            JOYNR_LOG_TRACE(logger,
                            "Skipping capabilitiesEntry with incompatible version, expected: " +
                                    std::to_string(interfaceVersion.getMajorVersion()) + "." +
                                    std::to_string(interfaceVersion.getMinorVersion()));
            discoveredIncompatibleVersions.insert(providerVersion);
            ++providersWithIncompatibleVersion;
            continue;
        }

        preFilteredDiscoveryEntries.push_back(discoveryEntry);
    }

    if (preFilteredDiscoveryEntries.empty()) {
        std::string errorMsg;
        if (providersWithoutSupportOnChange == discoveryEntries.size()) {
            errorMsg = "There was more than one entries in capabilitiesEntries, but none supported "
                       "on change subscriptions.";
            JOYNR_LOG_WARN(logger, errorMsg);
            arbitrationError.setMessage(errorMsg);
        } else if ((providersWithoutSupportOnChange + providersWithIncompatibleVersion) ==
                   discoveryEntries.size()) {
            errorMsg = "There was more than one entries in capabilitiesEntries, but none "
                       "was compatible.";
            JOYNR_LOG_WARN(logger, errorMsg);
            arbitrationError.setMessage(errorMsg);
        }
        return;
    } else {
        types::DiscoveryEntryWithMetaInfo res;

        try {
            res = arbitrationStrategyFunction->select(
                    discoveryQos.getCustomParameters(), preFilteredDiscoveryEntries);
        } catch (const exceptions::DiscoveryException& e) {
            arbitrationError = e;
        }
        if (!res.getParticipantId().empty()) {
            onSuccessCallback(res);
            arbitrationFinished = true;
        }
    }
}

} // namespace joynr
