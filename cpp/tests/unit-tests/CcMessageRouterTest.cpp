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

#include <cstdint>
#include <chrono>
#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "MessageRouterTest.h"

#include "joynr/Semaphore.h"
#include "tests/utils/MockObjects.h"
#include "joynr/InProcessMessagingAddress.h"
#include "joynr/system/RoutingTypes/ChannelAddress.h"
#include "joynr/system/RoutingTypes/MqttAddress.h"
#include "joynr/system/RoutingTypes/WebSocketAddress.h"
#include "joynr/system/RoutingTypes/WebSocketClientAddress.h"
#include "joynr/MessagingStubFactory.h"
#include "joynr/MessageQueue.h"
#include "joynr/MqttMulticastAddressCalculator.h"
#include "joynr/MulticastMessagingSkeletonDirectory.h"
#include "joynr/WebSocketMulticastAddressCalculator.h"
#include "libjoynr/in-process/InProcessMessagingStubFactory.h"
#include "joynr/SingleThreadedIOService.h"
#include "joynr/IPlatformSecurityManager.h"

using ::testing::Pointee;
using ::testing::Return;
using ::testing::WhenDynamicCastTo;

using namespace joynr;

class CcMessageRouterTest : public MessageRouterTest<CcMessageRouter> {
public:
    CcMessageRouterTest() :
        DEFAULT_IS_GLOBALLY_VISIBLE(true) {
    }
protected:
    void multicastMsgIsSentToAllMulticastRecivers(const bool isGloballyVisible);
    const bool DEFAULT_IS_GLOBALLY_VISIBLE;
};

TEST_F(CcMessageRouterTest, removeMulticastReceiver_failsIfProviderAddressNotAvailable) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    auto skeleton = std::make_shared<MockMessagingMulticastSubscriber>();
    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::WebSocketClientAddress>(
                skeleton);

    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });

    messageRouter->removeNextHop(providerParticipantId);

    Semaphore errorCallbackCalled;
    messageRouter->removeMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { FAIL() << "onSuccess called"; },
        [&errorCallbackCalled](const joynr::exceptions::ProviderRuntimeException&){ errorCallbackCalled.notify(); });
    EXPECT_TRUE(errorCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

void CcMessageRouterTest::multicastMsgIsSentToAllMulticastRecivers(const bool isProviderGloballyVisible)
{
    const std::string subscriberParticipantId1("subscriberPartId1");
    const std::string subscriberParticipantId2("subscriberPartId2");
    const std::string subscriberParticipantId3("subscriberPartId3");
    const std::string providerParticipantId("providerParticipantId");
    const std::string multicastNameAndPartitions("multicastName/partition0");
    const std::string multicastId(providerParticipantId + "/" + multicastNameAndPartitions);
    const std::string consumerRuntimeWebSocketClientAddressString("ConsumerRuntimeWebSocketAddress");

    // create a new pointer representin the multicast address
    std::shared_ptr<joynr::system::RoutingTypes::MqttAddress> multicastAddress(
                new joynr::system::RoutingTypes::MqttAddress(*globalTransport)
    );
    multicastAddress->setTopic(multicastId);

    auto expectedAddress1 = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>(consumerRuntimeWebSocketClientAddressString);
    auto expectedAddress2 = std::make_shared<const joynr::InProcessMessagingAddress>();
    auto expectedAddress3 = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>(consumerRuntimeWebSocketClientAddressString);
    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>("ProviderRuntimeWebSocketAddress");

    messageRouter->addProvisionedNextHop(subscriberParticipantId1, expectedAddress1, DEFAULT_IS_GLOBALLY_VISIBLE);
    messageRouter->addProvisionedNextHop(subscriberParticipantId2, expectedAddress2, DEFAULT_IS_GLOBALLY_VISIBLE);
    messageRouter->addProvisionedNextHop(subscriberParticipantId3, expectedAddress3, DEFAULT_IS_GLOBALLY_VISIBLE);
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, isProviderGloballyVisible);

    auto skeleton = std::make_shared<MockMessagingMulticastSubscriber>();
    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::WebSocketClientAddress>(
                skeleton);

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId1,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId2,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId3,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    mutableMessage.setType(joynr::Message::VALUE_MESSAGE_TYPE_MULTICAST());
    mutableMessage.setSender(providerParticipantId);
    mutableMessage.setRecipient(multicastId);

    // verify that the publication is sent only once to the websocket address used by
    // both subscriberParticipantId1 and subscriberParticipantId3 identified by
    // consumerRuntimeWebSocketClientAddressString
    EXPECT_CALL(
        *messagingStubFactory,
        create(
            Property(
                &std::shared_ptr<const joynr::system::RoutingTypes::Address>::get,
                WhenDynamicCastTo<const joynr::system::RoutingTypes::WebSocketClientAddress *>(
                    Pointee(
                        Property(
                            &joynr::system::RoutingTypes::WebSocketClientAddress::getId,
                            Eq(consumerRuntimeWebSocketClientAddressString)
                        )
                    )
                )
            )
        )
    ).Times(1);
    EXPECT_CALL(*messagingStubFactory, create(Pointee(Eq(*expectedAddress2)))).Times(1);
    size_t count = isProviderGloballyVisible ? 1 : 0;
    EXPECT_CALL(*messagingStubFactory, create(Pointee(Eq(*multicastAddress)))).Times(count);

    messageRouter->route(mutableMessage.getImmutableMessage());
}

TEST_F(CcMessageRouterTest, routeMulticastMessageFromWebSocketProvider_multicastMsgIsSentToAllMulticastRecivers) {
    bool isGloballyVisible = true;
    multicastMsgIsSentToAllMulticastRecivers(isGloballyVisible);
    isGloballyVisible = false;
    multicastMsgIsSentToAllMulticastRecivers(isGloballyVisible);
}

TEST_F(CcMessageRouterTest, removeMulticastReceiver_NonChildRouter_succeedsIfSkeletonNotAvailable) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::MqttAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    auto skeleton = std::make_shared<MockMessagingMulticastSubscriber>();
    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::MqttAddress>(
                skeleton);

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    multicastMessagingSkeletonDirectory->unregisterSkeleton<system::RoutingTypes::MqttAddress>();

    Semaphore successCallbackCalled;
    messageRouter->removeMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, removeMulticastReceiverOfStandaloneProvider_NonChildRouter_callsSkeleton) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::MqttAddress>();

    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    auto skeleton = std::make_shared<MockMessagingMulticastSubscriber>();

    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::MqttAddress>(
                skeleton);

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    // The message router shall unregister the subscription at the multicast skeleton
    EXPECT_CALL(*skeleton, unregisterMulticastSubscription(multicastId)).Times(1);

    Semaphore successCallbackCalled;
    messageRouter->removeMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, removeMulticastReceiverOfWebSocketProvider_NonChildRouter_succeeds) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    messageRouter->addMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );

    Semaphore successCallbackCalled;
    messageRouter->removeMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, removeMulticastReceiverOfInProcessProvider_NonChildRouter_succeeds) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto skeleton = std::make_shared<MockInProcessMessagingSkeleton>();

    auto providerAddress = std::make_shared<const joynr::InProcessMessagingAddress>(skeleton);
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });

    Semaphore successCallbackCalled;
    messageRouter->removeMulticastReceiver(
        multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; }
    );
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiverForMqttProvider_NonChildRouter_callsSkeleton) {
    const std::string subscriberParticipantId("subscriberPartId1");
    const std::string providerParticipantId("providerParticipantId");
    const std::string multicastId("participantId1/methodName/partition0");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::MqttAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    auto mockMqttMessagingMulticastSubscriber = std::make_shared<MockMessagingMulticastSubscriber>();
    std::shared_ptr<IMessagingMulticastSubscriber> mqttMessagingMulticastSubscriber = mockMqttMessagingMulticastSubscriber;

    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::MqttAddress>(
                mqttMessagingMulticastSubscriber);

    EXPECT_CALL(*mockMqttMessagingMulticastSubscriber, registerMulticastSubscription(multicastId));

    Semaphore successCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiverForHttpProvider_NonChildRouter_callsSkeleton) {
    const std::string subscriberParticipantId("subscriberPartId1");
    const std::string providerParticipantId("providerParticipantId");
    const std::string multicastId("participantId1/methodName/partition0");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::ChannelAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    auto skeleton = std::make_shared<MockMessagingMulticastSubscriber>();

    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::ChannelAddress>(
                skeleton);

    EXPECT_CALL(*skeleton, registerMulticastSubscription(multicastId));

    Semaphore successCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiverForWebSocketProvider_NonChildRouter_succeeds) {
    const std::string subscriberParticipantId("subscriberPartId1");
    const std::string providerParticipantId("providerParticipantId");
    const std::string multicastId("participantId1/methodName/partition0");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::WebSocketClientAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    Semaphore successCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiverForInProcessProvider_NonChildRouter_succeeds) {
    const std::string subscriberParticipantId("subscriberPartId1");
    const std::string providerParticipantId("providerParticipantId");
    const std::string multicastId("participantId1/methodName/partition0");

    auto skeleton = std::make_shared<MockInProcessMessagingSkeleton>();

    auto providerAddress = std::make_shared<const joynr::InProcessMessagingAddress>(skeleton);
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    Semaphore successCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException&){ FAIL() << "onError called"; });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiver_NonChildRouter_failsIfProviderAddressNotAvailable) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    Semaphore errorCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        []() { FAIL() << "onSuccess called"; },
        [&errorCallbackCalled](const joynr::exceptions::ProviderRuntimeException&)
        {
            errorCallbackCalled.notify();
        });
    EXPECT_TRUE(errorCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, addMulticastReceiver_NonChildRouter_succeedsIfSkeletonNotAvailable) {
    const std::string multicastId("multicastId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string providerParticipantId("providerParticipantId");

    auto providerAddress = std::make_shared<const joynr::system::RoutingTypes::MqttAddress>();
    messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);

    Semaphore successCallbackCalled;
    messageRouter->addMulticastReceiver(multicastId,
        subscriberParticipantId,
        providerParticipantId,
        [&successCallbackCalled]() { successCallbackCalled.notify(); },
        [](const joynr::exceptions::ProviderRuntimeException& exception)
        {
            FAIL() << "onError called: " << exception.what();
        });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}

TEST_F(CcMessageRouterTest, loadNonExistingMulticastDirectoryPersistenceFile) {
    EXPECT_NO_THROW(messageRouter->loadMulticastReceiverDirectory("not-existing.persist"));
}

TEST_F(CcMessageRouterTest, persistMulticastReceiverDirectory) {
    const std::string providerParticipantId("providerParticipantId");
    const std::string subscriberParticipantId("subscriberParticipantId");
    const std::string multicastId = util::createMulticastId(providerParticipantId, "multicastName", {});

    auto providerAddress = std::make_shared<system::RoutingTypes::MqttAddress>();
    auto multicastSubscriber = std::make_shared<MockMessagingMulticastSubscriber>();

    const std::string persistencyFilename = "multicast-receiver-directory-test.persist";
    const bool isGloballyVisible = true;
    std::remove(persistencyFilename.c_str());
    // Load method stores the filename which will later be used to save the multicast receiver directory.
    messageRouter->loadMulticastReceiverDirectory(persistencyFilename);

    multicastMessagingSkeletonDirectory->registerSkeleton<system::RoutingTypes::MqttAddress>(multicastSubscriber);
    messageRouter->addNextHop(providerParticipantId, providerAddress, isGloballyVisible);
    messageRouter->addMulticastReceiver(multicastId, subscriberParticipantId, providerParticipantId, []() {}, nullptr);

    messageRouter = createMessageRouter();
    messageRouter->addNextHop(providerParticipantId, providerAddress, isGloballyVisible);

    EXPECT_CALL(*multicastSubscriber, registerMulticastSubscription(multicastId)).Times(1);
    messageRouter->loadMulticastReceiverDirectory(persistencyFilename);
}

TEST_F(CcMessageRouterTest, doNotSaveInProcessMessagingAddressToFile) {
    const std::string providerParticipantId("providerParticipantId");
    const std::string routingTablePersistenceFilename = "test-RoutingTable.persist";
    std::remove(routingTablePersistenceFilename.c_str());

    messageRouter->loadRoutingTable(routingTablePersistenceFilename);
    {
        auto skeleton = std::make_shared<MockInProcessMessagingSkeleton>();
        auto providerAddress = std::make_shared<const joynr::InProcessMessagingAddress>(skeleton);
        messageRouter->addProvisionedNextHop(providerParticipantId, providerAddress, DEFAULT_IS_GLOBALLY_VISIBLE);
    }

    messageRouter = createMessageRouter();
    messageRouter->loadRoutingTable(routingTablePersistenceFilename);

    Semaphore successCallbackCalled;
    messageRouter->resolveNextHop(providerParticipantId,
        [&successCallbackCalled](const bool& resolved) {
        if(resolved) {
            FAIL() << "resolve should not succeed.";}
        else {
            successCallbackCalled.notify();
        }},
        [&successCallbackCalled](const joynr::exceptions::ProviderRuntimeException&){ successCallbackCalled.notify(); });
    EXPECT_TRUE(successCallbackCalled.waitFor(std::chrono::milliseconds(5000)));
}
