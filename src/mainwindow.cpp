/***************************************************************************
*   Copyright (C) 2009 by Matteo Agostinelli                              *
*   agostinelli@gmail.com                                                 *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "mainwindow.h"
#include "livepreviewwidget.h"
#include "previewgenerator.h"
#include "cirkuitsettings.h"
#include "graphicsdocument.h"
#include "circuitmacrosmanager.h"
#include "ui_cirkuit_general_settings.h"

#include <KApplication>
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#include <KStandardAction>
#include <KStandardDirs>
#include <KRecentFilesAction>
#include <KFileDialog>
#include <KMessageBox>
#include <KIO/NetAccess>
#include <KSaveFile>
#include <KMenu>
#include <QTextStream>
#include <QFileInfo>
#include <QTimer>
#include <KXMLGUIFactory>
#include <KConfigDialog>
#include <KStatusBar>
#include <QProcess>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/Editor>
#include <KTextEditor/EditorChooser>
#include "graphicsbuilder.h"

class CirkuitGeneralForm : public QWidget, public Ui::CirkuitGeneralForm
{
	public:
		CirkuitGeneralForm(QWidget *parent = 0)
		: QWidget(parent)
		{
			setupUi(this);
		}
};

MainWindow::MainWindow(QWidget *)
{	
	KTextEditor::Editor *editor = KTextEditor::EditorChooser::editor();
	
	if (!editor) 
	{
		KMessageBox::error(this, i18n("A KDE text-editor component could not be found;\n"
		"please check your KDE installation."));
		kapp->exit(1);
	}
	
	m_doc = (GraphicsDocument*) (editor->createDocument(0));
	m_view = qobject_cast<KTextEditor::View*>(m_doc->createView(this));
	
	m_livePreviewWidget = new LivePreviewWidget(i18n("Live preview"), this);
	addDockWidget(Qt::TopDockWidgetArea, m_livePreviewWidget);
	
	mimeTypes << "application/x-cirkuit" << "text/x-tex";
	m_currentFile = KUrl("");
	updateTitle();
	
	setCentralWidget(m_view);
	setupActions();
	
	setXMLFile("cirkuitui.rc");
	createShellGUI(true);
	substituteSaveAsAction();
	
	guiFactory()->addClient(m_view);
	m_view->setContextMenu(qobject_cast<KMenu *> (factory()->container("ktexteditor_popup", this)));
	
	setGeometry(100,100,CirkuitSettings::width(),CirkuitSettings::height());

	m_generator = new PreviewGenerator;
	newCmDocument();
	
	m_updateTimer = new QTimer;
	m_updateTimer->setSingleShot(true);
	
	updateConfiguration();
	statusBar()->showMessage("Ready", 3000);
	
	connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(buildPreview()));
	connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(startBuildNotification()));
	connect(m_doc, SIGNAL(textChanged(KTextEditor::Document*)), m_updateTimer, SLOT(start()));
	connect(m_doc, SIGNAL(modifiedChanged(KTextEditor::Document*)), this, SLOT(documentModified(KTextEditor::Document*)));
	
	connect(m_generator, SIGNAL(applicationError(const QString&,const QString&)), this, SLOT(displayError(const QString&,const QString&)));
	
	checkCircuitMacros();
}

void MainWindow::setupActions()
{
	KStandardAction::quit(kapp, SLOT(quit()), actionCollection());
	KStandardAction::open(this, SLOT(openFile()), actionCollection());
	KStandardAction::save(this, SLOT(save()), actionCollection());
	KStandardAction::saveAs(this, SLOT(saveAs()), actionCollection());			
	KStandardAction::clear(this, SLOT(clear()), actionCollection());
	KStandardAction::preferences(this, SLOT(configure()),
										  actionCollection());
										  
	recentFilesAction = KStandardAction::openRecent(this, SLOT(loadFile( const KUrl& )),
																	actionCollection());
																	
	KAction* exportAction = new KAction(KIcon("document-export"), i18n("Export..."), 0);
	actionCollection()->addAction("export", exportAction);
	connect(exportAction, SIGNAL(triggered()), this, SLOT(exportFile()));
	
	KAction* newCmFileAction = new KAction(i18n("Circuit Macros file"), 0);
	actionCollection()->addAction("new_cmfile", newCmFileAction);
	connect(newCmFileAction, SIGNAL(triggered()), this, SLOT(newCmDocument()));
	
	KAction* newTikzFileAction = new KAction(i18n("TikZ file"), 0);
	actionCollection()->addAction("new_tikzfile", newTikzFileAction);
	connect(newTikzFileAction, SIGNAL(triggered()), this, SLOT(newTikzDocument()));
	
	KAction* buildPreviewAction = new KAction(i18n("Build preview"), 0);
	buildPreviewAction->setShortcut(Qt::ALT + Qt::Key_1);
	actionCollection()->addAction("build_preview", buildPreviewAction);
	connect(buildPreviewAction, SIGNAL(triggered()), this, SLOT(buildPreview()));
	
	KAction* showManualAction = new KAction(KIcon("help-contents"), i18n("Show Circuit Macros manual"),0);
	actionCollection()->addAction( "showManual", showManualAction );
	connect(showManualAction, SIGNAL(triggered()), this, SLOT(showManual()));
	
	KAction* showExamplesAction = new KAction(i18n("Show Circuit Macros examples"),0);
	actionCollection()->addAction( "showExamples", showExamplesAction );
	connect(showExamplesAction, SIGNAL(triggered()), this, SLOT(showExamples()));
	
	QAction* showLivePreviewAction = m_livePreviewWidget->toggleViewAction();
	actionCollection()->addAction( "show_live_preview", showLivePreviewAction );
	
	KConfig *config = CirkuitSettings::self()->config();
	recentFilesAction->loadEntries(config->group("recent_files"));
}

void MainWindow::substituteSaveAsAction()
{
	foreach (QAction* action, m_view->actionCollection()->actions())
	{
		if (action->text().contains("Save"))
		{
			m_view->actionCollection()->removeAction(action);
		}
	}
}

void MainWindow::clear()
{
	m_doc->clear();
}

void MainWindow::openFile()
{
	QString filename;
	
	KFileDialog openFileDialog(KUrl(), "", this);
	openFileDialog.setWindowTitle(i18n("Open file - Cirkuit"));
	openFileDialog.setOperationMode(KFileDialog::Opening);
	openFileDialog.setMimeFilter(mimeTypes);
	if (openFileDialog.exec() == QDialog::Accepted)
		filename = openFileDialog.selectedFile();
	
	if (!filename.isEmpty())
	{
		m_livePreviewWidget->clear();
		recentFilesAction->addUrl(KUrl(filename));
		loadFile(filename);
	}
}

void MainWindow::loadFile(const KUrl& url)
{
	m_currentFile = url;
	m_view->document()->openUrl(url);
	buildPreview();
	updateTitle();
}

void MainWindow::save()
{
	if (m_currentFile.isEmpty())
	{
		saveAs();
		return;
	}
	
	m_doc->save();
}

void MainWindow::saveAs()
{
	QString filename;
	
	KFileDialog saveFileDialog(KUrl(), "", this);
	saveFileDialog.setWindowTitle(i18n("Save file - Cirkuit"));
	saveFileDialog.setOperationMode(KFileDialog::Saving);
	saveFileDialog.setMimeFilter(mimeTypes, "application/x-cirkuit");
	if (saveFileDialog.exec() == QDialog::Accepted)
		filename = saveFileDialog.selectedFile();
	
	if (!filename.isEmpty())
	{
		if (QFile::exists(filename) && KMessageBox::questionYesNoCancel(0, i18n("Do you want to overwrite the existing file?"), i18n("File exists")) == KMessageBox::Yes)
		{
			recentFilesAction->addUrl(KUrl(filename));
			saveAsFile(filename);
		}
	}
}

void MainWindow::saveAsFile(const KUrl& url)
{
	m_doc->saveAs(url);
}

void MainWindow::exportFile()
{
	QString origDir = m_currentFile.directory();
	m_generator->build(m_doc, origDir);
	
	QString path = KFileDialog::getSaveFileName(KUrl(), "*.pdf|" + i18n("PDF files (*.pdf)"));
	
	if (path != 0)
	{
		if (QFile::exists(path) && KMessageBox::questionYesNoCancel(0, i18n("Do you want to overwrite the existing file?"), i18n("File exists")) == KMessageBox::Yes)
			QFile::remove(path);
		
		QFile::copy(m_generator->builder()->generatedPath(".pdf"), path);
	}
}

void MainWindow::documentModified(KTextEditor::Document* doc)
{
	setWindowModified(doc->isModified());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	CirkuitSettings::setHeight(height());
	CirkuitSettings::setWidth(width());
	
	KConfig *config = CirkuitSettings::self()->config();
	recentFilesAction->saveEntries(config->group("recent_files"));

	CirkuitSettings::self()->writeConfig();
	
	if (!m_doc->closeUrl())
		event->ignore();
	else
	{
		m_generator->clearTempFiles();
		event->accept();
	}
}

void MainWindow::buildPreview()
{
	QString origDir = m_currentFile.directory();
	m_generator->build(m_doc, origDir);
	m_livePreviewWidget->setImage(m_generator->preview());
	m_updateTimer->stop();
	statusBar()->showMessage("Preview built", 3000);
}

void MainWindow::startBuildNotification()
{
	statusBar()->showMessage("Building preview...", 1000);
}

void MainWindow::newCmDocument()
{
	m_doc->closeUrl();
	m_currentFile = "";
	m_doc->clear();
	m_doc->setText(".PS\ncct_init\n\n\n\n.PE");
	
	KTextEditor::Cursor cursor = m_view->cursorPosition();
	cursor.setLine(3);
	m_view->setCursorPosition(cursor);
	m_doc->setModified(false);
}

void MainWindow::newTikzDocument()
{
	m_doc->closeUrl();
	m_currentFile = "";
	m_doc->clear();
	m_doc->setText("\\begin{tikzpicture}\n\n\\end{tikzpicture}");
	
	KTextEditor::Cursor cursor = m_view->cursorPosition();
	cursor.setLine(1);
	m_view->setCursorPosition(cursor);
	m_doc->setModified(false);
}

void MainWindow::displayError(const QString& app, const QString& msg)
{
	KMessageBox::information(this, i18n("Error"), i18n("Error reported by %1: %2").arg(app).arg(msg));
}

void MainWindow::updateTitle()
{
	m_windowTitle = "Cirkuit";
	
	if (!m_currentFile.isEmpty())
		m_windowTitle += " - " + m_currentFile.fileName();
	
	m_windowTitle += "[*]";
	setWindowTitle(m_windowTitle);
}

void MainWindow::configure()
{
	if ( KConfigDialog::showDialog( "settings" ) )
		return; 
	
	KConfigDialog* dialog = new KConfigDialog(this, "settings", CirkuitSettings::self() );
	
	CirkuitGeneralForm* confWdg = new CirkuitGeneralForm();
	dialog->addPage( confWdg, i18n("General"), "general" ); 
	
	connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(updateConfiguration()));
	dialog->show();
}

void MainWindow::updateConfiguration()
{
	m_updateTimer->setInterval(CirkuitSettings::refreshInterval());
}

void MainWindow::showManual()
{
	QProcess* previewProc = new QProcess;
	
	QStringList args;
	args << KStandardDirs::locateLocal("data", "cirkuit/circuit_macros/doc/CMman.pdf");
	
	previewProc->start("okular", args);  
}

void MainWindow::showExamples()
{
	QProcess* previewProc = new QProcess;
	
	QStringList args;
	args << KStandardDirs::locateLocal("data", "cirkuit/circuit_macros/examples/examples.ps");
	
	previewProc->start("okular", args);  
}

void MainWindow::checkCircuitMacros()
{
	cmm = new CircuitMacrosManager;
	connect(cmm, SIGNAL(newVersionAvailable(const QString&)), this, SLOT(askIfUpgrade(const QString&)));
	connect(cmm, SIGNAL(configured()), this, SLOT(circuitMacrosConfigured()));
	if (cmm->checkExistence())
	{
		qDebug() << "Circuit macros found!";
		qDebug() << QString("version %1").arg(cmm->installedVersion()).toStdString().c_str();
		cmm->checkOnlineVersion();
	}
	
	else
	{
		qDebug() << "Circuit macros NOT found!!!!";
		if (KMessageBox::questionYesNo(this, i18n("Circuit Macros could not be found on your system. The application will not work if the macros are not installed. Do you want to proceed with the installation?"), i18n("Installation needed")) == KMessageBox::Yes)
		{
			cmm->downloadLatest();
			statusBar()->showMessage(i18n("Download Circuit Macros. Please wait..."));
		}
	}
}

void MainWindow::askIfUpgrade(const QString& version)
{
	if (KMessageBox::questionYesNo(this, i18n("A new version of Circuit Macros (version %1) is available. Do you want to upgrade?").arg(version), i18n("Upgrade")) == KMessageBox::Yes)
	{
		cmm->downloadLatest();
	}
}

void MainWindow::circuitMacrosConfigured()
{
	statusBar()->showMessage(i18n("Ready"), 5000);
}
