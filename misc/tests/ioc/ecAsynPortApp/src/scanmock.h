class ScanMock
{
    epicsMessageQueueId commandq;
    const char * socket_name;
public: 
    epicsThreadId scanner_thread;
    epicsThreadId timer_thread;
    epicsThreadId reader_thread;
    ScanMock(const char * socket_name);
    void scanner();
    void timer();
    void reader();
};
