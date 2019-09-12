//
//  AstroHaven.h
//  CAstroHaven
//
//  Created by Rodolphe Pineau on 2017-4-6
//  AstroHaven X2 plugin

#ifndef __AstroHaven__
#define __AstroHaven__
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#ifdef SB_WIN_BUILD
#include <time.h>
#endif

#include <math.h>
#include <string.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#define PLUGIN_DEBUG 2

#define SERIAL_BUFFER_SIZE 4096
#define MAX_TIMEOUT 250
#define ND_LOG_BUFFER_SIZE 256
// error codes
enum AstroHavenErrors {PluginOK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, NO_DATA_TIMEOUT};

// Error code
enum AstroHavenShutterState {OPEN=1, OPENING, OPENING_A, OPENING_B, CLOSED, CLOSING, CLOSING_A, CLOSING_B, UNKNOWN};

class CAstroHaven
{
public:
    CAstroHaven();
    ~CAstroHaven();

    int         Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // Dome commands
    int syncDome(double dAz, double dEl);
    int parkDome(void);
    int unparkDome(void);
    int gotoAzimuth(double dNewAz);
    int openShutter();
    int closeShutter();
	int	findHome();
	
    // command complete functions
    int isGoToComplete(bool &bComplete);
    int isOpenComplete(bool &bComplete);
    int isCloseComplete(bool &bComplete);
    int isParkComplete(bool &bComplete);
    int isUnparkComplete(bool &bComplete);
    int isFindHomeComplete(bool &bComplete);

    double getCurrentAz();
    double getCurrentEl();

protected:

    int             readResponse(char *pszRespBuffer, unsigned int nBufferLen, unsigned long nExpectRespLen = 1, int TimeOut = MAX_TIMEOUT);
    int             domeCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             setShutterStateToClosed();
    
    SleeperInterface    *m_pSleeper;

    bool            m_bIsConnected;

    double          m_dCurrentAzPosition;
    double          m_dCurrentElPosition;

    int             m_nASideState;
    int             m_nBSideState;
	int             m_nCurrentShutterAction;

    SerXInterface   *m_pSerx;

    int             m_nShutterState;

#ifdef PLUGIN_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif
