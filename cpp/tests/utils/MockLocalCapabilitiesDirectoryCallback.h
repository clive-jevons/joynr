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
#ifndef MOCKCAPABILITIESRESULTCALLBACK_H_
#define MOCKCAPABILITIESRESULTCALLBACK_H_

#include <vector>
#include "joynr/ILocalCapabilitiesCallback.h"
#include "joynr/Semaphore.h"

class MockLocalCapabilitiesDirectoryCallback : public joynr::ILocalCapabilitiesCallback {

public:
    MockLocalCapabilitiesDirectoryCallback();

    virtual void capabilitiesReceived(std::vector<joynr::CapabilityEntry> capabilities) override;

    std::vector<joynr::CapabilityEntry> getResults(int timeout);
    void clearResults();

    virtual ~MockLocalCapabilitiesDirectoryCallback();

private:
    std::vector<joynr::CapabilityEntry> results;
    joynr::Semaphore semaphore;
};

#endif //MOCKCAPABILITIESRESULTCALLBACK_H_
