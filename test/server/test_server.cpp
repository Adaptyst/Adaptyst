// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "mocks.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <mutex>
#include <gtest/gtest.h>

#ifndef SERVER_TEST_TIMEOUT
#define SERVER_TEST_TIMEOUT 60s
#endif

// Repeating each test multiple times increases the chance of
// detecting race-condition-related bugs if all other
// analyses failed to spot them beforehand
#ifndef SERVER_TEST_REPEAT
#define SERVER_TEST_REPEAT 100000
#endif

using namespace testing;
using namespace std::chrono_literals;

TEST(ServerTest, ZeroMaxConnections) {
  for (int i = 0; i < SERVER_TEST_REPEAT; i++) {
    unsigned int buf_size = 21348;
    unsigned long long file_timeout_speed = 687211;
    volatile bool interrupted = false;

    int created_connections = 0;
    int created_file_acceptors = 0;
    int created_clients = 0;
    adaptyst::Connection *last_connection = nullptr;
    adaptyst::Acceptor *last_file_acceptor = nullptr;

    test::MockAcceptor::Factory factory([=](test::MockAcceptor &acceptor) {
      EXPECT_CALL(acceptor, real_accept(buf_size)).Times(1);
      EXPECT_CALL(acceptor, close).Times(1);
    }, [&](test::MockConnection &connection) {
      created_connections++;
      last_connection = &connection;
      EXPECT_CALL(connection, write("try_again", true)).Times(0);
      EXPECT_CALL(connection, close).Times(1);
    }, false);

    std::unique_ptr<adaptyst::Acceptor> acceptor = factory.make_acceptor(UNLIMITED_ACCEPTED);

    std::unique_ptr<adaptyst::Client::Factory> client_factory =
      std::make_unique<test::MockClient::Factory>([&](test::MockClient &client) {
        created_clients++;
        EXPECT_CALL(client, construct(last_connection,
                                      last_file_acceptor,
                                      file_timeout_speed)).Times(1);
        client.set_interrupt_ptr(&interrupted);
        EXPECT_CALL(client, real_process(fs::current_path())).Times(1).WillOnce([&]() {
          interrupted = true;
        });
      }, true);

    std::unique_ptr<adaptyst::Acceptor::Factory> file_acceptor_factory =
      std::make_unique<test::MockAcceptor::Factory>([&](test::MockAcceptor &a) {
        created_file_acceptors++;
        last_file_acceptor = &a;
        EXPECT_CALL(a, construct(UNLIMITED_ACCEPTED)).Times(1);
        EXPECT_CALL(a, close).Times(1);
      }, [&](test::MockConnection &c) { }, true);

    // A separate scope is needed for ensuring the correct order
    // of destructor calls (gmock will seg fault otherwise).
    {
      adaptyst::Server server(acceptor, 0, buf_size, file_timeout_speed);
      auto async_future = std::async([&]() {
        server.run(client_factory, file_acceptor_factory);
      });
      ASSERT_EQ(async_future.wait_for(SERVER_TEST_TIMEOUT), std::future_status::ready);
    }

    ASSERT_EQ(created_clients, 1);
    ASSERT_EQ(created_connections, 1);
    ASSERT_EQ(created_file_acceptors, 1);
  }
}

TEST(ServerTest, TwoMaxConnections) {
  for (int i = 0; i < SERVER_TEST_REPEAT; i++) {
    unsigned int buf_size = 471;
    unsigned long long file_timeout_speed = 5758;
    volatile bool interrupted = false;

    int try_again_cnt = 0;
    int created_connections = 0;
    int created_file_acceptors = 0;
    int created_clients = 0;
    adaptyst::Connection *last_connection = nullptr;
    adaptyst::Acceptor *last_file_acceptor = nullptr;

    test::MockAcceptor::Factory factory([&](test::MockAcceptor &acceptor) {
        EXPECT_CALL(acceptor, real_accept(buf_size)).Times(AtLeast(5));
        EXPECT_CALL(acceptor, close).Times(1);
      }, [&](test::MockConnection &connection) {
        created_connections++;
        last_connection = &connection;
        EXPECT_CALL(connection, write("try_again", true)).Times(AtLeast(0))
          .WillRepeatedly(InvokeWithoutArgs([&]() {
            interrupted = true;
            try_again_cnt++;
          }));
        EXPECT_CALL(connection, close).Times(1);
      }, false);

    std::unique_ptr<adaptyst::Acceptor> acceptor = factory.make_acceptor(UNLIMITED_ACCEPTED);
    fs::path current_path = fs::current_path();

    // A separate scope is needed for ensuring the correct order
    // of destructor calls (gmock will seg fault otherwise).
    {
      adaptyst::Server server(acceptor, 2, buf_size, file_timeout_speed);

      std::unique_ptr<adaptyst::Client::Factory> client_factory =
        std::make_unique<test::MockClient::Factory>([&](test::MockClient &client) {
          created_clients++;

          EXPECT_CALL(client, construct(last_connection,
                                        last_file_acceptor,
                                        file_timeout_speed)).Times(1);
          client.set_interrupt_ptr(&interrupted);
          EXPECT_CALL(client, real_process(current_path)).Times(1);

          if (created_clients == 4) {
            server.interrupt();
            interrupted = true;
          } else {
            interrupted = false;
          }
        }, true);

      std::unique_ptr<adaptyst::Acceptor::Factory> file_acceptor_factory =
        std::make_unique<test::MockAcceptor::Factory>([&](test::MockAcceptor &a) {
          created_file_acceptors++;
          last_file_acceptor = &a;
          EXPECT_CALL(a, construct(UNLIMITED_ACCEPTED)).Times(1);
          EXPECT_CALL(a, close).Times(1);
        }, [&](test::MockConnection &c) { }, true);

      std::future<void> async_future = std::async([&]() {
        server.run(client_factory, file_acceptor_factory);
      });
      ASSERT_EQ(async_future.wait_for(SERVER_TEST_TIMEOUT), std::future_status::ready);
    }

    ASSERT_EQ(created_clients, 4);
    ASSERT_GT(created_connections, 4);
    ASSERT_EQ(created_file_acceptors, 4);
    ASSERT_EQ(try_again_cnt, created_connections - created_clients);
  }
}
