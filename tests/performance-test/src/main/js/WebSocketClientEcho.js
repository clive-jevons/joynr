/*jslint node: true */

/*
 * #%L
 * %%
 * Copyright (C) 2017 BMW Car IT GmbH
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

var args = process.argv.slice(2);
var port =  args[0];
var times = args[1];
var receivedMessages=0
var remainingMessagesToSend=times
var startTimestamp

if (!port || !times) {
    console.log("usage: node WebSocketClientEcho.js port times");
    return;
}

var WebSocket = require("ws");
var ws = new WebSocket("ws://localhost:" + port);

// sample request message
var toSend =
        {
            "_typeName" : "joynr.JoynrMessage",
            "customHeaders" : {},
            "header" : {
                "expiryDate" : "1468570049406",
                "msgId" : "bbdf49f0-f585-44e6-b152-4812d5106547",
                "from" : "e4025dd3-ee2e-466e-b6ca-ba6cc8d20787",
                "to" : "4300528c-23ba-488e-8567-4bf344e084e9"
            },
            "payload" : "{\"_typeName\":\"joynr.Request\",\"methodName\":\"updateRoute\",\"paramDatatypes\":[\"joynr.types.Localisation.GpsPosition[]\"],\"params\":[[{\"_typeName\":\"joynr.types.Localisation.GpsPosition\",\"latitude\":11.65,\"longitude\":49.0065}]],\"requestReplyId\":\"947d9e50-572e-43e6-9aca-865505b20e24\"}",
            "type" : "request"
        };

var killServerMessage = "killServer";
var totalTime;

var send = function(data) {
    remainingMessagesToSend--;

    if (remainingMessagesToSend >= 0) {
        ws.send(data, function (error) {
            if (error === undefined) {
                send(data);
            }
        });
    }
};

ws.on('message', function(data, flags) {
    receivedMessages++;

    if(receivedMessages >= times) {
        var elapsedTimeMs = Date.now() - startTimestamp;

        console.log("Elapsed time : " + elapsedTimeMs + " ms");
        console.log("Throughput   : " + (times / (elapsedTimeMs / 1000.0)) + " Msgs/s");

        ws.send(killServerMessage, function () {
            ws.terminate();
        });
        return;
    }
});

ws.on('open', function open() {
    console.log("sending times:" + times);
    startTimestamp = Date.now();
    send(JSON.stringify(toSend));
});
