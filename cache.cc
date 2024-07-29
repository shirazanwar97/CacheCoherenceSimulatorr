/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;


Cache::Cache(int s,int a,int b )
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   busRdxCounter = flushCounter = memTransactionCounter = invalidationCounter = 0;
   interventionCounter = BusUpdCounter = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
  
   //*******************//
   //initialize your counters here//
   //*******************//
 
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
         cache[i][j].invalidate();
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
ulong Cache::Access(ulong addr, uchar op, bool isDragon, bool isClean) {
   ulong busSignal = BusEmpty;
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
         
   if(op == 'w') writes++;
   else          reads++;
   
   cacheLine * line = findLine(addr);
   if(line == NULL)/*miss*/
   {
      memTransactionCounter++;
      if(op == 'w') writeMisses++;
      else readMisses++;

      cacheLine *newline = fillLine(addr);
      if(op == 'w') newline->setFlags(DIRTY);    
      
      if(isDragon)   
         busSignal = DragonStateUpdate(newline, op, isClean, false);
      
   }
   else
   {
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if(op == 'w') line->setFlags(DIRTY);
      if(isDragon)   
         busSignal = DragonStateUpdate(line, op, isClean, true);
   }

   return busSignal;
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
   if(cache[i][j].isValid()) {
      if(cache[i][j].getTag() == tag)
      {
         pos = j; 
         break; 
      }
   }
   if(pos == assoc) {
      return NULL;
   }
   else {
      return &(cache[i][pos]); 
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getFlags() == DIRTY && (victim->getCurrentState() != SharedClean)) {
      writeBack(addr);
      memTransactionCounter++;
   }
      
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats()
{
   printf("===== Simulation results      =====\n");
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
}

ulong Cache::DragonStateUpdate(cacheLine* line, uchar op, bool isClean, bool istagMatch) {
   if(!istagMatch) {  //miss
    if(isClean) {
      if(op == 'r') 
         line->setCurrentState(Exclusive); 
      else 
         line->setCurrentState(ModifiedDragon);
     } //clean
    else  //not clean
    {
     if(op == 'r')  {
         line->setCurrentState(SharedClean); 
         return BusRead;
         }
     else {
         line->setCurrentState(SharedModified); 
         BusUpdCounter++; 
         return BusReadNdUdpdate;
      }
    }
   }
   else  { //hit
     if(isClean) { //clean
       if(op == 'w')  {
         ulong currState = line->getCurrentState();
         line->setCurrentState(ModifiedDragon);
         if((currState == SharedClean) || (currState == SharedModified)) 
         {
            BusUpdCounter++; 
            return BusUpdate;
         }
       }
     }
     else {//not clean
       if(op == 'w') { 
         BusUpdCounter++; 
         line->setCurrentState(SharedModified); 
         return BusUpdate;
         }
     }
   }
return '\0';
}

void Cache::recievingCoreSideForDragon(ulong busSignal, ulong addr) 
{
   cacheLine * line = findLine(addr);
   if(line != NULL) { //hit
    ulong currState = line->getCurrentState();
    if(busSignal == BusRead) 
    {
      if(currState == Exclusive) {
        line->setCurrentState(SharedClean); 
        interventionCounter++;
        }
      else if(currState == SharedModified)  {
        flushCounter++; 
        writeBack(addr); 
        memTransactionCounter++; 
        }
      else if(currState == ModifiedDragon) {
        flushCounter++; 
        writeBack(addr); 
        memTransactionCounter++; 
        interventionCounter++;
        line->setCurrentState(SharedModified);
        }
    }

    if(busSignal == BusUpdate)
      if(currState == SharedModified || currState == SharedClean) 
         line->setCurrentState(SharedClean);
       
    
    if(busSignal == BusReadNdUdpdate){
      if(currState == SharedModified)  {
            flushCounter++; 
            writeBack(addr); 
            memTransactionCounter++; 
        }
      if(currState == ModifiedDragon)  {
            flushCounter++; 
            writeBack(addr); 
            memTransactionCounter++; 
            interventionCounter++;
        }
      if(currState == Exclusive)  
            interventionCounter++;

      line->setCurrentState(SharedClean);
      }
   }
}