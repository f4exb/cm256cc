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
#include <fstream>
#include <sys/time.h>

#include "../cm256.h"

long long getUSecs()
{
    struct timeval tp;
    gettimeofday(&tp, 0);
    return (long long) tp.tv_sec * 1000000L + tp.tv_usec;
}

bool example4()
{
#pragma pack(push, 1)
    struct Sample
    {
        uint16_t i;
        uint16_t q;
    };
    struct Header
    {
        uint16_t frameIndex;
        uint8_t  blockIndex;
        uint8_t  filler;
    };

    static const int samplesPerBlock = (512 - sizeof(Header)) / sizeof(Sample);

    struct ProtectedBlock
    {
        Sample samples[samplesPerBlock];
    };
    struct SuperBlock
    {
        Header         header;
        ProtectedBlock protectedBlock;
    };

    struct FileHeader
    {
        cm256_encoder_params m_cm256Params;
        int m_txBlocks;
    };
#pragma pack(pop)

    if (cm256_init())
    {
        return false;
    }

    std::ifstream rxFile;
    rxFile.open("cm256.test", std::ios::in | std::ios::binary);

    FileHeader fileHeader;

    rxFile.read((char *) &fileHeader, sizeof(FileHeader));

    std::cerr << "example4 Rx:"
            << " BlockBytes: " << fileHeader.m_cm256Params.BlockBytes
            << " OriginalCount: " << fileHeader.m_cm256Params.OriginalCount
            << " RecoveryCount: " << fileHeader.m_cm256Params.RecoveryCount
            << " m_txBlocks: " << fileHeader.m_txBlocks << std::endl;

    SuperBlock* rxBuffer = new SuperBlock[256]; // received blocks
    int nbRxBlocks = fileHeader.m_txBlocks;

    for (int i = 0; i < nbRxBlocks; i++)
    {
        rxFile.read((char *) &rxBuffer[i], sizeof(SuperBlock));
    }

    rxFile.close();

    Sample *samplesBuffer = new Sample[samplesPerBlock * (fileHeader.m_cm256Params.OriginalCount)];
    ProtectedBlock* retrievedDataBuffer = (ProtectedBlock *) samplesBuffer;
    ProtectedBlock* recoveryBuffer = new ProtectedBlock[fileHeader.m_cm256Params.OriginalCount];
    cm256_block rxDescriptorBlocks[fileHeader.m_cm256Params.OriginalCount];
    int recoveryCount = 0;
    int nbBlocks = 0;

    for (int i = 0; i < nbRxBlocks; i++)
    {
        int blockIndex = rxBuffer[i].header.blockIndex;

        if (nbBlocks < fileHeader.m_cm256Params.OriginalCount) // not enough data store it
        {
            rxDescriptorBlocks[i].Index = blockIndex;

            if (blockIndex < fileHeader.m_cm256Params.OriginalCount) // it's a data block
            {
                retrievedDataBuffer[blockIndex] = rxBuffer[i].protectedBlock;
                rxDescriptorBlocks[i].Block = (void *) &retrievedDataBuffer[blockIndex];
            }
            else // it's a recovery block
            {
                recoveryBuffer[recoveryCount] = rxBuffer[i].protectedBlock;
                rxDescriptorBlocks[i].Block = (void *) &recoveryBuffer[recoveryCount];
                recoveryCount++;
            }
        }

        nbBlocks++;

        if (nbBlocks == fileHeader.m_cm256Params.OriginalCount) // ready
        {
            if (recoveryCount > 0)
            {
                long long ts = getUSecs();

                if (cm256_decode(fileHeader.m_cm256Params, rxDescriptorBlocks))
                {
                    delete[] rxBuffer;
                    delete[] samplesBuffer;
                    delete[] recoveryBuffer;

                    return false;
                }

                long long usecs = getUSecs() - ts;
                std::cerr << "recover missing blocks..." << std::endl;

                for (int ir = 0; ir < recoveryCount; ir++) // recover missing blocks
                {
                    int blockIndex = rxDescriptorBlocks[fileHeader.m_cm256Params.OriginalCount - recoveryCount + ir].Index;
                    retrievedDataBuffer[blockIndex] = recoveryBuffer[ir];
                    std::cerr << ir << ":" << blockIndex << ": " << recoveryBuffer[ir].samples[0].i << std::endl;
                }
            }
        }
    }

    std::cerr << "final..." << std::endl;

    SuperBlock* refBuffer = new SuperBlock[256]; // reference blocks
    std::ifstream refFile;
    refFile.open("ref.test", std::ios::in | std::ios::binary);

    FileHeader refFileHeader;

    refFile.read((char *) &refFileHeader, sizeof(FileHeader));

    for (int i = 0; i < refFileHeader.m_cm256Params.OriginalCount + refFileHeader.m_cm256Params.RecoveryCount; i++)
    {
        refFile.read((char *) &refBuffer[i], sizeof(SuperBlock));
    }

    refFile.close();

    for (int i = 0; i < fileHeader.m_cm256Params.OriginalCount; i++)
    {
        bool compOKi = true;
        bool compOKq = true;

        for (int k = 0; k < samplesPerBlock; k++)
        {
            if (retrievedDataBuffer[i].samples[k].i != refBuffer[i].protectedBlock.samples[k].i)
            {
                std::cerr << i << ": error: " << k << ": i: " << retrievedDataBuffer[i].samples[k].i << "/" << refBuffer[i].protectedBlock.samples[k].i << std::endl;
                compOKi = false;
                break;
            }

            if (retrievedDataBuffer[i].samples[k].q != refBuffer[i].protectedBlock.samples[k].q)
            {
                std::cerr << i << ": error: " << k << ": q: " << retrievedDataBuffer[i].samples[k].q << "/" << refBuffer[i].protectedBlock.samples[k].q << std::endl;
                compOKq = false;
                break;
            }
        }

        if (compOKi && compOKq)
        {
            std::cerr << i << ": OK" << std::endl;
        }
    }

    delete[] refBuffer;
    delete[] samplesBuffer;
    delete[] recoveryBuffer;
    delete[] rxBuffer;

    return true;
}


int main()
{
    std::cerr << "example4:" << std::endl;

    if (!example4())
    {
        std::cerr << "example4 failed" << std::endl << std::endl;
        return 1;
    }

    std::cerr << "example4 successful" << std::endl;

    return 0;
}



