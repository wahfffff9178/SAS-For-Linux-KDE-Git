#include "passworddialog.h"

#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QIcon>
#include <QFile>    // 👈 新增
#include <QTimer>   // 👈 新增
#include <QPixmap>  // 👈 新增

PasswordDialog::PasswordDialog(QWidget *parent)
: QDialog(parent)
{
    setWindowTitle(QStringLiteral("更改密码"));
    setMinimumWidth(380);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // 1. 用户信息区域 (头像 + 用户名)
    auto *userLayout = new QVBoxLayout;
    userLayout->setAlignment(Qt::AlignHCenter);
    userLayout->setSpacing(10);

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(80, 80);

    QString facePath = QDir::homePath() + QStringLiteral("/.face");
    QPixmap avatar;
    if (QFile::exists(facePath)) {
        avatar.load(facePath);
    }
    if (avatar.isNull()) {
        avatar = QIcon::fromTheme(QStringLiteral("user-identity")).pixmap(80, 80);
    }
    m_avatarLabel->setPixmap(avatar.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    QString username = qgetenv("USER");
    if (username.isEmpty()) username = qgetenv("LOGNAME");
    m_userLabel = new QLabel(username, this);
    m_userLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));

    userLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    userLayout->addWidget(m_userLabel, 0, Qt::AlignHCenter);

    // 2. 输入框区域
    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(12);

    m_oldPwEdit = new QLineEdit(this);
    m_oldPwEdit->setEchoMode(QLineEdit::Password);
    m_oldPwEdit->setPlaceholderText(QStringLiteral("请输入当前密码"));

    m_newPwEdit = new QLineEdit(this);
    m_newPwEdit->setEchoMode(QLineEdit::Password);
    m_newPwEdit->setPlaceholderText(QStringLiteral("请输入新密码"));

    m_confirmPwEdit = new QLineEdit(this);
    m_confirmPwEdit->setEchoMode(QLineEdit::Password);
    m_confirmPwEdit->setPlaceholderText(QStringLiteral("请再次输入新密码"));

    formLayout->addRow(QStringLiteral("原密码:"), m_oldPwEdit);
    formLayout->addRow(QStringLiteral("新密码:"), m_newPwEdit);
    formLayout->addRow(QStringLiteral("确认密码:"), m_confirmPwEdit);

    // 3. 状态提示文本
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #ff5555;"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    // 4. 按钮区域
    auto *btnLayout = new QHBoxLayout;
    m_confirmBtn = new QPushButton(QStringLiteral("确认"), this);
    m_confirmBtn->setDefault(true);
    m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);

    btnLayout->addStretch();
    btnLayout->addWidget(m_confirmBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();

    // 5. 组装主布局
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(userLayout);
    mainLayout->addSpacing(15);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addSpacing(10);
    mainLayout->addLayout(btnLayout);

    // 6. 信号连接
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmBtn, &QPushButton::clicked, this, &PasswordDialog::onConfirmClicked);
    connect(m_confirmPwEdit, &QLineEdit::returnPressed, this, &PasswordDialog::onConfirmClicked);

    // 👈 修正：把实时校验放在构造函数里，而不是放到报错逻辑里
    auto validatePasswords = [this]() {
        if (m_newPwEdit->text() != m_confirmPwEdit->text() && !m_confirmPwEdit->text().isEmpty()) {
            m_statusLabel->setText(QStringLiteral("两次输入的新密码不一致！"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #ff5555;"));
            m_statusLabel->show();
        } else {
            m_statusLabel->hide();
        }
    };
    connect(m_newPwEdit, &QLineEdit::textChanged, this, validatePasswords);
    connect(m_confirmPwEdit, &QLineEdit::textChanged, this, validatePasswords);

    m_oldPwEdit->setFocus();
}

void PasswordDialog::onConfirmClicked()
{
    QString oldPw = m_oldPwEdit->text();
    QString newPw = m_newPwEdit->text();
    QString confirmPw = m_confirmPwEdit->text();

    if (oldPw.isEmpty() || newPw.isEmpty() || confirmPw.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("所有输入框均不能为空！"));
        m_statusLabel->show();
        return;
    }
    if (newPw != confirmPw) {
        m_statusLabel->setText(QStringLiteral("两次输入的新密码不一致！"));
        m_statusLabel->show();
        return;
    }

    m_statusLabel->hide();
    m_confirmBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
    m_oldPwEdit->setEnabled(false);
    m_newPwEdit->setEnabled(false);
    m_confirmPwEdit->setEnabled(false);

    m_oldPw = oldPw;
    m_newPw = newPw;
    m_outputBuffer.clear();
    m_step = 0;

    m_passwdProcess = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LANG", "C"); // 强制英文
    m_passwdProcess->setProcessEnvironment(env);
    m_passwdProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_passwdProcess, &QProcess::readyRead, this, &PasswordDialog::onProcessReadyRead);
    connect(m_passwdProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PasswordDialog::onProcessFinished);

    m_passwdProcess->start(QStringLiteral("passwd"));
    if (!m_passwdProcess->waitForStarted(1000)) {
        m_statusLabel->setText(QStringLiteral("无法启动 passwd 进程！"));
        m_statusLabel->show();
        onProcessFinished(1, QProcess::CrashExit);
    }
}

void PasswordDialog::onProcessReadyRead()
{
    // 👈 修正：使用缓冲区拼接，防止输出流被截断
    m_outputBuffer += QString::fromUtf8(m_passwdProcess->readAll());

    if (m_step == 0 && (m_outputBuffer.contains(QStringLiteral("Current password:")) ||
        m_outputBuffer.contains(QStringLiteral("UNIX password:")))) {
        m_passwdProcess->write(m_oldPw.toUtf8() + "\n");
    m_step = 1;
    m_outputBuffer.clear();
        } else if (m_step == 1 && m_outputBuffer.contains(QStringLiteral("New password:"))) {
            m_passwdProcess->write(m_newPw.toUtf8() + "\n");
            m_step = 2;
            m_outputBuffer.clear();
        } else if (m_step == 2 && m_outputBuffer.contains(QStringLiteral("Retype new password:"))) {
            m_passwdProcess->write(m_newPw.toUtf8() + "\n");
            m_step = 3;
            m_outputBuffer.clear();
        }
}

void PasswordDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #55ff55;"));
        m_statusLabel->setText(QStringLiteral("密码修改成功！"));
        m_statusLabel->show();
        QTimer::singleShot(1500, this, &QDialog::accept); // 1.5秒后自动关闭
    } else {
        QString errOutput = QString::fromUtf8(m_passwdProcess->readAll());
        if (errOutput.isEmpty() && m_outputBuffer.isEmpty()) {
            errOutput = QStringLiteral("密码修改失败，请检查原密码是否正确。");
        } else {
            errOutput = (errOutput + m_outputBuffer).trimmed(); // 整合错误信息
        }

        // 过滤掉 passwd 底层打印的多余换行
        errOutput.replace("\n", " ");

        m_statusLabel->setStyleSheet(QStringLiteral("color: #ff5555;"));
        m_statusLabel->setText(errOutput);
        m_statusLabel->show();

        m_confirmBtn->setEnabled(true);
        m_cancelBtn->setEnabled(true);
        m_oldPwEdit->setEnabled(true);
        m_newPwEdit->setEnabled(true);
        m_confirmPwEdit->setEnabled(true);
        m_oldPwEdit->clear();
        m_newPwEdit->clear();     // 失败后建议把新密码也清空，让用户重输
        m_confirmPwEdit->clear();
        m_oldPwEdit->setFocus();
    }
}
