// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// cspell: words

#include "eventhubs_test_base.hpp"

#include <azure/core/context.hpp>
#include <azure/identity.hpp>
#include <azure/messaging/eventhubs.hpp>

#include <numeric>

#include <gtest/gtest.h>

// cspell: ignore edboptions

namespace Azure { namespace Messaging { namespace EventHubs { namespace Test {

  class ProducerClientTest : public EventHubsTestBaseParameterized {
  protected:
    std::string GetEventHubName()
    {
      if (GetParam() == AuthType::Emulator)
      {
        return "eh1";
      }
      return GetEnv("EVENTHUB_NAME");
    }
  };

  TEST_P(ProducerClientTest, ConnectionStringNoEntityPath)
  {
    if (GetParam() != AuthType::ConnectionString)
    {
      GTEST_SKIP();
    }
    std::string const connStringNoEntityPath = GetEnv("EVENTHUB_CONNECTION_STRING");
    std::string eventHubName{GetEnv("EVENTHUB_NAME")};

    Azure::Messaging::EventHubs::ProducerClient client{connStringNoEntityPath, eventHubName};
    EXPECT_EQ(eventHubName, client.GetEventHubName());
  }

  TEST_P(ProducerClientTest, ConnectionStringEntityPath)
  {
    if (GetParam() != AuthType::ConnectionString)
    {
      GTEST_SKIP();
    }
    std::string eventHubName{GetEnv("EVENTHUB_NAME")};
    std::string const connString = GetEnv("EVENTHUB_CONNECTION_STRING");

    Azure::Messaging::EventHubs::ProducerClient client{connString, eventHubName};
    EXPECT_EQ(eventHubName, client.GetEventHubName());
  }

  TEST_P(ProducerClientTest, SendMessage_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions producerOptions;
    producerOptions.Name = "sender-link";
    producerOptions.ApplicationID = "some";

    auto client{CreateProducerClient("", producerOptions)};

    auto message2{std::make_shared<Azure::Core::Amqp::Models::AmqpMessage>()};
    Azure::Messaging::EventHubs::Models::EventData message1;
    message2->SetBody(Azure::Core::Amqp::Models::AmqpValue("Hello7"));

    message1.Body = {'H', 'e', 'l', 'l', 'o', '2'};

    Azure::Messaging::EventHubs::Models::EventData message3;
    message3.Body = {'H', 'e', 'l', 'l', 'o', '3'};

    Azure::Messaging::EventHubs::EventDataBatchOptions edboptions;
    edboptions.MaxBytes = (std::numeric_limits<uint16_t>::max)();
    edboptions.PartitionId = "1";
    Azure::Messaging::EventHubs::EventDataBatch eventBatch{client->CreateBatch(edboptions)};

    Azure::Messaging::EventHubs::EventDataBatchOptions edboptions2;
    edboptions2.MaxBytes = (std::numeric_limits<uint16_t>::max)();
    ;
    edboptions2.PartitionId = "2";
    Azure::Messaging::EventHubs::EventDataBatch eventBatch2{client->CreateBatch(edboptions2)};

    EXPECT_TRUE(eventBatch.TryAdd(message1));
    EXPECT_TRUE(eventBatch.TryAdd(message2));

    EXPECT_TRUE(eventBatch2.TryAdd(message3));
    EXPECT_TRUE(eventBatch2.TryAdd(message2));

    for (int i = 0; i < 5; i++)
    {
      EXPECT_NO_THROW(client->Send(eventBatch));
    }
  }

  TEST_P(ProducerClientTest, EventHubRawMessageSend_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions producerOptions;
    producerOptions.Name = "sender-link";
    producerOptions.ApplicationID = "some";

    auto client{CreateProducerClient("", producerOptions)};

    client->Send(Azure::Messaging::EventHubs::Models::EventData{"This is a test message"});

    // Send using the implicit EventData constructor.
    client->Send(std::string{"String test message"});

    // Send using a vector of implicit EventData constructor with a binary buffer.
    client->Send({{12, 13, 14, 15}, {16, 17, 18, 19}});
  }

  TEST_P(ProducerClientTest, GetEventHubProperties_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions producerOptions;
    producerOptions.Name = "sender-link";
    producerOptions.ApplicationID = "some";

    auto client{CreateProducerClient("", producerOptions)};

    auto result = client->GetEventHubProperties();
    EXPECT_EQ(result.Name, GetEventHubName());
    EXPECT_TRUE(result.PartitionIds.size() > 0);
  }

  TEST_P(ProducerClientTest, GetPartitionProperties_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions producerOptions;
    producerOptions.Name = "sender-link";
    producerOptions.ApplicationID = "some";

    auto client{CreateProducerClient("", producerOptions)};

    ASSERT_NO_THROW([&]() {
      auto result = client->GetPartitionProperties("0");
      EXPECT_EQ(result.Name, GetEventHubName());
      EXPECT_EQ(result.PartitionId, "0");
    }());
  }

  TEST_P(ProducerClientTest, GetEventHubProperties_Multithreaded_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions options;
    options.ApplicationID = testing::UnitTest::GetInstance()->current_test_info()->name();

    options.Name = testing::UnitTest::GetInstance()->current_test_case()->name();
    auto client(CreateProducerClient("", options));

    std::string eventHubName{GetEventHubName()};

    std::vector<std::thread> threads;
    std::vector<size_t> iterationsPerThread;
    for (int i = 0; i < 20; i++)
    {
      threads.emplace_back([&client, eventHubName, &iterationsPerThread]() {
        size_t iterations = 0;
        std::chrono::system_clock::duration timeout = std::chrono::seconds(3);
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
        while ((std::chrono::system_clock::now() - start) <= timeout)
        {
          Azure::Messaging::EventHubs::Models::EventHubProperties result;
          ASSERT_NO_THROW(result = client->GetEventHubProperties());
          EXPECT_EQ(result.Name, eventHubName);
          EXPECT_TRUE(result.PartitionIds.size() > 0);
          std::this_thread::yield();
          iterations++;
        }
        iterationsPerThread.push_back(iterations);
      });
    }
    GTEST_LOG_(INFO) << "Waiting for threads to finish.";
    for (auto& t : threads)
    {
      if (t.joinable())
      {
        t.join();
      }
    }
    GTEST_LOG_(INFO) << "Threads finished.";
    for (const auto i : iterationsPerThread)
    {
      GTEST_LOG_(INFO) << "Thread iterations: " << i;
    }
  }

  TEST_P(ProducerClientTest, GetPartitionProperties_Multithreaded_LIVEONLY_)
  {
    Azure::Messaging::EventHubs::ProducerClientOptions options;
    options.ApplicationID = testing::UnitTest::GetInstance()->current_test_info()->name();

    options.Name = testing::UnitTest::GetInstance()->current_test_case()->name();
    auto client{CreateConsumerClient()};

    auto ehProperties = client->GetEventHubProperties();
    std::vector<std::thread> threads;
    std::vector<size_t> iterationsPerThread;
    for (const auto& partition : ehProperties.PartitionIds)
    {
      threads.emplace_back(std::thread([&client, partition, ehProperties, &iterationsPerThread]() {
        GTEST_LOG_(INFO) << "Thread started for partition: " << partition << ".\n";
        for (int i = 0; i < 20; i++)
        {
          std::vector<std::thread> partitionThreads;
          partitionThreads.emplace_back(
              [&client, &partition, ehProperties, &iterationsPerThread]() {
                size_t iterations = 0;
                std::chrono::system_clock::duration timeout = std::chrono::seconds(3);
                std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
                while ((std::chrono::system_clock::now() - start) <= timeout)
                {
                  Azure::Messaging::EventHubs::Models::EventHubPartitionProperties result;
                  ASSERT_NO_THROW(result = client->GetPartitionProperties(partition));
                  EXPECT_EQ(result.Name, ehProperties.Name);
                  EXPECT_EQ(result.PartitionId, partition);
                  std::this_thread::yield();
                  iterations++;
                }
                iterationsPerThread.push_back(iterations);
              });
          for (auto& t : partitionThreads)
          {
            if (t.joinable())
            {
              t.join();
            }
          }
        }
        GTEST_LOG_(INFO) << "Thread finished for partition: " << partition << ".\n";
      }));
    }
    GTEST_LOG_(INFO) << "Waiting for threads to finish.";
    for (auto& t : threads)
    {
      if (t.joinable())
      {
        t.join();
      }
    }
    GTEST_LOG_(INFO) << iterationsPerThread.size() << " threads finished.";
  }

  namespace {
    static std::string GetSuffix(const testing::TestParamInfo<AuthType>& info)
    {
      std::string stringValue = "";
      switch (info.param)
      {
        case AuthType::ConnectionString:
          stringValue = "ConnectionString_LIVEONLY_";
          break;
        case AuthType::Key:
          stringValue = "Key_LIVEONLY_";
          break;
        case AuthType::Emulator:
          stringValue = "Emulator";
          break;
      }
      return stringValue;
    }
  } // namespace

  INSTANTIATE_TEST_SUITE_P(
      EventHubs,
      ProducerClientTest,
      ::testing::Values(AuthType::Key, AuthType::ConnectionString /*, AuthType::Emulator*/),
      GetSuffix);
}}}} // namespace Azure::Messaging::EventHubs::Test
