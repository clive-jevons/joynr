/*jslint node: true */

/*
 * #%L
 * %%
 * Copyright (C) 2011 - 2015 BMW Car IT GmbH
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

var log = require("./logging.js").log;
var prettyLog = require("./logging.js").prettyLog;

var runDemo = function(radioProxy, onDone) {
    radioProxy.isOn.get().fail(function(error) {
        prettyLog("radioProxy.isOn.get.fail: " + error);
    }).then(function(value) {
        prettyLog("Is the radio on?: radioProxy.isOn.get.done: " + value);
        return radioProxy.isOn.set({
            value : true
        });
    }).fail(function(error) {
        prettyLog("radioProxy.isOn.set(" + true + ").fail: " + error);
    }).then(function() {
        prettyLog("Switch the radio on: radioProxy.isOn.set(" + true + ").done");
        return radioProxy.isOn.get();
    }).fail(function(error) {
        prettyLog("radioProxy.isOn.get.fail: " + error);
    }).then(function(value) {
        prettyLog("The radio should be on now: radioProxy.isOn.get.then: " + value);
        return radioProxy.numberOfStations.get();
    }).fail(function(error) {
        prettyLog("radioProxy.numberOfStations.get.fail: " + error);
    }).then(function(value) {
        prettyLog("radioProxy.numberOfStations.get.done: " + value);
        return radioProxy.addFavoriteStation({
            radioStation : "runDemoFavoriteStation"
        });
    }).fail(function(error) {
        prettyLog("radioProxy.addFavoriteStation(" + JSON.stringify({
            radioStation : "runDemoFavoriteStation"
        }) + ").fail: " + error);
    }).then(function(value) {
        prettyLog("radioProxy.addFavoriteStation(" + JSON.stringify({
            radioStation : "runDemoFavoriteStation"
        }) + ").done. Return value of operation from provider: " + JSON.stringify(value));
        return radioProxy.numberOfStations.get();
    }).fail(function(error) {
        prettyLog("radioProxy.numberOfStations.get.fail: " + error);
    }).then(function(value) {
        prettyLog("radioProxy.numberOfStations.get.done: " + value);
        if (onDone) {
            onDone();
        }
    });
};

var subscription = require("./subscription.js");
var isOnSubscription = {};
isOnSubscription.subscriptions = {};
isOnSubscription.setJoynr = function(joynr) {
    this.joynr = joynr;
};
isOnSubscription.subscribe =
        function(radioProxy, onDone) {
            if (!this.joynr) {
                log("you first have to initialize me with setJoynr! Aborting.");
            } else {
                var subscriptionQosOnChange = new this.joynr.proxy.OnChangeSubscriptionQos({
                    minInterval : 50
                });

                var onPublicationIsOn = function(value) {
                    prettyLog("Received update of isOn: " + value);
                };

                subscription.subscribeAttribute(
                        radioProxy,
                        "isOn",
                        "onChange",
                        subscriptionQosOnChange,
                        onPublicationIsOn,
                        this.subscriptions,
                        onDone);
            }
        };
isOnSubscription.unsubscribe = function(radioProxy, onDone) {
    subscription.unsubscribeAttributeSubscriptions(radioProxy, "isOn", this.subscriptions, onDone);
};

var runInteractiveConsole =
        function(radioProxy, onDone) {
            var readline = require('readline');
            var rl = readline.createInterface(process.stdin, process.stdout);
            rl.setPrompt('>> ');
            var MODES = {
                HELP : {
                    value : "h",
                    description : "help",
                    options : {}
                },
                QUIT : {
                    value : "q",
                    description : "quit",
                    options : {}
                },
                SET_IS_ON : {
                    value : "setIsOn",
                    description : "set value for isOn",
                    options : {
                        TRUE : "true",
                        FALSE : "false"
                    }
                },
                GET_IS_ON : {
                    value : "getIsOn",
                    description : "get value for isOn",
                    options : {}
                },
                ADD_FAVORITE_STATION : {
                    value : "addFavStation",
                    description : "add a Favorite Station",
                    options : {
                        NAME : "name"
                    }
                },
                GET_NUM_STATIONS : {
                    value : "getNumStations",
                    description : "get the number of favorite stations",
                    options : {}
                },
                SUBSCRIBE : {
                    value : "subscribe",
                    description : "subscribe to isOn attribute",
                    options : {}
                },
                UNSUBSCRIBE : {
                    value : "unsubscribe",
                    description : "unsubscribe from isOn attribute",
                    options : {}
                }
            };

            var showHelp = require("./console_common.js");
            rl.on('line', function(line) {
                var input = line.trim().split(' ');
                switch (input[0]) {
                    case MODES.HELP.value:
                        showHelp(MODES);
                        break;
                    case MODES.QUIT.value:
                        rl.close();
                        break;
                    case MODES.SET_IS_ON.value:
                        var value;
                        if (!input[1]) {
                            log("please define an option");
                        } else if (input[1] === MODES.SET_IS_ON.options.TRUE) {
                            value = true;
                        } else if (input[1] === MODES.SET_IS_ON.options.FALSE) {
                            value = false;
                        } else {
                            log('invalid option: ' + input[1]);
                        }
                        if (value !== undefined) {
                            radioProxy.isOn.set({
                                value : value
                            }).done(function() {
                                log("radioProxy.isOn.set(" + value + ").done");
                            }).fail(function(error) {
                                log("radioProxy.isOn.set(" + value + ").fail: " + error);
                            });
                        }
                        break;
                    case MODES.GET_IS_ON.value:
                        radioProxy.isOn.get().done(function(value) {
                            prettyLog("radioProxy.isOn.get.done: " + value);
                        }).fail(function(error) {
                            prettyLog("radioProxy.isOn.get.fail: " + error);
                        });
                        break;
                    case MODES.ADD_FAVORITE_STATION.value:
                        if (!input[1]) {
                            log("please define a name");
                        } else {
                            var operationArguments = {
                                radioStation : input[1]
                            };
                            radioProxy.addFavoriteStation(operationArguments).done(
                                    function(returnValue) {
                                        log("radioProxy.addFavoriteStation("
                                            + JSON.stringify(operationArguments)
                                            + ").done. Return value of operation from provider: "
                                            + JSON.stringify(returnValue));
                                    }).fail(
                                    function(error) {
                                        log("radioProxy.addFavoriteStation("
                                            + JSON.stringify(operationArguments)
                                            + ").fail: "
                                            + error);
                                    });
                        }
                        break;
                    case MODES.GET_NUM_STATIONS.value:
                        radioProxy.numberOfStations.get().done(function(value) {
                            prettyLog("radioProxy.numberOfStations.get.done: " + value);
                        }).fail(function(error) {
                            prettyLog("radioProxy.numberOfStations.get.fail: " + error);
                        });
                        break;
                    case MODES.SUBSCRIBE.value:
                        isOnSubscription.subscribe(radioProxy);
                        break;
                    case MODES.UNSUBSCRIBE.value:
                        isOnSubscription.unsubscribe(radioProxy);
                        break;
                    case '':
                        break;
                    default:
                        log('unknown input: ' + input);
                        break;
                }
                rl.prompt();
            });

            rl.on('close', function() {
                if (onDone) {
                    onDone();
                }
            });

            showHelp(MODES);
            rl.prompt();
        };

if (process.argv.length !== 3) {
    log("please pass a domain as argument");
    process.exit(0);
}
var domain = process.argv[2];
log("domain: " + domain);

var joynr = require("joynr");
var provisioning = require("./provisioning_common.js");
joynr.load(provisioning, function(error, loadedJoynr) {
    if (error) {
        throw error;
    }
    log("joynr started");
    joynr = loadedJoynr;
    var messagingQos = new joynr.messaging.MessagingQos({
        ttl : 60000
    });
    var RadioProxy = require("../generated/js/joynr/vehicle/RadioProxy.js");
    joynr.proxyBuilder.build(RadioProxy, {
        domain : domain,
        messagingQos : messagingQos
    }).done(function(radioProxy) {
        log("radio proxy build");
        runDemo(radioProxy, function() {
            isOnSubscription.setJoynr(joynr); // TODO this should not be necessary => setting the
                                                // values for OnChangeSubscriptionQos should be
                                                // possible without joynr
            isOnSubscription.subscribe(radioProxy, function() {
                runInteractiveConsole(radioProxy, function() {
                    log("stopping all isOn subscriptions");
                    isOnSubscription.unsubscribe(radioProxy, function() {
                        process.exit(0);
                    });
                });
            });
        });
    }).fail(function(error) {
        log("error building radioProxy: " + error);
    });
});