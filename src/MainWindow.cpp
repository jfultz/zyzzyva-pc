//---------------------------------------------------------------------------
// MainWindow.cpp
//
// The main window for the word study application.
//
// Copyright 2004 Michael W Thelen.  Permission to copy, use, modify, sell and
// distribute this software is granted provided this copyright notice appears in
// all copies. This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//---------------------------------------------------------------------------

#include "MainWindow.h"
#include "JudgeForm.h"
#include "SearchForm.h"
#include "SettingsDialog.h"
#include "WordEngine.h"
#include <qapplication.h>
#include <qdir.h>
#include <qfiledialog.h>
#include <qlayout.h>
#include <qmenubar.h>
#include <qmessagebox.h>
#include <qstatusbar.h>

const QString CHOOSER_TITLE = "Choose a Word List";
const QString IMPORT_COMPLETE_TITLE = "Import Complete";
const QString JUDGE_TAB_TITLE = "Judge";
const QString SEARCH_TAB_TITLE = "Search";

const QString SETTINGS_MAIN = "/Zyzzyva";
const QString SETTINGS_GEOMETRY = "/geometry";
const QString SETTINGS_GEOMETRY_X = "/x";
const QString SETTINGS_GEOMETRY_Y = "/y";
const QString SETTINGS_GEOMETRY_WIDTH = "/width";
const QString SETTINGS_GEOMETRY_HEIGHT = "/height";

//---------------------------------------------------------------------------
// MainWindow
//
//! Constructor.
//
//! @param parent the parent widget
//! @param name the name of this widget
//! @param f widget flags
//---------------------------------------------------------------------------
MainWindow::MainWindow (QWidget* parent, const char* name, WFlags f)
    : QMainWindow (parent, name, f), engine (new WordEngine()),
      settingsDialog (new SettingsDialog (this, "settingsDialog", true))
{
    QPopupMenu* filePopup = new QPopupMenu (this);
    Q_CHECK_PTR (filePopup);
    filePopup->insertItem ("&Import...", this, SLOT (import()), CTRL+Key_I);
    filePopup->insertSeparator();
    filePopup->insertItem ("&Quit", qApp, SLOT (quit()));
    menuBar()->insertItem ("&File", filePopup);

    QPopupMenu* editPopup = new QPopupMenu (this);
    Q_CHECK_PTR (editPopup);
    editPopup->insertItem ("&Preferences", this, SLOT (editSettings()));
    menuBar()->insertItem ("&Edit", editPopup);

    tabStack = new QTabWidget (this, "tabStack");
    Q_CHECK_PTR (tabStack);
    setCentralWidget (tabStack);

    judgeForm = new JudgeForm (engine, tabStack, "judgeForm");
    Q_CHECK_PTR (judgeForm);
    tabStack->addTab (judgeForm, JUDGE_TAB_TITLE);

    messageLabel = new QLabel (this, "messageLabel");
    Q_CHECK_PTR (messageLabel);
    statusBar()->addWidget (messageLabel, 2);

    searchForm = new SearchForm (engine, tabStack, "searchForm");
    Q_CHECK_PTR (searchForm);
    tabStack->addTab (searchForm, SEARCH_TAB_TITLE);

    statusLabel = new QLabel (this, "statusLabel");
    Q_CHECK_PTR (statusLabel);
    statusBar()->addWidget (statusLabel, 1);
    setNumWords (0);

    readSettings();
}

//---------------------------------------------------------------------------
// ~MainWindow
//
//! Destructor.  Save application settings.
//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    writeSettings();
}

//---------------------------------------------------------------------------
// import
//
//! Allow the user to import a word list from a file.
//---------------------------------------------------------------------------
void
MainWindow::import()
{
    QString file = QFileDialog::getOpenFileName (QDir::current().path(),
                                                 "All Files (*.*)", this,
                                                 "fileDialog",
                                                 CHOOSER_TITLE);
    if (file.isNull()) return;
    QApplication::setOverrideCursor (Qt::waitCursor);
    int imported = engine->importFile (file);
    QApplication::restoreOverrideCursor();

    setNumWords (imported);
    QMessageBox::information (this, IMPORT_COMPLETE_TITLE,
                              "Imported " + QString::number (imported)
                              + " words.",
                              QMessageBox::Ok);
}

//---------------------------------------------------------------------------
// editSettings
//
//! Allow the user to edit application settings.  If the user makes changes
//! and accepts the dialog, write the settings.  If the user rejects the
//! dialog, restore the settings after the dialog is closed.
//---------------------------------------------------------------------------
void
MainWindow::editSettings()
{
    settings.beginGroup (SETTINGS_MAIN);
    if (settingsDialog->exec() == QDialog::Accepted)
        settingsDialog->writeSettings (settings);
    else
        settingsDialog->readSettings (settings);
    settings.endGroup();
}

//---------------------------------------------------------------------------
// setNumWords
//
//! Update the label displaying the number of words loaded.
//
//! @param num the new number of words loaded
//---------------------------------------------------------------------------
void
MainWindow::setNumWords (int num)
{
    statusLabel->setText (QString::number (num) + " words loaded");
}

//---------------------------------------------------------------------------
// readSettings
//
//! Read application settings.
//---------------------------------------------------------------------------
void
MainWindow::readSettings()
{
    settings.beginGroup (SETTINGS_MAIN);
    settings.beginGroup (SETTINGS_GEOMETRY);
    int x = settings.readNumEntry (SETTINGS_GEOMETRY_X, 50);
    int y = settings.readNumEntry (SETTINGS_GEOMETRY_Y, 50);
    int w = settings.readNumEntry (SETTINGS_GEOMETRY_WIDTH, 640);
    int h = settings.readNumEntry (SETTINGS_GEOMETRY_HEIGHT, 480);
    settings.endGroup();
    settingsDialog->readSettings (settings);
    settings.endGroup();
    setGeometry (x, y, w, h);
}

//---------------------------------------------------------------------------
// writeSettings
//
//! Write application settings.
//---------------------------------------------------------------------------
void
MainWindow::writeSettings()
{
    settings.beginGroup (SETTINGS_MAIN);
    settings.beginGroup (SETTINGS_GEOMETRY);
    settings.writeEntry (SETTINGS_GEOMETRY_X, x());
    settings.writeEntry (SETTINGS_GEOMETRY_Y, y());
    settings.writeEntry (SETTINGS_GEOMETRY_WIDTH, width());
    settings.writeEntry (SETTINGS_GEOMETRY_HEIGHT, height());
    settings.endGroup();
    settingsDialog->writeSettings (settings);
    settings.endGroup();
}
