#ifndef  SENTRY_HPP
# define SENTRY_HPP

# include <crails/utils/backtrace.hpp>
# include <crails/datatree.hpp>
# include <crails/client.hpp>

namespace Crails
{
  class SentrySettings
  {
    SINGLETON(SentrySettings)
  protected:
    void key(const std::string&);
    void secret(const std::string&);
    void version(const std::string&);
    void project_id(const std::string&);
    void server_url(const std::string&);
    void server_protocol(const std::string&);
  };

  class Sentry
  {
    friend class SentrySettings;
    static const std::string sentry_key,
                             sentry_secret,
                             sentry_version,
                             project_id,
                             server_url,
                             server_protocol;
  public:
    Sentry();

    static void capture_exception(Data, const std::exception&);

  private:
    static void set_message_context(Data);
    static void set_message_request(Data message, Data params);
    static void initialize_exception_message(Data message, Data params, const std::exception&);
    static void initialize_backtrace(Data, const boost_ext::backtrace&);

    static void send_message(Data);
    static std::string sentry_auth_header();
    static std::string get_server_url();

    static Crails::ClientInterface& require_client();
    static thread_local std::unique_ptr<Crails::ClientInterface> client;
  };
}

#endif
