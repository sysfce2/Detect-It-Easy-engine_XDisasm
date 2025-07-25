// Copyright (c) 2019-2025 hors<horsicq@gmail.com>
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
#include "xdisasm.h"

XDisasm::XDisasm(QObject *pParent) : QObject(pParent) {
    g_pOptions = 0;
    g_nStartAddress = 0;
    g_disasm_handle = 0;
    g_bStop = false;
}

XDisasm::~XDisasm() {
    if (g_disasm_handle) {
        cs_close(&g_disasm_handle);
        g_disasm_handle = 0;
    }
}

void XDisasm::setData(QIODevice *pDevice, XDisasm::OPTIONS *pOptions, qint64 nStartAddress, XDisasm::DM dm) {
    this->g_pDevice = pDevice;
    this->g_pOptions = pOptions;
    this->g_nStartAddress = nStartAddress;
    this->g_dm = dm;
}

void XDisasm::_disasm(qint64 nInitAddress, qint64 nAddress) {
    g_pOptions->stats.mmapRefFrom.insert(nAddress, nInitAddress);
    g_pOptions->stats.mmapRefTo.insert(nInitAddress, nAddress);

    while (!g_bStop) {
        if (g_pOptions->stats.mapRecords.contains(nAddress)) {
            break;
        }

        bool bStopBranch = false;
        int nDelta = 0;

        qint64 nOffset = XBinary::addressToOffset(&(g_pOptions->stats.memoryMap),
                                                  nAddress);  // TODO optimize if image
        if (nOffset != -1) {
            char opcode[N_X64_OPCODE_SIZE];

            XBinary::_zeroMemory(opcode, N_X64_OPCODE_SIZE);

            size_t nDataSize = XBinary::read_array(g_pDevice, nOffset, opcode, N_X64_OPCODE_SIZE);

            uint8_t *pData = (uint8_t *)opcode;

            cs_insn *pInsn = 0;
            size_t nNumberOfOpcodes = cs_disasm(g_disasm_handle, pData, nDataSize, nAddress, 1, &pInsn);

            if (nNumberOfOpcodes > 0) {
                if (pInsn->size > 1) {
                    bStopBranch = !XBinary::isAddressPhysical(&(g_pOptions->stats.memoryMap), nAddress + pInsn->size - 1);
                }

                if (!bStopBranch) {
                    for (int i = 0; i < pInsn->detail->x86.op_count; i++) {
                        if (pInsn->detail->x86.operands[i].type == X86_OP_IMM) {
                            qint64 nImm = pInsn->detail->x86.operands[i].imm;

                            if (isJmpOpcode(pInsn->id)) {
                                if (isCallOpcode(pInsn->id)) {
                                    g_pOptions->stats.stCalls.insert(nImm);
                                } else {
                                    g_pOptions->stats.stJumps.insert(nImm);
                                }

                                if (nAddress != nImm) {
                                    _disasm(nAddress, nImm);
                                }
                            }
                        }
                    }

                    RECORD opcode = {};
                    opcode.nOffset = nOffset;
                    opcode.nSize = pInsn->size;
                    opcode.type = RECORD_TYPE_OPCODE;

                    if (!_insertOpcode(nAddress, &opcode)) {
                        bStopBranch = true;
                    }

                    nDelta = pInsn->size;

                    if (isEndBranchOpcode(pInsn->id)) {
                        bStopBranch = true;
                    }
                }

                cs_free(pInsn, nNumberOfOpcodes);
            } else {
                bStopBranch = true;
            }

            if (XBinary::_isMemoryZeroFilled(opcode, N_X64_OPCODE_SIZE)) {
                bStopBranch = true;
            }
        }

        if (nDelta) {
            nAddress += nDelta;
        } else {
            bStopBranch = true;
        }

        if (bStopBranch) {
            break;
        }
    }
}

void XDisasm::processDisasm() {
    g_bStop = false;

    if (!g_pOptions->stats.bInit) {
        g_pOptions->stats.csarch = CS_ARCH_X86;
        g_pOptions->stats.csmode = CS_MODE_16;

        XBinary::FT fileType = g_pOptions->fileType;

        if (fileType == XBinary::FT_UNKNOWN) {
            fileType = XBinary::getPrefFileType(g_pDevice);
        }

        if ((fileType == XBinary::FT_PE32) || (fileType == XBinary::FT_PE64)) {
            XPE pe(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = pe.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = pe.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = pe.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = pe.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = pe.getOverlayOffset();

            XBinary::MODE modeBinary = pe.getMode();

            g_pOptions->stats.csarch = CS_ARCH_X86;
            if (modeBinary == XBinary::MODE_32) {
                g_pOptions->stats.csmode = CS_MODE_32;
            } else if (modeBinary == XBinary::MODE_64) {
                g_pOptions->stats.csmode = CS_MODE_64;
            }
        } else if ((fileType == XBinary::FT_ELF32) || (fileType == XBinary::FT_ELF64)) {
            XELF elf(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = elf.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = elf.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = elf.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = elf.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = elf.getOverlayOffset();
        } else if ((fileType == XBinary::FT_MACHO32) || (fileType == XBinary::FT_MACHO64)) {
            XMACH mach(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = mach.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = mach.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = mach.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = mach.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = mach.getOverlayOffset();
        } else if (fileType == XBinary::FT_MSDOS) {
            XMSDOS msdos(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = msdos.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = msdos.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = msdos.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = msdos.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = msdos.getOverlayOffset();
        } else if (fileType == XBinary::FT_NE) {
            XNE ne(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = ne.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = ne.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = ne.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = ne.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = ne.getOverlayOffset();
        } else if ((fileType == XBinary::FT_LE) || (fileType == XBinary::FT_LX)) {
            XLE le(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = le.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = le.getEntryPointAddress(&g_pOptions->stats.memoryMap);
            g_pOptions->stats.bIsOverlayPresent = le.isOverlayPresent();
            g_pOptions->stats.nOverlaySize = le.getOverlaySize();
            g_pOptions->stats.nOverlayOffset = le.getOverlayOffset();
        } else if (fileType == XBinary::FT_COM) {
            XCOM xcom(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            g_pOptions->stats.memoryMap = xcom.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = xcom.getEntryPointAddress(&g_pOptions->stats.memoryMap);
        } else if ((fileType == XBinary::FT_BINARY16) || (fileType == XBinary::FT_BINARY)) {
            XBinary binary(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            binary.setArch("8086");
            binary.setMode(XBinary::MODE_16);

            g_pOptions->stats.memoryMap = binary.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = binary.getEntryPointAddress(&g_pOptions->stats.memoryMap);
        } else if (fileType == XBinary::FT_BINARY32) {
            XBinary binary(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            binary.setArch("386");
            binary.setMode(XBinary::MODE_32);

            g_pOptions->stats.memoryMap = binary.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = binary.getEntryPointAddress(&g_pOptions->stats.memoryMap);
        } else if (fileType == XBinary::FT_BINARY64) {
            XBinary binary(g_pDevice, g_pOptions->bIsImage, g_pOptions->nImageBase);

            binary.setArch("AMD64");
            binary.setMode(XBinary::MODE_64);

            g_pOptions->stats.memoryMap = binary.getMemoryMap();
            g_pOptions->stats.nEntryPointAddress = binary.getEntryPointAddress(&g_pOptions->stats.memoryMap);
        }

        g_pOptions->stats.nImageBase = g_pOptions->stats.memoryMap.nModuleAddress;
        //        pOptions->stats.nImageSize=XBinary::getTotalVirtualSize(&(pOptions->stats.memoryMap));
        g_pOptions->stats.nImageSize = g_pOptions->stats.memoryMap.nImageSize;

        if (XBinary::isX86asm(g_pOptions->stats.memoryMap.sArch)) {
            g_pOptions->stats.csarch = CS_ARCH_X86;
            if ((g_pOptions->stats.memoryMap.mode == XBinary::MODE_16) || (g_pOptions->stats.memoryMap.mode == XBinary::MODE_16SEG)) {
                g_pOptions->stats.csmode = CS_MODE_16;
            } else if (g_pOptions->stats.memoryMap.mode == XBinary::MODE_32) {
                g_pOptions->stats.csmode = CS_MODE_32;
            } else if (g_pOptions->stats.memoryMap.mode == XBinary::MODE_64) {
                g_pOptions->stats.csmode = CS_MODE_64;
            }

            cs_err err = cs_open(g_pOptions->stats.csarch, g_pOptions->stats.csmode, &g_disasm_handle);
            if (!err) {
                cs_option(g_disasm_handle, CS_OPT_DETAIL,
                          CS_OPT_ON);  // TODO Check
            }

            _disasm(0, g_pOptions->stats.nEntryPointAddress);

            if (g_nStartAddress != -1) {
                if (g_nStartAddress != g_pOptions->stats.nEntryPointAddress) {
                    _disasm(0, g_nStartAddress);
                }
            }

            _adjust();
            _updatePositions();

            g_pOptions->stats.bInit = true;

            if (g_disasm_handle) {
                cs_close(&g_disasm_handle);
                g_disasm_handle = 0;
            }
        } else {
            emit errorMessage(QString("%1: %2").arg("Architecture").arg(g_pOptions->stats.memoryMap.sArch));
        }
    } else {
        if (XBinary::isX86asm(g_pOptions->stats.memoryMap.sArch)) {
            // TODO move to function
            if (g_disasm_handle == 0) {
                cs_err err = cs_open(g_pOptions->stats.csarch, g_pOptions->stats.csmode, &g_disasm_handle);
                if (!err) {
                    cs_option(g_disasm_handle, CS_OPT_DETAIL,
                              CS_OPT_ON);  // TODO Check
                }
            }

            _disasm(0, g_nStartAddress);

            _adjust();
            _updatePositions();

            if (g_disasm_handle) {
                cs_close(&g_disasm_handle);
                g_disasm_handle = 0;
            }
        } else {
            emit errorMessage(QString("%1: %2").arg("Architecture").arg(g_pOptions->stats.memoryMap.sArch));
        }
    }

    emit processFinished();
}

void XDisasm::processToData() {
    g_pOptions->stats.mapRecords.remove(this->g_nStartAddress);

    _adjust();
    _updatePositions();

    emit processFinished();
}

void XDisasm::process() {
    if (g_dm == DM_DISASM) {
        processDisasm();
    } else if (g_dm == DM_TODATA) {
        processToData();
    }
}

void XDisasm::stop() {
    g_bStop = true;
}

XDisasm::STATS *XDisasm::getStats() {
    return &(g_pOptions->stats);
}

void XDisasm::_adjust() {
    g_pOptions->stats.mapLabelStrings.clear();
    g_pOptions->stats.mapVB.clear();

    if (!g_bStop) {
        g_pOptions->stats.mapLabelStrings.insert(g_pOptions->stats.nEntryPointAddress, "entry_point");

        QSetIterator<qint64> iFL(g_pOptions->stats.stCalls);
        while (iFL.hasNext() && (!g_bStop)) {
            qint64 nAddress = iFL.next();

            if (!g_pOptions->stats.mapLabelStrings.contains(nAddress)) {
                g_pOptions->stats.mapLabelStrings.insert(nAddress, QString("func_%1").arg(nAddress, 0, 16));
            }
        }

        QSetIterator<qint64> iJL(g_pOptions->stats.stJumps);
        while (iJL.hasNext() && (!g_bStop)) {
            qint64 nAddress = iJL.next();

            if (!g_pOptions->stats.mapLabelStrings.contains(nAddress)) {
                g_pOptions->stats.mapLabelStrings.insert(nAddress, QString("lab_%1").arg(nAddress, 0, 16));
            }
        }

        //    QSet<qint64> stFunctionLabels;
        //    QSet<qint64> stJmpLabels;
        //    QMap<qint64,qint64> mapDataSizeLabels; // Set Max
        //    QSet<qint64> stDataLabels;

        // TODO Strings
        QMapIterator<qint64, XDisasm::RECORD> iRecords(g_pOptions->stats.mapRecords);
        while (iRecords.hasNext() && (!g_bStop)) {
            iRecords.next();

            qint64 nAddress = iRecords.key();

            VIEW_BLOCK record;
            record.nAddress = nAddress;
            record.nOffset = iRecords.value().nOffset;
            record.nSize = iRecords.value().nSize;

            if (iRecords.value().type == RECORD_TYPE_OPCODE) {
                record.type = VBT_OPCODE;
            } else if (iRecords.value().type == RECORD_TYPE_DATA) {
                record.type = VBT_DATA;
            }

            if (!g_pOptions->stats.mapVB.contains(nAddress)) {
                g_pOptions->stats.mapVB.insert(nAddress, record);
            }
        }

        int nNumberOfRecords = g_pOptions->stats.memoryMap.listRecords.count();  // TODO

        for (int i = 0; (i < nNumberOfRecords) && (!g_bStop); i++) {
            qint64 nRegionAddress = g_pOptions->stats.memoryMap.listRecords.at(i).nAddress;
            qint64 nRegionOffset = g_pOptions->stats.memoryMap.listRecords.at(i).nOffset;
            qint64 nRegionSize = g_pOptions->stats.memoryMap.listRecords.at(i).nSize;

            if (nRegionAddress != -1) {
                for (qint64 nCurrentAddress = nRegionAddress, nCurrentOffset = nRegionOffset;
                     (nCurrentAddress < (nRegionAddress + nRegionSize)) && (!g_bStop);) {
                    VIEW_BLOCK vb = g_pOptions->stats.mapVB.value(nCurrentAddress);

                    if (!vb.nSize) {
                        QMap<qint64, VIEW_BLOCK>::const_iterator iter = g_pOptions->stats.mapVB.lowerBound(nCurrentAddress);

                        qint64 nBlockAddress = 0;
                        qint64 nBlockOffset = 0;
                        qint64 nBlockSize = 0;

                        qint64 nIterKey = iter.key();

                        if (g_pOptions->stats.mapVB.count()) {
                            if (nIterKey == g_pOptions->stats.mapVB.firstKey())  // TODO move outside 'for'
                            {
                                //                                nBlockAddress=pOptions->stats.nImageBase;
                                nBlockAddress = g_pOptions->stats.memoryMap.listRecords.at(0).nAddress;
                                nBlockOffset = g_pOptions->stats.memoryMap.listRecords.at(0).nOffset;

                                if (nIterKey < (nRegionAddress + nRegionSize)) {
                                    nBlockSize = iter.key() - g_pOptions->stats.nImageBase;
                                } else {
                                    nBlockSize = (nRegionAddress + nRegionSize) - nBlockAddress;
                                }
                            } else if (iter == g_pOptions->stats.mapVB.end()) {
                                nBlockAddress = nCurrentAddress;
                                nBlockOffset = nCurrentOffset;

                                nBlockSize = (nRegionAddress + nRegionSize) - nBlockAddress;
                            } else {
                                nBlockAddress = nCurrentAddress;
                                nBlockOffset = nCurrentOffset;
                                if (nIterKey < (nRegionAddress + nRegionSize)) {
                                    nBlockSize = iter.key() - nBlockAddress;
                                } else {
                                    nBlockSize = (nRegionAddress + nRegionSize) - nBlockAddress;
                                }
                            }
                        } else {
                            nBlockAddress = nCurrentAddress;
                            nBlockOffset = nCurrentOffset;

                            nBlockSize = (nRegionAddress + nRegionSize) - nBlockAddress;
                        }

                        qint64 _nAddress = nBlockAddress;
                        qint64 _nOffset = nBlockOffset;
                        qint64 _nSize = nBlockSize;

                        if (_nOffset != -1) {
                            while (_nSize >= 16) {
                                VIEW_BLOCK record;
                                record.nAddress = _nAddress;
                                record.nOffset = _nOffset;
                                record.nSize = 16;
                                record.type = VBT_DATABLOCK;

                                if (!g_pOptions->stats.mapVB.contains(_nAddress)) {
                                    g_pOptions->stats.mapVB.insert(_nAddress, record);
                                }

                                _nSize -= 16;
                                _nAddress += 16;
                                _nOffset += 16;
                            }
                        } else {
                            VIEW_BLOCK record;
                            record.nAddress = _nAddress;
                            record.nOffset = -1;
                            record.nSize = _nSize;
                            record.type = VBT_DATABLOCK;

                            if (!g_pOptions->stats.mapVB.contains(_nAddress)) {
                                g_pOptions->stats.mapVB.insert(_nAddress, record);
                            }
                        }

                        nCurrentAddress = nBlockAddress + nBlockSize;
                        if (nBlockOffset != -1) {
                            nCurrentOffset = nBlockOffset + nBlockSize;
                        }
                    } else {
                        nCurrentAddress = vb.nAddress + vb.nSize;
                        if (nCurrentOffset != -1) {
                            nCurrentOffset = vb.nOffset + vb.nSize;
                        }
                    }
                }
            }
        }

        //        QMapIterator<qint64,qint64> iDS(stats.mmapDataLabels);
        //        while(iDS.hasNext())
        //        {
        //            iDS.next();

        //            qint64 nAddress=iDS.key();

        //            VIEW_BLOCK record;
        //            record.nOffset=pBinary->addressToOffset(pListMM,nAddress);
        //            record.nSize=iDS.value();
        //            record.type=VBT_DATA;

        //            if(!stats.mapVB.contains(nAddress))
        //            {
        //                stats.mapVB.insert(nAddress,record);
        //            }
        //        }

        //        QSetIterator<qint64> iD(stats.stDataLabels);
        //        while(iD.hasNext()&&(!bStop))
        //        {
        //            qint64 nAddress=iD.next();

        //            VIEW_BLOCK record;
        //            record.nOffset=pBinary->addressToOffset(pListMM,nAddress);
        //            record.nSize=0;
        //            record.type=VBT_DATA;

        //            if(!stats.mapVB.contains(nAddress))
        //            {
        //                stats.mapVB.insert(nAddress,record);
        //            }
        //        }

        // TODO Check errors
    }
}

void XDisasm::_updatePositions() {
    qint64 nImageSize = g_pOptions->stats.nImageSize;
    qint64 nNumberOfVBs = g_pOptions->stats.mapVB.count();
    qint64 nVBSize = getVBSize(&(g_pOptions->stats.mapVB));
    g_pOptions->stats.nPositions = nImageSize + nNumberOfVBs - nVBSize;  // TODO

    g_pOptions->stats.mapPositions.clear();
    // TODO cache
    qint64 nCurrentAddress = g_pOptions->stats.nImageBase;  // TODO

    for (qint64 i = 0; i < g_pOptions->stats.nPositions; i++) {
        bool bIsVBPresent = g_pOptions->stats.mapVB.contains(nCurrentAddress);

        if (bIsVBPresent) {
            g_pOptions->stats.mapPositions.insert(i, nCurrentAddress);
            g_pOptions->stats.mapAddresses.insert(nCurrentAddress, i);

            nCurrentAddress += g_pOptions->stats.mapVB.value(nCurrentAddress).nSize - 1;
        }

        nCurrentAddress++;
    }
}

bool XDisasm::_insertOpcode(qint64 nAddress, XDisasm::RECORD *pOpcode) {
    g_pOptions->stats.mapRecords.insert(nAddress, *pOpcode);

    int nNumberOfRecords = g_pOptions->stats.mapRecords.count();

    return (nNumberOfRecords < N_OPCODE_COUNT);
}

qint64 XDisasm::getVBSize(QMap<qint64, XDisasm::VIEW_BLOCK> *pMapVB) {
    qint64 nResult = 0;

    QMapIterator<qint64, XDisasm::VIEW_BLOCK> i(*pMapVB);
    while (i.hasNext()) {
        i.next();

        nResult += i.value().nSize;
    }

    return nResult;
}

QString XDisasm::getDisasmString(csh disasm_handle, qint64 nAddress, char *pData, qint32 nDataSize) {
    QString sResult;

    cs_insn *pInsn = 0;
    size_t nNumberOfOpcodes = cs_disasm(disasm_handle, (uint8_t *)pData, nDataSize, nAddress, 1, &pInsn);

    if (nNumberOfOpcodes > 0) {
        QString sMnemonic = pInsn->mnemonic;
        QString sArgs = pInsn->op_str;

        sResult = sMnemonic;
        if (sArgs != "") {
            sResult += " " + sArgs;
        }

        cs_free(pInsn, nNumberOfOpcodes);
    }

    return sResult;
}

QList<XDisasm::SIGNATURE_RECORD> XDisasm::getSignature(XDisasm::SIGNATURE_OPTIONS *pSignatureOptions, qint64 nAddress) {
    QList<SIGNATURE_RECORD> listResult;

    csh _disasm_handle;
    cs_err err = cs_open(pSignatureOptions->csarch, pSignatureOptions->csmode, &_disasm_handle);
    if (!err) {
        cs_option(_disasm_handle, CS_OPT_DETAIL, CS_OPT_ON);
    }

    QSet<qint64> stRecords;

    bool bStopBranch = false;

    for (int i = 0; (i < pSignatureOptions->nCount) && (!bStopBranch); i++) {
        qint64 nOffset = XBinary::addressToOffset(&(pSignatureOptions->memoryMap), nAddress);
        if (nOffset != -1) {
            char opcode[N_X64_OPCODE_SIZE];

            XBinary::_zeroMemory(opcode, N_X64_OPCODE_SIZE);

            size_t nDataSize = XBinary::read_array(pSignatureOptions->pDevice, nOffset, opcode, N_X64_OPCODE_SIZE);

            uint8_t *pData = (uint8_t *)opcode;

            cs_insn *pInsn = 0;
            size_t count = cs_disasm(_disasm_handle, pData, nDataSize, nAddress, 1, &pInsn);

            if (count > 0) {
                if (pInsn->size > 1) {
                    bStopBranch = !XBinary::isAddressPhysical(&(pSignatureOptions->memoryMap), nAddress + pInsn->size - 1);
                }

                if (stRecords.contains(nAddress)) {
                    bStopBranch = true;
                }

                if (!bStopBranch) {
                    SIGNATURE_RECORD record = {};

                    record.nAddress = nAddress;
                    record.sOpcode = pInsn->mnemonic;
                    QString sArgs = pInsn->op_str;

                    if (sArgs != "") {
                        record.sOpcode += " " + sArgs;
                    }

                    record.baOpcode = QByteArray(opcode, pInsn->size);

                    record.nDispOffset = pInsn->detail->x86.encoding.disp_offset;
                    record.nDispSize = pInsn->detail->x86.encoding.disp_size;
                    record.nImmOffset = pInsn->detail->x86.encoding.imm_offset;
                    record.nImmSize = pInsn->detail->x86.encoding.imm_size;

                    stRecords.insert(nAddress);

                    nAddress += pInsn->size;

                    if (pSignatureOptions->sm == XDisasm::SM_RELATIVEADDRESS) {
                        for (int i = 0; i < pInsn->detail->x86.op_count; i++) {
                            if (pInsn->detail->x86.operands[i].type == X86_OP_IMM) {
                                qint64 nImm = pInsn->detail->x86.operands[i].imm;

                                if (isJmpOpcode(pInsn->id)) {
                                    nAddress = nImm;
                                    record.bIsConst = true;
                                }
                            }
                        }
                    }

                    listResult.append(record);
                }

                cs_free(pInsn, count);
            } else {
                bStopBranch = true;
            }
        }
    }

    cs_close(&_disasm_handle);

    return listResult;
}

bool XDisasm::isEndBranchOpcode(uint nOpcodeID) {
    bool bResult = false;

    // TODO more checks
    if ((nOpcodeID == X86_INS_JMP) || (nOpcodeID == X86_INS_RET) || (nOpcodeID == X86_INS_INT3)) {
        bResult = true;
    }

    return bResult;
}

bool XDisasm::isJmpOpcode(uint nOpcodeID) {
    bool bResult = false;

    if ((nOpcodeID == X86_INS_JMP) || (nOpcodeID == X86_INS_JA) || (nOpcodeID == X86_INS_JAE) || (nOpcodeID == X86_INS_JB) || (nOpcodeID == X86_INS_JBE) ||
        (nOpcodeID == X86_INS_JCXZ) || (nOpcodeID == X86_INS_JE) || (nOpcodeID == X86_INS_JECXZ) || (nOpcodeID == X86_INS_JG) || (nOpcodeID == X86_INS_JGE) ||
        (nOpcodeID == X86_INS_JL) || (nOpcodeID == X86_INS_JLE) || (nOpcodeID == X86_INS_JNE) || (nOpcodeID == X86_INS_JNO) || (nOpcodeID == X86_INS_JNP) ||
        (nOpcodeID == X86_INS_JNS) || (nOpcodeID == X86_INS_JO) || (nOpcodeID == X86_INS_JP) || (nOpcodeID == X86_INS_JRCXZ) || (nOpcodeID == X86_INS_JS) ||
        (nOpcodeID == X86_INS_LOOP) || (nOpcodeID == X86_INS_LOOPE) || (nOpcodeID == X86_INS_LOOPNE) || (nOpcodeID == X86_INS_CALL)) {
        bResult = true;
    }

    return bResult;
}

bool XDisasm::isCallOpcode(uint nOpcodeID) {
    bool bResult = false;

    if (nOpcodeID == X86_INS_CALL) {
        bResult = true;
    }

    return bResult;
}
