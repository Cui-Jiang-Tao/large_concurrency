// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "PollPoller.h"

#include "Channel.h"
#include "Logging.h"
#include "Types.h"

#include <assert.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop *loop) : Poller(loop) {}

PollPoller::~PollPoller() {}

/**
 * poll机制与select机制类似，通过管理文件描述符来进行轮询，效率更高，并且处理的连接个数不受内核的限制。
 * 通过poll的方式进行轮询，通过Poller::fillActiveChannels得到活跃事件
 */
Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  // XXX pollfds_ shouldn't change
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);

  Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
  } else if (numEvents == 0) {
    LOG_TRACE << " nothing happended";
  } else {
    LOG_SYSERR << "PollPoller::poll()";
  }

  return now;
}

/**
 * struct pollfd {
    int fd;         	  	// 用于检测的文件描述符
    short events;         // 等待的事件类型
    short revents;        // 实际发生的事件类型
  } ;

  通过fd 获得发生事件的Chanel集合
**/
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList *activeChannels) const {
  for (PollFdList::const_iterator pfd = pollfds_.begin();
       pfd != pollfds_.end() && numEvents > 0; ++pfd) {
    if (pfd->revents > 0) {
      --numEvents;
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);
      assert(ch != channels_.end());

      Channel *channel = ch->second;
      assert(channel->fd() == pfd->fd);

      channel->set_revents(pfd->revents);
      // pfd->revents = 0;  //??? 暂不清楚意图
      activeChannels->push_back(channel);
    }
  }
}

/**
 * 主要时将Channel负责的文件描述符(fd)注册到Poller对象中, 或更新Poller对象
 */
void PollPoller::updateChannel(Channel *channel) {
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

  if (channel->index() < 0) {
    // index < 0说明是一个新的通道
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());

    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0; // channel->revents()
    pollfds_.push_back(pfd);

    int idx = static_cast<int>(pollfds_.size()) - 1;
    channel->set_index(idx);
    channels_[pfd.fd] = channel; //通过文件描述符(fd) 映射 Channel
  } else {
    // update existing one
    //已存在的Channel
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);

    // index的取值必定在channels_.size()内
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

    /**
     * 更新pollfds_容器的信息(struct pollfd)
     */
    struct pollfd &pfd = pollfds_[idx];

    // pfd.fd == -1 代表此Chanel不需被Poller::poll检测
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);

    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;

    //如果不再关注该Channel中文件描述符事件，则直接将该文件描述符赋值为其相反数减一。
    // 将一个通道暂时更改为不关注事件，但不从Poller中移除该通道
    if (channel->isNoneEvent()) {
      // ignore this pollfd
      // 暂时忽略该文件描述符的事件
      // 这里pfd.fd 可以直接设置为-1
      pfd.fd = -channel->fd() - 1; // 这样子设置是为了removeChannel优化
    }
  }
}

void PollPoller::removeChannel(Channel *channel) {
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();

  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());

  int idx = channel->index();

  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

  const struct pollfd &pfd = pollfds_[idx];
  (void)pfd;
  assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());

  size_t n = channels_.erase(channel->fd());
  assert(n == 1);
  (void)n;
  
  if (implicit_cast<size_t>(idx) == pollfds_.size() - 1) {
    pollfds_.pop_back();
  } else {
    // 这里移除的算法复杂度是O(1)，将待删除元素与最后一个元素交换再pop_back
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
    if (channelAtEnd < 0) {
      channelAtEnd = -channelAtEnd - 1;
    }
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}
