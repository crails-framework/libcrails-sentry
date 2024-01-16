#include "sentry.hpp"

using namespace Crails;
using namespace std;

void SentrySettings::key(const std::string& value)
{
  const_cast<string&>(Sentry::sentry_key) = value;
}

void SentrySettings::secret(const std::string& value)
{
  const_cast<string&>(Sentry::sentry_secret) = value;
}

void SentrySettings::version(const std::string& value)
{
  const_cast<string&>(Sentry::sentry_version) = value;
}

void SentrySettings::project_id(const std::string& value)
{
  const_cast<string&>(Sentry::project_id) = value;
}

void SentrySettings::server_url(const std::string& value)
{
  const_cast<string&>(Sentry::server_url) = value;
}

void SentrySettings::server_protocol(const std::string& value)
{
  const_cast<string&>(Sentry::server_protocol) = value;
}
