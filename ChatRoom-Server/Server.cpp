#include "Server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QJsonArray>

ChatRoomServer::ChatRoomServer(QObject* parent)
	:QObject(parent)
{
	init();
	m_server.listen(QHostAddress::AnyIPv4, 8888);
}

ChatRoomServer::~ChatRoomServer()
{
	m_server.close();
}

void ChatRoomServer::init()
{
	connect(&m_server, &QTcpServer::newConnection,this, &ChatRoomServer::onNewConnection);
}

void ChatRoomServer::onNewConnection()
{
	while (m_server.hasPendingConnections())
	{
		QTcpSocket* clientScoket =  m_server.nextPendingConnection();
		if (!clientScoket)
		{
			qWarning() << m_server.errorString();
			continue;
		}
		connect(clientScoket, &QTcpSocket::readyRead, this, &ChatRoomServer::onReadyRead);
		connect(clientScoket, &QTcpSocket::disconnected, this, &ChatRoomServer::onDisConnected);

	}
}

void ChatRoomServer::onReadyRead()
{
	auto socket = dynamic_cast<QTcpSocket*>(sender());
	auto data = socket->readAll();
	qDebug() <<"data: "<< QString(data);
	QJsonParseError err;
	auto jdoc =  QJsonDocument::fromJson(data,&err);
	if (err.error != QJsonParseError::NoError)
	{
		QJsonObject obj;
		obj.insert("type", "error");
		obj.insert("message", "你发送的jsond数据貌似有点点，毛病" + err.errorString());
		socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
		return;
	}

	//登录的时候保存你的信息
	if (jdoc["type"] == "login")
	{
		User::ptr user(new User);
		user->id = jdoc["id"].toDouble();
		user->username = jdoc["username"].toString();

		if(!m_userHash.isEmpty())
		{
			//给其他的所有用户，发送上线通知
			QJsonObject jobj;
			jobj.insert("type", "sys_online");
			jobj.insert("username", user->username);
			jobj.insert("id", user->id);
			auto data = QJsonDocument(jobj).toJson(QJsonDocument::Compact);
			
			for (auto it = m_userHash.begin(); it != m_userHash.end(); ++it)
			{
				it.key()->write(data);
			}

			//给当前登录的用户，发送所有的用户列表
			QJsonArray jArray;
			for (auto user : m_userHash)
			{
				QJsonObject jUser;
				jUser.insert("username", user->username);
				jUser.insert("id", user->id);
				jArray.append(jUser);
			}
			QJsonObject root;
			root.insert("user_list", jArray);
			root.insert("type", "user_list");
			socket->write(QJsonDocument(root).toJson(QJsonDocument::Compact));
		}


		qDebug() << "登录成功" << user->id << user->username;
		m_userHash.insert(socket, std::move(user));
	}
	else if (jdoc["type"] == "message")
	{
		auto mtype = static_cast<MessageType>(jdoc["mtype"].toInt());

		if (mtype == MessageType::MType_Group)
		{
			//转发消息
			for (auto it = m_userHash.cbegin(); it != m_userHash.cend(); ++it)
			{
				//除了当前发消息的用户之外的所有的用户
				if (it.key() != socket)
				{
					it.key()->write(data);
				}
			}
		}
		else 
		{
			auto username = jdoc["to_username"].toString();
			auto userid = jdoc["to_userid"].toDouble();

			qDebug() << username << userid;
			
			for (auto it = m_userHash.cbegin(); it != m_userHash.cend(); ++it)
			{
				auto user = it.value();
				if (user->id == userid)
				{
					it.key()->write(data);
					break;
				}
			}

		}
	}
	/*else if (jdoc["type"] == "file_message")
	{
		qDebug() << "file_message" << QString(data);
		//转发消息
		for (auto it = m_userHash.cbegin(); it != m_userHash.cend(); ++it)
		{
			//除了当前发消息的用户之外的所有的用户
			if (it.key() != socket)
			{
				it.key()->write(data);
			}
		}
	}
	*/
	else {
		//转发消息
		for (auto it = m_userHash.cbegin(); it != m_userHash.cend(); ++it)
		{
			//除了当前发消息的用户之外的所有的用户
			if (it.key() != socket)
			{
				it.key()->write(data);
			}
		}

	}


}

void ChatRoomServer::onDisConnected()
{
	qDebug() << "server " << "client offline";
	//如果有用户下线了，那么通知其他的所有用户
	auto socket = dynamic_cast<QTcpSocket*>(sender());
	QJsonObject jobj;
	jobj.insert("type", "sys_offline");
	jobj.insert("username", m_userHash.value(socket)->username);
	jobj.insert("id", m_userHash.value(socket)->id);
	//从hash表中删除掉
	m_userHash.remove(socket);

	//发给其他的用户
	for (auto it = m_userHash.begin(); it != m_userHash.end(); ++it)
	{
		it.key()->write(QJsonDocument(jobj).toJson(QJsonDocument::Compact));
	}
}
