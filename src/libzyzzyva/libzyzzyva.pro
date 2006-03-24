#---------------------------------------------------------------------------
# zyzzyva.pro
#
# Build configuration file for Zyzzyva using qmake.
#
# Copyright 2005, 2006 Michael W Thelen <mike@pietdepsi.com>.
#
# This file is part of Zyzzyva.
#
# Zyzzyva is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Zyzzyva is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#---------------------------------------------------------------------------

TEMPLATE = lib
TARGET = zyzzyva
CONFIG += qt release thread warn_on assistant
QT += xml

ROOT = ../..
DESTDIR = $$ROOT/bin
MOC_DIR = build/moc
OBJECTS_DIR = build/obj
INCLUDEPATH += build/moc
DEPENDPATH += build/moc

# Source files
SOURCES = \
    AboutDialog.cpp \
    AnalyzeQuizDialog.cpp \
    Auxil.cpp \
    DefineForm.cpp \
    DefinitionBox.cpp \
    DefinitionDialog.cpp \
    HelpDialog.cpp \
    IntroForm.cpp \
    JudgeForm.cpp \
    JudgeDialog.cpp \
    LetterBag.cpp \
    LoadAnagramsThread.cpp \
    LoadDefinitionsThread.cpp \
    MainSettings.cpp \
    MainWindow.cpp \
    NewQuizDialog.cpp \
    QuizCanvas.cpp \
    QuizEngine.cpp \
    QuizForm.cpp \
    QuizProgress.cpp \
    QuizSpec.cpp \
    QuizTimerSpec.cpp \
    Rand.cpp \
    SearchForm.cpp \
    SearchCondition.cpp \
    SearchConditionForm.cpp \
    SearchSpec.cpp \
    SearchSpecForm.cpp \
    SettingsDialog.cpp \
    WordEngine.cpp \
    WordEntryDialog.cpp \
    WordGraph.cpp \
    WordListDialog.cpp \
    WordTableDelegate.cpp \
    WordTableModel.cpp \
    WordTableView.cpp \
    WordValidator.cpp \
    WordVariationDialog.cpp \
    ZPushButton.cpp

# Header files that must be run through moc
HEADERS = \
    AboutDialog.h \
    ActionForm.h \
    AnalyzeQuizDialog.h \
    DefineForm.h \
    DefinitionBox.h \
    DefinitionDialog.h \
    DefinitionLabel.h \
    HelpDialog.h \
    IntroForm.h \
    JudgeForm.h \
    JudgeDialog.h \
    LoadAnagramsThread.h \
    LoadDefinitionsThread.h \
    MainWindow.h \
    NewQuizDialog.h \
    QuizCanvas.h \
    QuizForm.h \
    QuizQuestionLabel.h \
    SearchForm.h \
    SearchConditionForm.h \
    SearchSpecForm.h \
    SettingsDialog.h \
    WordEngine.h \
    WordEntryDialog.h \
    WordLineEdit.h \
    WordListDialog.h \
    WordTableDelegate.h \
    WordTableModel.h \
    WordTableView.h \
    WordTextEdit.h \
    WordValidator.h \
    WordVariationDialog.h \
    ZPushButton.h