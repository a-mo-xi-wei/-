#ifndef SERVER_H_
#define SERVER_H_

#include "../common/common.h"

#include<QTcpServer>
#include<QTcpSocket>
#include<QHash>
#include<memory>

struct User
{
	using ptr = std::shared_ptr<User>;
	qreal id;
	QString username;
};

class ChatRoomServer : public QObject
{
public:
	ChatRoomServer(QObject* parent = nullptr);
	~ChatRoomServer();

	void init();

public: //slots:
	/** 有新的客户端连接上 */
	void onNewConnection();
	/** 有客户端发送消息了 */
	void onReadyRead();
	/** 有客户端退出了 */
	void onDisConnected();
private:
	QTcpServer m_server;
	QHash<QTcpSocket*, User::ptr> m_userHash;
};

#endif // !SERVER_H_
