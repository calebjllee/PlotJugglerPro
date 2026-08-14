/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SHAREPOINT_CONFIG_H
#define SHAREPOINT_CONFIG_H

namespace PJ
{
namespace SharePointConfig
{
static constexpr const char* CLIENT_ID = "40fae8dc-3cd0-407d-b95d-669c0dade2cf";
static constexpr const char* TENANT_ID = "ee2d6d72-9535-4242-a077-acf185782f9b";
static constexpr const char* SITE_HOSTNAME = "umd0.sharepoint.com";
static constexpr const char* SITE_PATH = "/TeamsTerpsRacingEV";
static constexpr const char* ROOT_PATH = "/_Overall-EV27/Testing Logs";
static constexpr const char* GRAPH_SCOPE =
    "https://graph.microsoft.com/Sites.Selected offline_access";
}  // namespace SharePointConfig
}  // namespace PJ

#endif  // SHAREPOINT_CONFIG_H
