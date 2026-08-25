// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef LOGFILE_H
#define LOGFILE_H

/*
 * Logfile is a class for specifying the log file format.
 * The loggers (loggers.h) face both
 *  1. the log file, using the base class Logger (defined here)
 *  2. the simulator, using the base classes in loggertypes.h
 */

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "config.h"
#include "eventlist.h"
#include "queue.h"

//#include "network.h" // mod
//#include "networkDevices.h"
//#include "loggertypes.h"
//#include "network.h"

class Logfile;
class Logger;

class RawLogEvent {
 public:
    RawLogEvent(double time, uint32_t type, uint32_t id, uint32_t ev, 
		double val1, double val2, double val3);
    virtual string str();
    double _time;
    uint32_t _type;
    uint32_t _id;
    uint32_t _ev;
    double _val1; 
    double _val2; 
    double _val3;
};

class Logfile: public EventSource{
   public:
      Logfile(const string& filename, EventList& eventlist, int type, networkDevices* net, dynamicTopology* top, string sys);
      //~Logfile();
      //void setStartTime(simtime_picosec starttime);
      //void write(const string& msg);
      //void writeName(Logged& logged);
      //void writeRecord(uint32_t type, uint32_t id, uint32_t ev, double val1, double val2, double val3); // prepend uint64_t time
      //void addLogger(Logger& logger);
      void doNextEvent();
      void writeQueueHeader();
      //void writeQueue(vector<vector<vector<int>>> &workingQueue, int timeSlice);
      void writeQueue();
      void writeLogHeader();
      void writeLogFCT(int src, int dst, int flowSize, double duration, double startTime); // Prepare for FCT log 

      simtime_picosec _starttime;
      int logType;

      networkDevices* netDev;
      dynamicTopology* dynTop;

   private:
      EventList& _eventlist;
      string systemName;

      //vector<Logger*> _loggers;
      // managing the files for writing
      //void transposeLog();
      stringstream _preamble;
      string _logfilename;
      string _queuefilename;
      FILE* _logfile;
      //bool _startedTrace;
      long int _numRecords;
};

#endif
