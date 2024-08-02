#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Util/SEventfilterObject.h"
#include "Util/SChatBubble.h"
#include <QInputDialog>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include <QDateTime>
#include <QStyledItemDelegate>

#include <QSettings>
#include <QFileDialog>
#include <QFile>

class NullItemDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter* painter,
		const QStyleOptionViewItem& option, const QModelIndex& index) const override {}

};

MainWindow::MainWindow(QWidget* parent)
	:QWidget(parent)
	,ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	initUi();
	setWindowFlag(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);
	auto efo = new SEventFilterObject(this);
	efo->setEnableDropShadow(true, false);
	installEventFilter(efo);

	QSettings settings("chatroom-config.ini", QSettings::IniFormat);
	auto host = QHostAddress(settings.value("host/host", "127.0.0.1").toString());
	qDebug() << host;
	m_client.connectToHost(host, 8888);
}

MainWindow::~MainWindow()
{
	m_client.close();
	delete ui;
}

void MainWindow::initUi()
{
	SChatBubble* b = new SChatBubble(QPixmap(), "hello wrold");
	ui->chatMsgListWidget->addItem(b);
	ui->chatMsgListWidget->setItemWidget(b, b);

	ui->chatMsgListWidget->setItemDelegate(new NullItemDelegate(this));
	ui->chatMsgSendEdit->setFocus();
	ui->msgSendBtn->setFocusPolicy(Qt::FocusPolicy::NoFocus);	//不让发送按钮接受焦点

	m_userid = QDateTime::currentDateTime().toMSecsSinceEpoch();

	connect(&m_client, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
	connect(&m_client, &QTcpSocket::connected, this, &MainWindow::onConnected);
	connect(&m_client, &QTcpSocket::disconnected, this, &MainWindow::onDisConnected);
	connect(&m_client, &QTcpSocket::errorOccurred, this, &MainWindow::onErrorOccurred);
}

/* 发送消息 */
void MainWindow::on_msgSendBtn_clicked()
{
	auto message = ui->chatMsgSendEdit->toPlainText();
	if (message.isEmpty())
		return;
	ui->chatMsgSendEdit->clear();

	SChatBubble* b = new SChatBubble(QPixmap(), message,SChatBubble::BubbleRight);
	ui->chatMsgListWidget->addItem(b);
	ui->chatMsgListWidget->setItemWidget(b, b);

	ui->chatMsgListWidget->scrollToBottom();	//把listwidget内容滚动到最下面

	//把消息发送给服务器
	QJsonObject jobj;
	jobj.insert("type", "message");
	jobj.insert("mtype", static_cast<int>(m_mtype));
	jobj.insert("content", message);
	if (m_mtype == MessageType::MType_Friend)
	{
		jobj.insert("to_userid", m_chatUserID);
		jobj.insert("to_username", m_chatUsername);
		jobj.insert("from_userid",m_userid);
		jobj.insert("from_username", m_username);
	}
	auto json = QJsonDocument(jobj).toJson(QJsonDocument::Compact);
	qDebug() << json;
	m_client.write(json);
}

void MainWindow::on_fileSendBtn_clicked()
{
	auto filename =  QFileDialog::getOpenFileName(this, "选择文件", "./", "Image (*.jpg;*.png;*.gif);;All (*.*)");
	if (filename.isEmpty())
		return;

	SChatBubble* b = new SChatBubble(QPixmap(), "发送文件[" + QFileInfo(filename).fileName() + "]",SChatBubble::BubbleRight);
	ui->chatMsgListWidget->addItem(b);
	ui->chatMsgListWidget->setItemWidget(b, b);

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
	{
		qWarning() << "文件打开失败" << file.errorString();
		return;
	}

	//发送头
	QJsonObject jobj;
	jobj.insert("type", "file_message");
	jobj.insert("filename", QFileInfo(filename).fileName());
	jobj.insert("filelength", file.size());

	auto json = QJsonDocument(jobj).toJson(QJsonDocument::Compact);
	auto size = json.size();
	m_client.write((const char*) & size,sizeof(qsizetype));
	m_client.write(json);

	
	//发送数据
	//m_client.write(file.readAll());
	while (!file.atEnd())
	{
		m_client.write(file.read(1024 * 1024 * 1024));
	}

}

/* 联系人选择改变 */
void MainWindow::on_friendListWidget_itemClicked(QListWidgetItem* item)
{
	qDebug() << item->text() << item->data(Qt::UserRole).toULongLong();
	m_mtype = MessageType::MType_Friend;
	m_chatUserID = ui->friendListWidget->currentItem()->data(Qt::UserRole).toDouble();
	m_chatUsername = ui->friendListWidget->currentItem()->text();

}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
	ui->systemBroadCatListWidget->addItem("服务器连接失败...");
}

void MainWindow::onConnected()
{
	//让用户输入用户名
	m_username = QInputDialog::getText(this, "欢迎使用", "请输入用户名");
	ui->curUserLab->setText(m_username);
	//登录
	QJsonObject obj;
	obj.insert("type", "login");
	obj.insert("username",m_username);
	obj.insert("id", (qreal)m_userid);

	m_client.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void MainWindow::onDisConnected()
{
}

void MainWindow::onReadyRead()
{
	auto data = m_client.readAll();

	QJsonParseError err;
	auto jdom = QJsonDocument::fromJson(data, &err);
	if (err.error != QJsonParseError::NoError)
	{
		qWarning() << "解析json数据失败" << err.errorString() << QString::fromUtf8(data);
		return;
	}

	//获取消息类型
	auto type = jdom["type"].toString();
	if (type == "sys_online")
	{
		ui->systemBroadCatListWidget->addItem(jdom["username"].toString() + " 上线了！");
		auto item = new QListWidgetItem(jdom["username"].toString());
		item->setData(Qt::UserRole, jdom["id"].toDouble());
		ui->friendListWidget->addItem(item);
	}
	else if (type == "user_list")
	{
		qDebug() << QString::fromUtf8(data);

		auto user_list = jdom["user_list"].toArray();
		for (size_t i = 0; i < user_list.size(); i++)
		{
			auto juser = user_list[i].toObject();
			auto item = new QListWidgetItem(juser.value("username").toString());
			item->setData(Qt::UserRole, juser.value("id").toDouble());
			ui->friendListWidget->addItem(item);
		}
	}
	else if (type == "sys_offline")
	{
		qDebug() << "client" << "client offline";
		//把下线的用户，从列表中移除掉
		ui->systemBroadCatListWidget->addItem(jdom["username"].toString() + " 下线了~");
		auto item_list = ui->friendListWidget->findItems(jdom["usernamee"].toString(), Qt::MatchFlag::MatchContains);
		for (auto item : item_list)
		{
			if (item->data(Qt::UserRole).toDouble() == jdom["id"].toDouble())
			{
				auto index = ui->friendListWidget->indexFromItem(item);
				delete ui->friendListWidget->takeItem(index.row());
				break;
			}
		}
	}
	else if (type == "message")
	{
		SChatBubble* b = new SChatBubble(QPixmap(), jdom["content"].toString());
		ui->chatMsgListWidget->addItem(b);
		ui->chatMsgListWidget->setItemWidget(b, b);

		if(static_cast<MessageType>( jdom["mtype"].toInt()) == MessageType::MType_Friend)
		{
			m_mtype = MessageType::MType_Friend;
			m_chatUserID = jdom["from_userid"].toDouble();
			m_chatUsername = jdom["from_username"].toString();
		}
	}
	else
	{
		//读取数据包长度	
		if (m_isRecvFile == false)
		{
			qsizetype size =  *(qsizetype*)data.constData();
			//判断后续的数据是否足够size这么多
			QByteArray jdata(data.constData() + sizeof(qsizetype), size);

			QJsonParseError err;
			auto jdom = QJsonDocument::fromJson(jdata, &err);
			if (err.error != QJsonParseError::NoError)
			{
				qWarning() << "解析json数据失败 2" << err.errorString() << QString::fromUtf8(data);
				return;
			}

			m_file = new QFile(jdom["filename"].toString(), this);
			if (!m_file->open(QIODevice::WriteOnly))
			{
				qWarning() << "准备接受文件，但是文件打开失败，无法保存" << m_file->errorString();
				return;
			}
			m_file->write(data.constData() + sizeof(qsizetype) + size,data.size() - sizeof(qsizetype) - size);

			m_totalSize = jdom["filelength"].toDouble();
			m_isRecvFile = true;



			SChatBubble* b = new SChatBubble(QPixmap(), "接受文件 [" + jdom["content"].toString() + "]中...");
			ui->chatMsgListWidget->addItem(b);
			ui->chatMsgListWidget->setItemWidget(b, b);
		}


	}

	if (m_isRecvFile)
	{

		//接受文件
		//如果已经接受完毕了
		if (m_recvSize >= m_totalSize)
		{
			SChatBubble* b = new SChatBubble(QPixmap(), "接受文件 [" + jdom["content"].toString() + "]完成");
			ui->chatMsgListWidget->addItem(b);
			ui->chatMsgListWidget->setItemWidget(b, b);

			m_file->close();
			m_file->deleteLater();
			m_file = nullptr;

			m_totalSize = 0;
			m_recvSize = 0;
			m_isRecvFile = false;
		}
		m_recvSize += data.size();
		m_file->write(data);
	}
}

