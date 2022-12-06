// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "Channel.h"
#include "EventLoop.h"
#include "Logging.h"

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 事件
 */
const int Channel::kNoneEvent = 0;                //没有事件：0
const int Channel::kReadEvent = POLLIN | POLLPRI; //可读事件：3
const int Channel::kWriteEvent = POLLOUT;         //可写事件：4

Channel::Channel(EventLoop *loop, int fd__)
    : loop_(loop), fd_(fd__), events_(0), revents_(0), index_(-1),
      logHup_(true), tied_(false), eventHandling_(false) {
  ;
}

Channel::~Channel() { assert(!eventHandling_); }

void Channel::tie(const boost::shared_ptr<void> &obj) {
  tie_ = obj;
  tied_ = true;
}

//将Chanel注册到Poller对象的polldfs数组中
void Channel::update() { loop_->updateChannel(this); }

// 调用这个函数之前确保调用disableAll
void Channel::remove() {
  assert(isNoneEvent());
  loop_->removeChannel(this);
}

//处理绑定的事件
void Channel::handleEvent(Timestamp receiveTime) {
  boost::shared_ptr<void> guard;
  if (tied_) {
    guard = tie_.lock(); //确定shared_ptr指针还存在
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

/**
 * poll函数的事件标志符值：
*    常量	                 说明
    POLLIN	        普通或优先级带数据可读
    POLLRDNORM    	普通数据可读
    POLLRDBAND    	优先级带数据可读
    POLLPRI	        高优先级数据可读
    POLLOUT	        普通数据可写
    POLLWRNORM	    普通数据可写
    POLLWRBAND	    优先级带数据可写
    POLLERR	        发生错误
    POLLHUP	        对方描述符挂起
    POLLNVAL	      描述字不是一个打开的文件
 *
 */
//开始选择性处理Channel对象绑定的事件
void Channel::handleEventWithGuard(Timestamp receiveTime) {
  eventHandling_ = true; //处于处理事件中

  // close
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
    if (logHup_) {
      LOG_WARN << "Channel::handle_event() POLLHUP";
    }
    //符合关闭套接字的事件，回调close函数
    if (closeCallback_)
      closeCallback_();
  }

  //Invalid polling request.
  if (revents_ & POLLNVAL) {
    LOG_WARN << "Channel::handle_event() POLLNVAL";
  }

  // error
  if (revents_ & (POLLERR | POLLNVAL)) {
    if (errorCallback_)
      errorCallback_();
  }

  // read
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
    if (readCallback_)
      readCallback_(receiveTime);
  }

  // write
  if (revents_ & POLLOUT) {
    if (writeCallback_)
      writeCallback_();
  }

  eventHandling_ = false; //事件已处理完成
}

//将Channel的事件信息保存到string中
string Channel::reventsToString() const {
  std::ostringstream oss;
  oss << fd_ << ": ";
  if (revents_ & POLLIN)
    oss << "IN ";
  if (revents_ & POLLPRI)
    oss << "PRI ";
  if (revents_ & POLLOUT)
    oss << "OUT ";
  if (revents_ & POLLHUP)
    oss << "HUP ";
  if (revents_ & POLLRDHUP)
    oss << "RDHUP ";
  if (revents_ & POLLERR)
    oss << "ERR ";
  if (revents_ & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
