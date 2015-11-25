#include "SourcesAndOpenFiles.hxx"

#include <QSettings>
#include <QInputDialog>
#include <QMenu>
#include <QDebug>

SourcesAndOpenFiles::SourcesAndOpenFiles(QWidget* p_parent):
  QWidget(p_parent) {

  setupUi(this);

  // Settings
  QSettings settings("ValentinMicheletINC", "QtSourceBrowser");
  /// Uncomment below to reinit source directory
  //settings.setValue("SourceDirectory", "");

  // Root directory name
  QString sourceDirectory = settings.value("SourceDirectory").toString();
  while (sourceDirectory.isEmpty()) {
    sourceDirectory = QInputDialog::getText(this, "Source Directory", "Provide the source directory");
    if (sourceDirectory.endsWith("/")) {
      sourceDirectory.remove(sourceDirectory.size()-1, 1);
    }
    settings.setValue("SourceDirectory", sourceDirectory);
  }
  m_rootDirectoryName = settings.value("SourceDirectory").toString();

  // Search Line Edit
  connect(m_searchLineEdit, SIGNAL(textChanged(QString)), this, SLOT(searchFiles(QString)));

  // File System Model
  m_sourceModel = new SourceFileSystemModel(this);
  m_sourceRootIndex = m_sourceModel->setRootPath(m_rootDirectoryName);

  // Source Tree View
  m_sourcesTreeView->setModel(m_sourceModel);
  m_sourcesTreeView->setRootIndex(m_sourceRootIndex);
  m_sourcesTreeView->resizeColumnToContents(0);
  m_sourcesTreeView->setHeaderHidden(true);
  connect(m_sourcesTreeView, SIGNAL(doubleClicked(QModelIndex)), this, SIGNAL(openSourceCodeRequested(QModelIndex)));

  // Source Search Model
  m_sourceSearchModel = new OpenDocumentsModel(this);
  fillSourceModelFromSettingsDirectory(m_rootDirectoryName);

  // Source Search Proxy model
  m_proxyModel = new SourceFileSystemProxyModel(QModelIndex());
  m_proxyModel->setSourceModel(m_sourceSearchModel);

  // Source Search List View
  m_sourceSearchView->setModel(m_proxyModel);
  connect(m_sourceSearchView, SIGNAL(clicked(QModelIndex)), this, SIGNAL(openSourceCodeFromOpenDocumentsRequested(QModelIndex)));
  connect(m_sourceSearchView, SIGNAL(clicked(QModelIndex)), this, SLOT(expandTreeView(QModelIndex)));

  // Show only first column
  for (int k = 1; k < m_sourceModel->columnCount(); ++k)
    m_sourcesTreeView->hideColumn(k);

  //Open documents model
  m_openDocumentsModel = new OpenDocumentsModel;
  m_openDocumentsProxyModel = new QSortFilterProxyModel;
  m_openDocumentsProxyModel->setSourceModel(m_openDocumentsModel);
  connect(m_openDocumentsModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(sortOpenDocuments(QModelIndex,int,int)));

  // Open documents view
  m_openDocumentsView->setModel(m_openDocumentsProxyModel);
  connect(m_openDocumentsView, SIGNAL(clicked(QModelIndex)), this, SIGNAL(openSourceCodeFromOpenDocumentsRequested(QModelIndex)));

  // Splitter
  m_splitter->setStretchFactor(0, 2);
  m_splitter->setStretchFactor(1, 1);
}

void SourcesAndOpenFiles::openSourceCodeFromFileName(const QString& p_fileName) {
  m_searchLineEdit->setText("^"+p_fileName.toLower()+"(_p)?\\.");
  QMenu contextMenu(tr("Context menu"), this);
  contextMenu.setStyleSheet("QMenu { menu-scrollable: 1; }");

  for (int k = 0; k < m_sourceSearchView->model()->rowCount(); ++k) {
    QString sourceFileName = m_sourceSearchView->model()->index(k, 0).data().toString();
    QAction* action = new QAction(sourceFileName, this);
    connect(action, SIGNAL(triggered()), this, SLOT(openSourceCodeFromMenu()));
    contextMenu.addAction(action);
    if (sourceFileName.endsWith(".cpp")) {
      action->setIcon(QIcon("../QtSourceCodeBrowser/icons/cppFile.png"));
    } else if (sourceFileName.endsWith(".h")) {
      action->setIcon(QIcon("../QtSourceCodeBrowser/icons/hFile.png"));
    }
    m_actionSourcesMap.insert(action, m_sourceSearchView->model()->index(k, 0));
  }
  contextMenu.exec(cursor().pos());
  connect(&contextMenu, SIGNAL(destroyed(QObject*)), this, SLOT(destroyContextualMenu(QObject*)));
}

void SourcesAndOpenFiles::setCurrentIndex(QModelIndex const& p_index)
{
  m_openDocumentsView->setCurrentIndex(p_index);
}

QString SourcesAndOpenFiles::getCurrentOpenDocumentAbsolutePath() const
{
  return m_openDocumentsView->currentIndex().data(Qt::ToolTipRole).toString();
}

void SourcesAndOpenFiles::insertDocument(const QString& p_fileName, const QString& p_absoluteFilePath)
{
  m_openDocumentsModel->insertDocument(p_fileName, p_absoluteFilePath);
}

QModelIndex SourcesAndOpenFiles::getCurrentOpenDocumentIndex(const QString& p_fileName, const QString& p_absoluteFilePath)
{
  return m_openDocumentsProxyModel->mapFromSource(m_openDocumentsModel->indexFromFile(p_fileName, p_absoluteFilePath));
}

void SourcesAndOpenFiles::addOrRemoveStarToOpenDocument(bool p_add)
{
  QModelIndex currentOpenDocumentIndex = m_openDocumentsProxyModel->mapToSource(m_openDocumentsView->currentIndex());
  QString newCurrentOpenDocumentName = m_openDocumentsModel->data(currentOpenDocumentIndex).toString();
  if (p_add && !newCurrentOpenDocumentName.endsWith("*")) {
    newCurrentOpenDocumentName += "*";
  } else if (!p_add && newCurrentOpenDocumentName.endsWith("*")) {
    newCurrentOpenDocumentName.remove(newCurrentOpenDocumentName.size()-1, 1);
  }
  m_openDocumentsModel->setData(currentOpenDocumentIndex, newCurrentOpenDocumentName);
}

void SourcesAndOpenFiles::searchFiles(QString const& p_fileName) {
  if (p_fileName.isEmpty() && m_sourcesStackedWidget->currentWidget() != m_sourcesTreeView->parentWidget()) {
    m_sourcesStackedWidget->setCurrentWidget(m_sourcesTreeView->parentWidget());
  } else if (!p_fileName.isEmpty() && m_sourcesStackedWidget->currentWidget() != m_sourceSearchView->parentWidget()) {
    m_sourcesStackedWidget->setCurrentWidget(m_sourceSearchView->parentWidget());
  }

  m_proxyModel->setFilterRegExp(p_fileName);
}

void SourcesAndOpenFiles::sortOpenDocuments(QModelIndex, int, int) {
  m_openDocumentsProxyModel->sort(0);
}

void SourcesAndOpenFiles::destroyContextualMenu(QObject* p_object) {
  Q_UNUSED(p_object)
  qDeleteAll(m_actionSourcesMap.keys());
  m_actionSourcesMap.clear();
  m_searchLineEdit->clear();
}

void SourcesAndOpenFiles::openSourceCodeFromMenu() {
  QAction* actionSender = dynamic_cast<QAction*>(sender());
  Q_ASSERT(actionSender);
  emit openSourceCodeFromOpenDocumentsRequested(m_actionSourcesMap.value(actionSender));
}

void SourcesAndOpenFiles::expandTreeView(const QModelIndex& p_index)
{
  m_searchLineEdit->clear();
  QString absolutePath = p_index.data(Qt::ToolTipRole).toString();
  absolutePath.remove(m_rootDirectoryName);
  QStringList path = absolutePath.split("/", QString::SkipEmptyParts);
}

void SourcesAndOpenFiles::fillSourceModelFromSettingsDirectory(QString const& p_directoryName) {
  QDir directory(p_directoryName);
  QStringList subDirectories = directory.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
  QStringList sourceCodeFiles = directory.entryList(QStringList() << "*.cpp" << "*.h", QDir::Files);

  for (QString const& sourceCodeFile: sourceCodeFiles) {
    m_sourceSearchModel->insertDocument(sourceCodeFile, p_directoryName+QDir::separator()+sourceCodeFile);
  }

  for (QString const& subDirectory: subDirectories) {
    fillSourceModelFromSettingsDirectory(p_directoryName+QDir::separator()+subDirectory);
  }
}