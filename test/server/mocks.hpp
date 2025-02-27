// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef MOCKS_HPP_
#define MOCKS_HPP_

#include "server.hpp"
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::StrictMock;
namespace fs = std::filesystem;

namespace test {
  inline void assert_file_equals(fs::path path, std::string content,
                                 bool is_json) {
    std::ifstream is(path);

    if (!is) {
      FAIL();
    }

    std::string s = "";

    while (is) {
      char buf[1024];
      is.get(buf, 1024);

      s += std::string(buf);
    }

    if (is_json) {
      ASSERT_EQ(nlohmann::json::parse(content), nlohmann::json::parse(s));
    } else {
      ASSERT_EQ(content, s);
    }
  }

  class MockNotifiable : public adaptyst::Notifiable {
  public:
    MOCK_METHOD(void, notify, (), (override));
  };

  class MockSubclient : public adaptyst::Subclient {
  private:
    adaptyst::Notifiable &context;

  protected:
    MockSubclient(adaptyst::Notifiable &context) : context(context) {}

  public:
    class Factory : public adaptyst::Subclient::Factory {
    private:
      std::function<void(MockSubclient &)> init;
      bool call_constructor;

    public:
      Factory(std::function<void(MockSubclient &)> init,
              bool call_constructor) {
        this->init = init;
        this->call_constructor = call_constructor;
      }

      std::unique_ptr<Subclient> make_subclient(adaptyst::Notifiable &context,
                                                std::string profiled_filename,
                                                unsigned int buf_size) {
        std::unique_ptr<StrictMock<MockSubclient> > subclient(new StrictMock<MockSubclient>(context));
        this->init(*subclient);

        if (call_constructor) {
          subclient->construct(context, profiled_filename, buf_size);
        }

        return subclient;
      }

      std::string get_type() {
        return "mock";
      }
    };

    MOCK_METHOD(void, construct, (adaptyst::Notifiable &,
                                  std::string,
                                  unsigned int));
    MOCK_METHOD(void, real_process, ());
    MOCK_METHOD(nlohmann::json &, get_result, (), (override));
    MOCK_METHOD(std::string, get_connection_instructions, (), (override));

    void process() {
      this->context.notify();
      this->real_process();
    }
  };

  class MockClient : public adaptyst::Client {
  private:
    volatile bool *interrupted = nullptr;

  protected:
    MockClient() { }

  public:
    class Factory : public adaptyst::Client::Factory {
    private:
      std::function<void(MockClient &)> init;
      bool call_constructor;

    public:
      Factory(std::function<void(MockClient &)> init,
              bool call_constructor) {
        this->init = init;
        this->call_constructor = call_constructor;
      }

      std::unique_ptr<Client> make_client(std::unique_ptr<adaptyst::Connection> &connection,
                                          std::unique_ptr<adaptyst::Acceptor> &file_acceptor,
                                          unsigned long long file_timeout_speed) {
        std::unique_ptr<StrictMock<MockClient> > client(new StrictMock<MockClient>());
        this->init(*client);

        if (call_constructor) {
          client->construct(connection.get(),
                            file_acceptor.get(),
                            file_timeout_speed);
        }

        return client;
      }
    };

    MOCK_METHOD(void, construct, (adaptyst::Connection *,
                                  adaptyst::Acceptor *,
                                  unsigned long long));
    MOCK_METHOD(void, real_process, (fs::path));
    MOCK_METHOD(void, notify, (), (override));

    void set_interrupt_ptr(volatile bool *interrupted) {
      this->interrupted = interrupted;
    }

    void process(fs::path working_dir) {
      this->real_process(working_dir);

      if (this->interrupted) {
        while (!(*this->interrupted)) { }
      }
    }
  };

  class MockConnection : public adaptyst::Connection {
  public:
    ~MockConnection() { this->close(); }

    MOCK_METHOD(unsigned int, get_buf_size, (), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(int, read, (char *, unsigned int, long), (override));
    MOCK_METHOD(std::string, read, (long), (override));
    MOCK_METHOD(void, write, (std::string, bool), (override));
    MOCK_METHOD(void, write, (fs::path), (override));
  };

  class MockAcceptor : public adaptyst::Acceptor {
  private:
    std::function<void(MockConnection &)> connection_init;

  protected:
    MockAcceptor(std::function<void(MockConnection &)> connection_init,
                 int max_accepted) : Acceptor(max_accepted) {
      this->connection_init = connection_init;
    }

    std::unique_ptr<adaptyst::Connection> accept_connection(unsigned int buf_size) {
      this->real_accept(buf_size);

      std::unique_ptr<MockConnection> connection = std::make_unique<MockConnection>();
      this->connection_init(*connection);
      return connection;
    }

  public:
    class Factory : public adaptyst::Acceptor::Factory {
    private:
      std::function<void(MockAcceptor &)> acceptor_init;
      std::function<void(MockConnection &)> connection_init;
      bool call_constructor;

    public:
      Factory(std::function<void(MockAcceptor &)> acceptor_init,
              std::function<void(MockConnection &)> connection_init,
              bool call_constructor) {
        this->acceptor_init = acceptor_init;
        this->connection_init = connection_init;
        this->call_constructor = call_constructor;
      }

      std::unique_ptr<Acceptor> make_acceptor(int max_accepted) {
        std::unique_ptr<StrictMock<MockAcceptor> > acceptor(new StrictMock<MockAcceptor>(connection_init,
                                                                                         max_accepted));
        this->acceptor_init(*acceptor);

        if (this->call_constructor) {
          acceptor->construct(max_accepted);
        }

        return acceptor;
      }

      std::string get_type() {
        return "mock";
      }
    };

    ~MockAcceptor() { this->close(); }

    std::string get_type() {
      return "mock";
    }

    MOCK_METHOD(void, construct, (int));
    MOCK_METHOD(std::unique_ptr<adaptyst::Connection>, real_accept, (unsigned int));
    MOCK_METHOD(std::string, get_connection_instructions, (), (override));
    MOCK_METHOD(void, close, (), (override));
  };
};

#endif
