// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "queue_client_test.hpp"

#include <chrono>
#include <thread>

namespace Azure { namespace Storage { namespace Test {

  TEST_F(QueueClientTest, EnqueueMessage)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    const std::string message = "message content.";
    auto res = queueClient.EnqueueMessage(message).Value;
    EXPECT_FALSE(res.MessageId.empty());
    EXPECT_TRUE(IsValidTime(res.InsertedOn));
    EXPECT_TRUE(IsValidTime(res.ExpiresOn - std::chrono::hours(24 * 7)));
    EXPECT_FALSE(res.PopReceipt.empty());
    EXPECT_TRUE(IsValidTime(res.NextVisibleOn));

    auto peekedMessage = queueClient.PeekMessages().Value.Messages[0];

    EXPECT_EQ(peekedMessage.Body, message);
    EXPECT_EQ(peekedMessage.MessageId, res.MessageId);
    EXPECT_EQ(peekedMessage.InsertedOn, res.InsertedOn);
    EXPECT_EQ(peekedMessage.ExpiresOn, res.ExpiresOn);

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, ReceiveMessage)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    EXPECT_TRUE(queueClient.ReceiveMessages().Value.Messages.empty());

    const std::string message = "message content.";
    auto res = queueClient.EnqueueMessage(message).Value;

    Queues::ReceiveMessagesOptions receiveOptions;
    receiveOptions.VisibilityTimeout = 1;
    auto receivedMessage = queueClient.ReceiveMessages(receiveOptions).Value.Messages[0];

    EXPECT_EQ(receivedMessage.Body, message);
    EXPECT_EQ(receivedMessage.MessageId, res.MessageId);
    EXPECT_EQ(receivedMessage.InsertedOn, res.InsertedOn);
    EXPECT_EQ(receivedMessage.ExpiresOn, res.ExpiresOn);
    EXPECT_FALSE(receivedMessage.PopReceipt.empty());
    EXPECT_TRUE(IsValidTime(receivedMessage.NextVisibleOn));
    EXPECT_EQ(receivedMessage.DequeueCount, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    receivedMessage = queueClient.ReceiveMessages().Value.Messages[0];
    EXPECT_EQ(receivedMessage.DequeueCount, 2);

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, ReceiveMessages)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    Queues::ReceiveMessagesOptions receiveOptions;
    receiveOptions.MaxMessages = 1;
    auto receivedMessages = queueClient.ReceiveMessages(receiveOptions).Value.Messages;
    EXPECT_TRUE(receivedMessages.empty());
    receivedMessages = queueClient.ReceiveMessages().Value.Messages;
    EXPECT_TRUE(receivedMessages.empty());

    const std::string message1 = "message content.1";
    const std::string message2 = "message content.2";
    const std::string message3 = "message content.3";
    const std::string message4 = "message content.4";
    queueClient.EnqueueMessage(message1);
    queueClient.EnqueueMessage(message2);
    queueClient.EnqueueMessage(message3);
    queueClient.EnqueueMessage(message4);

    receiveOptions.MaxMessages = 1;
    receivedMessages = queueClient.ReceiveMessages(receiveOptions).Value.Messages;

    EXPECT_EQ(receivedMessages.size(), static_cast<size_t>(receiveOptions.MaxMessages.Value()));
    EXPECT_EQ(receivedMessages[0].Body, message1);

    receivedMessages = queueClient.ReceiveMessages().Value.Messages;
    EXPECT_EQ(receivedMessages.size(), size_t(1));
    EXPECT_EQ(receivedMessages[0].Body, message2);

    receiveOptions.MaxMessages = 10;
    receivedMessages = queueClient.ReceiveMessages(receiveOptions).Value.Messages;
    EXPECT_EQ(receivedMessages.size(), size_t(2));
    EXPECT_EQ(receivedMessages[0].Body, message3);
    EXPECT_EQ(receivedMessages[1].Body, message4);

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, PeekMessage)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    EXPECT_TRUE(queueClient.PeekMessages().Value.Messages.empty());

    const std::string message = "message content.";
    auto res = queueClient.EnqueueMessage(message).Value;

    auto peekedMessage = queueClient.PeekMessages().Value.Messages[0];

    EXPECT_EQ(peekedMessage.Body, message);
    EXPECT_EQ(peekedMessage.MessageId, res.MessageId);
    EXPECT_EQ(peekedMessage.InsertedOn, res.InsertedOn);
    EXPECT_EQ(peekedMessage.ExpiresOn, res.ExpiresOn);
    EXPECT_EQ(peekedMessage.DequeueCount, 0);
    EXPECT_TRUE(peekedMessage.PopReceipt.empty());
    EXPECT_EQ(peekedMessage.NextVisibleOn, Azure::DateTime());

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, PeekMessages)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    Queues::PeekMessagesOptions peekOptions;
    peekOptions.MaxMessages = 1;
    auto peekedMessages = queueClient.PeekMessages(peekOptions).Value.Messages;
    EXPECT_TRUE(peekedMessages.empty());
    peekedMessages = queueClient.PeekMessages().Value.Messages;
    EXPECT_TRUE(peekedMessages.empty());

    const std::string message1 = "message content.1";
    const std::string message2 = "message content.2";
    const std::string message3 = "message content.3";
    const std::string message4 = "message content.4";
    queueClient.EnqueueMessage(message1);
    queueClient.EnqueueMessage(message2);
    queueClient.EnqueueMessage(message3);
    queueClient.EnqueueMessage(message4);

    peekOptions.MaxMessages = 1;
    peekedMessages = queueClient.PeekMessages(peekOptions).Value.Messages;

    EXPECT_EQ(peekedMessages.size(), static_cast<size_t>(peekOptions.MaxMessages.Value()));
    EXPECT_EQ(peekedMessages[0].Body, message1);

    peekedMessages = queueClient.PeekMessages().Value.Messages;
    EXPECT_EQ(peekedMessages.size(), size_t(1));
    EXPECT_EQ(peekedMessages[0].Body, message1);

    peekOptions.MaxMessages = 10;
    peekedMessages = queueClient.PeekMessages(peekOptions).Value.Messages;
    EXPECT_EQ(peekedMessages.size(), size_t(4));
    EXPECT_EQ(peekedMessages[0].Body, message1);
    EXPECT_EQ(peekedMessages[1].Body, message2);
    EXPECT_EQ(peekedMessages[2].Body, message3);
    EXPECT_EQ(peekedMessages[3].Body, message4);

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, UpdateMessage)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    const std::string message = "message content.";
    const std::string updatedMessage = "MESSAGE CONTENT2";
    auto res = queueClient.EnqueueMessage(message).Value;

    auto updateRes = queueClient.UpdateMessage(res.MessageId, res.PopReceipt, 0).Value;
    EXPECT_FALSE(updateRes.PopReceipt.empty());
    EXPECT_TRUE(IsValidTime(updateRes.NextVisibleOn));

    auto peekedMessage = queueClient.PeekMessages().Value.Messages[0];
    EXPECT_EQ(peekedMessage.Body, message);

    Queues::UpdateMessageOptions updateOptions;
    updateOptions.MessageText = updatedMessage;
    queueClient.UpdateMessage(res.MessageId, updateRes.PopReceipt, 1, updateOptions);
    EXPECT_TRUE(queueClient.PeekMessages().Value.Messages.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    peekedMessage = queueClient.PeekMessages().Value.Messages[0];
    EXPECT_EQ(peekedMessage.Body, updatedMessage);

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, DeleteMessage)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    const std::string message = "message content.";
    auto res = queueClient.EnqueueMessage(message).Value;

    EXPECT_NO_THROW(queueClient.DeleteMessage(res.MessageId, res.PopReceipt));
    EXPECT_TRUE(queueClient.PeekMessages().Value.Messages.empty());

    queueClient.Delete();
  }

  TEST_F(QueueClientTest, ClearMessages)
  {
    auto queueClient = Azure::Storage::Queues::QueueClient::CreateFromConnectionString(
        StandardStorageConnectionString(), LowercaseRandomString());
    queueClient.Create();

    const std::string message = "message content.";
    queueClient.EnqueueMessage(message);

    EXPECT_NO_THROW(queueClient.ClearMessages());
    EXPECT_TRUE(queueClient.PeekMessages().Value.Messages.empty());

    queueClient.Delete();
  }

}}} // namespace Azure::Storage::Test