/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include "cache.h"
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

Cache** cacheArray;

int getBusSignal(int currentState, char operation, ulong prId, int protocol){
    if(protocol == 0){
        if(currentState == INVALID){ //INVALID
            if(operation == 'w'){
                cacheArray[prId]->busRdxCounter++;
                return BusReadX;
            }
            else if(operation == 'r')
                return BusRead;
        }
    }
    if(protocol == 1){

    }
    return '\0';
}

int getbusSignalForDragon(int currentState, char operation, ulong prId, cacheLine* cacheLine, bool isClean){
    if(cacheLine==NULL){ //miss
        if(!isClean){
            if(operation == 'r' && currentState == SharedClean)
                return BusRead;
            else if(operation == 'w' && currentState == SharedModified) {
                cacheArray[prId]->BusUpdCounter++;
                return BusReadNdUdpdate;
            }
        }
    }
    if(cacheLine!=NULL){ //hit
        if(currentState == SharedClean || currentState == SharedModified){
            if(operation == 'w'){
                cacheArray[prId]->BusUpdCounter++;
                return BusUpdate;
            }
        }
    }
    return '\0';
}

void recievingCoreSide(ulong adr, int busSig, ulong prId){
    cacheLine* x = cacheArray[prId]->findLine(adr);
    if(x!=NULL){
    int currentState = x->getCurrentState();
         if((currentState == 2) && ((busSig == BusRead)|| (busSig == BusReadX))){
            cacheArray[prId]->flushCounter++;
            cacheArray[prId]->writeBack(adr);
            cacheArray[prId]->memTransactionCounter++;
        }
    x->invalidate();
    cacheArray[prId]->invalidationCounter++;
    }
}

int getUpdatedCurrentState(int currentState, char operation){
    if(currentState == INVALID){
        if(operation == 'w')
            return MODIFIED;
        else
            return CLEAN;
    }
    if(currentState == CLEAN){
        if(operation == 'w')
            return MODIFIED;
    }
    return currentState;
}

bool checkIfClean(ulong prId, ulong address, ulong numProcessors){
    for(ulong i=0; i < numProcessors; i++){
        if(i!=prId){
        cacheLine* cacheLine = cacheArray[i]->findLine(address);
        if(cacheLine!=NULL)
            return false;
        }
    }
    return true;
}

void dragon(ulong processorId, char operationType, ulong address, ulong numberOfProcessors) {
    ulong busSignal;

    bool isClean = checkIfClean(processorId, address, numberOfProcessors);

    //call requesting core
    busSignal = cacheArray[processorId]->Access(address, operationType, true, isClean);
    //call receiving core
    for(ulong i = 0; i < numberOfProcessors; i++){
        if(i != processorId)       
            cacheArray[i]->recievingCoreSideForDragon(busSignal, address);
    }
}

void MSI(ulong processorId, char operationType, ulong address, ulong numberOfProcessors) {
    cacheLine* cacheLine = cacheArray[processorId]->findLine(address);
    int currentState = INVALID;
    if(cacheLine!=NULL)
        currentState = cacheLine->getCurrentState();
    //1. send busSig to the bus or interconnect
    int busSig = getBusSignal(currentState, operationType, processorId, 0); // 0 = MSI
    //act according to the bus signal on the receiving core.
    for(ulong i = 0; i < numberOfProcessors; i++){
        if(i!=processorId)
            recievingCoreSide(address, busSig, i);
    }

    //2. Fetch the data from memory to the cache and set the currentState as Shared/clean
    cacheArray[processorId]->Access(address, operationType, false, false); // do I need to access here now since already called findLine above?
    cacheArray[processorId]->findLine(address)->setCurrentState(getUpdatedCurrentState(currentState, operationType));
}

void printSimulationResult(ulong num_processors, int protocol){
        for(ulong i = 0; i < num_processors; i++){
        cout << "============ Simulation results (Cache "<< i <<") ============" << endl;
        cout << "01. number of reads:                            " << cacheArray[i]->getReads() << endl;
        cout << "02. number of read misses:                      " << cacheArray[i]->getRM() << endl;
        cout << "03. number of writes:                           " << cacheArray[i]->getWrites() << endl;
        cout << "04. number of write misses:                     " << cacheArray[i]->getWM() << endl; 
        cout << "05. total miss rate:                            "<< std::fixed << std::setprecision(2) << (100*(((float)(cacheArray[i]->getRM()+ cacheArray[i]->getWM()))/((float)(cacheArray[i]->getReads()+ cacheArray[i]->getWrites())))) <<"%"<<endl;
        cout << "06. number of writebacks:                       " << cacheArray[i]->getWB() << endl;
        cout << "07. number of memory transactions:              " << cacheArray[i]->memTransactionCounter << endl;
        if(protocol==0)
            cout << "08. number of invalidations:                    " << cacheArray[i]->invalidationCounter << endl;
        if(protocol == 1)
            cout << "08. number of interventions:                    " << cacheArray[i]->interventionCounter << endl;            
        cout << "09. number of flushes:                          " << cacheArray[i]->flushCounter << endl;        
        if(protocol == 0)
            cout << "10. number of BusRdX:                           " << cacheArray[i]->busRdxCounter << endl;
        if(protocol == 1)
            cout << "10. number of Bus Transactions(BusUpd):         " << cacheArray[i]->BusUpdCounter << endl; 
        }
}

void printDebug(ulong num_processors, int protocol){
        for(ulong i = 0; i < num_processors; i++){
        cout << "============ Simulation results (Cache "<< i <<") ============" << endl;
        cout << "06. number of writebacks:                       " << cacheArray[i]->getWB() << endl;
        cout << "07. number of memory transactions:              " << cacheArray[i]->memTransactionCounter << endl;
        // if(protocol == 1)
        //     cout << "08. number of interventions:                    " << cacheArray[i]->interventionCounter << endl;            
        cout << "09. number of flushes:                          " << cacheArray[i]->flushCounter << endl;        
        }
}

int main(int argc, char *argv[])
{
    ifstream fin;
    FILE * pFile;
    // int currentState;

    if(argv[1] == NULL){
         printf("input format: ");
         printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
         exit(0);
        }

    ulong cache_size     = atoi(argv[1]);
    ulong cache_assoc    = atoi(argv[2]);
    ulong blk_size       = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    ulong protocol       = atoi(argv[5]); /* 0:MODIFIED_MSI 1:DRAGON*/
    char *fname        = (char *) malloc(20);
    fname              = argv[6];


    cout << "===== 506 Personal information =====" << endl;
    cout << "Name" << endl;
    cout << "unity" << endl;
    cout << "ECE406 Students? NO" << endl;

    printf("===== 506 SMP Simulator configuration =====\n");
    // print out simulator configuration here
    cout << "L1_SIZE:                " << cache_size << endl;
    cout << "L1_ASSOC:               " << cache_assoc << endl;
    cout << "L1_BLOCKSIZE:           " << blk_size << endl;
    cout << "NUMBER OF PROCESSORS:   " << num_processors << endl;
    if(protocol == 0)
        cout << "COHERENCE PROTOCOL:     " << "MSI" << endl;
    if(protocol == 1)
        cout << "COHERENCE PROTOCOL:     " << "DRAGON" << endl;
    cout << "TRACE FILE:             " << fname << endl;
    
    // Using pointers so that we can use inheritance */
    cacheArray = (Cache **) malloc(num_processors * sizeof(Cache));
    for(ulong i = 0; i < num_processors; i++) {
            cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size);
    }

    pFile = fopen (fname,"r");
    if(pFile == 0)
    {   
        printf("Trace file problem\n");
        exit(0);
    }
    
    ulong proc;
    char op;
    ulong addr;

    // int line = 1;
    while(fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF)
    {
        if(protocol == 0) {
            MSI(proc, op, addr, num_processors);
        }
        if(protocol == 1){
            dragon(proc, op, addr, num_processors);
        }
    }

    printSimulationResult(num_processors, protocol);

    fclose(pFile);
    
}