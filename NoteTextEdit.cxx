#include "NoteTextEdit.hxx"
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextCursor>
#include <QImage>
#include <QByteArray>
#include <QBuffer>
#include <QMouseEvent>
#include <QDebug>

#include <stdlib.h>


NoteTextEdit::NoteTextEdit(QWidget* p_parent):
  QTextBrowser(p_parent),
  m_currentCursor() {

  setMouseTracking(true);
  setOpenExternalLinks(true);
  setUndoRedoEnabled(true);

  connect(this, SIGNAL(textChanged()), this, SLOT(hasModificationsNotSaved()));
}

void NoteTextEdit::connectTextChanged() {
  connect(this, SIGNAL(textChanged()), this, SLOT(hasModificationsNotSaved()));
}

void NoteTextEdit::disconnectTextChanged() {
  disconnect(this, SIGNAL(textChanged()), this, SLOT(hasModificationsNotSaved()));
}

bool NoteTextEdit::canInsertFromMimeData(const QMimeData* p_source) const {
  return p_source->hasImage() || QTextEdit::canInsertFromMimeData(p_source);
}

void NoteTextEdit::insertFromMimeData(const QMimeData* p_source) {
  if (p_source->hasImage()) {
    QStringList formats = p_source->formats();
    QString format;
    for (int i=0; i<formats.size(); i++) {
      if (formats[i] == "image/bmp")  { format = "BMP";  break; }
      if (formats[i] == "image/jpeg") { format = "JPG";  break; }
      if (formats[i] == "image/jpg")  { format = "JPG";  break; }
      if (formats[i] == "image/gif")  { format = "GIF";  break; }
      if (formats[i] == "image/png")  { format = "PNG";  break; }
      if (formats[i] == "image/pbm")  { format = "PBM";  break; }
      if (formats[i] == "image/pgm")  { format = "PGM";  break; }
      if (formats[i] == "image/ppm")  { format = "PPM";  break; }
      if (formats[i] == "image/tiff") { format = "TIFF"; break; }
      if (formats[i] == "image/xbm")  { format = "XBM";  break; }
      if (formats[i] == "image/xpm")  { format = "XPM";  break; }
    }
    if (!format.isEmpty()) {
      dropImage(qvariant_cast<QImage>(p_source->imageData()), format);
      //dropImage(qvariant_cast<QImage>(p_source->imageData()), "JPG"); // Sorry, ale cokoli jiného dlouho trvá
      return;
    }
  }

  QTextEdit::insertFromMimeData(p_source);
}

QMimeData* NoteTextEdit::createMimeDataFromSelection() const {
  return QTextEdit::createMimeDataFromSelection();
}

void NoteTextEdit::dropImage(QImage const& p_image, QString const& p_format) {
  QByteArray bytes;
  QBuffer buffer(&bytes);

  buffer.open(QIODevice::WriteOnly);
  p_image.save(&buffer, p_format.toLocal8Bit().data());
  buffer.close();

  QByteArray base64 = bytes.toBase64();
  QByteArray base64l;

  for (int i = 0; i < base64.size(); ++i) {
    base64l.append(base64[i]);
    if (i%80 == 0) {
      base64l.append("\n");
    }
  }

  QTextCursor cursor = textCursor();
  QTextImageFormat imageFormat;
  imageFormat.setWidth(p_image.width());
  imageFormat.setHeight(p_image.height());
  imageFormat.setName(QString("data:image/%1;base64,%2").arg(QString("%1.%2").arg(rand()).arg(p_format)).arg(base64l.data()));
  cursor.insertImage(imageFormat);
}

void NoteTextEdit::mouseMoveEvent(QMouseEvent* p_event) {
  if (!isReadOnly()) {
    QTextBrowser::mouseMoveEvent(p_event);
    return;
  }

  QTextCursor cursor = cursorForPosition(p_event->pos());
  cursor.select(QTextCursor::WordUnderCursor);

  if (p_event->modifiers() == Qt::CTRL) {
    if (QRegExp("^Q.+").exactMatch(cursor.selectedText())) {
      m_currentCursor = cursor;
      emit transformToLinkRequested(m_currentCursor);
    } else {
      emit transformLinkBackRequested();
      m_currentCursor = QTextCursor();
    }
  }
  else {
    if (!m_currentCursor.isNull()) {
      emit transformLinkBackRequested();
      m_currentCursor = QTextCursor();
    }
  }

  QTextBrowser::mouseMoveEvent(p_event);
}

void NoteTextEdit::mousePressEvent(QMouseEvent* p_event) {
  if (p_event->modifiers() == Qt::CTRL) {
    emit transformLinkBackRequested();
    m_currentCursor = QTextCursor();
    emit contextMenuRequested(cursorForPosition(p_event->pos()));
  }

  QTextBrowser::mousePressEvent(p_event);
}

void NoteTextEdit::keyReleaseEvent(QKeyEvent* p_event) {
  if (p_event->key() == Qt::Key_Control) {
    emit transformLinkBackRequested();
    m_currentCursor = QTextCursor();
  }

  QTextBrowser::keyReleaseEvent(p_event);
}

void NoteTextEdit::mouseDoubleClickEvent(QMouseEvent* p_event) {
  emit editNotesRequested();

  QTextBrowser::mouseDoubleClickEvent(p_event);
}

void NoteTextEdit::hasModificationsNotSaved() {
  emit modificationsNotSaved(true);
}
