// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "azure/identity/managed_identity_credential.hpp"
#include "credential_test_helper.hpp"

#include <azure/core/diagnostics/logger.hpp>
#include <azure/core/internal/environment.hpp>

#include <fstream>

#include <gtest/gtest.h>

#if defined(AZ_PLATFORM_LINUX)
#include <unistd.h>

#include <sys/stat.h> // for mkdir
#include <sys/types.h>
#elif defined(AZ_PLATFORM_WINDOWS)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

using Azure::Core::ResourceIdentifier;
using Azure::Core::Uuid;
using Azure::Core::Credentials::TokenCredentialOptions;
using Azure::Core::Http::HttpMethod;
using Azure::Core::Http::HttpStatusCode;
using Azure::Identity::ManagedIdentityCredential;
using Azure::Identity::ManagedIdentityCredentialOptions;
using Azure::Identity::ManagedIdentityId;
using Azure::Identity::_detail::ManagedIdentityIdKind;
using Azure::Identity::Test::_detail::CredentialTestHelper;

namespace Azure { namespace Identity { namespace Test {

  TEST(ManagedIdentityId, Basic)
  {
    {
      ManagedIdentityId const miType;
      EXPECT_EQ(miType.GetId(), "");
      EXPECT_EQ(miType.GetManagedIdentityIdKind(), ManagedIdentityIdKind::SystemAssigned);
    }
    {
      ManagedIdentityId const miType(ManagedIdentityIdKind::SystemAssigned, "");
      EXPECT_EQ(miType.GetId(), "");
      EXPECT_EQ(miType.GetManagedIdentityIdKind(), ManagedIdentityIdKind::SystemAssigned);

      ManagedIdentityId const miTypeFactory = ManagedIdentityId::SystemAssigned();
      EXPECT_EQ(miTypeFactory.GetId(), "");
      EXPECT_EQ(miTypeFactory.GetManagedIdentityIdKind(), ManagedIdentityIdKind::SystemAssigned);
    }
    {
      ManagedIdentityId const miType(ManagedIdentityIdKind::ClientId, "clientId");
      EXPECT_EQ(miType.GetId(), "clientId");
      EXPECT_EQ(miType.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ClientId);

      ManagedIdentityId const miTypeFactory
          = ManagedIdentityId::FromUserAssignedClientId("clientId");
      EXPECT_EQ(miTypeFactory.GetId(), "clientId");
      EXPECT_EQ(miTypeFactory.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ClientId);
    }
    {
      ManagedIdentityId const miType(ManagedIdentityIdKind::ObjectId, "objectId");
      EXPECT_EQ(miType.GetId(), "objectId");
      EXPECT_EQ(miType.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ObjectId);

      ManagedIdentityId const miTypeFactory
          = ManagedIdentityId::FromUserAssignedObjectId("objectId");
      EXPECT_EQ(miTypeFactory.GetId(), "objectId");
      EXPECT_EQ(miTypeFactory.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ObjectId);
    }
    {
      ManagedIdentityId const miType(ManagedIdentityIdKind::ResourceId, "resourceId");
      EXPECT_EQ(miType.GetId(), "resourceId");
      EXPECT_EQ(miType.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ResourceId);

      ManagedIdentityId const miTypeFactory = ManagedIdentityId::FromUserAssignedResourceId(
          ResourceIdentifier("/subscriptions/resourceId"));
      EXPECT_EQ(miTypeFactory.GetId(), "/subscriptions/resourceId");
      EXPECT_EQ(miTypeFactory.GetManagedIdentityIdKind(), ManagedIdentityIdKind::ResourceId);
    }
    {
      ManagedIdentityCredentialOptions options;
      EXPECT_EQ(options.IdentityId.GetId(), "");
      EXPECT_EQ(
          options.IdentityId.GetManagedIdentityIdKind(), ManagedIdentityIdKind::SystemAssigned);
    }
  }

  TEST(ManagedIdentityId, Invalid)
  {
    EXPECT_THROW(
        ManagedIdentityId(ManagedIdentityIdKind::SystemAssigned, "clientId"),
        std::invalid_argument);

    EXPECT_THROW(ManagedIdentityId(ManagedIdentityIdKind::ClientId, ""), std::invalid_argument);
    EXPECT_THROW(ManagedIdentityId(ManagedIdentityIdKind::ObjectId, ""), std::invalid_argument);
    EXPECT_THROW(ManagedIdentityId(ManagedIdentityIdKind::ResourceId, ""), std::invalid_argument);

    EXPECT_THROW(ManagedIdentityId::FromUserAssignedClientId(""), std::invalid_argument);
    EXPECT_THROW(ManagedIdentityId::FromUserAssignedObjectId(""), std::invalid_argument);
    EXPECT_THROW(
        ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier("")),
        std::invalid_argument);

    ManagedIdentityCredentialOptions options;
    options.IdentityId = ManagedIdentityId(static_cast<ManagedIdentityIdKind>(99), "");
    std::unique_ptr<ManagedIdentityCredential const> managedIdentityCredentialWithInvalidOptions;
    EXPECT_THROW(
        managedIdentityCredentialWithInvalidOptions
        = std::make_unique<ManagedIdentityCredential>(options),
        std::invalid_argument);
  }

  TEST(ManagedIdentityCredential, GetCredentialName)
  {
    CredentialTestHelper::EnvironmentOverride const env({
        {"MSI_ENDPOINT", ""},
        {"MSI_SECRET", ""},
        {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
        {"IMDS_ENDPOINT", ""},
        {"IDENTITY_HEADER", "CLIENTSECRET"},
        {"IDENTITY_SERVER_THUMBPRINT", ""},
    });

    ManagedIdentityCredential const cred;
    EXPECT_EQ(cred.GetCredentialName(), "ManagedIdentityCredential");
  }

  TEST(ManagedIdentityCredential, AppServiceV2019)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(1));
          EXPECT_EQ(log[0].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential will be created with App Service 2019 source.");
          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01");

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("X-IDENTITY-HEADER"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request1.Headers.find("X-IDENTITY-HEADER"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request2.Headers.find("X-IDENTITY-HEADER"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AppServiceV2019ClientId)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(
              "fedcba98-7654-3210-0123-456789abcdef", options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(1));
          EXPECT_EQ(log[0].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential will be created with App Service 2019 source"
              " and Client ID 'fedcba98-7654-3210-0123-456789abcdef'.");

          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&client_id=fedcba98-7654-3210-0123-456789abcdef"
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&client_id=fedcba98-7654-3210-0123-456789abcdef"
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&client_id=fedcba98-7654-3210-0123-456789abcdef");

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("X-IDENTITY-HEADER"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request1.Headers.find("X-IDENTITY-HEADER"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request2.Headers.find("X-IDENTITY-HEADER"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AppServiceV2019ResourceId)
  {
    std::string const resourceId
        = "/subscriptions/abcdef01-2345-6789-9876-543210fedcba/locations/MyLocation";

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier(resourceId));

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-08-01&mi_res_id=" + resourceId
            + "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-08-01&mi_res_id=" + resourceId
            + "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-08-01&mi_res_id=" + resourceId);

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("X-IDENTITY-HEADER"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request1.Headers.find("X-IDENTITY-HEADER"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request2.Headers.find("X-IDENTITY-HEADER"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, AppServiceV2019ObjectId)
  {
    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedObjectId("abcdef01-2345-6789-0876-543210fedcba");

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://visualstudio.com"
        "?api-version=2019-08-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba"); // cspell:disable-line

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("X-IDENTITY-HEADER"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request1.Headers.find("X-IDENTITY-HEADER"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");

      EXPECT_NE(request2.Headers.find("X-IDENTITY-HEADER"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("X-IDENTITY-HEADER"), "CLIENTSECRET2");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, AppServiceV2019InvalidUrl)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    using Azure::Core::Credentials::AuthenticationException;
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com:INVALID/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> appServiceV2019ManagedIdentityCredential;
          EXPECT_THROW(
              appServiceV2019ManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(1));
          EXPECT_EQ(log[0].first, Logger::Level::Warning);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential with App Service 2019 source: "
              "Failed to create: The environment variable \'IDENTITY_ENDPOINT\' "
              "contains an invalid URL.");
          log.clear();

          return appServiceV2019ManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AppServiceV2019UnsupportedUrl)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    using Azure::Core::Credentials::AuthenticationException;
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com:65536/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> appServiceV2019ManagedIdentityCredential;
          EXPECT_THROW(
              appServiceV2019ManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return appServiceV2019ManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, AppServiceV2017)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(2));

          EXPECT_EQ(log[0].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2019 source.");

          EXPECT_EQ(log[1].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[1].second,
              "Identity: ManagedIdentityCredential will be created with App Service 2017 source.");

          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01");

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("secret"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request1.Headers.find("secret"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request2.Headers.find("secret"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("secret"), "CLIENTSECRET1");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AppServiceV2017ClientId)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(
              "fedcba98-7654-3210-0123-456789abcdef", options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(2));

          EXPECT_EQ(log[0].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2019 source.");

          EXPECT_EQ(log[1].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[1].second,
              "Identity: ManagedIdentityCredential will be created with App Service 2017 source"
              " and Client ID 'fedcba98-7654-3210-0123-456789abcdef'.");

          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&clientid=fedcba98-7654-3210-0123-456789abcdef"
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&clientid=fedcba98-7654-3210-0123-456789abcdef"
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&clientid=fedcba98-7654-3210-0123-456789abcdef");

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("secret"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request1.Headers.find("secret"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request2.Headers.find("secret"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("secret"), "CLIENTSECRET1");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AppServiceV2017ResourceId)
  {
    std::string const resourceId
        = "/subscriptions/abcdef01-2345-6789-9876-543210fedcba/locations/MyLocation";

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier(resourceId));

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://microsoft.com?api-version=2017-09-01&mi_res_id=" + resourceId
            + "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://microsoft.com?api-version=2017-09-01&mi_res_id=" + resourceId
            + "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://microsoft.com?api-version=2017-09-01&mi_res_id=" + resourceId);

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("secret"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request1.Headers.find("secret"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request2.Headers.find("secret"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("secret"), "CLIENTSECRET1");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, AppServiceV2017ObjectId)
  {
    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedObjectId("abcdef01-2345-6789-0876-543210fedcba");

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://microsoft.com"
        "?api-version=2017-09-01"
        "&principal_id=abcdef01-2345-6789-0876-543210fedcba"); // cspell:disable-line

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("secret"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request1.Headers.find("secret"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("secret"), "CLIENTSECRET1");

      EXPECT_NE(request2.Headers.find("secret"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("secret"), "CLIENTSECRET1");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, AppServiceV2017InvalidUrl)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    using Azure::Core::Credentials::AuthenticationException;
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com:INVALID/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> appServiceV2017ManagedIdentityCredential;
          EXPECT_THROW(
              appServiceV2017ManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return appServiceV2017ManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, AppServiceV2017UnsupportedUrl)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    using Azure::Core::Credentials::AuthenticationException;
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com:65536/"},
              {"MSI_SECRET", "CLIENTSECRET1"},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "CLIENTSECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> appServiceV2017ManagedIdentityCredential;
          EXPECT_THROW(
              appServiceV2017ManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return appServiceV2017ManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, CloudShell)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", "SECRET2"},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(3));

          EXPECT_EQ(log[0].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2019 source.");

          EXPECT_EQ(log[1].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[1].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2017 source.");

          EXPECT_EQ(log[2].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[2].second,
              "Identity: ManagedIdentityCredential will be created with Cloud Shell source.");

          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Post);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Post);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Post);

    EXPECT_EQ(request0.AbsoluteUrl, "https://microsoft.com");
    EXPECT_EQ(request1.AbsoluteUrl, "https://microsoft.com");
    EXPECT_EQ(request2.AbsoluteUrl, "https://microsoft.com");

    EXPECT_EQ(request0.Body, "resource=https%3A%2F%2Fazure.com"); // cspell:disable-line
    EXPECT_EQ(request1.Body, "resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line
    EXPECT_EQ(request2.Body, std::string());

    {
      EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("Metadata"), "true");

      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, CloudShellClientId)
  {
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> cloudShellManagedIdentityCredential;
          EXPECT_THROW(
              cloudShellManagedIdentityCredential = std::make_unique<ManagedIdentityCredential>(
                  "fedcba98-7654-3210-0123-456789abcdef", options),
              AuthenticationException);

          return cloudShellManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, CloudShellResourceId)
  {
    using Azure::Core::Credentials::AuthenticationException;

    std::string const resourceId
        = "/subscriptions/abcdef01-2345-6789-9876-543210fedcba/locations/MyLocation";

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier(resourceId));

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> cloudShellManagedIdentityCredential;
          EXPECT_THROW(
              cloudShellManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return cloudShellManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, CloudShellObjectId)
  {
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedObjectId("abcdef01-2345-6789-0876-543210fedcba");

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> cloudShellManagedIdentityCredential;
          EXPECT_THROW(
              cloudShellManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return cloudShellManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, CloudShellScope)
  {
    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Post);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Post);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Post);

    EXPECT_EQ(request0.AbsoluteUrl, "https://microsoft.com");
    EXPECT_EQ(request1.AbsoluteUrl, "https://microsoft.com");
    EXPECT_EQ(request2.AbsoluteUrl, "https://microsoft.com");

    EXPECT_EQ(request0.Body,
              "resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(request1.Body,
              "resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(request2.Body, "");

    {
      EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("Metadata"), "true");

      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, CloudShellInvalidUrl)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", "https://microsoft.com:INVALID/"},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> cloudShellManagedIdentityCredential;
          EXPECT_THROW(
              cloudShellManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return cloudShellManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  // This helper creates the necessary directories required for the Azure Arc tests, and returns
  // where we expect the valid arc key to exist.
  std::string CreateDirectoryAndGetKeyPath()
  {
    std::string keyPath;
#if defined(AZ_PLATFORM_LINUX)
    keyPath = "/var/opt/azcmagent/tokens";
    int result = system(std::string("sudo mkdir -p ").append(keyPath).c_str());
    if (result != 0 && errno != EEXIST)
    {
      GTEST_LOG_(ERROR) << "Directory creation failure in an AzureArc test: " << keyPath
                        << " Result: " << result << " Error : " << errno;
      EXPECT_TRUE(false);
    }
    // Add write permission for owner, group, and others
    result = system(std::string("sudo chmod -R 0777 ").append(keyPath).c_str());
    if (result != 0)
    {
      GTEST_LOG_(ERROR) << "Failed to change permissions for " << keyPath << ": " << strerror(errno)
                        << std::endl;
      EXPECT_TRUE(false);
    }
    keyPath += "/";
#elif defined(AZ_PLATFORM_WINDOWS)
    keyPath = Azure::Core::_internal::Environment::GetVariable("ProgramData");
    if (keyPath.empty())
    {
      GTEST_LOG_(ERROR) << "We can't get ProgramData folder path in an AzureArc test.";
      EXPECT_TRUE(false);
    }
    // Unlike linux, we can't use mkdir on Windows, since it is deprecated. We will use
    // CreateDirectory instead.
    // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/mkdir?view=msvc-170
    keyPath += "\\AzureConnectedMachineAgent";
    if (!CreateDirectory(keyPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
      GTEST_LOG_(ERROR) << "Directory creation failure in an AzureArc test: " << keyPath
                        << " Error: " << GetLastError();
      EXPECT_TRUE(false);
    }
    keyPath += "\\Tokens";
    if (!CreateDirectory(keyPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
      GTEST_LOG_(ERROR) << "Directory creation failure in an an AzureArc test: " << keyPath
                        << " Error: " << GetLastError();
      EXPECT_TRUE(false);
    }
    keyPath += "\\";
#endif
    return keyPath;
  }

  TEST(ManagedIdentityCredential, AzureArc)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    std::string keyPath = CreateDirectoryAndGetKeyPath();
    if (keyPath.empty())
    {
      GTEST_SKIP_("Skipping AzureArc test on unsupported OSes.");
    }

    {
      std::ofstream secretFile(
          keyPath + "managed_identity_credential_test1.key",
          std::ios_base::out | std::ios_base::trunc);

      secretFile << "SECRET1";
    }

    {
      std::ofstream secretFile(
          keyPath + "managed_identity_credential_test2.key",
          std::ios_base::out | std::ios_base::trunc);

      secretFile << "SECRET2";
    }

    {
      std::ofstream secretFile(
          keyPath + "managed_identity_credential_test3.key",
          std::ios_base::out | std::ios_base::trunc);

      secretFile << "SECRET3";
    }

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          auto credential = std::make_unique<ManagedIdentityCredential>(options);

          EXPECT_EQ(log.size(), LogMsgVec::size_type(4));

          EXPECT_EQ(log[0].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[0].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2019 source.");

          EXPECT_EQ(log[1].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[1].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with App Service 2017 source.");

          EXPECT_EQ(log[2].first, Logger::Level::Verbose);
          EXPECT_EQ(
              log[2].second,
              "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
              "to be created with Cloud Shell source.");

          EXPECT_EQ(log[3].first, Logger::Level::Informational);
          EXPECT_EQ(
              log[3].second,
              "Identity: ManagedIdentityCredential will be created with Azure Arc source.");

          log.clear();

          return credential;
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        {{HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "ABC ABC=" + keyPath + "managed_identity_credential_test1.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}},
         {HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "XYZ XYZ=" + keyPath + "managed_identity_credential_test2.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}", {}},
         {HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "ABC ABC=" + keyPath + "managed_identity_credential_test3.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}", {}}});

    EXPECT_EQ(actual.Requests.size(), 6U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);
    auto const& request3 = actual.Requests.at(3);
    auto const& request4 = actual.Requests.at(4);
    auto const& request5 = actual.Requests.at(5);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request3.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request4.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request5.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-11-01&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-11-01&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-11-01&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request3.AbsoluteUrl,
        "https://visualstudio.com?api-version=2019-11-01&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(request4.AbsoluteUrl, "https://visualstudio.com?api-version=2019-11-01");
    EXPECT_EQ(request5.AbsoluteUrl, "https://visualstudio.com?api-version=2019-11-01");

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());
    EXPECT_TRUE(request3.Body.empty());
    EXPECT_TRUE(request4.Body.empty());
    EXPECT_TRUE(request5.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("Metadata"), "true");

      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");

      EXPECT_NE(request3.Headers.find("Metadata"), request3.Headers.end());
      EXPECT_EQ(request3.Headers.at("Metadata"), "true");

      EXPECT_NE(request4.Headers.find("Metadata"), request4.Headers.end());
      EXPECT_EQ(request4.Headers.at("Metadata"), "true");

      EXPECT_NE(request5.Headers.find("Metadata"), request5.Headers.end());
      EXPECT_EQ(request5.Headers.at("Metadata"), "true");
    }

    {
      EXPECT_EQ(request0.Headers.find("Authorization"), request0.Headers.end());

      EXPECT_NE(request1.Headers.find("Authorization"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Authorization"), "Basic SECRET1");

      EXPECT_EQ(request2.Headers.find("Authorization"), request2.Headers.end());

      EXPECT_NE(request3.Headers.find("Authorization"), request3.Headers.end());
      EXPECT_EQ(request3.Headers.at("Authorization"), "Basic SECRET2");

      EXPECT_EQ(request4.Headers.find("Authorization"), request4.Headers.end());

      EXPECT_NE(request5.Headers.find("Authorization"), request5.Headers.end());
      EXPECT_EQ(request5.Headers.at("Authorization"), "Basic SECRET3");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, AzureArcClientId)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> azureArcManagedIdentityCredential;
          EXPECT_THROW(
              azureArcManagedIdentityCredential = std::make_unique<ManagedIdentityCredential>(
                  "fedcba98-7654-3210-0123-456789abcdef", options),
              AuthenticationException);

          return azureArcManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, AzureArcResourceId)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedObjectId("abcdef01-2345-6789-0876-543210fedcba");

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> azureArcManagedIdentityCredential;
          EXPECT_THROW(
              azureArcManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return azureArcManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, AzureArcObjectId)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    std::string const resourceId
        = "/subscriptions/abcdef01-2345-6789-9876-543210fedcba/locations/MyLocation";

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier(resourceId));

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> azureArcManagedIdentityCredential;
          EXPECT_THROW(
              azureArcManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return azureArcManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, AzureArcAuthHeaderMissing)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));
  }

  TEST(ManagedIdentityCredential, AzureArcUnexpectedHttpStatusCode)
  {
    {
      std::ofstream secretFile(
          "managed_identity_credential_test0.txt", std::ios_base::out | std::ios_base::trunc);

      secretFile << "SECRET0";
    }

    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Forbidden,
          "",
          {{"WWW-Authenticate", "ABC ABC=managed_identity_credential_test0.txt"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));
  }

  TEST(ManagedIdentityCredential, AzureArcAuthHeaderNoEquals)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABCSECRET1"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));
  }

  TEST(ManagedIdentityCredential, AzureArcAuthHeaderTwoEquals)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABC=SECRET1=SECRET2"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));
  }

  TEST(ManagedIdentityCredential, AzureArcInvalidKey)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    std::string keyPath;

#if defined(AZ_PLATFORM_LINUX)
    keyPath = "/var/opt/azcmagent/tokens/";
#elif defined(AZ_PLATFORM_WINDOWS)
    keyPath = Azure::Core::_internal::Environment::GetVariable("ProgramData");
    if (keyPath.empty())
    {
      GTEST_LOG_(ERROR) << "We can't get ProgramData folder path in AzureArcInvalidKey test.";
      EXPECT_TRUE(false);
    }
    keyPath += "\\AzureConnectedMachineAgent\\Tokens\\";
#else
    // Unsupported OS
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABC ABC=foo.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    GTEST_SKIP_("Skipping the rest of AzureArcInvalidKey tests on unsupported OSes.");
#endif

    // Invalid Key Path - empty directory
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABC ABC=foo.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    // Invalid Key Path - unexpected directory
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABC ABC=C:\\Foo\\foo.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    // Invalid Key Path - unexpected extension, filename is short
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized, "", {{"WWW-Authenticate", "ABC ABC=" + keyPath + "a.b"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    // Invalid Key Path - unexpected extension
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "ABC ABC=" + keyPath + "foo.txt"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    // Invalid Key Path - file missing
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "ABC ABC=" + keyPath + "foo.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));

    keyPath = CreateDirectoryAndGetKeyPath();
    if (keyPath.empty())
    {
      GTEST_SKIP_("Skipping AzureArcInvalidKey test on unsupported OSes.");
    }

    {
      std::ofstream secretFile(keyPath + "toolarge.key", std::ios_base::out | std::ios_base::trunc);

      if (!secretFile.is_open())
      {
        GTEST_LOG_(ERROR) << "Failed to create a test file required in AzureArcInvalidKey test.";
        EXPECT_TRUE(false);
      }

      std::string fileContents(4097, '.');

      secretFile << fileContents;
    }

    // Invalid Key Path - file too large
    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}},
        {{HttpStatusCode::Unauthorized,
          "",
          {{"WWW-Authenticate", "ABC ABC=" + keyPath + "toolarge.key"}}},
         {HttpStatusCode::Ok, "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}", {}}},
        [](auto& credential, auto& tokenRequestContext, auto& context) {
          AccessToken token;
          EXPECT_THROW(
              token = credential.GetToken(tokenRequestContext, context), AuthenticationException);
          return token;
        }));
  }

  TEST(ManagedIdentityCredential, AzureArcInvalidUrl)
  {
    using Azure::Core::Credentials::AccessToken;
    using Azure::Core::Credentials::AuthenticationException;

    static_cast<void>(CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com:INVALID/"},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", "0123456789abcdef0123456789abcdef01234567"},
          });

          std::unique_ptr<ManagedIdentityCredential const> azureArcManagedIdentityCredential;
          EXPECT_THROW(
              azureArcManagedIdentityCredential
              = std::make_unique<ManagedIdentityCredential>(options),
              AuthenticationException);

          return azureArcManagedIdentityCredential;
        },
        {},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"}));
  }

  TEST(ManagedIdentityCredential, Imds)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    {
      auto const actual = CredentialTestHelper::SimulateTokenRequest(
          [&](auto transport) {
            TokenCredentialOptions options;
            options.Transport.Transport = transport;

            CredentialTestHelper::EnvironmentOverride const env({
                {"MSI_ENDPOINT", ""},
                {"MSI_SECRET", ""},
                {"IDENTITY_ENDPOINT", ""},
                {"IMDS_ENDPOINT", ""},
                {"IDENTITY_HEADER", ""},
                {"IDENTITY_SERVER_THUMBPRINT", ""},
            });

            auto credential = std::make_unique<ManagedIdentityCredential>(options);

            EXPECT_EQ(log.size(), LogMsgVec::size_type(5));

            EXPECT_EQ(log[0].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[0].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2019 source.");

            EXPECT_EQ(log[1].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[1].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2017 source.");

            EXPECT_EQ(log[2].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[2].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Cloud Shell source.");

            EXPECT_EQ(log[3].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[3].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Azure Arc source.");

            EXPECT_EQ(log[4].first, Logger::Level::Informational);
            EXPECT_EQ(
                log[4].second,
                "Identity: ManagedIdentityCredential will be created "
                "with Azure Instance Metadata Service source."
                "\nSuccessful creation does not guarantee further successful token retrieval.");

            log.clear();

            return credential;
          },
          {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}, {}, {}, {}},
          std::vector<std::string>{
              "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
              "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN4\", \"refresh_in\":9999}",
              "{\"expires_in\":7199, \"access_token\":\"ACCESSTOKEN5\"}",
              "{\"expires_in\":7202, \"access_token\":\"ACCESSTOKEN6\"}"});

      EXPECT_EQ(actual.Requests.size(), 6U);
      EXPECT_EQ(actual.Responses.size(), 6U);

      auto const& request0 = actual.Requests.at(0);
      auto const& request1 = actual.Requests.at(1);
      auto const& request2 = actual.Requests.at(2);
      auto const& request3 = actual.Requests.at(3);
      auto const& request4 = actual.Requests.at(4);
      auto const& request5 = actual.Requests.at(5);

      auto const& response0 = actual.Responses.at(0);
      auto const& response1 = actual.Responses.at(1);
      auto const& response2 = actual.Responses.at(2);
      auto const& response3 = actual.Responses.at(3);
      auto const& response4 = actual.Responses.at(4);
      auto const& response5 = actual.Responses.at(5);

      EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request3.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request4.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request5.HttpMethod, HttpMethod::Get);

      EXPECT_EQ(
          request0.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

      EXPECT_EQ(
          request1.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

      EXPECT_EQ(
          request2.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request3.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request4.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request5.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_TRUE(request0.Body.empty());
      EXPECT_TRUE(request1.Body.empty());
      EXPECT_TRUE(request2.Body.empty());
      EXPECT_TRUE(request3.Body.empty());
      EXPECT_TRUE(request4.Body.empty());
      EXPECT_TRUE(request5.Body.empty());

      {
        EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
        EXPECT_EQ(request0.Headers.at("Metadata"), "true");

        EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
        EXPECT_EQ(request1.Headers.at("Metadata"), "true");

        EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
        EXPECT_EQ(request2.Headers.at("Metadata"), "true");

        EXPECT_NE(request3.Headers.find("Metadata"), request3.Headers.end());
        EXPECT_EQ(request3.Headers.at("Metadata"), "true");

        EXPECT_NE(request4.Headers.find("Metadata"), request4.Headers.end());
        EXPECT_EQ(request4.Headers.at("Metadata"), "true");

        EXPECT_NE(request5.Headers.find("Metadata"), request5.Headers.end());
        EXPECT_EQ(request5.Headers.at("Metadata"), "true");
      }

      EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
      EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
      EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");
      EXPECT_EQ(response3.AccessToken.Token, "ACCESSTOKEN4");
      EXPECT_EQ(response4.AccessToken.Token, "ACCESSTOKEN5");
      EXPECT_EQ(response5.AccessToken.Token, "ACCESSTOKEN6");

      using namespace std::chrono_literals;
      EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
      EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

      EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
      EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

      EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
      EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

      EXPECT_GE(response3.AccessToken.ExpiresOn, response3.EarliestExpiration + 9999s);
      EXPECT_LE(response3.AccessToken.ExpiresOn, response3.LatestExpiration + 9999s);

      EXPECT_GE(response4.AccessToken.ExpiresOn, response4.EarliestExpiration + 7199s);
      EXPECT_LE(response4.AccessToken.ExpiresOn, response4.LatestExpiration + 7199s);

      EXPECT_GE(response5.AccessToken.ExpiresOn, response5.EarliestExpiration + 3601s);
      EXPECT_LE(response5.AccessToken.ExpiresOn, response5.LatestExpiration + 3601s);
    }
    {
      auto const actual = CredentialTestHelper::SimulateTokenRequest(
          [&](auto transport) {
            ManagedIdentityCredentialOptions options;
            options.Transport.Transport = transport;
            options.IdentityId = ManagedIdentityId::SystemAssigned();

            CredentialTestHelper::EnvironmentOverride const env({
                {"MSI_ENDPOINT", ""},
                {"MSI_SECRET", ""},
                {"IDENTITY_ENDPOINT", ""},
                {"IMDS_ENDPOINT", ""},
                {"IDENTITY_HEADER", ""},
                {"IDENTITY_SERVER_THUMBPRINT", ""},
            });

            log.clear();

            auto credential = std::make_unique<ManagedIdentityCredential>(options);

            EXPECT_EQ(log.size(), LogMsgVec::size_type(5));

            EXPECT_EQ(log[0].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[0].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2019 source.");

            EXPECT_EQ(log[1].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[1].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2017 source.");

            EXPECT_EQ(log[2].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[2].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Cloud Shell source.");

            EXPECT_EQ(log[3].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[3].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Azure Arc source.");

            EXPECT_EQ(log[4].first, Logger::Level::Informational);
            EXPECT_EQ(
                log[4].second,
                "Identity: ManagedIdentityCredential will be created "
                "with Azure Instance Metadata Service source."
                "\nSuccessful creation does not guarantee further successful token retrieval.");

            log.clear();

            return credential;
          },
          {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}, {}, {}, {}},
          std::vector<std::string>{
              "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
              "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN4\", \"refresh_in\":9999}",
              "{\"expires_in\":7199, \"access_token\":\"ACCESSTOKEN5\"}",
              "{\"expires_in\":7202, \"access_token\":\"ACCESSTOKEN6\"}"});

      EXPECT_EQ(actual.Requests.size(), 6U);
      EXPECT_EQ(actual.Responses.size(), 6U);

      auto const& request0 = actual.Requests.at(0);
      auto const& request1 = actual.Requests.at(1);
      auto const& request2 = actual.Requests.at(2);
      auto const& request3 = actual.Requests.at(3);
      auto const& request4 = actual.Requests.at(4);
      auto const& request5 = actual.Requests.at(5);

      auto const& response0 = actual.Responses.at(0);
      auto const& response1 = actual.Responses.at(1);
      auto const& response2 = actual.Responses.at(2);
      auto const& response3 = actual.Responses.at(3);
      auto const& response4 = actual.Responses.at(4);
      auto const& response5 = actual.Responses.at(5);

      EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request3.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request4.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request5.HttpMethod, HttpMethod::Get);

      EXPECT_EQ(
          request0.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

      EXPECT_EQ(
          request1.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

      EXPECT_EQ(
          request2.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request3.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request4.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_EQ(
          request5.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01");

      EXPECT_TRUE(request0.Body.empty());
      EXPECT_TRUE(request1.Body.empty());
      EXPECT_TRUE(request2.Body.empty());
      EXPECT_TRUE(request3.Body.empty());
      EXPECT_TRUE(request4.Body.empty());
      EXPECT_TRUE(request5.Body.empty());

      {
        EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
        EXPECT_EQ(request0.Headers.at("Metadata"), "true");

        EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
        EXPECT_EQ(request1.Headers.at("Metadata"), "true");

        EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
        EXPECT_EQ(request2.Headers.at("Metadata"), "true");

        EXPECT_NE(request3.Headers.find("Metadata"), request3.Headers.end());
        EXPECT_EQ(request3.Headers.at("Metadata"), "true");

        EXPECT_NE(request4.Headers.find("Metadata"), request4.Headers.end());
        EXPECT_EQ(request4.Headers.at("Metadata"), "true");

        EXPECT_NE(request5.Headers.find("Metadata"), request5.Headers.end());
        EXPECT_EQ(request5.Headers.at("Metadata"), "true");
      }

      EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
      EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
      EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");
      EXPECT_EQ(response3.AccessToken.Token, "ACCESSTOKEN4");
      EXPECT_EQ(response4.AccessToken.Token, "ACCESSTOKEN5");
      EXPECT_EQ(response5.AccessToken.Token, "ACCESSTOKEN6");

      using namespace std::chrono_literals;
      EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
      EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

      EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
      EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

      EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
      EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);

      EXPECT_GE(response3.AccessToken.ExpiresOn, response3.EarliestExpiration + 9999s);
      EXPECT_LE(response3.AccessToken.ExpiresOn, response3.LatestExpiration + 9999s);

      EXPECT_GE(response4.AccessToken.ExpiresOn, response4.EarliestExpiration + 7199s);
      EXPECT_LE(response4.AccessToken.ExpiresOn, response4.LatestExpiration + 7199s);

      EXPECT_GE(response5.AccessToken.ExpiresOn, response5.EarliestExpiration + 3601s);
      EXPECT_LE(response5.AccessToken.ExpiresOn, response5.LatestExpiration + 3601s);
    }

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, ImdsClientId)
  {
    using Azure::Core::Diagnostics::Logger;
    using LogMsgVec = std::vector<std::pair<Logger::Level, std::string>>;
    LogMsgVec log;
    Logger::SetLevel(Logger::Level::Verbose);
    Logger::SetListener([&](auto lvl, auto msg) { log.push_back(std::make_pair(lvl, msg)); });

    {
      auto const actual = CredentialTestHelper::SimulateTokenRequest(
          [&](auto transport) {
            TokenCredentialOptions options;
            options.Transport.Transport = transport;

            CredentialTestHelper::EnvironmentOverride const env({
                {"MSI_ENDPOINT", ""},
                {"MSI_SECRET", ""},
                {"IDENTITY_ENDPOINT", ""},
                {"IMDS_ENDPOINT", ""},
                {"IDENTITY_HEADER", ""},
                {"IDENTITY_SERVER_THUMBPRINT", ""},
            });

            auto credential = std::make_unique<ManagedIdentityCredential>(
                "fedcba98-7654-3210-0123-456789abcdef", options);

            EXPECT_EQ(log.size(), LogMsgVec::size_type(5));

            EXPECT_EQ(log[0].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[0].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2019 source.");

            EXPECT_EQ(log[1].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[1].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2017 source.");

            EXPECT_EQ(log[2].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[2].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Cloud Shell source.");

            EXPECT_EQ(log[3].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[3].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Azure Arc source.");

            EXPECT_EQ(log[4].first, Logger::Level::Informational);
            EXPECT_EQ(
                log[4].second,
                "Identity: ManagedIdentityCredential will be created "
                "with Azure Instance Metadata Service source"
                " and Client ID 'fedcba98-7654-3210-0123-456789abcdef'."
                "\nSuccessful creation does not guarantee further successful token retrieval.");

            log.clear();

            return credential;
          },
          {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
          std::vector<std::string>{
              "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
              "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

      EXPECT_EQ(actual.Requests.size(), 3U);
      EXPECT_EQ(actual.Responses.size(), 3U);

      auto const& request0 = actual.Requests.at(0);
      auto const& request1 = actual.Requests.at(1);
      auto const& request2 = actual.Requests.at(2);

      auto const& response0 = actual.Responses.at(0);
      auto const& response1 = actual.Responses.at(1);
      auto const& response2 = actual.Responses.at(2);

      EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

      EXPECT_EQ(
          request0.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef"
          "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

      EXPECT_EQ(
          request1.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef"
          "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

      EXPECT_EQ(
          request2.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef");

      EXPECT_TRUE(request0.Body.empty());
      EXPECT_TRUE(request1.Body.empty());
      EXPECT_TRUE(request2.Body.empty());

      {
        EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
        EXPECT_EQ(request0.Headers.at("Metadata"), "true");

        EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
        EXPECT_EQ(request1.Headers.at("Metadata"), "true");

        EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
        EXPECT_EQ(request2.Headers.at("Metadata"), "true");
      }

      EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
      EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
      EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

      using namespace std::chrono_literals;
      EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
      EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

      EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
      EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

      EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
      EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
    }
    {
      auto const actual = CredentialTestHelper::SimulateTokenRequest(
          [&](auto transport) {
            ManagedIdentityCredentialOptions options;
            options.Transport.Transport = transport;
            options.IdentityId = ManagedIdentityId::FromUserAssignedClientId(
                "fedcba98-7654-3210-0123-456789abcdef");

            CredentialTestHelper::EnvironmentOverride const env({
                {"MSI_ENDPOINT", ""},
                {"MSI_SECRET", ""},
                {"IDENTITY_ENDPOINT", ""},
                {"IMDS_ENDPOINT", ""},
                {"IDENTITY_HEADER", ""},
                {"IDENTITY_SERVER_THUMBPRINT", ""},
            });

            log.clear();

            auto credential = std::make_unique<ManagedIdentityCredential>(options);

            EXPECT_EQ(log.size(), LogMsgVec::size_type(5));

            EXPECT_EQ(log[0].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[0].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2019 source.");

            EXPECT_EQ(log[1].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[1].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with App Service 2017 source.");

            EXPECT_EQ(log[2].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[2].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Cloud Shell source.");

            EXPECT_EQ(log[3].first, Logger::Level::Verbose);
            EXPECT_EQ(
                log[3].second,
                "Identity: ManagedIdentityCredential: Environment is not set up for the credential "
                "to be created with Azure Arc source.");

            EXPECT_EQ(log[4].first, Logger::Level::Informational);
            EXPECT_EQ(
                log[4].second,
                "Identity: ManagedIdentityCredential will be created "
                "with Azure Instance Metadata Service source"
                " and Client ID 'fedcba98-7654-3210-0123-456789abcdef'."
                "\nSuccessful creation does not guarantee further successful token retrieval.");

            log.clear();

            return credential;
          },
          {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
          std::vector<std::string>{
              "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
              "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
              "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

      EXPECT_EQ(actual.Requests.size(), 3U);
      EXPECT_EQ(actual.Responses.size(), 3U);

      auto const& request0 = actual.Requests.at(0);
      auto const& request1 = actual.Requests.at(1);
      auto const& request2 = actual.Requests.at(2);

      auto const& response0 = actual.Responses.at(0);
      auto const& response1 = actual.Responses.at(1);
      auto const& response2 = actual.Responses.at(2);

      EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
      EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

      EXPECT_EQ(
          request0.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef"
          "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

      EXPECT_EQ(
          request1.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef"
          "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

      EXPECT_EQ(
          request2.AbsoluteUrl,
          "http://169.254.169.254/metadata/identity/oauth2/token"
          "?api-version=2018-02-01"
          "&client_id=fedcba98-7654-3210-0123-456789abcdef");

      EXPECT_TRUE(request0.Body.empty());
      EXPECT_TRUE(request1.Body.empty());
      EXPECT_TRUE(request2.Body.empty());

      {
        EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
        EXPECT_EQ(request0.Headers.at("Metadata"), "true");

        EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
        EXPECT_EQ(request1.Headers.at("Metadata"), "true");

        EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
        EXPECT_EQ(request2.Headers.at("Metadata"), "true");
      }

      EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
      EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
      EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

      using namespace std::chrono_literals;
      EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
      EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

      EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
      EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

      EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
      EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
    }

    Logger::SetListener(nullptr);
  }

  TEST(ManagedIdentityCredential, ImdsResourceId)
  {
    std::string const resourceId
        = "/subscriptions/abcdef01-2345-6789-9876-543210fedcba/locations/MyLocation";

    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [&](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedResourceId(ResourceIdentifier(resourceId));

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", ""},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", ""},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&msi_res_id="
            + resourceId + "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&msi_res_id="
            + resourceId + "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&msi_res_id="
            + resourceId);

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("Metadata"), "true");

      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, ImdsObjectId)
  {
    auto const actual = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          ManagedIdentityCredentialOptions options;
          options.Transport.Transport = transport;
          options.IdentityId
              = ManagedIdentityId::FromUserAssignedObjectId("abcdef01-2345-6789-0876-543210fedcba");

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", ""},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", ""},
          });

          return std::make_unique<ManagedIdentityCredential>(options);
        },
        {{"https://azure.com/.default"}, {"https://outlook.com/.default"}, {}},
        std::vector<std::string>{
            "{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}",
            "{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}",
            "{\"expires_in\":9999, \"access_token\":\"ACCESSTOKEN3\"}"});

    EXPECT_EQ(actual.Requests.size(), 3U);
    EXPECT_EQ(actual.Responses.size(), 3U);

    auto const& request0 = actual.Requests.at(0);
    auto const& request1 = actual.Requests.at(1);
    auto const& request2 = actual.Requests.at(2);

    auto const& response0 = actual.Responses.at(0);
    auto const& response1 = actual.Responses.at(1);
    auto const& response2 = actual.Responses.at(2);

    EXPECT_EQ(request0.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request0.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token"
        "?api-version=2018-02-01"
        "&object_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token"
        "?api-version=2018-02-01"
        "&object_id=abcdef01-2345-6789-0876-543210fedcba" // cspell:disable-line
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token"
        "?api-version=2018-02-01"
        "&object_id=abcdef01-2345-6789-0876-543210fedcba"); // cspell:disable-line

    EXPECT_TRUE(request0.Body.empty());
    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request0.Headers.find("Metadata"), request0.Headers.end());
      EXPECT_EQ(request0.Headers.at("Metadata"), "true");

      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");
    }

    EXPECT_EQ(response0.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN2");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN3");

    using namespace std::chrono_literals;
    EXPECT_GE(response0.AccessToken.ExpiresOn, response0.EarliestExpiration + 3600s);
    EXPECT_LE(response0.AccessToken.ExpiresOn, response0.LatestExpiration + 3600s);

    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 4999s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 4999s);
  }

  TEST(ManagedIdentityCredential, ImdsCreation)
  {
    auto const actual1 = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", "https://visualstudio.com/"},
              {"IMDS_ENDPOINT", ""},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", ""},
          });

          return std::make_unique<ManagedIdentityCredential>(
              "fedcba98-7654-3210-0123-456789abcdef", options);
        },
        {{"https://azure.com/.default"}},
        {"{\"expires_in\":3600, \"access_token\":\"ACCESSTOKEN1\"}"});

    auto const actual2 = CredentialTestHelper::SimulateTokenRequest(
        [](auto transport) {
          TokenCredentialOptions options;
          options.Transport.Transport = transport;

          CredentialTestHelper::EnvironmentOverride const env({
              {"MSI_ENDPOINT", ""},
              {"MSI_SECRET", ""},
              {"IDENTITY_ENDPOINT", ""},
              {"IMDS_ENDPOINT", "https://xbox.com/"},
              {"IDENTITY_HEADER", ""},
              {"IDENTITY_SERVER_THUMBPRINT", ""},
          });

          return std::make_unique<ManagedIdentityCredential>(
              "01234567-89ab-cdef-fedc-ba9876543210", options);
        },
        {{"https://outlook.com/.default"}},
        {"{\"expires_in\":7200, \"access_token\":\"ACCESSTOKEN2\"}"});

    EXPECT_EQ(actual1.Requests.size(), 1U);
    EXPECT_EQ(actual1.Responses.size(), 1U);

    EXPECT_EQ(actual2.Requests.size(), 1U);
    EXPECT_EQ(actual2.Responses.size(), 1U);

    auto const& request1 = actual1.Requests.at(0);
    auto const& response1 = actual1.Responses.at(0);

    auto const& request2 = actual2.Requests.at(0);
    auto const& response2 = actual2.Responses.at(0);

    EXPECT_EQ(request1.HttpMethod, HttpMethod::Get);
    EXPECT_EQ(request2.HttpMethod, HttpMethod::Get);

    EXPECT_EQ(
        request1.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token"
        "?api-version=2018-02-01"
        "&client_id=fedcba98-7654-3210-0123-456789abcdef"
        "&resource=https%3A%2F%2Fazure.com"); // cspell:disable-line

    EXPECT_EQ(
        request2.AbsoluteUrl,
        "http://169.254.169.254/metadata/identity/oauth2/token"
        "?api-version=2018-02-01"
        "&client_id=01234567-89ab-cdef-fedc-ba9876543210"
        "&resource=https%3A%2F%2Foutlook.com"); // cspell:disable-line

    EXPECT_TRUE(request1.Body.empty());
    EXPECT_TRUE(request2.Body.empty());

    {
      EXPECT_NE(request1.Headers.find("Metadata"), request1.Headers.end());
      EXPECT_EQ(request1.Headers.at("Metadata"), "true");

      EXPECT_NE(request2.Headers.find("Metadata"), request2.Headers.end());
      EXPECT_EQ(request2.Headers.at("Metadata"), "true");
    }

    EXPECT_EQ(response1.AccessToken.Token, "ACCESSTOKEN1");
    EXPECT_EQ(response2.AccessToken.Token, "ACCESSTOKEN2");

    using namespace std::chrono_literals;
    EXPECT_GE(response1.AccessToken.ExpiresOn, response1.EarliestExpiration + 3600s);
    EXPECT_LE(response1.AccessToken.ExpiresOn, response1.LatestExpiration + 3600s);

    EXPECT_GE(response2.AccessToken.ExpiresOn, response2.EarliestExpiration + 3600s);
    EXPECT_LE(response2.AccessToken.ExpiresOn, response2.LatestExpiration + 3600s);
  }

}}} // namespace Azure::Identity::Test
