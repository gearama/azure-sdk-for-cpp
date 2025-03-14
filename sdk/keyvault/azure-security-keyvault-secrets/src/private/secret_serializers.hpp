// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

/**
 * @file
 * @brief Serializers/deserializers for the KeyVault Secret client.
 *
 */

#pragma once
#include "azure/keyvault/secrets/keyvault_backup_secret.hpp"
#include "azure/keyvault/secrets/keyvault_deleted_secret.hpp"
#include "azure/keyvault/secrets/keyvault_secret.hpp"
#include "azure/keyvault/secrets/keyvault_secret_paged_response.hpp"

#include <azure/core/http/http.hpp>
#include <azure/core/internal/json/json.hpp>

#include <stdint.h>
#include <vector>

using namespace Azure::Security::KeyVault::Secrets;

namespace Azure { namespace Security { namespace KeyVault { namespace Secrets { namespace _detail {
  struct SecretSerializer final
  {
    // Extract the host out of the URL (with port if available)
    static std::string GetUrlAuthorityWithScheme(Azure::Core::Url const& url)
    {
      std::string urlString;
      if (!url.GetScheme().empty())
      {
        urlString += url.GetScheme() + "://";
      }
      urlString += url.GetHost();
      if (url.GetPort() != 0)
      {
        urlString += ":" + std::to_string(url.GetPort());
      }
      return urlString;
    }

    // parse the ID url to extract relevant data
    void static inline ParseIDUrl(SecretProperties& secretProperties, std::string const& url)
    {
      Azure::Core::Url sid(url);
      secretProperties.Id = url;
      secretProperties.VaultUrl = GetUrlAuthorityWithScheme(sid);
      auto const& path = sid.GetPath();
      //  path is in the form of `verb/keyName{/keyVersion}`
      if (path.length() > 0)
      {
        auto const separatorChar = '/';
        auto pathEnd = path.end();
        auto start = path.begin();
        start = std::find(start, pathEnd, separatorChar);
        start += 1;
        auto separator = std::find(start, pathEnd, separatorChar);
        if (separator != pathEnd)
        {
          secretProperties.Name = std::string(start, separator);
          start = separator + 1;
          secretProperties.Version = std::string(start, pathEnd);
        }
        else
        {
          // Nothing but the name+
          secretProperties.Name = std::string(start, pathEnd);
        }
      }
    }
  };
}}}}} // namespace Azure::Security::KeyVault::Secrets::_detail
