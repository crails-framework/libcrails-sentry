#include <crails/logger.hpp>
#include <chrono>
#include "sentry.hpp"

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
static std::string demangle(const char* name)
{
  int status = -4;
  std::unique_ptr<char, void(*)(void*)> res {
    abi::__cxa_demangle(name, NULL, NULL, &status),
    std::free
  };
  return !status ? res.get() : name;
}
#else
static std::string demangle(const char* name) { return name; }
#endif

using namespace Crails;
using namespace std;
using namespace boost;

template<class T>
static string get_type_as_string(const T& t)
{
  return demangle(typeid(t).name());
}

static string sentry_getenv(const char* varname)
{
  const char* result = std::getenv(varname);

  return result ? string(result) : string();
}

const string Sentry::sentry_key      = sentry_getenv("SENTRY_KEY");
const string Sentry::sentry_secret   = sentry_getenv("SENTRY_SECRET");
const string Sentry::sentry_version  = "7";
const string Sentry::project_id      = sentry_getenv("SENTRY_PROJECT_ID");
const string Sentry::server_url      = "sentry.io";
const string Sentry::server_protocol = "https";

Sentry::Sentry()
{
}

void Sentry::set_message_context(Data message)
{
  message["platform"] = "c++";
  message["logger"]   = "crails";
  message["project"]  = project_id;
}

void Sentry::set_message_request(Data message, Data params)
{
  Data request_data = message["request"];
  Data user_agent   = params["headers"]["User-Agent"];

  if (params["headers"].exists())
  {
    request_data["headers"] = "";
    request_data["headers"].merge(params["headers"]);
    request_data["headers"]["Cookie"] = "";
    if (request_data["headers"]["Cookie"].exists())
      request_data["headers"]["Cookie"].destroy();
  }

  Data data = request_data["data"];
  vector<string> ignored_keys = {
    "headers", "uri", "method", "response-data", "response-time", "controller-data"
  };

  data = "";
  data.merge(params);
  for (auto key : ignored_keys)
    data[key].destroy();

  string url = params["headers"]["Host"].defaults_to<string>("");
  if (url.size() > 0) { url = "http://" + url; }
  url += params["uri"].defaults_to<string>("");
  request_data["url"] = url;
  request_data["method"] = params["method"].defaults_to<string>("");
}

void Sentry::initialize_exception_message(Data message, Data params, const std::exception& e)
{
  message["culprit"] = "<anonymous>";
  message["message"] = e.what();
  set_message_context(message);
  set_message_request(message, params);
}

void Sentry::initialize_backtrace(Data exception_data, const boost_ext::backtrace& e)
{
  Data frames = exception_data["stacktrace"]["frames"];

  for (unsigned short i = 3 ; i < e.stack_size() ; ++i)
  {
    DataTree frame;
    string trace_line = e.trace_line(i);

    size_t func_name_beg = trace_line.find(":") + 2;
    size_t func_name_end = trace_line.find("+");
    size_t file_name_beg = trace_line.find(" in ") + 4;

    frame["in_app"]   = trace_line.find("libcrails-app.so") != string::npos;
    frame["filename"] = trace_line.substr(file_name_beg);
    frame["function"] = trace_line.substr(func_name_beg, func_name_end - func_name_beg);
    frames.push_back(frame.as_data());
  }
}

void Sentry::capture_exception(Data params, const std::exception& e)
{
  try
  {
    DataTree message;

    initialize_exception_message(message.as_data(), params, e);
    {
      Data exceptions_data = message["exception"];
      DataTree exception_data;

      exception_data["type"] = get_type_as_string(e);
      exception_data["value"] = e.what();
      {
        boost_ext::backtrace const *tr = dynamic_cast<boost_ext::backtrace const*>(&e);

        if (tr)
          initialize_backtrace(exception_data, *tr);
      }
      exceptions_data.push_back(exception_data.as_data());
    }
    send_message(message.as_data());
  }
  catch (const std::exception& e)
  {
    logger << Logger::Info << "[Sentry] Failed to log exception to sentry server: " << e.what() << Logger::endl;
  }
  catch (...)
  {
    logger << Logger::Info << "[Sentry] Failed to log exception to sentry server." << Logger::endl;
  }
}

Crails::ClientInterface& Sentry::require_client()
{
  if (!client)
  {
    if (server_protocol == "https")
      client.reset(new Crails::Ssl::Client(server_url, 443));
    else
      client.reset(new Crails::Client(server_url, 80));
    client->connect();
  }
  return *client;
}

string Sentry::get_server_url()
{
  return "/api/" + project_id + "/store/";
}

string Sentry::sentry_auth_header()
{
  time_t timestamp = std::chrono::system_clock::to_time_t(
    std::chrono::system_clock::now()
  );

  return string("Sentry ")
    + "sentry_version=" + sentry_version + ','
    + "sentry_client=libcrails-sentry,"
    + "sentry_sentry_timestamp" + std::to_string(timestamp) + ','
    + "sentry_key=" + sentry_key + ','
    + "sentry_secret=" + sentry_secret;
}

static void monkey_patch_json_body(string& b, string val)
{
  string with_quote = '"' + val + '"';
  string::size_type n = 0;

  while ((n = b.find(with_quote, n)) != string::npos)
    b.replace(n, with_quote.size(), val);
}

void Sentry::send_message(Data message)
{
  Client::Request  request{HttpVerb::post, get_server_url(), 11};
  Client::Response response;
  string           json_data = message.to_json();

  monkey_patch_json_body(json_data, "true");
  monkey_patch_json_body(json_data, "false");
  request.set(HttpHeader::accept, "application/json");
  request.set(HttpHeader::connection, "close");
  request.set(HttpHeader::content_type, "application/json");
  request.set("X-Sentry-Auth", sentry_auth_header());
  request.body() = json_data;
  request.content_length(json_data.length());
  response = require_client().query(request);
  logger << Logger::Info << "[Sentry] ";
  if (response.result() == HttpStatus::ok)
    logger << "exception logged with id " << response.body() << Logger::endl;
  else 
    logger << "failed to log exception: " << response << Logger::endl;
  client.reset(nullptr);
}
