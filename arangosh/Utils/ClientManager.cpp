////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "ClientManager.h"

#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

namespace {
arangodb::Result getHttpErrorMessage(
    arangodb::httpclient::SimpleHttpResult* result) {
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::StringUtils::itoa;

  if (!result) {
    // no result to pull from
    return {TRI_ERROR_INTERNAL, "no response from server!"};
  }

  int code = TRI_ERROR_NO_ERROR;
  // set base message
  std::string message("got error from server: HTTP " +
                      itoa(result->getHttpReturnCode()) + " (" +
                      result->getHttpReturnMessage() + ")");

  // assume vpack body, catch otherwise
  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    int serverCode =
        VelocyPackHelper::getNumericValue<int>(body, "errorNum", 0);
    std::string const& serverMessage =
        VelocyPackHelper::getStringValue(body, "errorMessage", "");

    if (serverCode > 0) {
      code = serverCode;
      message.append(": ArangoError " + itoa(serverCode) + ": " +
                     serverMessage);
    }
  } catch (...) {
    // no need to recover, fallthrough for default error message
  }
  return {code, std::move(message)};
}
}  // namespace

namespace arangodb {

ClientManager::ClientManager(LogTopic& topic) : _topic{topic} {}

ClientManager::~ClientManager() {}

std::unique_ptr<httpclient::SimpleHttpClient> ClientManager::getConnectedClient(
    bool force, bool verbose) {
  std::unique_ptr<httpclient::SimpleHttpClient> httpClient(nullptr);

  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  TRI_ASSERT(client);

  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG_TOPIC(FATAL, _topic) << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  // set client parameters
  std::string dbName = client->databaseName();
  httpClient->params().setLocationRewriter(static_cast<void*>(client),
                                           &rewriteLocation);
  httpClient->params().setUserNamePassword("/", client->username(),
                                           client->password());

  // now connect by retrieving version
  std::string const versionString = httpClient->getServerVersion();
  if (!httpClient->isConnected()) {
    LOG_TOPIC(ERR, _topic) << "Could not connect to endpoint '"
                           << client->endpoint() << "', database: '" << dbName
                           << "', username: '" << client->username() << "'";
    LOG_TOPIC(FATAL, _topic)
        << "Error message: '" << httpClient->getErrorMessage() << "'";

    FATAL_ERROR_EXIT();
  }

  if (verbose) {
    // successfully connected
    LOG_TOPIC(INFO, _topic) << "Server version: " << versionString;
  }

  // validate server version
  std::pair<int, int> version =
      rest::Version::parseVersionString(versionString);
  if (version.first < 3) {
    // we can connect to 3.x
    LOG_TOPIC(ERR, _topic) << "Error: got incompatible server version '"
                           << versionString << "'";

    if (!force) {
      FATAL_ERROR_EXIT();
    }
  }

  return httpClient;
}

std::string ClientManager::rewriteLocation(void* data,
                                           std::string const& location) {
  // if it already starts with "/_db/", we are done
  if (location.compare(0, 5, "/_db/") == 0) {
    return location;
  }

  // assume data is `ClientFeature*`; must be enforced externally
  ClientFeature* client = static_cast<ClientFeature*>(data);
  std::string const& dbname = client->databaseName();

  // prefix with `/_db/${dbname}/`
  if (location[0] == '/') {
    // already have leading "/", leave it off
    return "/_db/" + dbname + location;
  }
  return "/_db/" + dbname + "/" + location;
}

std::pair<Result, bool> ClientManager::getArangoIsCluster(
    httpclient::SimpleHttpClient& client) {
  using arangodb::basics::VelocyPackHelper;

  Result result{TRI_ERROR_NO_ERROR};
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    result.reset(TRI_ERROR_INTERNAL, "no response from server!");
    return {result, false};
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == (int)rest::ResponseCode::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = VelocyPackHelper::getStringValue(body, "role", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      result = ::getHttpErrorMessage(response.get());
      LOG_TOPIC(ERR, _topic)
          << "got error while checking cluster mode: " << result.errorMessage();
      client.setErrorMessage(result.errorMessage(), false);
    } else {
      result.reset(TRI_ERROR_INTERNAL);
    }

    client.disconnect();
  }

  return {result, (role == "COORDINATOR")};
}

std::pair<Result, bool> ClientManager::getArangoIsUsingEngine(
    httpclient::SimpleHttpClient& client, std::string const& name) {
  using arangodb::basics::VelocyPackHelper;

  Result result{TRI_ERROR_NO_ERROR};
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, "/_api/engine", "", 0));
  if (response == nullptr || !response->isComplete()) {
    result.reset(TRI_ERROR_INTERNAL, "no response from server!");
    return {result, false};
  }

  std::string engine = "UNDEFINED";

  if (response->getHttpReturnCode() ==
      static_cast<int>(rest::ResponseCode::OK)) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      engine = VelocyPackHelper::getStringValue(body, "name", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      result = ::getHttpErrorMessage(response.get());
      LOG_TOPIC(ERR, _topic) << "got error while checking storage engine: "
                             << result.errorMessage();
      client.setErrorMessage(result.errorMessage(), false);
    } else {
      result.reset(TRI_ERROR_INTERNAL);
    }

    client.disconnect();
  }

  return {result, (engine == name)};
}

}  // namespace arangodb
