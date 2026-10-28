#pragma once

#include <QDialog>
#include <QProcess>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget *parent = nullptr);

private slots:
    void onConfirmClicked();
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_userLabel = nullptr;
    QLineEdit *m_oldPwEdit = nullptr;
    QLineEdit *m_newPwEdit = nullptr;
    QLineEdit *m_confirmPwEdit = nullptr;
    QPushButton *m_confirmBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    QProcess *m_passwdProcess = nullptr;
    QString m_oldPw;
    QString m_newPw;
    QString m_outputBuffer; // 👈 新增：用于拼接输出流，防止被截断
    int m_step = 0;
};
