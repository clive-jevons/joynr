/*
 * #%L
 * %%
 * Copyright (C) 2011 - 2013 BMW Car IT GmbH
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
#include "joynr/KeywordArbitrator.h"
#include "joynr/DiscoveryQos.h"
#include "joynr/system/IDiscovery.h"
#include "joynr/types/DiscoveryEntry.h"
#include "joynr/RequestStatus.h"

#include "joynr/TypeUtil.h"

#include <cassert>

namespace joynr
{

KeywordArbitrator::KeywordArbitrator(const std::string& domain,
                                     const std::string& interfaceName,
                                     joynr::system::IDiscoverySync& discoveryProxy,
                                     const DiscoveryQos& discoveryQos)
        : ProviderArbitrator(domain, interfaceName, discoveryProxy, discoveryQos),
          keyword(discoveryQos.getCustomParameter(DiscoveryQos::KEYWORD_PARAMETER()).getValue()),
          logger(joynr_logging::Logging::getInstance()->getLogger("KArb", "KeywordArbitrator"))
{
}

void KeywordArbitrator::attemptArbitration()
{
    std::vector<joynr::types::DiscoveryEntry> result;
    try {
        discoveryProxy.lookup(result, domain, interfaceName, systemDiscoveryQos);
        receiveCapabilitiesLookupResults(result);
    } catch (exceptions::JoynrException& e) {
        LOG_ERROR(logger,
                  FormatString("Unable to lookup provider (domain: %1, interface: %2) "
                               "from discovery. Error: %3.")
                          .arg(domain)
                          .arg(interfaceName)
                          .arg(e.getMessage())
                          .str());
    }
}

void KeywordArbitrator::receiveCapabilitiesLookupResults(
        const std::vector<joynr::types::DiscoveryEntry>& discoveryEntries)
{
    // Check for an empty list of results
    if (discoveryEntries.size() == 0) {
        return;
    }

    // Loop through the result list
    for (joynr::types::DiscoveryEntry discoveryEntry : discoveryEntries) {
        types::ProviderQos providerQos = discoveryEntry.getQos();
        LOG_TRACE(logger,
                  FormatString("Looping over capabilitiesEntry: %1")
                          .arg(discoveryEntry.toString())
                          .str());

        // Check that the provider supports onChange subscriptions if this was requested
        if (discoveryQos.getProviderMustSupportOnChange() &&
            !providerQos.getSupportsOnChangeSubscriptions()) {
            continue;
        }

        // Search the QosParameters for the keyword field
        std::vector<types::CustomParameter> qosParameters = providerQos.getCustomParameters();
        for (types::CustomParameter parameter : qosParameters) {
            std::string name = parameter.getName();
            if (name == DiscoveryQos::KEYWORD_PARAMETER() && keyword == parameter.getValue()) {
                std::string res = discoveryEntry.getParticipantId();
                LOG_TRACE(logger, FormatString("setting res to %1").arg(res).str());
                joynr::types::CommunicationMiddleware::Enum preferredConnection(
                        selectPreferredCommunicationMiddleware(discoveryEntry.getConnections()));
                updateArbitrationStatusParticipantIdAndAddress(
                        ArbitrationStatus::ArbitrationSuccessful, res, preferredConnection);
                return;
            }
        }
    }

    // If this point is reached, no provider with the keyword was found
}

} // namespace joynr
