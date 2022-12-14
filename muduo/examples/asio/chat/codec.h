#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include "Buffer.h"
#include "Endian.h"
#include "Logging.h"
#include "TcpConnection.h"

class LengthHeaderCodec : boost::noncopyable {
public:
  typedef std::function<void(const muduo::net::TcpConnectionPtr &,
                             const muduo::string &message, muduo::Timestamp)>
      StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback &cb)
      : messageCallback_(cb) {}

  void onMessage(const muduo::net::TcpConnectionPtr &conn,
                 muduo::net::Buffer *buf, muduo::Timestamp receiveTime) {
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
      const void *data = buf->peek();
      int32_t be32 = *static_cast<const int32_t *>(data); // SIGBUS
      const int32_t len = muduo::net::sockets::networkToHost32(be32);
      if (len > 65536 || len < 0) {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown(); // FIXME: disable reading
        break;
      }
      // 代表一组消息已到达
      else if (buf->readableBytes() >= len + kHeaderLen) {
        buf->retrieve(kHeaderLen); //移动4字节
        muduo::string message(buf->peek(), len);
        messageCallback_(conn, message, receiveTime); //处理读取的消息
        buf->retrieve(len);
      } else {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection *conn,
            const muduo::StringPiece &message) {
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);
    buf.prepend(&be32, sizeof be32);
    conn->send(&buf);
  }

private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
