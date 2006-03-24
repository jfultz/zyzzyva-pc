//---------------------------------------------------------------------------
// ActionForm.h
//
// A base class for main action forms.
//
// Copyright 2005 Michael W Thelen <mike@pietdepsi.com>.
//
// This file is part of Zyzzyva.
//
// Zyzzyva is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Zyzzyva is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//---------------------------------------------------------------------------

#ifndef ZYZZYVA_ACTION_FORM_H
#define ZYZZYVA_ACTION_FORM_H

#include <QFrame>
#include <QString>

class ActionForm : public QFrame
{
    Q_OBJECT

    public:
    enum ActionFormType {
        UnknownFormType,
        QuizFormType,
        SearchFormType,
        DefineFormType,
        JudgeFormType,
        IntroFormType
    };

    public:
    ActionForm (ActionFormType t, QWidget* parent = 0, Qt::WFlags f = 0)
        : QFrame (parent, f), type (t) { }
    virtual ~ActionForm() { }
    virtual ActionFormType getType() const { return type; }
    virtual QString getStatusString() const { return QString::null; }

    signals:
    void statusChanged (const QString& status);

    private:
    ActionFormType type;
};

#endif // ZYZZYVA_ACTION_FORM_H