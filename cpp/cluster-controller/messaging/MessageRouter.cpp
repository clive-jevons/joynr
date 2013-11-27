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
#include "joynr/MessageRouter.h"
#include "joynr/DispatcherUtils.h"
#include "joynr/MessagingStubFactory.h"
#include "joynr/Directory.h"
#include "joynr/JoynrMessagingEndpointAddress.h"
#include "joynr/joynrlogging.h"
#include "joynr/DelayedScheduler.h"
#include "joynr/EndpointAddressBase.h"
#include "joynr/LocalCapabilitiesDirectory.h"
#include "joynr/LocalChannelUrlDirectory.h"

#include <cassert>

namespace joynr {

using namespace joynr_logging;
Logger* MessageRouter::logger = Logging::getInstance()->getLogger("MSG", "MessageRouter");

MessageRouter::~MessageRouter() {
    threadPool.waitForDone();
    delete delayedScheduler;
    delete messagingStubFactory;
}

MessageRouter::MessageRouter(Directory<QString, EndpointAddressBase>* partId2MessagingEndpointDirectory,
                             int messageSendRetryInterval,
                             int maxThreads):
    messagingStubFactory(NULL),
    partId2MessagingEndpointDirectory(partId2MessagingEndpointDirectory),
    threadPool(),
    delayedScheduler()
{
    threadPool.setMaxThreadCount(maxThreads);
    delayedScheduler = new ThreadPoolDelayedScheduler(threadPool, QString("MessageRouter-DelayedScheduler"), messageSendRetryInterval);
}

void MessageRouter::init(ICommunicationManager &comMgr)
{
    assert(messagingStubFactory == NULL);
    messagingStubFactory = new MessagingStubFactory(comMgr);
    addProvisionedCapabilitiesDirectoryAddress();
    addProvisionedChannelUrlDirectoryAddress();
}

void MessageRouter::addProvisionedCapabilitiesDirectoryAddress()
{
    QSharedPointer<EndpointAddressBase> endpointAddress(
                new JoynrMessagingEndpointAddress(MessagingSettings::SETTING_CAPABILITIES_DIRECTORY_CHANNELID()));
    partId2MessagingEndpointDirectory->add(LocalCapabilitiesDirectory::CAPABILITIES_DIRECTORY_PARTICIPANTID(),
                                           endpointAddress);
}

void MessageRouter::addProvisionedChannelUrlDirectoryAddress() {
    QSharedPointer<EndpointAddressBase> endpointAddress(
                new JoynrMessagingEndpointAddress(MessagingSettings::SETTING_CHANNEL_URL_DIRECTORY_CHANNELID()));
    partId2MessagingEndpointDirectory->add(LocalChannelUrlDirectory::CHANNEL_URL_DIRECTORY_PARTICIPANTID(),
                                           endpointAddress);
}

/**
  * Q (RDZ): What happens if the message cannot be forwarded? Exception? Log file entry?
  * Q (RDZ): When are messagingstubs removed? They are stored indefinitely in the factory
  */
void MessageRouter::route(const JoynrMessage& message, const MessagingQos& qos) {
    assert(messagingStubFactory != NULL);
    // neither JoynrMessage nor MessagingQos give a decaytime, so it doesn't make sense to check for
    // a passed TTL. The TTL itself is only relative, not absolute, so it cannot be used here.
    /*
    if (QDateTime::currentMSecsSinceEpoch() >  qos.getRoundTripTtl_ms()) {
        LOG_DEBUG(logger, "received an expired Message. Dropping the message");
        return;
    }
    */
    LOG_TRACE(logger, "Routing Message.");
    QString destinationPartId = message.getHeaderTo();
    QSharedPointer <EndpointAddressBase> destEndpointAddress =
            partId2MessagingEndpointDirectory->lookup(destinationPartId);
    if (destEndpointAddress == NULL) {
        LOG_DEBUG(logger, "No endpoint address found for participantId " + destinationPartId + ". Dropping the message");
        partId2MessagingEndpointDirectory->lookup(destinationPartId);
        return;
    }
    QSharedPointer<IMessaging> messagingStub = messagingStubFactory
                                                ->create(destinationPartId,
                                                         destEndpointAddress);
    if (messagingStub.isNull()) {
        LOG_DEBUG(logger, "No send-stub found for endpoint address. Dropping the message");
        return;
    }
    //make runnable (to execute send on the stub) and schedule it
    threadPool.start(new MessageRunnable(
                         message,
                         qos,
                         messagingStub,
                         *delayedScheduler,
                         DispatcherUtils::convertTtlToAbsoluteTime(qos.getTtl()) )
                     );
}

/****
  * IMPLEMENTATION OF THE MESSAGE RUNNABLE
  *
  */
Logger* MessageRunnable::logger = Logging::getInstance()->getLogger("MSG", "MessageRunnable");
int MessageRunnable::messageRunnableCounter = 0;

MessageRunnable::MessageRunnable(const JoynrMessage& message,
                                 const MessagingQos& qos,
                                 QSharedPointer<IMessaging> messagingStub,
                                 DelayedScheduler& delayedScheduler,
                                 const QDateTime& decayTime)
    : ObjectWithDecayTime(decayTime),
      message(message),
      qos(qos),
      messagingStub(messagingStub),
      delayedScheduler(delayedScheduler)
{
    messageRunnableCounter++;
}

MessageRunnable::~MessageRunnable(){
    messageRunnableCounter--;
}

void MessageRunnable::run() {
    if(!isExpired()) {
        messagingStub->transmit(message, qos);
    } else {
        LOG_DEBUG(logger, "Message expired. Dropping the message");
    }
}




} // namespace joynr
