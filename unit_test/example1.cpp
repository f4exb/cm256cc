/*
    Copyright (c) 2016 Edouard M. Griffiths.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of CM256 nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <cstdlib>
#include <unistd.h>

#include "example1.h"

Example1::Example1(int samplesPerBlock, int nbOriginalBlocks, int nbFecBlocks)
{
    m_params.BlockBytes = samplesPerBlock * sizeof(Sample);
    m_params.OriginalCount = nbOriginalBlocks;
    m_params.RecoveryCount = nbFecBlocks;
}

Example1::~Example1()
{

}

void Example1::makeDataBlocks(SuperBlock *txBlocks, uint16_t frameNumber)
{
    std::srand(frameNumber);

    for (int iblock = 0; iblock < m_params.OriginalCount; iblock++)
    {
        txBlocks[iblock].header.frameIndex = frameNumber;
        txBlocks[iblock].header.blockIndex = (uint8_t) iblock;

        for (int isample = 0; isample < nbSamplesPerBlock; isample++)
        {
            txBlocks[iblock].protectedBlock.samples[isample].i = rand();
            txBlocks[iblock].protectedBlock.samples[isample].q = rand();
        }
    }
}

bool Example1::makeFecBlocks(SuperBlock *txBlocks, uint16_t frameIndex)
{
    for (int i = 0; i < m_params.OriginalCount; i++)
    {
        m_txDescriptorBlocks[i].Block = (void *) &txBlocks[i].protectedBlock;
        m_txDescriptorBlocks[i].Index = i;
    }

    if (cm256_encode(m_params, m_txDescriptorBlocks, m_txRecovery))
    {
        std::cerr << "example2: encode failed" << std::endl;
        return false;
    }

    for (int i = 0; i < m_params.RecoveryCount; i++)
    {
        txBlocks[i + m_params.OriginalCount].header.blockIndex = i + m_params.OriginalCount;
        txBlocks[i + m_params.OriginalCount].header.frameIndex = frameIndex;
        txBlocks[i + m_params.OriginalCount].protectedBlock = m_txRecovery[i];
    }

    return true;
}

void Example1::transmitBlocks(SuperBlock *txBlocks, const std::string& destaddress, int destport, int txDelay)
{
    for (int i = 0; i < m_params.OriginalCount + m_params.RecoveryCount; i++)
    {
        m_socket.SendDataGram((const void *) &txBlocks[i], (int) udpSize, destaddress, destport);
        usleep(txDelay);
    }
}


bool example1_tx(const std::string& dataaddress, int dataport, std::atomic_bool& stopFlag)
{
    SuperBlock txBlocks[256];
    Example1 ex1(nbSamplesPerBlock, nbOriginalBlocks, nbRecoveryBlocks);

    for (uint16_t frameNumber = 0; !stopFlag.load(); frameNumber++)
    {
        ex1.makeDataBlocks(txBlocks, frameNumber);

        if (!ex1.makeFecBlocks(txBlocks, frameNumber))
        {
            std::cerr << "example1_tx: encode error" << std::endl;
            break;
        }

        ex1.transmitBlocks(txBlocks, dataaddress, dataport, 300);
    }

    return true;
}

