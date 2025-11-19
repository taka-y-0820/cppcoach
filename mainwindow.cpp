#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFile>
#include <QDir>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("cppcoach");

    connect(ui->runButton, &QPushButton::clicked,
        this, &MainWindow::onRunClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onRunClicked() {
    QString code = ui->codeEdit->toPlainText();

    QString tempDir = QDir::tempPath();
    QString tempCpp = tempDir + "/temp.cpp";
    QString tempExe = tempDir + "/temp.exe";

    QFile file(tempCpp);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->outputResult->setPlainText("Error: Cannot write temp.cpp");
        return;
    }
    file.write(code.toUtf8());
    file.close();

    QProcess compile;
    compile.setWorkingDirectory(tempDir);

    compile.start(
        "C:/Qt/Tools/mingw1310_64/bin/g++.exe",
        { tempCpp, "-o", tempExe }
        );

    if (!compile.waitForFinished(8000)) {
        ui->outputResult->setPlainText("Compile timeout or failure.");
        return;
    }

    QString stderrCompile = compile.readAllStandardError();
    if (!stderrCompile.isEmpty()) {
        ui->outputResult->setPlainText("Compile Error:\n" + stderrCompile);
        return;
    }

    QProcess run;
    run.setWorkingDirectory(tempDir);

    run.start(tempExe);

    if (!run.waitForFinished(8000)) {
        ui->outputResult->setPlainText("Run timeout.");
        return;
    }

    QString output = run.readAllStandardOutput();
    QString errors = run.readAllStandardError();

    ui->outputResult->setPlainText("Output:\n" + output + errors);
}

