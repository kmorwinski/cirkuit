/*
    Copyright (C) 2009  Matteo Agostinelli <agostinelli@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "documenttemplate.h"
#include <QTextStream>
#include <QFile>

using namespace Cirkuit;

DocumentTemplate::DocumentTemplate(const QString& path, QObject* parent): QObject(parent)
{
    m_path = path;
}

QString DocumentTemplate::insert(const QString& code, const QString& keyword)
{
    QFile file(m_path);
    file.open(QIODevice::ReadOnly);
    QTextStream stream(&file);
    QString output = stream.readAll().replace(keyword, code);
    file.close();
    return output;
}