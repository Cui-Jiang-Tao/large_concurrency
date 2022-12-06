// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "Channel.h"
#include "Socket.h"

namespace muduo {
namespace net {

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : boost::noncopyable {
public:
  typedef boost::function<void(int sockfd, const InetAddress &)>
      NewConnectionCallback;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  void listen();

private:
  void handleRead();

  EventLoop *loop_;
  Socket acceptSocket_; // listening socket(即server socket)
  Channel acceptChannel_; // 用于观察acceptSocket_的readable事件，并回调Accptor::handleRead()
  NewConnectionCallback newConnectionCallback_;
  bool listenning_;
  int idleFd_;  // 一个空闲的文件描述符，用来处理 Too many open files 的情况
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_ACCEPTOR_H
