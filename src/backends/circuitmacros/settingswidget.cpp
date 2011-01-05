/*
    Copyright (C) 2011  Matteo Agostinelli <agostinelli@gmail.com>

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

#include "settingswidget.h"

CircuitMacrosSettingsWidget::CircuitMacrosSettingsWidget(QWidget* parent, Qt::WindowFlags f): QWidget(parent, f)
{
    setupUi(this);
    setVisibleTemplateChooser(kcfg_PicInterpreter->currentIndex());
    connect(kcfg_PicInterpreter, SIGNAL(currentIndexChanged(int)), this, SLOT(setVisibleTemplateChooser(int)));
}

void CircuitMacrosSettingsWidget::setVisibleTemplateChooser(int index)
{
    kcfg_cmtemplateurl->setVisible(index != 1);
    lbl_cmtemplate->setVisible(index != 1);
      
    kcfg_cmtikztemplateurl->setVisible(index == 1);
    lbl_cmtikztemplate->setVisible(index == 1);
}
