/*
** Copyright (C) 2013 Jiří Procházka (Hobrasoft)
** Contact: http://www.hobrasoft.cz/
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file is under the terms of the GNU Lesser General Public License
** version 2.1 as published by the Free Software Foundation and appearing
** in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the
** GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
*/

#include "NoteRichTextEdit.hxx"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFontDatabase>
#include <QInputDialog>
#include <QColorDialog>
#include <QTextList>
#include <QtDebug>
#include <QFileDialog>
#include <QImageReader>
#include <QSettings>
#include <QBuffer>
#include <QUrl>
#include <QPlainTextEdit>
#include <QMenu>
#include <QDialog>
#include <QTextDocumentFragment>

NoteRichTextEdit::NoteRichTextEdit(QWidget* p_parent):
  QWidget(p_parent),
  m_previousCursor(),
  m_previousHtmlSelection() {

  setupUi(this);
  m_lastBlockList = 0;

  connect(f_textedit, SIGNAL(currentCharFormatChanged(QTextCharFormat)), this, SLOT(slotCurrentCharFormatChanged(QTextCharFormat)));
  connect(f_textedit, SIGNAL(cursorPositionChanged()), this, SLOT(slotCursorPositionChanged()));
  connect(f_textedit, SIGNAL(modificationsNotSaved(bool)), this, SLOT(emitModificationsNotSaved(bool)));
  connect(f_textedit, SIGNAL(editNotesRequested()), this, SLOT(editOn()));

  m_fontsize_h1 = 18;
  m_fontsize_h2 = 16;
  m_fontsize_h3 = 14;
  m_fontsize_h4 = 12;

  fontChanged(f_textedit->font());
  bgColorChanged(f_textedit->textColor());

  // Edit button
  f_edit_button->setFixedSize(27, 27);
  f_edit_button->setIcon(QIcon("../QtSourceCodeBrowser/icons/edit.png"));
  connect(f_edit_button, SIGNAL(clicked()), this, SLOT(editOn()));

  // Save button
  f_save->setIcon(QIcon("../QtSourceCodeBrowser/icons/save.png"));
  connect(f_save, SIGNAL(clicked()), this, SLOT(saveDraft()));

  // paragraph formatting
  m_paragraphItems
    << tr("Standard")
    << tr("Heading 1")
    << tr("Heading 2")
    << tr("Heading 3")
    << tr("Heading 4")
    << tr("Monospace");
  f_paragraph->addItems(m_paragraphItems);
  connect(f_paragraph, SIGNAL(activated(int)), this, SLOT(textStyle(int)));

  // undo & redo
  f_undo->setShortcut(QKeySequence::Undo);
  f_undo->setIcon(QIcon("../QtSourceCodeBrowser/icons/undo.png"));
  f_redo->setShortcut(QKeySequence::Redo);
  f_redo->setIcon(QIcon("../QtSourceCodeBrowser/icons/redo.png"));

  connect(f_textedit->document(), SIGNAL(undoAvailable(bool)), f_undo, SLOT(setEnabled(bool)));
  connect(f_textedit->document(), SIGNAL(redoAvailable(bool)), f_redo, SLOT(setEnabled(bool)));

  f_undo->setEnabled(f_textedit->document()->isUndoAvailable());
  f_redo->setEnabled(f_textedit->document()->isRedoAvailable());

  connect(f_undo, SIGNAL(clicked()), f_textedit, SLOT(undo()));
  connect(f_redo, SIGNAL(clicked()), f_textedit, SLOT(redo()));

  // link
  f_link->setShortcut(Qt::CTRL + Qt::Key_L);
  f_link->setIcon(QIcon("../QtSourceCodeBrowser/icons/link.png"));
  connect(f_link, SIGNAL(clicked(bool)), this, SLOT(textLink(bool)));

  // bold, italic & underline
  f_bold->setShortcut(Qt::CTRL + Qt::Key_B);
  f_bold->setIcon(QIcon("../QtSourceCodeBrowser/icons/bold.png"));
  f_italic->setShortcut(Qt::CTRL + Qt::Key_I);
  f_italic->setIcon(QIcon("../QtSourceCodeBrowser/icons/italic.png"));
  f_underline->setShortcut(Qt::CTRL + Qt::Key_U);
  f_underline->setIcon(QIcon("../QtSourceCodeBrowser/icons/underline.png"));
  f_strikeout->setIcon(QIcon("../QtSourceCodeBrowser/icons/strike.png"));

  connect(f_bold, SIGNAL(clicked()), this, SLOT(textBold()));
  connect(f_italic, SIGNAL(clicked()), this, SLOT(textItalic()));
  connect(f_underline, SIGNAL(clicked()), this, SLOT(textUnderline()));
  connect(f_strikeout, SIGNAL(clicked()), this, SLOT(textStrikeout()));

  // lists
  f_list_bullet->setShortcut(Qt::CTRL + Qt::Key_Minus);
  f_list_bullet->setIcon(QIcon("../QtSourceCodeBrowser/icons/bulletList.png"));
  f_list_ordered->setShortcut(Qt::CTRL + Qt::Key_Equal);
  f_list_ordered->setIcon(QIcon("../QtSourceCodeBrowser/icons/orderedList.png"));

  connect(f_list_bullet, SIGNAL(clicked(bool)), this, SLOT(listBullet(bool)));
  connect(f_list_ordered, SIGNAL(clicked(bool)), this, SLOT(listOrdered(bool)));

  // indentation
  f_textedit->setTabChangesFocus(false);
  f_textedit->document()->setIndentWidth(20);
  f_indent_dec->setShortcut(Qt::SHIFT + Qt::Key_Tab);
  f_indent_inc->setShortcut(Qt::CTRL + Qt::Key_Tab);

  connect(f_indent_inc, SIGNAL(clicked()), this, SLOT(increaseIndentation()));
  f_indent_inc->setIcon(QIcon("../QtSourceCodeBrowser/icons/indent-increase.png"));
  connect(f_indent_dec, SIGNAL(clicked()), this, SLOT(decreaseIndentation()));
  f_indent_dec->setIcon(QIcon("../QtSourceCodeBrowser/icons/indent-decrease.png"));

  // font size
  QFontDatabase db;
  for (int size: db.standardSizes()) {
    f_fontsize->addItem(QString::number(size));
  }
  f_fontsize->setFixedWidth(50);

  connect(f_fontsize, SIGNAL(activated(QString)), this, SLOT(textSize(QString)));
  f_fontsize->setCurrentIndex(f_fontsize->findText(QString::number(QApplication::font().pointSize())));

  // text foreground color
  QPixmap pix(16, 16);
  f_fgcolor->setFixedHeight(27);
  pix.fill(QApplication::palette().foreground().color());
  f_fgcolor->setIcon(pix);

  connect(f_fgcolor, SIGNAL(clicked()), this, SLOT(textFgColor()));

  // text background color
  f_bgcolor->setFixedHeight(27);
  pix.fill(QApplication::palette().background().color());
  f_bgcolor->setIcon(pix);

  connect(f_bgcolor, SIGNAL(clicked()), this, SLOT(textBgColor()));

  // images
  //f_image->setIcon(QIcon("../QtSourceCodeBrowser/icons/image.png"));
  //connect(f_image, SIGNAL(clicked()), this, SLOT(insertImage()));

  // code
  f_code->setIcon(QIcon("../QtSourceCodeBrowser/icons/code.png"));
  f_code->setShortcut(Qt::CTRL + Qt::Key_K);
  connect(f_code, SIGNAL(clicked(bool)), this, SLOT(insertCode(bool)));

  // Qt class links
  connect(f_textedit, SIGNAL(transformToLinkRequested(QTextCursor)), this, SLOT(transformTextToInternalLink(QTextCursor)));
  connect(f_textedit, SIGNAL(transformLinkBackRequested()), this, SLOT(transformInternalLinkBack()));
  connect(f_textedit, SIGNAL(contextMenuRequested(QTextCursor)), this, SLOT(requesContextualMenuFromCursorPosition(QTextCursor)));

  // Hide tool bar
  f_toolbar->hide();

  // Mouse tracking
  setMouseTracking(true);
}

/// PUBLIC

QString NoteRichTextEdit::toPlainText() const {
  return f_textedit->toPlainText();
}

QString NoteRichTextEdit::toHtml() const {
  QString s = f_textedit->toHtml();
  // convert emails to links
  s = s.replace(QRegExp("(<[^a][^>]+>(?:<span[^>]+>)?|\\s)([a-zA-Z\\d]+@[a-zA-Z\\d]+\\.[a-zA-Z]+)"), "\\1<a href=\"mailto:\\2\">\\2</a>");
  // convert links
  s = s.replace(QRegExp("(<[^a][^>]+>(?:<span[^>]+>)?|\\s)((?:https?|ftp|file)://[^\\s'\"<>]+)"), "\\1<a href=\"\\2\">\\2</a>");
  // see also: Utils::linkify()
  return s;
}

QTextDocument* NoteRichTextEdit::document() const {
  return f_textedit->document();
}

QTextCursor NoteRichTextEdit::textCursor() const {
  return f_textedit->textCursor();
}

void NoteRichTextEdit::setTextCursor(const QTextCursor& p_cursor) {
  f_textedit->setTextCursor(p_cursor);
}

void NoteRichTextEdit::editOn() {
  f_textedit->setReadOnly(false);
  f_textedit->setUndoRedoEnabled(true);
  f_edit_button->hide();
  f_toolbar->show();
  f_textedit->viewport()->setCursor(Qt::IBeamCursor);
  f_textedit->setStyleSheet("background-image: url(\"../QtSourceCodeBrowser/images/draft.png\");");
}

void NoteRichTextEdit::editOff() {
  f_textedit->setReadOnly(true);
  f_textedit->setUndoRedoEnabled(false);
  f_edit_button->show();
  f_toolbar->hide();
  f_textedit->viewport()->setCursor(Qt::ArrowCursor);
  f_textedit->setStyleSheet("background-image: none;");
}

void NoteRichTextEdit::textSource() {
  QDialog* dialog = new QDialog(this);
  QPlainTextEdit* pte = new QPlainTextEdit(dialog);
  pte->setPlainText(f_textedit->toHtml());
  QGridLayout* gl = new QGridLayout(dialog);
  gl->addWidget(pte,0,0,1,1);
  dialog->setWindowTitle(tr("Document source"));
  dialog->setMinimumWidth (400);
  dialog->setMinimumHeight(600);
  dialog->exec();

  f_textedit->setHtml(pte->toPlainText());

  delete dialog;
}

void NoteRichTextEdit::setPlainText(const QString& p_text) {
  f_textedit->setPlainText(p_text);
}

void NoteRichTextEdit::setHtml(const QString& p_text) {
  f_textedit->setHtml(p_text);
}

void NoteRichTextEdit::textBold() {
  QTextCharFormat fmt;
  fmt.setFontWeight(f_bold->isChecked() ? QFont::Bold : QFont::Normal);
  mergeFormatOnWordOrSelection(fmt);
}

void NoteRichTextEdit::focusInEvent(QFocusEvent* p_event) {
  Q_UNUSED(p_event);

  f_textedit->setFocus(Qt::TabFocusReason);
}

void NoteRichTextEdit::mouseMoveEvent(QMouseEvent* p_event)
{
  if (p_event->modifiers() == Qt::CTRL) {
    transformInternalLinkBack();
  }
}

void NoteRichTextEdit::textUnderline() {
  QTextCharFormat fmt;
  fmt.setFontUnderline(f_underline->isChecked());
  mergeFormatOnWordOrSelection(fmt);
}

void NoteRichTextEdit::textItalic() {
  QTextCharFormat fmt;
  fmt.setFontItalic(f_italic->isChecked());
  mergeFormatOnWordOrSelection(fmt);
}

void NoteRichTextEdit::textStrikeout() {
  QTextCharFormat fmt;
  fmt.setFontStrikeOut(f_strikeout->isChecked());
  mergeFormatOnWordOrSelection(fmt);
}

void NoteRichTextEdit::textSize(const QString &p_point) {
  qreal pointSize = p_point.toFloat();
  if (pointSize > 0) {
    QTextCharFormat fmt;
    fmt.setFontPointSize(pointSize);
    mergeFormatOnWordOrSelection(fmt);
  }
}

void NoteRichTextEdit::textLink(bool p_checked) {
  bool unlink = false;
  QTextCharFormat fmt;
  if (p_checked) {
    QString url = f_textedit->currentCharFormat().anchorHref();
    bool ok;
    QString newUrl = QInputDialog::getText(this, tr("Create a link"), tr("Link URL:"), QLineEdit::Normal, url, &ok);
    if (ok) {
      fmt.setAnchor(true);
      fmt.setAnchorHref(newUrl);
      fmt.setForeground(QApplication::palette().color(QPalette::Link));
      fmt.setFontUnderline(true);
    } else {
      unlink = true;
    }
  } else {
    unlink = true;
  }
  if (unlink) {
    fmt.setAnchor(false);
    fmt.setForeground(QApplication::palette().color(QPalette::Text));
    fmt.setFontUnderline(false);
  }
  mergeFormatOnWordOrSelection(fmt);
}

void NoteRichTextEdit::textStyle(int p_index) {
  QTextCursor cursor = f_textedit->textCursor();
  cursor.beginEditBlock();

  // standard
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::BlockUnderCursor);
  }
  QTextCharFormat fmt;
  cursor.setCharFormat(fmt);
  f_textedit->setCurrentCharFormat(fmt);

  if (p_index == ParagraphHeading1 || p_index == ParagraphHeading2
   || p_index == ParagraphHeading3 || p_index == ParagraphHeading4) {
    if (p_index == ParagraphHeading1) {
      fmt.setFontPointSize(m_fontsize_h1);
    }
    if (p_index == ParagraphHeading2) {
      fmt.setFontPointSize(m_fontsize_h2);
    }
    if (p_index == ParagraphHeading3) {
      fmt.setFontPointSize(m_fontsize_h3);
    }
    if (p_index == ParagraphHeading4) {
      fmt.setFontPointSize(m_fontsize_h4);
    }
    fmt.setFontWeight(QFont::Bold);
  }
  if (p_index == ParagraphMonospace) {
    fmt = cursor.charFormat();
    fmt.setFontFamily("Monospace");
    fmt.setFontStyleHint(QFont::Monospace);
    fmt.setFontFixedPitch(true);
  }
  cursor.setCharFormat(fmt);
  f_textedit->setCurrentCharFormat(fmt);

  cursor.endEditBlock();
}

void NoteRichTextEdit::textFgColor() {
  QColor col = QColorDialog::getColor(f_textedit->textColor(), this);
  QTextCursor cursor = f_textedit->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }
  QTextCharFormat fmt = cursor.charFormat();
  if (col.isValid()) {
    fmt.setForeground(col);
  } else {
    fmt.clearForeground();
  }
  cursor.setCharFormat(fmt);
  f_textedit->setCurrentCharFormat(fmt);
  fgColorChanged(col);
}

void NoteRichTextEdit::textBgColor() {
  QColor col = QColorDialog::getColor(f_textedit->textBackgroundColor(), this);
  QTextCursor cursor = f_textedit->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }
  QTextCharFormat fmt = cursor.charFormat();
  if (col.isValid()) {
    fmt.setBackground(col);
  } else {
    fmt.clearBackground();
  }
  cursor.setCharFormat(fmt);
  f_textedit->setCurrentCharFormat(fmt);
  bgColorChanged(col);
}

void NoteRichTextEdit::listBullet(bool p_checked) {
  if (p_checked) {
    f_list_ordered->setChecked(false);
  }
  list(p_checked, QTextListFormat::ListDisc);
}

void NoteRichTextEdit::listOrdered(bool p_checked) {
  if (p_checked) {
    f_list_bullet->setChecked(false);
  }
  list(p_checked, QTextListFormat::ListDecimal);
}

void NoteRichTextEdit::list(bool p_checked, QTextListFormat::Style p_style) {
  QTextCursor cursor = f_textedit->textCursor();
  cursor.beginEditBlock();
  if (!p_checked) {
    QTextBlockFormat obfmt = cursor.blockFormat();
    QTextBlockFormat bfmt;
    bfmt.setIndent(obfmt.indent());
    cursor.setBlockFormat(bfmt);
  } else {
    QTextListFormat listFmt;
    if (cursor.currentList()) {
      listFmt = cursor.currentList()->format();
    }
    listFmt.setStyle(p_style);
    cursor.createList(listFmt);
  }
  cursor.endEditBlock();
}

void NoteRichTextEdit::mergeFormatOnWordOrSelection(const QTextCharFormat &p_format) {
  QTextCursor cursor = f_textedit->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }
  cursor.mergeCharFormat(p_format);
  f_textedit->mergeCurrentCharFormat(p_format);
  f_textedit->setFocus(Qt::TabFocusReason);
}

void NoteRichTextEdit::slotCursorPositionChanged() {
  QTextList* l = f_textedit->textCursor().currentList();
  if (m_lastBlockList && (l == m_lastBlockList || (l != 0 && m_lastBlockList != 0 && l->format().style() == m_lastBlockList->format().style()))) {
    return;
  }

  m_lastBlockList = l;
  if (l) {
    QTextListFormat lfmt = l->format();
    if (lfmt.style() == QTextListFormat::ListDisc) {
      f_list_bullet->setChecked(true);
      f_list_ordered->setChecked(false);
    } else if (lfmt.style() == QTextListFormat::ListDecimal) {
      f_list_bullet->setChecked(false);
      f_list_ordered->setChecked(true);
    } else {
      f_list_bullet->setChecked(false);
      f_list_ordered->setChecked(false);
    }
  } else {
    f_list_bullet->setChecked(false);
    f_list_ordered->setChecked(false);
  }
}

void NoteRichTextEdit::fontChanged(const QFont &p_font) {
  f_fontsize->setCurrentIndex(f_fontsize->findText(QString::number(p_font.pointSize())));
  f_bold->setChecked(p_font.bold());
  f_italic->setChecked(p_font.italic());
  f_underline->setChecked(p_font.underline());
  f_strikeout->setChecked(p_font.strikeOut());
  if (p_font.pointSize() == m_fontsize_h1) {
    f_paragraph->setCurrentIndex(ParagraphHeading1);
  } else if (p_font.pointSize() == m_fontsize_h2) {
    f_paragraph->setCurrentIndex(ParagraphHeading2);
  } else if (p_font.pointSize() == m_fontsize_h3) {
    f_paragraph->setCurrentIndex(ParagraphHeading3);
  } else if (p_font.pointSize() == m_fontsize_h4) {
    f_paragraph->setCurrentIndex(ParagraphHeading4);
  } else {
    if (p_font.fixedPitch() && p_font.family() == "Monospace") {
      f_paragraph->setCurrentIndex(ParagraphMonospace);
    } else {
      f_paragraph->setCurrentIndex(ParagraphStandard);
    }
  }
  if (f_textedit->textCursor().currentList()) {
    QTextListFormat lfmt = f_textedit->textCursor().currentList()->format();
    if (lfmt.style() == QTextListFormat::ListDisc) {
      f_list_bullet->setChecked(true);
      f_list_ordered->setChecked(false);
    } else if (lfmt.style() == QTextListFormat::ListDecimal) {
      f_list_bullet->setChecked(false);
      f_list_ordered->setChecked(true);
    } else {
      f_list_bullet->setChecked(false);
      f_list_ordered->setChecked(false);
    }
  } else {
    f_list_bullet->setChecked(false);
    f_list_ordered->setChecked(false);
  }
}

void NoteRichTextEdit::fgColorChanged(const QColor &p_color) {
  QPixmap pix(16, 16);
  if (p_color.isValid()) {
    pix.fill(p_color);
  } else {
    pix.fill(QApplication::palette().foreground().color());
  }
  f_fgcolor->setIcon(pix);
}

void NoteRichTextEdit::bgColorChanged(const QColor &p_color) {
  QPixmap pix(16, 16);
  if (p_color.isValid()) {
    pix.fill(p_color);
  } else {
    pix.fill(QApplication::palette().background().color());
  }
  f_bgcolor->setIcon(pix);
}

void NoteRichTextEdit::slotCurrentCharFormatChanged(const QTextCharFormat &p_format) {
  fontChanged(p_format.font());
  bgColorChanged((p_format.background().isOpaque()) ? p_format.background().color() : QColor());
  fgColorChanged((p_format.foreground().isOpaque()) ? p_format.foreground().color() : QColor());
  f_link->setChecked(p_format.isAnchor());

  f_code->setChecked(p_format.fontFamily() == "mono");
}

void NoteRichTextEdit::increaseIndentation() {
  indent(+1);
}

void NoteRichTextEdit::decreaseIndentation() {
  indent(-1);
}

void NoteRichTextEdit::indent(int p_delta) {
  QTextCursor cursor = f_textedit->textCursor();
  cursor.beginEditBlock();
  QTextBlockFormat bfmt = cursor.blockFormat();
  int ind = bfmt.indent();
  if (ind + p_delta >= 0) {
    bfmt.setIndent(ind + p_delta);
  }
  cursor.setBlockFormat(bfmt);
  cursor.endEditBlock();
}

void NoteRichTextEdit::openNotes(const QString& p_text) {
  f_textedit->disconnectTextChanged();

  if (p_text.isEmpty()) {
    setPlainText(p_text);
    return;
  }
  if (p_text[0] == '<') {
    setHtml(p_text);
  } else {
    setPlainText(p_text);
  }

  f_textedit->connectTextChanged();
}

void NoteRichTextEdit::insertImage() {
  QSettings s;
  QString attdir = s.value("general/filedialog-path").toString();
  QString file = QFileDialog::getOpenFileName(this, tr("Select an image"), attdir, tr("JPEG (*.jpg);; GIF (*.gif);; PNG (*.png);; BMP (*.bmp);; All (*)"));
  QImage image = QImageReader(file).read();

  f_textedit->dropImage(image, QFileInfo(file).suffix().toUpper().toLocal8Bit().data());
}

void NoteRichTextEdit::insertCode(bool checked) {
  QTextCursor cursor = f_textedit->textCursor();
  cursor.beginEditBlock();

  if (!checked) {
    cursor.select(QTextCursor::BlockUnderCursor);
    QString selectedText = cursor.selection().toPlainText();
    cursor.removeSelectedText();
    cursor.insertText(selectedText.replace(QRegExp("(\\s+_)*\\s+\\n\\s{2}"), "\n").remove(QRegExp("\\s+$")));
  } else {
    if (!cursor.hasSelection()) {
      cursor.select(QTextCursor::LineUnderCursor);
    }

    QStringList codePlainTextStringList = cursor.selection().toPlainText().split("\n");
    codePlainTextStringList.prepend("_");
    codePlainTextStringList.append(" ");
    int maxLineLength = 80;

    for (int k = 0; k < codePlainTextStringList.size(); ++k) {
      QString currentLine = codePlainTextStringList.at(k);
      currentLine.prepend("  ");
      while (currentLine.size() < maxLineLength)
        currentLine.append(" ");
      codePlainTextStringList.replace(k, currentLine);
    }

    QString codeInHtml = "<span style=\"font-family: mono; background-color: #404244; color: #FFFFFF;\">"+codePlainTextStringList.join("\n").replace("<", "&lt;").replace(" ", "&nbsp;").replace("\n", "<br/>")+"</span>";
    QRegExp qtWords("\\bQ[A-Za-z]+\\b");
    qtWords.setMinimal(true);

    int i = -1;
    while ((i = qtWords.indexIn(codeInHtml, i+1)) >= 0) {
      codeInHtml.replace(i, qtWords.cap(0).length(), "<span style=\"font-weight: bold; color: #5CAA15;\">"+qtWords.cap(0)+"</span>");
      i += qtWords.cap(0).length()+58;
    }

    cursor.removeSelectedText();
    cursor.insertHtml(codeInHtml);
  }
  cursor.endEditBlock();
}

void NoteRichTextEdit::transformTextToInternalLink(QTextCursor const& p_cursor) {
  if (!m_previousHtmlSelection.isEmpty()) {
    return;
  }

  f_textedit->viewport()->setCursor(QCursor(Qt::PointingHandCursor));

  m_previousCursor = p_cursor;
  QString qtClassHtml = m_previousCursor.selection().toHtml().split("<!--StartFragment-->").at(1).split("<!--EndFragment-->").at(0);
  m_previousHtmlSelection = qtClassHtml;

  QRegExp colorCSS("([ ;]color:#)([A-Fa-f0-9]{6})");
  QRegExp styleCSS("style=\"");
  colorCSS.setMinimal(true);
  int colorIndex = colorCSS.indexIn(qtClassHtml);

  if (colorIndex != -1) {
    qtClassHtml.replace(colorIndex+colorCSS.cap(1).length(), colorCSS.cap(2).length(), "46A2DA");
  } else {
    qtClassHtml.insert(styleCSS.indexIn(qtClassHtml)+styleCSS.cap(0).length(), "color:#46A2DA; ");
  }

  m_previousCursor.removeSelectedText();
  m_previousCursor.insertHtml(qtClassHtml);
  m_previousCursor.setPosition(m_previousCursor.position()-1);
  m_previousCursor.select(QTextCursor::WordUnderCursor);
}

void NoteRichTextEdit::transformInternalLinkBack() {
  if (m_previousCursor.selection().toHtml().isEmpty())
    return;

  f_textedit->viewport()->setCursor(QCursor(Qt::IBeamCursor));
  m_previousCursor.removeSelectedText();
  m_previousCursor.insertHtml(m_previousHtmlSelection);
  m_previousHtmlSelection.clear();
}

void NoteRichTextEdit::requesContextualMenuFromCursorPosition(QTextCursor const& p_cursor) {
  QTextCursor copyCursor = p_cursor;
  copyCursor.select(QTextCursor::WordUnderCursor);
  emit contextMenuRequested(copyCursor.selectedText());
}

void NoteRichTextEdit::saveDraft() {
  updateStackIndexAndRequestSaveNotes();
  editOff();
}

void NoteRichTextEdit::updateStackIndexAndRequestSaveNotes() {
  emit saveNotesRequested();
}

void NoteRichTextEdit::emitModificationsNotSaved(bool p_saved) {
  if (!f_textedit->isReadOnly()) {
    emit modificationsNotSaved(p_saved);
  }
}
