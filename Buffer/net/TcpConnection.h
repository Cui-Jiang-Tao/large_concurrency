// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Mutex.h"
#include "StringPiece.h"
#include "Types.h"

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace muduo {
namespace net {

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection> {
public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop *loop, const string &name, int sockfd,
                const InetAddress &localAddr, const InetAddress &peerAddr);
  ~TcpConnection();

  EventLoop *getLoop() const { return loop_; }
  const string &name() const { return name_; }
  const InetAddress &localAddress() { return localAddr_; }
  const InetAddress &peerAddress() { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }

  // void send(string&& message); // C++11
  void send(const void *message, size_t len);
  void send(const StringPiece &message);
  // void send(Buffer&& message); // C++11
  void send(Buffer *message); // this one will swap data
  void shutdown();            // NOT thread safe, no simultaneous calling
  void setTcpNoDelay(bool on);

  void setContext(const boost::any &context) { context_ = context; }

  const boost::any &getContext() const { return context_; }

  boost::any *getMutableContext() { return &context_; }

  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }

  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                size_t highWaterMark) {
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
  }

  Buffer *inputBuffer() { return &inputBuffer_; }

  /// Internal use only.
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished(); // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed(); // should be called only once

private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(const StringPiece &message);
  void sendInLoop(const void *message, size_t len);
  void shutdownInLoop();
  void setState(StateE s) { state_ = s; }

  EventLoop *loop_; // ??????EventLoop
  string name_;     // ?????????
  StateE state_;    // FIXME: use atomic variable
  // we don't expose those classes to client.
  boost::scoped_ptr<Socket> socket_;
  boost::scoped_ptr<Channel> channel_;
  InetAddress localAddr_;
  InetAddress peerAddr_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  // ?????????????????????????????????????????????????????????????????????????????????????????????????????????
  // outputBuffer_????????????????????????????????????????????????????????????????????????
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_; // ????????????????????????
  CloseCallback closeCallback_;
  size_t highWaterMark_; // ????????????
  Buffer inputBuffer_;   // ????????????????????????
  Buffer outputBuffer_;  // ????????????????????????
  boost::any context_;   // ??????????????????????????????????????????
};

typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_TCPCONNECTION_H
