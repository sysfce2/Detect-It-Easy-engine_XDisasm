// copyright (c) 2019-2025 hors<horsicq@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#ifndef DIALOGASMSIGNATURE_H
#define DIALOGASMSIGNATURE_H

#include <QClipboard>
#include <QDialog>

#include "xdisasmmodel.h"
#include "xlineedithex.h"
#include "xoptions.h"

namespace Ui {
class DialogAsmSignature;
}

class DialogAsmSignature : public QDialog {
    Q_OBJECT

public:
    explicit DialogAsmSignature(QWidget *pParent, QIODevice *pDevice, XDisasmModel *pModel, qint64 nAddress);
    ~DialogAsmSignature();
    void reload();

private slots:
    void on_pushButtonOK_clicked();
    void reloadSignature();
    void on_checkBoxSpaces_toggled(bool bChecked);
    void on_checkBoxUpper_toggled(bool bChecked);
    void on_lineEditWildcard_textChanged(const QString &sText);
    void on_pushButtonCopy_clicked();
    QString replaceWild(QString sString, qint32 nOffset, qint32 nSize, QChar cWild);
    void on_spinBoxCount_valueChanged(int nValue);

    void on_comboBoxMethod_currentIndexChanged(int nIndex);

private:
    Ui::DialogAsmSignature *ui;
    QIODevice *g_pDevice;
    XDisasmModel *g_pModel;
    qint64 g_nAddress;
    QList<XDisasm::SIGNATURE_RECORD> g_listRecords;
};

#endif  // DIALOGASMSIGNATURE_H
