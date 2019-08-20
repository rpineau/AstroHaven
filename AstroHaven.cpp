//
//  AstroHaven.cpp
//  CAstroHaven
//
//  Created by Rodolphe Pineau on 2017-4-6
//  AstroHaven X2 plugin

#include "AstroHaven.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

CAstroHaven::CAstroHaven()
{
    // set some sane values
    m_pSerx = NULL;
    m_bIsConnected = false;

    m_dCurrentAzPosition = 0.0;
    m_dCurrentElPosition = 0.0;

    m_nShutterState = UNKNOWN;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_AstroHaven.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_AstroHaven.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_AstroHaven.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::CAstroHaven] Version 2019_08_19_19015.\n", timestamp);
    fprintf(Logfile, "[%s] [CAstroHaven::CAstroHaven] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CAstroHaven::~CAstroHaven()
{

}

int CAstroHaven::Connect(const char *pszPort)
{
    int nErr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Connecting to %s.\n", timestamp, pszPort);
    fflush(Logfile);
#endif
    
    // 9600 8N1
    if(m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    m_pSleeper->sleep(2000);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Connected to %s.\n", timestamp, pszPort);
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] Getting shutter state.\n", timestamp);
    fflush(Logfile);
#endif

    // get the current shutter state just to check the connection, we don't care about the state for now.
    nErr = getShutterState(m_nShutterState);
    if(nErr) {
        nErr = false;
        return ERR_COMMNOLINK;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] AstroHaven init done\n", timestamp);
    fprintf(Logfile, "[%s] [CAstroHaven::Connect] m_bIsConnected = %s.\n", timestamp, m_bIsConnected?"True":"False");
    fflush(Logfile);
#endif

    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);

    return SB_OK;
}


void CAstroHaven::Disconnect()
{
    if(m_bIsConnected) {
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
}


int CAstroHaven::readResponse(char *pszRespBuffer, unsigned int nBufferLen, int TimeOut)
{
    int nErr = PluginOK;
    unsigned long ulBytesRead = 0;
    unsigned int ulTotalBytesRead = 0;
    char *pszBufPtr;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, TimeOut);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CAstroHaven::readResponse] readFile error\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }
        if (ulBytesRead !=1) {// timeout
            if(!ulTotalBytesRead)
                nErr = BAD_CMD_RESPONSE;
            else
                nErr = PluginOK;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CAstroHaven::readResponse] Timeout, ulTotalBytesRead = %ul\n", timestamp, ulTotalBytesRead);
            fflush(Logfile);
#endif
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != 0x00 && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \r

    return nErr;
}


int CAstroHaven::domeCommand(const char *pszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    m_pSerx->purgeTxRx();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CAstroHaven::domeCommand] Sending %s\n", timestamp, pszCmd);
    fflush(Logfile);
#endif

    nErr = m_pSerx->writeFile((void *)pszCmd, strlen(pszCmd), nBytesWrite);
    m_pSerx->flushTx();
    if(nErr)
        return nErr;

    // only read the response if we expect a response.
    if(pszResult) {
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr)
            return nErr;
        strncpy(pszResult, szResp, nResultMaxLen);
    }

    return nErr;
}


int CAstroHaven::getShutterState(int &nState)
{
    int nErr = PluginOK;
    bool bComplete = false;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    // send a close to make sure we start in the closed state, it should respond with AX and BY
    nErr = closeShutter();
    while (!bComplete) {
        m_pSleeper->sleep(1000);
        isCloseComplete(bComplete);
    }
        
    nState = m_nShutterState = CLOSED;
    
    return nErr;
}




int CAstroHaven::syncDome(double dAz, double dEl)
{

    m_dCurrentAzPosition = dAz;
    m_dCurrentElPosition = dEl;
    
    return PluginOK;
}

int CAstroHaven::parkDome()
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    return nErr;

}

int CAstroHaven::unparkDome()
{
    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);
    return 0;
}

int CAstroHaven::gotoAzimuth(double dNewAz)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dNewAz;
    if(m_nShutterState == OPEN)
        m_dCurrentElPosition = 90.0;
    else
        m_dCurrentElPosition = 0.0;

    return nErr;
}

int CAstroHaven::openShutter()
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    nErr = domeCommand("ab", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    m_nASideState = OPENING;
    m_nBSideState = OPENING;
    // we we already open ?
    if(strstr(szResp, "x")) {
        m_nASideState = OPEN;
        // if we need to sequence the shutter, we start opening the other side here.
    }
    if(strstr(szResp, "y")) {
        m_nBSideState = OPEN;
    }

    return nErr;
}

int CAstroHaven::closeShutter()
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = domeCommand("AB", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    m_nASideState = CLOSING;
    m_nBSideState = CLOSING;
    // we we already closed ?
    if(strstr(szResp, "X")) {
        m_nASideState = CLOSED;
        // if we need to sequence the shutter, we start closing the other side here.
    }
    if(strstr(szResp, "Y")) {
        m_nBSideState = CLOSED;
    }
    return nErr;
}


int CAstroHaven::isGoToComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;
}

int CAstroHaven::isOpenComplete(bool &bComplete)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_nASideState == OPEN && m_nBSideState == OPEN) {
        bComplete = true;
        return nErr;
    }

    bComplete = false;
    
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(!strlen(szResp)) //no response.
        return nErr;
    
    if(strstr(szResp, "x")) {
        m_nASideState = OPEN;
        // if we need to sequence the shutter, we start opening the other side here.
    }
    if(strstr(szResp, "y")) {
        m_nBSideState = OPEN;
    }

    if(m_nASideState == OPEN && m_nBSideState == OPEN) {
        bComplete = true;
        m_nShutterState = OPEN;
    }
    return nErr;
}

int CAstroHaven::isCloseComplete(bool &bComplete)
{
    int nErr = PluginOK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;
    
    bComplete = false;
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(!strlen(szResp)) //no response.
        return nErr;

    if(strstr(szResp, "X")) {
        m_nASideState = CLOSED;
        // if we need to sequence the shutter, we start closing the other side here.
    }
    if(strstr(szResp, "Y")) {
        m_nBSideState = CLOSED;
    }
    
    if(m_nASideState == CLOSED && m_nBSideState == CLOSED) {
        bComplete = true;
        m_nShutterState = CLOSED;
    }
    return nErr;
}


int CAstroHaven::isParkComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;
    return nErr;
}

int CAstroHaven::isUnparkComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;

    return nErr;
}

int CAstroHaven::isFindHomeComplete(bool &bComplete)
{
    int nErr = PluginOK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;

}


#pragma mark - Getter / Setter


double CAstroHaven::getCurrentAz()
{
    return m_dCurrentAzPosition;
}

double CAstroHaven::getCurrentEl()
{
    return m_dCurrentElPosition;
}
