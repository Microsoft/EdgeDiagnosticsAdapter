//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once

#include "stdafx.h"

NAN_METHOD(initialize);
NAN_METHOD(getEdgeInstances);
NAN_METHOD(setSecurityACLs);
NAN_METHOD(openEdge);
NAN_METHOD(closeEdge);
NAN_METHOD(killAll);
NAN_METHOD(serveChromeDevTools);
NAN_METHOD(connectTo);
NAN_METHOD(injectScriptTo);
NAN_METHOD(forwardTo);
NAN_METHOD(createNetworkProxyFor);
NAN_METHOD(closeNetworkProxyInstance);
