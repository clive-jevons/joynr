package io.joynr.messaging.mqtt;

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

import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.AdditionalAnswers.returnsFirstArg;
import static org.junit.Assert.fail;

import io.joynr.messaging.FailureAction;
import io.joynr.messaging.NoOpRawMessagingPreprocessor;
import io.joynr.messaging.RawMessagingPreprocessor;
import io.joynr.messaging.routing.MessageRouter;
import io.joynr.messaging.serialize.JsonSerializer;
import joynr.JoynrMessage;
import joynr.system.RoutingTypes.MqttAddress;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.runners.MockitoJUnitRunner;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

@RunWith(MockitoJUnitRunner.class)
public class MqttMessagingSkeletonTest {

    private MqttMessagingSkeleton subject;

    @Mock
    private MqttAddress ownAddress;

    @Mock
    private MessageRouter messageRouter;

    @Mock
    private MqttClientFactory mqttClientFactory;

    @Mock
    private MqttMessageSerializerFactory messageSerializerFactory;

    @Mock
    private JoynrMqttClient mqttClient;

    @Before
    public void setup() {
        subject = new MqttMessagingSkeleton(ownAddress,
                                            messageRouter,
                                            mqttClientFactory,
                                            messageSerializerFactory,
                                            new NoOpRawMessagingPreprocessor());
        when(mqttClientFactory.create()).thenReturn(mqttClient);
        subject.init();
        verify(mqttClient).subscribe(anyString());
        reset(mqttClient);
    }

    @Test
    public void testOnlySubscribeToMulticastIfNotAlreadySubscribed() {
        String multicastId = "multicastId";

        subject.registerMulticastSubscription(multicastId);
        verify(mqttClient).subscribe(eq(multicastId));
        reset(mqttClient);

        subject.registerMulticastSubscription(multicastId);
        verify(mqttClient, never()).subscribe(anyString());
    }

    @Test
    public void testMultilevelWildcardTranslated() {
        String multicastId = "one/two/*";

        subject.registerMulticastSubscription(multicastId);
        verify(mqttClient).subscribe(eq("one/two/#"));

        subject.unregisterMulticastSubscription(multicastId);
        verify(mqttClient).unsubscribe("one/two/#");
    }

    @Test
    public void testRawMessagePreprocessorIsCalled() throws Exception {
        RawMessagingPreprocessor preprocessor = mock(RawMessagingPreprocessor.class);
        ObjectMapper objectMapper = new ObjectMapper();
        when(preprocessor.process(anyString())).then(returnsFirstArg());
        when(messageSerializerFactory.create(Mockito.any(MqttAddress.class))).thenReturn(new JsonSerializer(objectMapper));
        subject = new MqttMessagingSkeleton(ownAddress,
                                            messageRouter,
                                            mqttClientFactory,
                                            messageSerializerFactory,
                                            preprocessor);
        JoynrMessage message = new JoynrMessage();
        message.setPayload("payload");
        subject.transmit(objectMapper.writeValueAsString(message), new FailureAction() {
            @Override
            public void execute(Throwable error) {
                fail("failure action was erroneously called");
            }
        });
        verify(messageRouter).route(message);
    }

}
