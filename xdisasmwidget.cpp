// copyright (c) 2019-2020 hors<horsicq@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "xdisasmwidget.h"
#include "ui_xdisasmwidget.h"

XDisasmWidget::XDisasmWidget(QWidget *pParent) :
    QWidget(pParent),
    ui(new Ui::XDisasmWidget)
{
    ui->setupUi(this);

    XOptions::setMonoFont(ui->tableViewDisasm);

    new QShortcut(QKeySequence(XShortcuts::GOTOENTRYPOINT), this,SLOT(_goToEntryPoint()));
    new QShortcut(QKeySequence(XShortcuts::GOTOADDRESS),    this,SLOT(_goToAddress()));
    new QShortcut(QKeySequence(XShortcuts::GOTOOFFSET),     this,SLOT(_goToOffset()));
    new QShortcut(QKeySequence(XShortcuts::GOTORELADDRESS), this,SLOT(_goToRelAddress()));
    new QShortcut(QKeySequence(XShortcuts::DUMPTOFILE),     this,SLOT(_dumpToFile()));
    new QShortcut(QKeySequence(XShortcuts::DISASM),         this,SLOT(_disasm()));
    new QShortcut(QKeySequence(XShortcuts::TODATA),         this,SLOT(_toData()));
    new QShortcut(QKeySequence(XShortcuts::SIGNATURE),      this,SLOT(_signature()));
    new QShortcut(QKeySequence(XShortcuts::COPYADDRESS),    this,SLOT(_copyAddress()));
    new QShortcut(QKeySequence(XShortcuts::COPYOFFSET),     this,SLOT(_copyOffset()));
    new QShortcut(QKeySequence(XShortcuts::COPYRELADDRESS), this,SLOT(_copyRelAddress()));
    new QShortcut(QKeySequence(XShortcuts::HEX),            this,SLOT(_hex()));

    pShowOptions=0;
    pDisasmOptions=0;
    pModel=0;

    __showOptions={};
    __disasmOptions={};
}

void XDisasmWidget::setData(QIODevice *pDevice, XDisasmModel::SHOWOPTIONS *pShowOptions, XDisasm::OPTIONS *pDisasmOptions, bool bAuto)
{
    this->pDevice=pDevice;

    if(pShowOptions)
    {
        this->pShowOptions=pShowOptions;
    }
    else
    {
        this->pShowOptions=&__showOptions;
    }

    if(pDisasmOptions)
    {
        this->pDisasmOptions=pDisasmOptions;
    }
    else
    {
        this->pDisasmOptions=&__disasmOptions;
    }

    QSet<XBinary::FT> stFT=XBinary::getFileTypes(pDevice);

    stFT.remove(XBinary::FT_BINARY);
    stFT.insert(XBinary::FT_BINARY16);
    stFT.insert(XBinary::FT_BINARY32);
    stFT.insert(XBinary::FT_BINARY64);
    stFT.insert(XBinary::FT_COM);

    QList<XBinary::FT> listFileTypes=XBinary::_getFileTypeListFromSet(stFT);

    int nCount=listFileTypes.count();

    ui->comboBoxType->clear();

    for(int i=0;i<nCount;i++)
    {
        XBinary::FT ft=listFileTypes.at(i);
        ui->comboBoxType->addItem(XBinary::fileTypeIdToString(ft),ft);
    }

    if(nCount)
    {
        if(pDisasmOptions->ft==XBinary::FT_UNKNOWN)
        {
            ui->comboBoxType->setCurrentIndex(nCount-1);
        }
        else
        {
            int nCount=ui->comboBoxType->count();

            for(int i=0;i<nCount;i++)
            {
                if(ui->comboBoxType->itemData(i).toUInt()==pDisasmOptions->ft)
                {
                    ui->comboBoxType->setCurrentIndex(i);

                    break;
                }
            }
        }
    }

    if(bAuto)
    {
        analyze();
    }
}

void XDisasmWidget::analyze()
{
    if(pDisasmOptions&&pShowOptions)
    {
        XBinary::FT ft=(XBinary::FT)ui->comboBoxType->currentData().toInt();
        pDisasmOptions->ft=ft;

        pDisasmOptions->stats={};

        QItemSelectionModel *modelOld=ui->tableViewDisasm->selectionModel();
        ui->tableViewDisasm->setModel(0);

        process(pDevice,pDisasmOptions,-1,XDisasm::DM_DISASM);

        pModel=new XDisasmModel(pDevice,&(pDisasmOptions->stats),pShowOptions,this);

        ui->tableViewDisasm->setModel(pModel);
        delete modelOld;

        int nSymbolWidth=XLineEditHEX::getSymbolWidth(this);

        // TODO 16/32/64 width
        ui->tableViewDisasm->setColumnWidth(0,nSymbolWidth*14);
        ui->tableViewDisasm->setColumnWidth(1,nSymbolWidth*8);
        ui->tableViewDisasm->setColumnWidth(2,nSymbolWidth*12);
        ui->tableViewDisasm->setColumnWidth(3,nSymbolWidth*20);
        ui->tableViewDisasm->setColumnWidth(4,nSymbolWidth*8);

        ui->tableViewDisasm->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Interactive);
        ui->tableViewDisasm->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Interactive);
        ui->tableViewDisasm->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Interactive);
        ui->tableViewDisasm->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Interactive);
        ui->tableViewDisasm->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);

        ui->pushButtonOverlay->setEnabled(pDisasmOptions->stats.bIsOverlayPresent);

        goToAddress(pDisasmOptions->stats.nEntryPointAddress);
    }
}

void XDisasmWidget::goToAddress(qint64 nAddress)
{
    if(pModel)
    {
        qint64 nPosition=pModel->addressToPosition(nAddress);

        _goToPosition(nPosition);
    }
}

void XDisasmWidget::goToOffset(qint64 nOffset)
{
    if(pModel)
    {
        qint64 nPosition=pModel->offsetToPosition(nOffset);

        _goToPosition(nPosition);
    }
}

void XDisasmWidget::goToRelAddress(qint64 nRelAddress)
{
    if(pModel)
    {
        qint64 nPosition=pModel->relAddressToPosition(nRelAddress);

        _goToPosition(nPosition);
    }
}

void XDisasmWidget::goToDisasmAddress(qint64 nAddress)
{
    if(!pDisasmOptions->stats.bInit)
    {
        process(pDevice,pDisasmOptions,nAddress,XDisasm::DM_DISASM);
    }

    goToAddress(nAddress);
}

void XDisasmWidget::goToEntryPoint()
{
    if(!pDisasmOptions->stats.bInit)
    {
        process(pDevice,pDisasmOptions,-1,XDisasm::DM_DISASM);
    }

    goToAddress(pDisasmOptions->stats.nEntryPointAddress);
}

void XDisasmWidget::disasm(qint64 nAddress)
{
    process(pDevice,pDisasmOptions,nAddress,XDisasm::DM_DISASM);

    goToAddress(nAddress);
}

void XDisasmWidget::toData(qint64 nAddress, qint64 nSize)
{
    process(pDevice,pDisasmOptions,nAddress,XDisasm::DM_TODATA);

    goToAddress(nAddress);
}

void XDisasmWidget::signature(qint64 nAddress, qint64 nSize)
{
    if(pDisasmOptions->stats.mapRecords.value(nAddress).type==XDisasm::RECORD_TYPE_OPCODE)
    {
        DialogAsmSignature ds(this,pDevice,pModel,nAddress);

        ds.exec();
    }
    else
    {
        qint64 nOffset=XBinary::addressToOffset(&(pDisasmOptions->stats.memoryMap),nAddress);

        if(nOffset!=-1)
        {
            DialogHexSignature dhs(this,pDevice,nOffset,nSize);

            dhs.exec();
        }
    }
}

void XDisasmWidget::hex(qint64 nOffset)
{
    QHexView::OPTIONS hexOptions={};

    XBinary binary(pDevice);

    hexOptions.memoryMap=binary.getMemoryMap();
    hexOptions.sBackupFileName=sBackupFileName;
    hexOptions.nStartAddress=nOffset;
    hexOptions.nStartSelectionAddress=nOffset;
    hexOptions.nSizeOfSelection=1;

    DialogHex dialogHex(this,pDevice,&hexOptions);

    connect(&dialogHex,SIGNAL(editState(bool)),this,SLOT(setEdited(bool)));

    dialogHex.exec();
}

void XDisasmWidget::clear()
{
    ui->tableViewDisasm->setModel(0);
}

XDisasmWidget::~XDisasmWidget()
{    
    delete ui;
}

void XDisasmWidget::process(QIODevice *pDevice,XDisasm::OPTIONS *pOptions, qint64 nStartAddress, XDisasm::DM dm)
{
    DialogDisasmProcess ddp(this);

    connect(&ddp,SIGNAL(errorMessage(QString)),this,SLOT(errorMessage(QString)));

    ddp.setData(pDevice,pOptions,nStartAddress,dm);
    ddp.exec();

//    if(pModel)
//    {
//        pModel->_beginResetModel();

//        DialogDisasmProcess ddp(this);

//        connect(&ddp,SIGNAL(errorMessage(QString)),this,SLOT(errorMessage(QString)));

//        ddp.setData(pDevice,pOptions,nStartAddress,dm);
//        ddp.exec();

//        pModel->_endResetModel();
//    }
}

XDisasm::STATS *XDisasmWidget::getDisasmStats()
{
    return &(pDisasmOptions->stats);
}

void XDisasmWidget::setBackupFileName(QString sBackupFileName)
{
    this->sBackupFileName=sBackupFileName;
}

void XDisasmWidget::on_pushButtonLabels_clicked()
{
    if(pModel)
    {
        DialogDisasmLabels dialogDisasmLabels(this,pModel->getStats());

        if(dialogDisasmLabels.exec()==QDialog::Accepted)
        {
            goToAddress(dialogDisasmLabels.getAddress());
        }
    }
}

void XDisasmWidget::on_tableViewDisasm_customContextMenuRequested(const QPoint &pos)
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        QMenu contextMenu(this);

        QMenu goToMenu(tr("Go to"),this);

        QAction actionGoToEntryPoint(tr("Entry point"),this);
        actionGoToEntryPoint.setShortcut(QKeySequence(XShortcuts::GOTOENTRYPOINT));
        connect(&actionGoToEntryPoint,SIGNAL(triggered()),this,SLOT(_goToEntryPoint()));

        QAction actionGoToAddress(tr("Virtual address"),this);
        actionGoToAddress.setShortcut(QKeySequence(XShortcuts::GOTOADDRESS));
        connect(&actionGoToAddress,SIGNAL(triggered()),this,SLOT(_goToAddress()));

        QAction actionGoToRelAddress(tr("Relative virtual address"),this);
        actionGoToRelAddress.setShortcut(QKeySequence(XShortcuts::GOTORELADDRESS));
        connect(&actionGoToRelAddress,SIGNAL(triggered()),this,SLOT(_goToRelAddress()));

        QAction actionGoToOffset(tr("File offset"),this);
        actionGoToOffset.setShortcut(QKeySequence(XShortcuts::GOTOOFFSET));
        connect(&actionGoToOffset,SIGNAL(triggered()),this,SLOT(_goToOffset()));

        goToMenu.addAction(&actionGoToEntryPoint);
        goToMenu.addAction(&actionGoToAddress);
        goToMenu.addAction(&actionGoToRelAddress);
        goToMenu.addAction(&actionGoToOffset);

        contextMenu.addMenu(&goToMenu);

        QMenu copyMenu(tr("Copy"),this);

        QAction actionCopyAddress(tr("Virtual address"),this);
        actionCopyAddress.setShortcut(QKeySequence(XShortcuts::COPYADDRESS));
        connect(&actionCopyAddress,SIGNAL(triggered()),this,SLOT(_copyAddress()));

        QAction actionCopyRelAddress(tr("Relative virtual address"),this);
        actionCopyRelAddress.setShortcut(QKeySequence(XShortcuts::COPYRELADDRESS));
        connect(&actionCopyRelAddress,SIGNAL(triggered()),this,SLOT(_copyRelAddress()));

        QAction actionCopyOffset(tr("File offset"),this);
        actionCopyOffset.setShortcut(QKeySequence(XShortcuts::COPYOFFSET));
        connect(&actionCopyOffset,SIGNAL(triggered()),this,SLOT(_copyOffset()));

        copyMenu.addAction(&actionCopyAddress);
        copyMenu.addAction(&actionCopyRelAddress);
        copyMenu.addAction(&actionCopyOffset);

        contextMenu.addMenu(&copyMenu);

        QAction actionHex(QString("Hex"),this);
        actionHex.setShortcut(QKeySequence(XShortcuts::HEX));
        connect(&actionHex,SIGNAL(triggered()),this,SLOT(_hex()));

        QAction actionSignature(tr("Signature"),this);
        actionSignature.setShortcut(QKeySequence(XShortcuts::SIGNATURE));
        connect(&actionSignature,SIGNAL(triggered()),this,SLOT(_signature()));

        QAction actionDump(tr("Dump to file"),this);
        actionDump.setShortcut(QKeySequence(XShortcuts::DUMPTOFILE));
        connect(&actionDump,SIGNAL(triggered()),this,SLOT(_dumpToFile()));

        QAction actionDisasm(tr("Disasm"),this);
        actionDisasm.setShortcut(QKeySequence(XShortcuts::DISASM));
        connect(&actionDisasm,SIGNAL(triggered()),this,SLOT(_disasm()));

        QAction actionToData(tr("To data"),this);
        actionToData.setShortcut(QKeySequence(XShortcuts::TODATA));
        connect(&actionToData,SIGNAL(triggered()),this,SLOT(_toData()));

        contextMenu.addAction(&actionHex);
        contextMenu.addAction(&actionSignature);

        if((selectionStat.nSize)&&XBinary::isSolidAddressRange(&(pModel->getStats()->memoryMap),selectionStat.nAddress,selectionStat.nSize))
        {
            contextMenu.addAction(&actionDump);
        }

        if(selectionStat.nCount==1)
        {
            contextMenu.addAction(&actionDisasm);
            contextMenu.addAction(&actionToData);
        }

        contextMenu.exec(ui->tableViewDisasm->viewport()->mapToGlobal(pos));

        // TODO data -> group
        // TODO add Label
        // TODO rename label
        // TODO remove label mb TODO custom label and Disasm label
    }
}

void XDisasmWidget::_goToAddress()
{
    if(pModel)
    {
        DialogGoToAddress da(this,&(pModel->getStats()->memoryMap),DialogGoToAddress::TYPE_ADDRESS);
        if(da.exec()==QDialog::Accepted)
        {
            goToAddress(da.getValue());
        }
    }
}

void XDisasmWidget::_goToRelAddress()
{
    if(pModel)
    {
        DialogGoToAddress da(this,&(pModel->getStats()->memoryMap),DialogGoToAddress::TYPE_REL_ADDRESS);
        if(da.exec()==QDialog::Accepted)
        {
            goToRelAddress(da.getValue());
        }
    }
}

void XDisasmWidget::_goToOffset()
{
    if(pModel)
    {
        DialogGoToAddress da(this,&(pModel->getStats()->memoryMap),DialogGoToAddress::TYPE_OFFSET);
        if(da.exec()==QDialog::Accepted)
        {
            goToOffset(da.getValue());
        }
    }
}

void XDisasmWidget::_goToEntryPoint()
{
    goToEntryPoint();
}

void XDisasmWidget::_copyAddress()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            QApplication::clipboard()->setText(QString("%1").arg(selectionStat.nAddress,0,16));
        }
    }
}

void XDisasmWidget::_copyOffset()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            QApplication::clipboard()->setText(QString("%1").arg(selectionStat.nOffset,0,16));
        }
    }
}

void XDisasmWidget::_copyRelAddress()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            QApplication::clipboard()->setText(QString("%1").arg(selectionStat.nRelAddress,0,16));
        }
    }
}

void XDisasmWidget::_dumpToFile()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            QString sFilter;
            sFilter+=QString("%1 (*.bin)").arg(tr("Raw data"));
            QString sSaveFileName="Result"; // TODO default directory / TODO getDumpName
            QString sFileName=QFileDialog::getSaveFileName(this,tr("Save dump"),sSaveFileName,sFilter);

            qint64 nOffset=XBinary::addressToOffset(&(pModel->getStats()->memoryMap),selectionStat.nAddress);

            if(!sFileName.isEmpty())
            {
                DialogDumpProcess dd(this,pDevice,nOffset,selectionStat.nSize,sFileName,DumpProcess::DT_OFFSET);

                dd.exec();
            }
        }
    }
}

void XDisasmWidget::_disasm()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            disasm(selectionStat.nAddress);
        }
    }
}

void XDisasmWidget::_toData()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            toData(selectionStat.nAddress,selectionStat.nSize);
        }
    }
}

void XDisasmWidget::_signature()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        if(selectionStat.nSize)
        {
            signature(selectionStat.nAddress,selectionStat.nSize);
        }
    }
}

void XDisasmWidget::_hex()
{
    if(pModel)
    {
        SELECTION_STAT selectionStat=getSelectionStat();

        hex(selectionStat.nOffset);
    }
}

XDisasmWidget::SELECTION_STAT XDisasmWidget::getSelectionStat()
{
    SELECTION_STAT result={};
    result.nAddress=-1;

    QModelIndexList il=ui->tableViewDisasm->selectionModel()->selectedRows();

    result.nCount=il.count();

    if(result.nCount)
    {
        result.nAddress=il.at(0).data(Qt::UserRole+XDisasmModel::UD_ADDRESS).toLongLong();
        result.nOffset=il.at(0).data(Qt::UserRole+XDisasmModel::UD_OFFSET).toLongLong();
        result.nRelAddress=il.at(0).data(Qt::UserRole+XDisasmModel::UD_RELADDRESS).toLongLong();

        qint64 nLastElementAddress=il.at(result.nCount-1).data(Qt::UserRole+XDisasmModel::UD_ADDRESS).toLongLong();
        qint64 nLastElementSize=il.at(result.nCount-1).data(Qt::UserRole+XDisasmModel::UD_SIZE).toLongLong();

        result.nSize=(nLastElementAddress+nLastElementSize)-result.nAddress;
    }

    return result;
}

void XDisasmWidget::on_pushButtonAnalyze_clicked()
{
    analyze();
}

void XDisasmWidget::_goToPosition(qint32 nPosition)
{
    if(ui->tableViewDisasm->verticalScrollBar()->maximum()==0)
    {
        ui->tableViewDisasm->verticalScrollBar()->setMaximum(nPosition); // Hack
    }

    ui->tableViewDisasm->verticalScrollBar()->setValue(nPosition);

    ui->tableViewDisasm->setCurrentIndex(ui->tableViewDisasm->model()->index(nPosition,0));
}

void XDisasmWidget::on_pushButtonOverlay_clicked()
{
    hex(pDisasmOptions->stats.nOverlayOffset);
}

void XDisasmWidget::setEdited(bool bState)
{
    if(bState)
    {
        analyze();
    }
}

void XDisasmWidget::on_pushButtonHex_clicked()
{
    hex(0);
}

void XDisasmWidget::errorMessage(QString sText)
{
    QMessageBox::critical(this,tr("Error"),sText);
}
