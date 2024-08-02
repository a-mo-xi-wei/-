#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include "../common/common.h"

#include<QWidget>
#include<QListWidgetItem>
#include<QTcpSocket>

class QFile;


namespace Ui { class MainWindow; }


class MainWindow : public QWidget
{
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();
	void initUi();
private slots:
    void on_msgSendBtn_clicked();

	void on_fileSendBtn_clicked();

    void on_friendListWidget_itemClicked(QListWidgetItem *item);

	void onErrorOccurred(QAbstractSocket::SocketError socketError);
	void onConnected();
	void onDisConnected();
	void onReadyRead();
private:
	Ui::MainWindow* ui{};
	QTcpSocket m_client;
	qreal m_userid;
	QString m_username;

	MessageType m_mtype{ MessageType::MType_Group };
	qreal m_chatUserID{};
	QString m_chatUsername{};

	bool m_isRecvFile{};	//是否正在接受文件
	quint64 m_totalSize;
	quint64 m_recvSize;
	QFile* m_file{};
};

#endif // !MAINWINDOW_H_
