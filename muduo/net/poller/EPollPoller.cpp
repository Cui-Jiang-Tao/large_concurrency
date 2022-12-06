// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "EPollPoller.h"

#include "Channel.h"
#include "Logging.h"

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h> //包含：struct epoll_event;

using namespace muduo;
using namespace muduo::net;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;   
} // namespace

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  //活跃的事件数量
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);

  Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);

    //活跃的事件数量太多了，扩容，动态调整
    if (implicit_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_TRACE << " nothing happended";
  } else {
    // numEvents < 0 的情况
    LOG_SYSERR << "EPollPoller::poll()";
  }
  return now;
}

void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  //活跃的事件数量不可能比装所有事件的容器的元素数量多
  assert(implicit_cast<size_t>(numEvents) <= events_.size());

  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);

// NDEBUG模式
#ifndef NDEBUG
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif

    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

//将Channel加入epoll中管理，updateChannel方法一定是I/O线程调用的
void EPollPoller::updateChannel(Channel *channel) {
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

  const int index = channel->index();
  if (index == kNew || index == kDeleted) {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();

    if (index == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else // index == kDeleted
    {
      //也就是说，当Channel对象的无事件，可以通过index_来标识为删除状态，并不会从channels_容器中移除
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded);     //设置Channel对象为添加状态
    update(EPOLL_CTL_ADD, channel); //添加到epoll中监视
  } else {
    // update existing one with EPOLL_CTL_MOD/DEL
    //更新已存在的Channel描述符，修改或删除
    int fd = channel->fd();
    (void)fd;

    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);

    //当Channel对象不绑定事件，那么就从epoll的监视中移除，并设置Channel对象为删除状态
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else { //更新epoll中监视的Channel对象的信息
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

//将Channel对象从channels_容器和epoll监控中移除，此操作必须是I/O线程调用的
void EPollPoller::removeChannel(Channel *channel) {
  Poller::assertInLoopThread();

  int fd = channel->fd();
  LOG_TRACE << "fd = " << fd;

  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent()); //要移除的Channel对象本身一定是无事件的

  int index = channel->index();

  //很奇怪，要移除的Channel对象应该就只能是kDeleted的啊？？？
  assert(index == kAdded || index == kDeleted);

  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }

  //重新设置Channel为未被管理的初始状态
  channel->set_index(kNew);
}

//更新Channel对象绑定的事件
void EPollPoller::update(int operation, Channel *channel) {
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();
  event.data.ptr = channel;

  int fd = channel->fd();
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_SYSERR << "epoll_ctl op=" << operation << " fd=" << fd;
    } else {
      LOG_SYSFATAL << "epoll_ctl op=" << operation << " fd=" << fd;
    }
  }
}
