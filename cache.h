/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum {
   NOTVALID = 0,
   VALID,
   DIRTY
};

enum PrState {
   INVALID = 0,
   CLEAN,
   MODIFIED
};

enum PrStateDragon {
   Exclusive=0,
   SharedClean,
   SharedModified,
   ModifiedDragon
};

enum PrSig {
   PrRead,
   PrWrite,
   PrRdMiss,
   PrWriteMiss
};

enum BusSignal {
   BusRead = 0,
   BusReadX,
   BusUpdate,
   BusReadNdUdpdate,
   BusEmpty
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:notValid, 1:valid, 2:dirty 
   ulong seq; 
 
public:
   int currentState = 0;
   cacheLine()                { tag = 0; Flags = 0; }
   ulong getTag()             { return tag; }
   ulong getFlags()           { return Flags;}
   ulong getSeq()             { return seq; }
   void setSeq(ulong Seq)     { seq = Seq;}
   void setFlags(ulong flags) {  Flags = flags;}
   void setTag(ulong a)       { tag = a; }
   void invalidate()          { tag = 0; Flags = NOTVALID; currentState = INVALID; } //useful function
   bool isValid()             { return ((Flags) != NOTVALID); }
   int getCurrentState()   {return currentState;}
   void setCurrentState(int currentSt) {currentState = currentSt;}
};

class Cache
{
// protected:
public:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads,readMisses,writes,writeMisses,writeBacks;


   //******///
   //add coherence counters here///
   //******///
   ulong busRdxCounter, flushCounter, memTransactionCounter, invalidationCounter;
   ulong interventionCounter, BusUpdCounter;

   cacheLine **cache;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
   

    ulong currentCycle;  
     
    Cache(int,int,int);
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM()     {return readMisses;} 
   ulong getWM()     {return writeMisses;} 
   ulong getReads()  {return reads;}       
   ulong getWrites() {return writes;}
   ulong getWB()     {return writeBacks;}
   
   void writeBack(ulong) {writeBacks++;}
   ulong Access(ulong,uchar,bool,bool);
   void printStats();
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   ulong DragonStateUpdate(cacheLine *, uchar, bool is_hit, bool);
   void  recievingCoreSideForDragon(ulong, ulong);
   //******///

};

#endif
