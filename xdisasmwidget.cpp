#include "xdisasmwidget.h"
#include "ui_xdisasmwidget.h"

XDisasmWidget::XDisasmWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::XDisasmWidget)
{
    ui->setupUi(this);

    QFont font=ui->tableViewDisasm->font();
    font.setFamily("Courier");
    ui->tableViewDisasm->setFont(font);

    new QShortcut(QKeySequence(XShortcuts::GOTOADDRESS),this,SLOT(_goToAddress()));

    disasmStats={};
    pModel=0;
}

void XDisasmWidget::setData(QIODevice *pDevice, XDisasmModel::SHOWOPTIONS *pOptions)
{
    this->pDevice=pDevice;
    this->pOptions=pOptions;

    pModel=new XDisasmModel(pDevice,&disasmStats,pOptions,this);

    QItemSelectionModel *modelOld=ui->tableViewDisasm->selectionModel();
    ui->tableViewDisasm->setModel(pModel);
    delete modelOld;
}

void XDisasmWidget::goToAddress(qint64 nAddress)
{
    if(pModel)
    {
        qint64 nPosition=pModel->addressToPosition(nAddress);

        if(ui->tableViewDisasm->verticalScrollBar()->maximum()==0)
        {
            ui->tableViewDisasm->verticalScrollBar()->setMaximum(nPosition); // Hack
        }

        ui->tableViewDisasm->verticalScrollBar()->setValue(nPosition);
    }
}

void XDisasmWidget::goToDisasmAddress(qint64 nAddress)
{
    if(!disasmStats.bInit)
    {
        process(nAddress);
    }

    goToAddress(nAddress);
}

void XDisasmWidget::goToEntryPoint()
{
    if(!disasmStats.bInit)
    {
        process(-1);
    }

    goToAddress(disasmStats.nEntryPointAddress);
}

void XDisasmWidget::disasm(qint64 nAddress)
{
    process(nAddress);
}

void XDisasmWidget::toData(qint64 nAddress, qint64 nSize)
{
    qDebug("void XDisasmWidget::toData(qint64 nAddress, qint64 nSize)");
}

void XDisasmWidget::clear()
{
    ui->tableViewDisasm->setModel(0);
}

XDisasmWidget::~XDisasmWidget()
{    
    delete ui;
}

void XDisasmWidget::process(qint64 nAddress)
{
    if(pModel)
    {
        pModel->_beginResetModel();

        DialogDisasmProcess ddp(this);

        ddp.setData(pDevice,false,XDisasm::MODE_UNKNOWN,nAddress,&disasmStats);
        ddp.exec();

        pModel->_endResetModel();
    }
}

void XDisasmWidget::on_pushButtonLabels_clicked()
{
    if(pModel)
    {
        DialogDisasmLabels dialogDisasmLabels(this,pModel->getStats());

        dialogDisasmLabels.exec();
    }
}

void XDisasmWidget::on_tableViewDisasm_customContextMenuRequested(const QPoint &pos)
{
    if(pModel)
    {
        QMenu contextMenu(this);

        QAction actionGoToAddress(tr("Go to Address"),this);
        actionGoToAddress.setShortcut(QKeySequence(XShortcuts::GOTOADDRESS));
        connect(&actionGoToAddress,SIGNAL(triggered()),this,SLOT(_goToAddress()));
        contextMenu.addAction(&actionGoToAddress);

        QAction actionDump(tr("Dump to File"),this); // TODO if selected
        actionDump.setShortcut(QKeySequence(XShortcuts::DUMPTOFILE));
        connect(&actionDump,SIGNAL(triggered()),this,SLOT(_dumpToFile()));
        contextMenu.addAction(&actionDump);

        QAction actionDisasm(tr("Disasm"),this); // TODO if 1 row selected
        actionDisasm.setShortcut(QKeySequence(XShortcuts::DISASM));
        connect(&actionDisasm,SIGNAL(triggered()),this,SLOT(_disasm()));
        contextMenu.addAction(&actionDisasm);

        QAction actionToData(tr("To Data"),this); // TODO if 1 row selected
        actionToData.setShortcut(QKeySequence(XShortcuts::TODATA));
        connect(&actionToData,SIGNAL(triggered()),this,SLOT(_toData()));
        contextMenu.addAction(&actionToData);

        contextMenu.exec(ui->tableViewDisasm->viewport()->mapToGlobal(pos));

        // TODO data -> group
    }
}

void XDisasmWidget::_goToAddress()
{
    if(pModel)
    {
        DialogGoToAddress da(this,&(pModel->getStats()->memoryMap));
        if(da.exec()==QDialog::Accepted)
        {
            goToAddress(da.getAddress());
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
            // TODO
            qDebug("_dumpToFile");
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

XDisasmWidget::SELECTION_STAT XDisasmWidget::getSelectionStat()
{
    SELECTION_STAT result={};
    result.nAddress=-1;

    QModelIndexList il=ui->tableViewDisasm->selectionModel()->selectedRows();

    result.nCount=il.count();

    if(result.nCount)
    {
        result.nAddress=il.at(0).data(Qt::UserRole+XDisasmModel::UD_ADDRESS).toLongLong();

        qint64 nLastElementAddress=il.at(result.nCount-1).data(Qt::UserRole+XDisasmModel::UD_ADDRESS).toLongLong();
        qint64 nLastElementSize=il.at(result.nCount-1).data(Qt::UserRole+XDisasmModel::UD_SIZE).toLongLong();

        result.nSize=(nLastElementAddress+nLastElementSize)-result.nAddress;
    }

    return result;
}
